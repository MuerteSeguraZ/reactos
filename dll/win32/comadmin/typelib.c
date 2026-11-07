/*
 * PROJECT:     ReactOS COM+ Administration
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Type library access for IDispatch
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#include "comadmin_private.h"

static ITypeLib *g_TypeLib = NULL;

static
HRESULT
ComAdminLoadTypeLib(
    _Outptr_ ITypeLib **TypeLib)
{
    WCHAR Path[MAX_PATH];
    ITypeLib *Loaded;
    ITypeLib *Previous;
    HRESULT hr;

    if (g_TypeLib != NULL)
    {
        ITypeLib_AddRef(g_TypeLib);
        *TypeLib = g_TypeLib;
        return S_OK;
    }

    if (GetModuleFileNameW(g_hInstance, Path, ARRAYSIZE(Path)) == 0)
        return HRESULT_FROM_WIN32(GetLastError());

    hr = LoadTypeLibEx(Path, REGKIND_NONE, &Loaded);
    if (FAILED(hr))
        return hr;

    Previous = InterlockedCompareExchangePointer((PVOID *)&g_TypeLib, Loaded, NULL);
    if (Previous != NULL)
    {
        ITypeLib_Release(Loaded);
        Loaded = Previous;
    }

    ITypeLib_AddRef(Loaded);
    *TypeLib = Loaded;
    return S_OK;
}

HRESULT
ComAdminGetTypeInfo(
    _In_ REFIID Iid,
    _Outptr_ ITypeInfo **TypeInfo)
{
    ITypeLib *TypeLib;
    HRESULT hr;

    *TypeInfo = NULL;

    hr = ComAdminLoadTypeLib(&TypeLib);
    if (FAILED(hr))
        return hr;

    hr = ITypeLib_GetTypeInfoOfGuid(TypeLib, Iid, TypeInfo);
    ITypeLib_Release(TypeLib);
    return hr;
}

HRESULT
ComAdminGetIDsOfNames(
    _In_ REFIID Iid,
    _In_reads_(NameCount) LPOLESTR *Names,
    _In_ UINT NameCount,
    _Out_writes_(NameCount) DISPID *DispIds)
{
    ITypeInfo *TypeInfo;
    HRESULT hr;

    hr = ComAdminGetTypeInfo(Iid, &TypeInfo);
    if (FAILED(hr))
        return hr;

    hr = DispGetIDsOfNames(TypeInfo, Names, NameCount, DispIds);
    ITypeInfo_Release(TypeInfo);
    return hr;
}

HRESULT
ComAdminInvoke(
    _In_ PVOID Instance,
    _In_ REFIID Iid,
    _In_ DISPID DispId,
    _In_ WORD Flags,
    _In_ DISPPARAMS *Params,
    _Out_opt_ VARIANT *Result,
    _Out_opt_ EXCEPINFO *ExcepInfo,
    _Out_opt_ UINT *ArgErr)
{
    ITypeInfo *TypeInfo;
    HRESULT hr;

    hr = ComAdminGetTypeInfo(Iid, &TypeInfo);
    if (FAILED(hr))
        return hr;

    hr = DispInvoke(Instance, TypeInfo, DispId, Flags, Params, Result, ExcepInfo, ArgErr);
    ITypeInfo_Release(TypeInfo);
    return hr;
}

VOID
ComAdminReleaseTypeLib(VOID)
{
    ITypeLib *TypeLib = InterlockedExchangePointer((PVOID *)&g_TypeLib, NULL);

    if (TypeLib != NULL)
        ITypeLib_Release(TypeLib);
}