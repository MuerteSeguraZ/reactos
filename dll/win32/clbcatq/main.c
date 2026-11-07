/*
 * PROJECT:     ReactOS COM+ catalog
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     COM+ configuration catalog DLL entry points
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#include <stdarg.h>

#define WIN32_NO_STATUS

#include <windef.h>
#include <winbase.h>
#include <ole2.h>

BOOL
WINAPI
DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD fdwReason,
    _In_opt_ PVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(hinstDLL);

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
 * CLASS_E_CLASSNOTAVAILABLE, no class is served yet.
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
    return CLASS_E_CLASSNOTAVAILABLE;
}

/**
 * @brief
 * Tells COM whether the DLL can be unloaded.
 *
 * @return
 * S_OK, since the DLL holds no objects.
 **/
HRESULT
WINAPI
DllCanUnloadNow(VOID)
{
    return S_OK;
}

/**
 * @brief
 * Registers the classes of this DLL.
 *
 * @return
 * S_OK, since there is nothing to register yet.
 **/
HRESULT
WINAPI
DllRegisterServer(VOID)
{
    return S_OK;
}

/**
 * @brief
 * Removes the registration made by DllRegisterServer.
 *
 * @return
 * S_OK, since there is nothing to remove yet.
 **/
HRESULT
WINAPI
DllUnregisterServer(VOID)
{
    return S_OK;
}