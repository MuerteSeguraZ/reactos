/*
 * PROJECT:     ReactOS COM+ Administration
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     COM+ administration DLL entry points and class factory
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#include "comadmin_private.h"

#define COMADMIN_CLSID_STRING       L"{F618C514-DFB8-11D1-A2CF-00805FC79235}"
#define COMADMIN_PROGID             L"COMAdmin.COMAdminCatalog"
#define COMADMIN_PROGID_VERSIONED   L"COMAdmin.COMAdminCatalog.1"
#define COMADMIN_CLASS_NAME         L"Catalog2 Class"

HINSTANCE g_hInstance = NULL;
LONG g_ObjectCount = 0;
static LONG g_LockCount = 0;

static
HRESULT
WINAPI
factory_QueryInterface(
    _In_ IClassFactory *iface,
    _In_ REFIID riid,
    _COM_Outptr_ PVOID *ppvObject)
{
    if (ppvObject == NULL)
        return E_POINTER;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory))
    {
        *ppvObject = iface;
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static
ULONG
WINAPI
factory_AddRef(
    _In_ IClassFactory *iface)
{
    return 2;
}

static
ULONG
WINAPI
factory_Release(
    _In_ IClassFactory *iface)
{
    return 1;
}

static
HRESULT
WINAPI
factory_CreateInstance(
    _In_ IClassFactory *iface,
    _In_opt_ IUnknown *pUnkOuter,
    _In_ REFIID riid,
    _COM_Outptr_ PVOID *ppvObject)
{
    if (ppvObject == NULL)
        return E_POINTER;

    *ppvObject = NULL;
    return ComAdminCreateCatalog(pUnkOuter, riid, ppvObject);
}

static
HRESULT
WINAPI
factory_LockServer(
    _In_ IClassFactory *iface,
    _In_ BOOL fLock)
{
    if (fLock)
        InterlockedIncrement(&g_LockCount);
    else
        InterlockedDecrement(&g_LockCount);

    return S_OK;
}

static const IClassFactoryVtbl g_FactoryVtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    factory_CreateInstance,
    factory_LockServer
};

static IClassFactory g_Factory = { &g_FactoryVtbl };

static
HRESULT
RegSetString(
    _In_ HKEY hRoot,
    _In_ PCWSTR SubKey,
    _In_opt_ PCWSTR ValueName,
    _In_ PCWSTR Value)
{
    HKEY hKey;
    LONG Result;

    Result = RegCreateKeyExW(hRoot, SubKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (Result != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(Result);

    Result = RegSetValueExW(hKey, ValueName, 0, REG_SZ, (const BYTE *)Value,
                            (lstrlenW(Value) + 1) * sizeof(WCHAR));
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(Result);
}

BOOL
WINAPI
DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD fdwReason,
    _In_opt_ PVOID lpvReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            g_hInstance = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);
            break;

        case DLL_PROCESS_DETACH:
            if (lpvReserved == NULL)
                ComAdminReleaseTypeLib();
            break;
    }

    return TRUE;
}

/**
 * @brief
 * Returns the class factory for a class served by this DLL.
 *
 * @param[in] rclsid
 * The class to create a factory for.
 *
 * @param[in] riid
 * The interface requested on the factory.
 *
 * @param[out] ppv
 * Receives the factory interface pointer.
 *
 * @return
 * S_OK on success, CLASS_E_CLASSNOTAVAILABLE if the class isn't served here.
 **/
_Check_return_
HRESULT
WINAPI
DllGetClassObject(
    _In_ REFCLSID rclsid,
    _In_ REFIID riid,
    _Outptr_ PVOID *ppv)
{
    if (ppv == NULL)
        return E_POINTER;

    *ppv = NULL;
    if (!IsEqualCLSID(rclsid, &CLSID_COMAdminCatalog))
        return CLASS_E_CLASSNOTAVAILABLE;

    return IClassFactory_QueryInterface(&g_Factory, riid, ppv);
}

/**
 * @brief
 * Tells COM whether the DLL can be unloaded.
 *
 * @return
 * S_OK if no objects or locks remain, S_FALSE otherwise.
 **/
HRESULT
WINAPI
DllCanUnloadNow(VOID)
{
    return (g_ObjectCount == 0 && g_LockCount == 0) ? S_OK : S_FALSE;
}

/**
 * @brief
 * Registers the COM+ catalog class and its type library.
 *
 * @return
 * S_OK on success, an error code otherwise.
 **/
HRESULT
WINAPI
DllRegisterServer(VOID)
{
    WCHAR Path[MAX_PATH];
    ITypeLib *TypeLib;
    HRESULT hr;

    if (GetModuleFileNameW(g_hInstance, Path, ARRAYSIZE(Path)) == 0)
        return HRESULT_FROM_WIN32(GetLastError());

    hr = LoadTypeLibEx(Path, REGKIND_REGISTER, &TypeLib);
    if (FAILED(hr))
        return hr;

    ITypeLib_Release(TypeLib);

    hr = RegSetString(HKEY_CLASSES_ROOT, L"CLSID\\" COMADMIN_CLSID_STRING, NULL, COMADMIN_CLASS_NAME);
    if (FAILED(hr))
        return hr;

    hr = RegSetString(HKEY_CLASSES_ROOT, L"CLSID\\" COMADMIN_CLSID_STRING L"\\InprocServer32", NULL, Path);
    if (FAILED(hr))
        return hr;

    hr = RegSetString(HKEY_CLASSES_ROOT, L"CLSID\\" COMADMIN_CLSID_STRING L"\\InprocServer32",
                      L"ThreadingModel", L"Both");
    if (FAILED(hr))
        return hr;

    hr = RegSetString(HKEY_CLASSES_ROOT, L"CLSID\\" COMADMIN_CLSID_STRING L"\\ProgID", NULL, COMADMIN_PROGID_VERSIONED);
    if (FAILED(hr))
        return hr;

    hr = RegSetString(HKEY_CLASSES_ROOT, L"CLSID\\" COMADMIN_CLSID_STRING L"\\VersionIndependentProgID",
                      NULL, COMADMIN_PROGID);
    if (FAILED(hr))
        return hr;

    hr = RegSetString(HKEY_CLASSES_ROOT, COMADMIN_PROGID, NULL, COMADMIN_CLASS_NAME);
    if (FAILED(hr))
        return hr;

    /* CLSIDFromProgID doesn't follow CurVer here */
    hr = RegSetString(HKEY_CLASSES_ROOT, COMADMIN_PROGID L"\\CLSID", NULL, COMADMIN_CLSID_STRING);
    if (FAILED(hr))
        return hr;

    hr = RegSetString(HKEY_CLASSES_ROOT, COMADMIN_PROGID L"\\CurVer", NULL, COMADMIN_PROGID_VERSIONED);
    if (FAILED(hr))
        return hr;

    hr = RegSetString(HKEY_CLASSES_ROOT, COMADMIN_PROGID_VERSIONED, NULL, COMADMIN_CLASS_NAME);
    if (FAILED(hr))
        return hr;

    return RegSetString(HKEY_CLASSES_ROOT, COMADMIN_PROGID_VERSIONED L"\\CLSID", NULL, COMADMIN_CLSID_STRING);
}

/**
 * @brief
 * Removes the registration made by DllRegisterServer.
 *
 * @return
 * S_OK on success, an error code otherwise.
 **/
HRESULT
WINAPI
DllUnregisterServer(VOID)
{
    static const PCWSTR Keys[] =
    {
        L"CLSID\\" COMADMIN_CLSID_STRING L"\\VersionIndependentProgID",
        L"CLSID\\" COMADMIN_CLSID_STRING L"\\ProgID",
        L"CLSID\\" COMADMIN_CLSID_STRING L"\\InprocServer32",
        L"CLSID\\" COMADMIN_CLSID_STRING,
        COMADMIN_PROGID L"\\CurVer",
        COMADMIN_PROGID L"\\CLSID",
        COMADMIN_PROGID,
        COMADMIN_PROGID_VERSIONED L"\\CLSID",
        COMADMIN_PROGID_VERSIONED
    };
    ULONG i;

    for (i = 0; i < ARRAYSIZE(Keys); i++)
        RegDeleteKeyW(HKEY_CLASSES_ROOT, Keys[i]);

    return UnRegisterTypeLib(&LIBID_COMAdmin, 1, 0, LANG_NEUTRAL,
#ifdef _WIN64
                             SYS_WIN64);
#else
                             SYS_WIN32);
#endif // _WIN64
}