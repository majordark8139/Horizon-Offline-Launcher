#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#ifndef COINIT_MULTITHREADED
#define COINIT_MULTITHREADED     0x0
#define COINIT_APARTMENTTHREADED 0x2
#endif

#ifndef REGDB_E_CLASSNOTREG
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154L)
#endif

#ifndef COWAIT_INPUTAVAILABLE
#define COWAIT_INPUTAVAILABLE 0x4
#endif

/* External classic ole32 declarations */
extern HRESULT WINAPI CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit);
extern void    WINAPI CoUninitialize(void);
extern HRESULT WINAPI CoCreateInstanceEx(
    const GUID *rclsid,
    void *punkOuter,
    DWORD dwClsCtx,
    void *pServerInfo,
    DWORD dwCount,
    void *pResults
);
extern HRESULT WINAPI CoWaitForMultipleHandles(
    DWORD dwFlags,
    DWORD dwTimeout,
    ULONG cHandles,
    const HANDLE *pHandles,
    LPDWORD lpdwIndex
);

/* ============================================================================
 * Minimal HSTRING Implementation
 * ============================================================================ */

#define HSTRING_FLAG_REFERENCE 0x1u

typedef struct ProxyHString {
    uint32_t flags;         /* bit 0 = 1: stack fast-pass ref, 0: heap-allocated */
    uint32_t length;        /* WCHAR count excluding null terminator */
    volatile LONG refcount; /* used when heap-allocated */
    uint32_t reserved;
    const WCHAR *buffer;    /* null-terminated UTF-16LE string */
} ProxyHString;

typedef ProxyHString *PROXY_HSTRING;

static const WCHAR g_empty_wstr[1] = { L'\0' };

static ProxyHString *AllocHeapHString(const WCHAR *src, UINT32 length) {
    SIZE_T allocSize = sizeof(ProxyHString) + ((SIZE_T)length + 1) * sizeof(WCHAR);
    ProxyHString *hdr = (ProxyHString *)HeapAlloc(GetProcessHeap(), 0, allocSize);
    if (!hdr) return NULL;

    WCHAR *dst = (WCHAR *)(hdr + 1);
    if (src && length > 0) {
        CopyMemory(dst, src, (SIZE_T)length * sizeof(WCHAR));
    }
    dst[length] = L'\0';

    hdr->flags = 0;
    hdr->length = length;
    hdr->refcount = 1;
    hdr->reserved = 0;
    hdr->buffer = dst;
    return hdr;
}

HRESULT WINAPI WindowsCreateString(
    const WCHAR *sourceString,
    UINT32 length,
    PROXY_HSTRING *string
) {
    if (!string) return E_INVALIDARG;
    *string = NULL;

    if (length == 0) return S_OK;
    if (!sourceString) return E_POINTER;

    ProxyHString *hdr = AllocHeapHString(sourceString, length);
    if (!hdr) return E_OUTOFMEMORY;

    *string = hdr;
    return S_OK;
}

HRESULT WINAPI WindowsCreateStringReference(
    const WCHAR *sourceString,
    UINT32 length,
    ProxyHString *hstringHeader,
    PROXY_HSTRING *string
) {
    if (!string || !hstringHeader) return E_INVALIDARG;
    *string = NULL;

    if (length == 0) return S_OK;
    if (!sourceString) return E_POINTER;
    if (sourceString[length] != L'\0') return E_INVALIDARG;

    hstringHeader->flags = HSTRING_FLAG_REFERENCE;
    hstringHeader->length = length;
    hstringHeader->refcount = 1;
    hstringHeader->reserved = 0;
    hstringHeader->buffer = sourceString;
    *string = hstringHeader;
    return S_OK;
}

HRESULT WINAPI WindowsDeleteString(PROXY_HSTRING string) {
    if (!string) return S_OK;
    if (string->flags & HSTRING_FLAG_REFERENCE) return S_OK;

    if (InterlockedDecrement(&string->refcount) == 0) {
        HeapFree(GetProcessHeap(), 0, string);
    }
    return S_OK;
}

HRESULT WINAPI WindowsDuplicateString(
    PROXY_HSTRING string,
    PROXY_HSTRING *newString
) {
    if (!newString) return E_INVALIDARG;
    *newString = NULL;
    if (!string) return S_OK;

    if (string->flags & HSTRING_FLAG_REFERENCE) {
        ProxyHString *copy = AllocHeapHString(string->buffer, string->length);
        if (!copy) return E_OUTOFMEMORY;
        *newString = copy;
        return S_OK;
    }

    InterlockedIncrement(&string->refcount);
    *newString = string;
    return S_OK;
}

const WCHAR *WINAPI WindowsGetStringRawBuffer(
    PROXY_HSTRING string,
    UINT32 *length
) {
    if (!string) {
        if (length) *length = 0;
        return g_empty_wstr;
    }
    if (length) *length = string->length;
    return string->buffer ? string->buffer : g_empty_wstr;
}

UINT32 WINAPI WindowsGetStringLen(PROXY_HSTRING string) {
    return string ? string->length : 0;
}

BOOL WINAPI WindowsIsStringEmpty(PROXY_HSTRING string) {
    return (!string || string->length == 0) ? TRUE : FALSE;
}

HRESULT WINAPI WindowsStringHasEmbeddedNull(
    PROXY_HSTRING string,
    BOOL *hasEmbedNull
) {
    if (!hasEmbedNull) return E_INVALIDARG;
    *hasEmbedNull = FALSE;
    if (!string || !string->buffer) return S_OK;

    for (UINT32 i = 0; i < string->length; ++i) {
        if (string->buffer[i] == L'\0') {
            *hasEmbedNull = TRUE;
            break;
        }
    }
    return S_OK;
}

HRESULT WINAPI WindowsCompareStringOrdinal(
    PROXY_HSTRING string1,
    PROXY_HSTRING string2,
    INT32 *result
) {
    if (!result) return E_INVALIDARG;

    UINT32 len1 = 0, len2 = 0;
    const WCHAR *buf1 = WindowsGetStringRawBuffer(string1, &len1);
    const WCHAR *buf2 = WindowsGetStringRawBuffer(string2, &len2);

    int cmp = CompareStringOrdinal(buf1, (int)len1, buf2, (int)len2, FALSE);
    if (cmp == 0) return HRESULT_FROM_WIN32(GetLastError());

    *result = cmp - 2;
    return S_OK;
}

HRESULT WINAPI WindowsPreallocateStringBuffer(
    UINT32 length,
    WCHAR **charBuffer,
    void **bufferHandle
) {
    if (!charBuffer || !bufferHandle) return E_INVALIDARG;
    *charBuffer = NULL;
    *bufferHandle = NULL;

    ProxyHString *hdr = AllocHeapHString(NULL, length);
    if (!hdr) return E_OUTOFMEMORY;

    *charBuffer = (WCHAR *)(hdr + 1);
    *bufferHandle = hdr;
    return S_OK;
}

HRESULT WINAPI WindowsPromoteStringBuffer(
    void *bufferHandle,
    PROXY_HSTRING *string
) {
    if (!string) return E_INVALIDARG;
    *string = NULL;
    if (!bufferHandle) return E_INVALIDARG;

    ProxyHString *hdr = (ProxyHString *)bufferHandle;
    if (hdr->length == 0) {
        HeapFree(GetProcessHeap(), 0, hdr);
        return S_OK;
    }

    ((WCHAR *)(hdr + 1))[hdr->length] = L'\0';
    *string = hdr;
    return S_OK;
}

HRESULT WINAPI WindowsDeleteStringBuffer(void *bufferHandle) {
    if (bufferHandle) {
        HeapFree(GetProcessHeap(), 0, bufferHandle);
    }
    return S_OK;
}

/* ============================================================================
 * Win8+ COM Polyfills
 * ============================================================================ */

static INIT_ONCE g_mta_once = INIT_ONCE_STATIC_INIT;
static HANDLE g_mta_ready_event = NULL;

static DWORD WINAPI MtaKeeperThread(LPVOID param) {
    (void)param;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (g_mta_ready_event) {
        SetEvent(g_mta_ready_event);
    }
    Sleep(INFINITE);
    return 0;
}

static BOOL CALLBACK InitMtaOnce(PINIT_ONCE initOnce, PVOID param, PVOID *context) {
    (void)initOnce;
    (void)param;
    (void)context;

    g_mta_ready_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_mta_ready_event) return FALSE;

    HANDLE hThread = CreateThread(NULL, 0, MtaKeeperThread, NULL, 0, NULL);
    if (!hThread) {
        CloseHandle(g_mta_ready_event);
        g_mta_ready_event = NULL;
        return FALSE;
    }

    WaitForSingleObject(g_mta_ready_event, INFINITE);
    CloseHandle(hThread);
    CloseHandle(g_mta_ready_event);
    g_mta_ready_event = NULL;
    return TRUE;
}

HRESULT WINAPI CoIncrementMTAUsage(void **pCookie) {
    if (!pCookie) return E_INVALIDARG;
    *pCookie = NULL;

    if (!InitOnceExecuteOnce(&g_mta_once, InitMtaOnce, NULL, NULL)) {
        return E_OUTOFMEMORY;
    }

    *pCookie = (void *)(uintptr_t)0x1;
    return S_OK;
}

HRESULT WINAPI CoDecrementMTAUsage(void *cookie) {
    if (!cookie) return E_INVALIDARG;
    return S_OK;
}

HRESULT WINAPI CoWaitForMultipleObjects(
    DWORD dwFlags,
    DWORD dwTimeout,
    ULONG cHandles,
    const HANDLE *pHandles,
    LPDWORD lpdwIndex
) {
    DWORD legacyFlags = (dwFlags & 0x2u) ? COWAIT_INPUTAVAILABLE : 0;
    return CoWaitForMultipleHandles(legacyFlags, dwTimeout, cHandles, pHandles, lpdwIndex);
}

HRESULT WINAPI CoCreateInstanceFromApp(
    const GUID *clsid,
    void *punkOuter,
    DWORD dwClsCtx,
    void *reserved,
    DWORD dwCount,
    void *pResults
) {
    (void)reserved;
    return CoCreateInstanceEx(clsid, punkOuter, dwClsCtx, NULL, dwCount, pResults);
}

HRESULT WINAPI CoDecodeProxy(
    DWORD dwClientPid,
    UINT64 ui64ProxyAddress,
    void *pServerInformation
) {
    (void)dwClientPid;
    (void)ui64ProxyAddress;
    (void)pServerInformation;
    return E_NOTIMPL;
}

/* ============================================================================
 * WinRT Runtime & Restricted Error Info Stubs
 * ============================================================================ */

HRESULT WINAPI RoInitialize(int initType) {
    DWORD coInit = (initType == 0) ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED;
    return CoInitializeEx(NULL, coInit);
}

void WINAPI RoUninitialize(void) {
    CoUninitialize();
}

HRESULT WINAPI RoGetActivationFactory(
    PROXY_HSTRING activatableClassId,
    const GUID *iid,
    void **factory
) {
    (void)activatableClassId;
    (void)iid;
    if (factory) *factory = NULL;
    return REGDB_E_CLASSNOTREG;
}

HRESULT WINAPI RoActivateInstance(
    PROXY_HSTRING activatableClassId,
    void **instance
) {
    (void)activatableClassId;
    if (instance) *instance = NULL;
    return REGDB_E_CLASSNOTREG;
}

BOOL WINAPI RoOriginateError(HRESULT error, PROXY_HSTRING message) {
    (void)error;
    (void)message;
    return FALSE;
}

BOOL WINAPI RoOriginateErrorW(HRESULT error, UINT max, const WCHAR *message) {
    (void)error;
    (void)max;
    (void)message;
    return FALSE;
}

BOOL WINAPI RoOriginateLanguageException(
    HRESULT error,
    PROXY_HSTRING message,
    void *languageException
) {
    (void)error;
    (void)message;
    (void)languageException;
    return FALSE;
}

BOOL WINAPI RoTransformError(HRESULT oldError, HRESULT newError, PROXY_HSTRING message) {
    (void)oldError;
    (void)newError;
    (void)message;
    return FALSE;
}

BOOL WINAPI RoTransformErrorW(
    HRESULT oldError,
    HRESULT newError,
    UINT cchMax,
    const WCHAR *message
) {
    (void)oldError;
    (void)newError;
    (void)cchMax;
    (void)message;
    return FALSE;
}

HRESULT WINAPI SetRestrictedErrorInfo(void *pRestrictedErrorInfo) {
    (void)pRestrictedErrorInfo;
    return S_OK;
}

HRESULT WINAPI GetRestrictedErrorInfo(void **ppRestrictedErrorInfo) {
    if (ppRestrictedErrorInfo) *ppRestrictedErrorInfo = NULL;
    return S_FALSE; /* S_FALSE = no extended error object */
}

HRESULT WINAPI RoReportUnhandledError(void *pRestrictedErrorInfo) {
    (void)pRestrictedErrorInfo;
    return S_OK;
}

HRESULT WINAPI RoGetAgileReference(
    DWORD options,
    const GUID *riid,
    void *pUnk,
    void **ppAgileReference
) {
    (void)options;
    (void)riid;
    (void)pUnk;
    if (ppAgileReference) *ppAgileReference = NULL;
    return E_NOTIMPL;
}

HRESULT WINAPI RoRegisterActivationFactories(
    PROXY_HSTRING *activatableClassIds,
    void **activationFactoryCallbacks,
    UINT32 count,
    void **cookie
) {
    (void)activatableClassIds;
    (void)activationFactoryCallbacks;
    (void)count;
    if (cookie) *cookie = NULL;
    return E_NOTIMPL;
}

void WINAPI RoRevokeActivationFactories(void *cookie) {
    (void)cookie;
}

HRESULT WINAPI RoGetParameterizedTypeInstanceIID(
    UINT32 nameElementCount,
    const WCHAR **nameElements,
    void *metaDataLocator,
    GUID *iid,
    void **pExtra
) {
    (void)nameElementCount;
    (void)nameElements;
    (void)metaDataLocator;
    (void)iid;
    if (pExtra) *pExtra = NULL;
    return E_NOTIMPL;
}

/* ============================================================================
 * DllMain
 * ============================================================================ */

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}
