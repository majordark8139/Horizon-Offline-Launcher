#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#pragma comment(lib, "advapi32.lib")

#ifndef ERROR_INVALID_PARAMETER
#define ERROR_INVALID_PARAMETER 87L
#endif
#ifndef ERROR_TIMEOUT
#define ERROR_TIMEOUT 1460L
#endif

typedef BOOL (WINAPI *pfnWaitOnAddress_t)(
    volatile VOID *Address,
    PVOID CompareAddress,
    SIZE_T AddressSize,
    DWORD dwMilliseconds
);
typedef VOID (WINAPI *pfnWakeByAddress_t)(PVOID Address);
typedef BOOLEAN (WINAPI *pfnRtlGenRandom_t)(PVOID RandomBuffer, ULONG RandomBufferLength);
typedef VOID (WINAPI *pfnGetSystemTimePreciseAsFileTime_t)(LPFILETIME lpSystemTimeAsFileTime);

static pfnWaitOnAddress_t g_RealWaitOnAddress = NULL;
static pfnWakeByAddress_t g_RealWakeByAddressSingle = NULL;
static pfnWakeByAddress_t g_RealWakeByAddressAll = NULL;
static pfnRtlGenRandom_t g_RtlGenRandom = NULL;
static pfnGetSystemTimePreciseAsFileTime_t g_RealGetSystemTimePreciseAsFileTime = NULL;
static INIT_ONCE g_CompatInitOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK InitCompatOnce(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context) {
    (void)InitOnce;
    (void)Parameter;
    (void)Context;

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32) {
        g_RealWaitOnAddress = (pfnWaitOnAddress_t)GetProcAddress(hKernel32, "WaitOnAddress");
        g_RealWakeByAddressSingle = (pfnWakeByAddress_t)GetProcAddress(hKernel32, "WakeByAddressSingle");
        g_RealWakeByAddressAll = (pfnWakeByAddress_t)GetProcAddress(hKernel32, "WakeByAddressAll");
        g_RealGetSystemTimePreciseAsFileTime = (pfnGetSystemTimePreciseAsFileTime_t)GetProcAddress(
            hKernel32, "GetSystemTimePreciseAsFileTime"
        );
    }

    HMODULE hAdvapi32 = GetModuleHandleW(L"advapi32.dll");
    if (!hAdvapi32) {
        hAdvapi32 = LoadLibraryW(L"advapi32.dll");
    }
    if (hAdvapi32) {
        g_RtlGenRandom = (pfnRtlGenRandom_t)GetProcAddress(hAdvapi32, "SystemFunction036");
    }

    return TRUE;
}

static void EnsureCompatInit(void) {
    InitOnceExecuteOnce(&g_CompatInitOnce, InitCompatOnce, NULL, NULL);
}

/* ============================================================================
 * Deadlock-free WaitOnAddress / WakeByAddress* polyfill using Win7-native
 * SRWLOCK + CONDITION_VARIABLE (replaces keyed events which block wakers on
 * zero waiters).
 * ============================================================================ */

#define NUM_WAIT_BUCKETS 256

typedef struct WaiterNode {
    struct WaiterNode *prev;
    struct WaiterNode *next;
    volatile VOID *address;
    CONDITION_VARIABLE cv;
    BOOL linked;
} WaiterNode;

typedef struct WaitBucket {
    SRWLOCK lock;
    WaiterNode *head;
    WaiterNode *tail;
} WaitBucket;

static WaitBucket g_wait_buckets[NUM_WAIT_BUCKETS];

static WaitBucket *GetWaitBucket(volatile VOID *addr) {
    uintptr_t v = (uintptr_t)addr;
#ifdef _WIN64
    v ^= v >> 17;
    v *= (uintptr_t)0xbf58476d1ce4e5b9ULL;
    v ^= v >> 31;
#else
    v ^= v >> 16;
    v *= (uintptr_t)0x45d9f3bU;
    v ^= v >> 16;
#endif
    return &g_wait_buckets[v & (NUM_WAIT_BUCKETS - 1)];
}

static void LinkWaiter(WaitBucket *b, WaiterNode *node) {
    node->prev = b->tail;
    node->next = NULL;
    if (b->tail) {
        b->tail->next = node;
    } else {
        b->head = node;
    }
    b->tail = node;
    node->linked = TRUE;
}

static void UnlinkWaiter(WaitBucket *b, WaiterNode *node) {
    if (!node->linked) return;
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        b->head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        b->tail = node->prev;
    }
    node->prev = NULL;
    node->next = NULL;
    node->linked = FALSE;
}

static BOOL BytesEqualAtomic(
    volatile const VOID *Address,
    const VOID *CompareAddress,
    SIZE_T AddressSize
) {
    switch (AddressSize) {
        case 1:  return (*(volatile const uint8_t *)Address) == (*(const uint8_t *)CompareAddress);
        case 2:  return (*(volatile const uint16_t *)Address) == (*(const uint16_t *)CompareAddress);
        case 4:  return (*(volatile const uint32_t *)Address) == (*(const uint32_t *)CompareAddress);
        case 8:  return (*(volatile const uint64_t *)Address) == (*(const uint64_t *)CompareAddress);
        default: return memcmp((const void *)Address, CompareAddress, AddressSize) == 0;
    }
}

BOOL WINAPI Win7_WaitOnAddress(
    volatile VOID *Address,
    PVOID CompareAddress,
    SIZE_T AddressSize,
    DWORD dwMilliseconds
) {
    EnsureCompatInit();
    if (g_RealWaitOnAddress) {
        return g_RealWaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds);
    }

    if (!Address || !CompareAddress ||
        (AddressSize != 1 && AddressSize != 2 && AddressSize != 4 && AddressSize != 8)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    WaitBucket *b = GetWaitBucket(Address);
    AcquireSRWLockExclusive(&b->lock);

    if (!BytesEqualAtomic(Address, CompareAddress, AddressSize)) {
        ReleaseSRWLockExclusive(&b->lock);
        return TRUE;
    }

    if (dwMilliseconds == 0) {
        ReleaseSRWLockExclusive(&b->lock);
        SetLastError(ERROR_TIMEOUT);
        return FALSE;
    }

    WaiterNode node;
    node.address = Address;
    InitializeConditionVariable(&node.cv);
    LinkWaiter(b, &node);

    ULONGLONG startTick = (dwMilliseconds != INFINITE) ? GetTickCount64() : 0;
    BOOL ok = TRUE;
    while (node.linked) {
        DWORD waitMs = INFINITE;
        if (dwMilliseconds != INFINITE) {
            ULONGLONG elapsed = GetTickCount64() - startTick;
            if (elapsed >= (ULONGLONG)dwMilliseconds) {
                ok = FALSE;
                SetLastError(ERROR_TIMEOUT);
                break;
            }
            waitMs = (DWORD)((ULONGLONG)dwMilliseconds - elapsed);
        }

        if (!SleepConditionVariableSRW(&node.cv, &b->lock, waitMs, 0)) {
            if (GetLastError() == ERROR_TIMEOUT) {
                if (!node.linked) {
                    ok = TRUE;
                } else {
                    ok = FALSE;
                    SetLastError(ERROR_TIMEOUT);
                }
                break;
            }
        }
    }

    if (node.linked) {
        UnlinkWaiter(b, &node);
    }
    ReleaseSRWLockExclusive(&b->lock);
    return ok;
}

static void WakeAddressImpl(PVOID Address, BOOL wakeAll) {
    WaitBucket *b = GetWaitBucket(Address);
    AcquireSRWLockExclusive(&b->lock);
    for (WaiterNode *n = b->head; n != NULL; ) {
        WaiterNode *next = n->next;
        if (n->address == Address) {
            UnlinkWaiter(b, n);
            WakeConditionVariable(&n->cv);
            if (!wakeAll) break;
        }
        n = next;
    }
    ReleaseSRWLockExclusive(&b->lock);
}

VOID WINAPI Win7_WakeByAddressSingle(PVOID Address) {
    EnsureCompatInit();
    if (g_RealWakeByAddressSingle) {
        g_RealWakeByAddressSingle(Address);
        return;
    }
    WakeAddressImpl(Address, FALSE);
}

VOID WINAPI Win7_WakeByAddressAll(PVOID Address) {
    EnsureCompatInit();
    if (g_RealWakeByAddressAll) {
        g_RealWakeByAddressAll(Address);
        return;
    }
    WakeAddressImpl(Address, TRUE);
}

BOOL WINAPI WaitOnAddress(
    volatile VOID *Address,
    PVOID CompareAddress,
    SIZE_T AddressSize,
    DWORD dwMilliseconds
) {
    return Win7_WaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds);
}

VOID WINAPI WakeByAddressSingle(PVOID Address) {
    Win7_WakeByAddressSingle(Address);
}

VOID WINAPI WakeByAddressAll(PVOID Address) {
    Win7_WakeByAddressAll(Address);
}

pfnWaitOnAddress_t __imp_WaitOnAddress = Win7_WaitOnAddress;
pfnWakeByAddress_t __imp_WakeByAddressSingle = Win7_WakeByAddressSingle;
pfnWakeByAddress_t __imp_WakeByAddressAll = Win7_WakeByAddressAll;

/* ============================================================================
 * ProcessPrng polyfill for bcryptprimitives.dll
 * ============================================================================ */

BOOL WINAPI Win7_ProcessPrng(PBYTE pbBuffer, SIZE_T cbBuffer) {
    EnsureCompatInit();
    if (g_RtlGenRandom) {
        return (BOOL)g_RtlGenRandom(pbBuffer, (ULONG)cbBuffer);
    }
    return FALSE;
}

BOOL WINAPI ProcessPrng(PBYTE pbBuffer, SIZE_T cbBuffer) {
    return Win7_ProcessPrng(pbBuffer, cbBuffer);
}

typedef BOOL (WINAPI *pfnProcessPrng_t)(PBYTE pbBuffer, SIZE_T cbBuffer);
pfnProcessPrng_t __imp_ProcessPrng = Win7_ProcessPrng;

/* ============================================================================
 * GetSystemTimePreciseAsFileTime polyfill for kernel32.dll
 * ============================================================================ */

VOID WINAPI Win7_GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime) {
    EnsureCompatInit();
    if (g_RealGetSystemTimePreciseAsFileTime) {
        g_RealGetSystemTimePreciseAsFileTime(lpSystemTimeAsFileTime);
    } else {
        GetSystemTimeAsFileTime(lpSystemTimeAsFileTime);
    }
}

VOID WINAPI GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime) {
    Win7_GetSystemTimePreciseAsFileTime(lpSystemTimeAsFileTime);
}

pfnGetSystemTimePreciseAsFileTime_t __imp_GetSystemTimePreciseAsFileTime = Win7_GetSystemTimePreciseAsFileTime;
