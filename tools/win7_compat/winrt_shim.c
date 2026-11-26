#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#ifndef COINIT_MULTITHREADED
#define COINIT_MULTITHREADED     0x0
#define COINIT_APARTMENTTHREADED 0x2
#endif

#ifndef REGDB_E_CLASSNOTREG
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154L)
#endif

extern HRESULT WINAPI CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit);
extern void    WINAPI CoUninitialize(void);

HRESULT WINAPI RoInitialize(int initType) {
    DWORD coInit = (initType == 0) ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED;
    return CoInitializeEx(NULL, coInit);
}

void WINAPI RoUninitialize(void) {
    CoUninitialize();
}

HRESULT WINAPI RoGetActivationFactory(
    void *activatableClassId,
    const GUID *iid,
    void **factory
) {
    (void)activatableClassId;
    (void)iid;
    if (factory) *factory = NULL;
    return REGDB_E_CLASSNOTREG;
}

HRESULT WINAPI RoActivateInstance(
    void *activatableClassId,
    void **instance
) {
    (void)activatableClassId;
    if (instance) *instance = NULL;
    return REGDB_E_CLASSNOTREG;
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
    void **activatableClassIds,
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

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}
