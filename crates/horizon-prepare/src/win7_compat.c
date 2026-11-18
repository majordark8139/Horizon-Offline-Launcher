#include <windows.h>

typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_TIMEOUT ((NTSTATUS)0x00000102L)

typedef NTSTATUS (NTAPI *pfnNtWaitForKeyedEvent)(
    HANDLE KeyedEventHandle,
    PVOID KeyValue,
    BOOLEAN Alertable,
    PLARGE_INTEGER Timeout
);
typedef NTSTATUS (NTAPI *pfnNtReleaseKeyedEvent)(
    HANDLE KeyedEventHandle,
    PVOID KeyValue,
    BOOLEAN Alertable,
    PLARGE_INTEGER Timeout
);

typedef BOOL (WINAPI *pfnWaitOnAddress_t)(
    volatile VOID *Address,
    PVOID CompareAddress,
    SIZE_T AddressSize,
    DWORD dwMilliseconds
);
typedef VOID (WINAPI *pfnWakeByAddress_t)(PVOID Address);

static pfnNtWaitForKeyedEvent g_NtWaitForKeyedEvent = NULL;
static pfnNtReleaseKeyedEvent g_NtReleaseKeyedEvent = NULL;
static pfnWaitOnAddress_t g_RealWaitOnAddress = NULL;
static pfnWakeByAddress_t g_RealWakeByAddressSingle = NULL;
static pfnWakeByAddress_t g_RealWakeByAddressAll = NULL;
static volatile LONG g_Initialized = 0;

static void InitSyncPrimitives(void) {
    if (InterlockedCompareExchange(&g_Initialized, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (hKernel32) {
            g_RealWaitOnAddress = (pfnWaitOnAddress_t)GetProcAddress(hKernel32, "WaitOnAddress");
            g_RealWakeByAddressSingle = (pfnWakeByAddress_t)GetProcAddress(hKernel32, "WakeByAddressSingle");
            g_RealWakeByAddressAll = (pfnWakeByAddress_t)GetProcAddress(hKernel32, "WakeByAddressAll");
        }

        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            g_NtWaitForKeyedEvent = (pfnNtWaitForKeyedEvent)GetProcAddress(hNtdll, "NtWaitForKeyedEvent");
            g_NtReleaseKeyedEvent = (pfnNtReleaseKeyedEvent)GetProcAddress(hNtdll, "NtReleaseKeyedEvent");
        }
    }
}

BOOL WINAPI Win7_WaitOnAddress(
    volatile VOID *Address,
    PVOID CompareAddress,
    SIZE_T AddressSize,
    DWORD dwMilliseconds
) {
    InitSyncPrimitives();

    if (g_RealWaitOnAddress) {
        return g_RealWaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds);
    }

    if (AddressSize == 1) {
        if (*(volatile char *)Address != *(const char *)CompareAddress) return TRUE;
    } else if (AddressSize == 2) {
        if (*(volatile short *)Address != *(const short *)CompareAddress) return TRUE;
    } else if (AddressSize == 4) {
        if (*(volatile int *)Address != *(const int *)CompareAddress) return TRUE;
    } else if (AddressSize == 8) {
        if (*(volatile long long *)Address != *(const long long *)CompareAddress) return TRUE;
    } else {
        if (memcmp((const void *)Address, CompareAddress, AddressSize) != 0) return TRUE;
    }

    LARGE_INTEGER timeout;
    PLARGE_INTEGER pTimeout = NULL;
    if (dwMilliseconds != INFINITE) {
        timeout.QuadPart = -10000LL * (LONGLONG)dwMilliseconds;
        pTimeout = &timeout;
    }

    if (g_NtWaitForKeyedEvent) {
        NTSTATUS status = g_NtWaitForKeyedEvent(NULL, (PVOID)Address, FALSE, pTimeout);
        if (status == STATUS_TIMEOUT) {
            SetLastError(ERROR_TIMEOUT);
            return FALSE;
        }
        return TRUE;
    }

    Sleep(1);
    return TRUE;
}

VOID WINAPI Win7_WakeByAddressSingle(PVOID Address) {
    InitSyncPrimitives();
    if (g_RealWakeByAddressSingle) {
        g_RealWakeByAddressSingle(Address);
        return;
    }
    if (g_NtReleaseKeyedEvent) {
        g_NtReleaseKeyedEvent(NULL, Address, FALSE, NULL);
    }
}

VOID WINAPI Win7_WakeByAddressAll(PVOID Address) {
    InitSyncPrimitives();
    if (g_RealWakeByAddressAll) {
        g_RealWakeByAddressAll(Address);
        return;
    }
    if (g_NtReleaseKeyedEvent) {
        while (g_NtReleaseKeyedEvent(NULL, Address, FALSE, NULL) == STATUS_SUCCESS) {}
    }
}

/* Primary symbols for direct calls */
BOOL WINAPI WaitOnAddress(volatile VOID *Address, PVOID CompareAddress, SIZE_T AddressSize, DWORD dwMilliseconds) {
    return Win7_WaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds);
}
VOID WINAPI WakeByAddressSingle(PVOID Address) {
    Win7_WakeByAddressSingle(Address);
}
VOID WINAPI WakeByAddressAll(PVOID Address) {
    Win7_WakeByAddressAll(Address);
}

/* __imp_ pointers for DLL import calls */
pfnWaitOnAddress_t __imp_WaitOnAddress = Win7_WaitOnAddress;
pfnWakeByAddress_t __imp_WakeByAddressSingle = Win7_WakeByAddressSingle;
pfnWakeByAddress_t __imp_WakeByAddressAll = Win7_WakeByAddressAll;
