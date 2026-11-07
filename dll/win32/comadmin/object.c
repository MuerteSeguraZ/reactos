/*
 * PROJECT:     ReactOS COM+ Administration
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     ICatalogObject implementation
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#include "comadmin_private.h"

typedef struct _COMADMIN_OBJECT
{
    ICatalogObject ICatalogObject_iface;
    LONG RefCount;
    COMADMIN_COLLECTION_TYPE Type;
    PWSTR Key;
    PWSTR Path;
} COMADMIN_OBJECT, *PCOMADMIN_OBJECT;

static
PCOMADMIN_OBJECT
impl_from_ICatalogObject(
    _In_ ICatalogObject *iface)
{
    return CONTAINING_RECORD(iface, COMADMIN_OBJECT, ICatalogObject_iface);
}

static
PCWSTR
ObjectGetKeyPropertyName(
    _In_ COMADMIN_COLLECTION_TYPE Type)
{
    switch (Type)
    {
        case CollectionApplications:
            return L"ID";
        case CollectionComponents:
            return L"CLSID";
        default:
            return L"Name";
    }
}

static
HRESULT
ObjectKeyToVariant(
    _In_ PCOMADMIN_OBJECT Object,
    _Out_ VARIANT *Value)
{
    VariantInit(Value);
    V_BSTR(Value) = SysAllocString(Object->Key);
    if (V_BSTR(Value) == NULL)
        return E_OUTOFMEMORY;

    V_VT(Value) = VT_BSTR;
    return S_OK;
}

static
HRESULT
WINAPI
object_QueryInterface(
    _In_ ICatalogObject *iface,
    _In_ REFIID riid,
    _COM_Outptr_ PVOID *ppvObject)
{
    if (ppvObject == NULL)
        return E_POINTER;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IDispatch) ||
        IsEqualIID(riid, &IID_ICatalogObject))
    {
        *ppvObject = iface;
        ICatalogObject_AddRef(iface);
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static
ULONG
WINAPI
object_AddRef(
    _In_ ICatalogObject *iface)
{
    PCOMADMIN_OBJECT This = impl_from_ICatalogObject(iface);

    return InterlockedIncrement(&This->RefCount);
}

static
ULONG
WINAPI
object_Release(
    _In_ ICatalogObject *iface)
{
    PCOMADMIN_OBJECT This = impl_from_ICatalogObject(iface);
    LONG RefCount = InterlockedDecrement(&This->RefCount);

    if (RefCount == 0)
    {
        HeapFree(GetProcessHeap(), 0, This->Key);
        HeapFree(GetProcessHeap(), 0, This->Path);
        HeapFree(GetProcessHeap(), 0, This);
        InterlockedDecrement(&g_ObjectCount);
    }

    return RefCount;
}

static
HRESULT
WINAPI
object_GetTypeInfoCount(
    _In_ ICatalogObject *iface,
    _Out_ UINT *pctinfo)
{
    *pctinfo = 1;
    return S_OK;
}

static
HRESULT
WINAPI
object_GetTypeInfo(
    _In_ ICatalogObject *iface,
    _In_ UINT iTInfo,
    _In_ LCID lcid,
    _Outptr_ ITypeInfo **ppTInfo)
{
    if (iTInfo != 0)
        return DISP_E_BADINDEX;

    return ComAdminGetTypeInfo(&IID_ICatalogObject, ppTInfo);
}

static
HRESULT
WINAPI
object_GetIDsOfNames(
    _In_ ICatalogObject *iface,
    _In_ REFIID riid,
    _In_reads_(cNames) LPOLESTR *rgszNames,
    _In_ UINT cNames,
    _In_ LCID lcid,
    _Out_writes_(cNames) DISPID *rgDispId)
{
    return ComAdminGetIDsOfNames(&IID_ICatalogObject, rgszNames, cNames, rgDispId);
}

static
HRESULT
WINAPI
object_Invoke(
    _In_ ICatalogObject *iface,
    _In_ DISPID dispIdMember,
    _In_ REFIID riid,
    _In_ LCID lcid,
    _In_ WORD wFlags,
    _In_ DISPPARAMS *pDispParams,
    _Out_opt_ VARIANT *pVarResult,
    _Out_opt_ EXCEPINFO *pExcepInfo,
    _Out_opt_ UINT *puArgErr)
{
    return ComAdminInvoke(iface, &IID_ICatalogObject, dispIdMember, wFlags,
                          pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static
HRESULT
WINAPI
object_get_Value(
    _In_ ICatalogObject *iface,
    _In_ BSTR bstrPropName,
    _Out_ VARIANT *pvarRetVal)
{
    PCOMADMIN_OBJECT This = impl_from_ICatalogObject(iface);
    HRESULT hr;

    if (bstrPropName == NULL || pvarRetVal == NULL)
        return E_INVALIDARG;

    if (lstrcmpiW(bstrPropName, ObjectGetKeyPropertyName(This->Type)) == 0)
        return ObjectKeyToVariant(This, pvarRetVal);

    hr = StoreReadValue(This->Path, bstrPropName, pvarRetVal);
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        if (lstrcmpiW(bstrPropName, L"Name") == 0)
            return ObjectKeyToVariant(This, pvarRetVal);

        return E_INVALIDARG;
    }

    return hr;
}

static
HRESULT
WINAPI
object_put_Value(
    _In_ ICatalogObject *iface,
    _In_ BSTR bstrPropName,
    _In_ VARIANT val)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
object_get_Key(
    _In_ ICatalogObject *iface,
    _Out_ VARIANT *pvarRetVal)
{
    PCOMADMIN_OBJECT This = impl_from_ICatalogObject(iface);

    if (pvarRetVal == NULL)
        return E_INVALIDARG;

    return ObjectKeyToVariant(This, pvarRetVal);
}

static
HRESULT
WINAPI
object_get_Name(
    _In_ ICatalogObject *iface,
    _Out_ VARIANT *pvarRetVal)
{
    PCOMADMIN_OBJECT This = impl_from_ICatalogObject(iface);
    HRESULT hr;

    if (pvarRetVal == NULL)
        return E_INVALIDARG;

    hr = StoreReadValue(This->Path, L"Name", pvarRetVal);
    if (FAILED(hr))
        return ObjectKeyToVariant(This, pvarRetVal);

    return hr;
}

static
HRESULT
WINAPI
object_IsPropertyReadOnly(
    _In_ ICatalogObject *iface,
    _In_ BSTR bstrPropName,
    _Out_ VARIANT_BOOL *pbRetVal)
{
    if (pbRetVal == NULL)
        return E_INVALIDARG;

    *pbRetVal = VARIANT_TRUE;
    return S_OK;
}

static
HRESULT
WINAPI
object_get_Valid(
    _In_ ICatalogObject *iface,
    _Out_ VARIANT_BOOL *pbRetVal)
{
    if (pbRetVal == NULL)
        return E_INVALIDARG;

    *pbRetVal = VARIANT_TRUE;
    return S_OK;
}

static
HRESULT
WINAPI
object_IsPropertyWriteOnly(
    _In_ ICatalogObject *iface,
    _In_ BSTR bstrPropName,
    _Out_ VARIANT_BOOL *pbRetVal)
{
    if (pbRetVal == NULL)
        return E_INVALIDARG;

    *pbRetVal = VARIANT_FALSE;
    return S_OK;
}

static const ICatalogObjectVtbl g_ObjectVtbl =
{
    object_QueryInterface,
    object_AddRef,
    object_Release,
    object_GetTypeInfoCount,
    object_GetTypeInfo,
    object_GetIDsOfNames,
    object_Invoke,
    object_get_Value,
    object_put_Value,
    object_get_Key,
    object_get_Name,
    object_IsPropertyReadOnly,
    object_get_Valid,
    object_IsPropertyWriteOnly
};

HRESULT
ComAdminCreateObject(
    _In_ COMADMIN_COLLECTION_TYPE Type,
    _In_ PCWSTR Key,
    _In_ PCWSTR Path,
    _Outptr_ ICatalogObject **Object)
{
    PCOMADMIN_OBJECT This;

    *Object = NULL;

    This = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*This));
    if (This == NULL)
        return E_OUTOFMEMORY;

    This->Key = StoreDuplicateString(Key);
    This->Path = StoreDuplicateString(Path);
    if (This->Key == NULL || This->Path == NULL)
    {
        HeapFree(GetProcessHeap(), 0, This->Key);
        HeapFree(GetProcessHeap(), 0, This->Path);
        HeapFree(GetProcessHeap(), 0, This);
        return E_OUTOFMEMORY;
    }

    This->ICatalogObject_iface.lpVtbl = &g_ObjectVtbl;
    This->RefCount = 1;
    This->Type = Type;

    InterlockedIncrement(&g_ObjectCount);
    *Object = &This->ICatalogObject_iface;
    return S_OK;
}