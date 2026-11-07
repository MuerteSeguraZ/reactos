/*
 * PROJECT:     ReactOS COM+ Administration
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     ICatalogCollection implementation
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#include "comadmin_private.h"

typedef struct _COMADMIN_COLLECTION
{
    ICatalogCollection ICatalogCollection_iface;
    LONG RefCount;
    COMADMIN_COLLECTION_TYPE Type;
    PWSTR Name;
    PWSTR Path;
    PWSTR *Keys;
    ULONG Count;
} COMADMIN_COLLECTION, *PCOMADMIN_COLLECTION;

typedef struct _COMADMIN_ENUM
{
    IEnumVARIANT IEnumVARIANT_iface;
    LONG RefCount;
    PCOMADMIN_COLLECTION Collection;
    ULONG Index;
} COMADMIN_ENUM, *PCOMADMIN_ENUM;

static
PCOMADMIN_COLLECTION
impl_from_ICatalogCollection(
    _In_ ICatalogCollection *iface)
{
    return CONTAINING_RECORD(iface, COMADMIN_COLLECTION, ICatalogCollection_iface);
}

static
PCOMADMIN_ENUM
impl_from_IEnumVARIANT(
    _In_ IEnumVARIANT *iface)
{
    return CONTAINING_RECORD(iface, COMADMIN_ENUM, IEnumVARIANT_iface);
}

static
HRESULT
CollectionCreateItem(
    _In_ PCOMADMIN_COLLECTION This,
    _In_ ULONG Index,
    _Outptr_ IDispatch **Item)
{
    ICatalogObject *Object;
    PWSTR Path;
    HRESULT hr;

    *Item = NULL;

    Path = StoreJoinPath(This->Path, This->Keys[Index]);
    if (Path == NULL)
        return E_OUTOFMEMORY;

    hr = ComAdminCreateObject(This->Type, This->Keys[Index], Path, &Object);
    HeapFree(GetProcessHeap(), 0, Path);
    if (FAILED(hr))
        return hr;

    hr = ICatalogObject_QueryInterface(Object, &IID_IDispatch, (PVOID *)Item);
    ICatalogObject_Release(Object);
    return hr;
}

/* IEnumVARIANT */

static
HRESULT
WINAPI
enum_QueryInterface(
    _In_ IEnumVARIANT *iface,
    _In_ REFIID riid,
    _COM_Outptr_ PVOID *ppvObject)
{
    if (ppvObject == NULL)
        return E_POINTER;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IEnumVARIANT))
    {
        *ppvObject = iface;
        IEnumVARIANT_AddRef(iface);
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static
ULONG
WINAPI
enum_AddRef(
    _In_ IEnumVARIANT *iface)
{
    PCOMADMIN_ENUM This = impl_from_IEnumVARIANT(iface);

    return InterlockedIncrement(&This->RefCount);
}

static
ULONG
WINAPI
enum_Release(
    _In_ IEnumVARIANT *iface)
{
    PCOMADMIN_ENUM This = impl_from_IEnumVARIANT(iface);
    LONG RefCount = InterlockedDecrement(&This->RefCount);

    if (RefCount == 0)
    {
        ICatalogCollection_Release(&This->Collection->ICatalogCollection_iface);
        HeapFree(GetProcessHeap(), 0, This);
        InterlockedDecrement(&g_ObjectCount);
    }

    return RefCount;
}

static
HRESULT
WINAPI
enum_Next(
    _In_ IEnumVARIANT *iface,
    _In_ ULONG celt,
    _Out_writes_to_(celt, *pceltFetched) VARIANT *rgVar,
    _Out_opt_ ULONG *pceltFetched)
{
    PCOMADMIN_ENUM This = impl_from_IEnumVARIANT(iface);
    ULONG Fetched = 0;
    IDispatch *Item;
    HRESULT hr = S_OK;

    if (rgVar == NULL || (pceltFetched == NULL && celt != 1))
        return E_INVALIDARG;

    while (Fetched < celt && This->Index < This->Collection->Count)
    {
        hr = CollectionCreateItem(This->Collection, This->Index, &Item);
        if (FAILED(hr))
            break;

        VariantInit(&rgVar[Fetched]);
        V_VT(&rgVar[Fetched]) = VT_DISPATCH;
        V_DISPATCH(&rgVar[Fetched]) = Item;
        Fetched++;
        This->Index++;
    }

    if (FAILED(hr))
    {
        while (Fetched > 0)
            VariantClear(&rgVar[--Fetched]);

        if (pceltFetched != NULL)
            *pceltFetched = 0;
        return hr;
    }

    if (pceltFetched != NULL)
        *pceltFetched = Fetched;

    return (Fetched == celt) ? S_OK : S_FALSE;
}

static
HRESULT
WINAPI
enum_Skip(
    _In_ IEnumVARIANT *iface,
    _In_ ULONG celt)
{
    PCOMADMIN_ENUM This = impl_from_IEnumVARIANT(iface);
    ULONG Remaining = This->Collection->Count - min(This->Index, This->Collection->Count);

    if (celt > Remaining)
    {
        This->Index = This->Collection->Count;
        return S_FALSE;
    }

    This->Index += celt;
    return S_OK;
}

static
HRESULT
WINAPI
enum_Reset(
    _In_ IEnumVARIANT *iface)
{
    PCOMADMIN_ENUM This = impl_from_IEnumVARIANT(iface);

    This->Index = 0;
    return S_OK;
}

static
HRESULT
WINAPI
enum_Clone(
    _In_ IEnumVARIANT *iface,
    _Outptr_ IEnumVARIANT **ppEnum);

static const IEnumVARIANTVtbl g_EnumVtbl =
{
    enum_QueryInterface,
    enum_AddRef,
    enum_Release,
    enum_Next,
    enum_Skip,
    enum_Reset,
    enum_Clone
};

static
HRESULT
EnumCreate(
    _In_ PCOMADMIN_COLLECTION Collection,
    _In_ ULONG Index,
    _Outptr_ IEnumVARIANT **Enum)
{
    PCOMADMIN_ENUM This;

    *Enum = NULL;

    This = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*This));
    if (This == NULL)
        return E_OUTOFMEMORY;

    This->IEnumVARIANT_iface.lpVtbl = &g_EnumVtbl;
    This->RefCount = 1;
    This->Collection = Collection;
    This->Index = Index;
    ICatalogCollection_AddRef(&Collection->ICatalogCollection_iface);

    InterlockedIncrement(&g_ObjectCount);
    *Enum = &This->IEnumVARIANT_iface;
    return S_OK;
}

static
HRESULT
WINAPI
enum_Clone(
    _In_ IEnumVARIANT *iface,
    _Outptr_ IEnumVARIANT **ppEnum)
{
    PCOMADMIN_ENUM This = impl_from_IEnumVARIANT(iface);

    if (ppEnum == NULL)
        return E_POINTER;

    return EnumCreate(This->Collection, This->Index, ppEnum);
}

/* ICatalogCollection */

static
HRESULT
WINAPI
collection_QueryInterface(
    _In_ ICatalogCollection *iface,
    _In_ REFIID riid,
    _COM_Outptr_ PVOID *ppvObject)
{
    if (ppvObject == NULL)
        return E_POINTER;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IDispatch) ||
        IsEqualIID(riid, &IID_ICatalogCollection))
    {
        *ppvObject = iface;
        ICatalogCollection_AddRef(iface);
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static
ULONG
WINAPI
collection_AddRef(
    _In_ ICatalogCollection *iface)
{
    PCOMADMIN_COLLECTION This = impl_from_ICatalogCollection(iface);

    return InterlockedIncrement(&This->RefCount);
}

static
ULONG
WINAPI
collection_Release(
    _In_ ICatalogCollection *iface)
{
    PCOMADMIN_COLLECTION This = impl_from_ICatalogCollection(iface);
    LONG RefCount = InterlockedDecrement(&This->RefCount);

    if (RefCount == 0)
    {
        StoreFreeKeys(This->Keys, This->Count);
        HeapFree(GetProcessHeap(), 0, This->Name);
        HeapFree(GetProcessHeap(), 0, This->Path);
        HeapFree(GetProcessHeap(), 0, This);
        InterlockedDecrement(&g_ObjectCount);
    }

    return RefCount;
}

static
HRESULT
WINAPI
collection_GetTypeInfoCount(
    _In_ ICatalogCollection *iface,
    _Out_ UINT *pctinfo)
{
    *pctinfo = 1;
    return S_OK;
}

static
HRESULT
WINAPI
collection_GetTypeInfo(
    _In_ ICatalogCollection *iface,
    _In_ UINT iTInfo,
    _In_ LCID lcid,
    _Outptr_ ITypeInfo **ppTInfo)
{
    if (iTInfo != 0)
        return DISP_E_BADINDEX;

    return ComAdminGetTypeInfo(&IID_ICatalogCollection, ppTInfo);
}

static
HRESULT
WINAPI
collection_GetIDsOfNames(
    _In_ ICatalogCollection *iface,
    _In_ REFIID riid,
    _In_reads_(cNames) LPOLESTR *rgszNames,
    _In_ UINT cNames,
    _In_ LCID lcid,
    _Out_writes_(cNames) DISPID *rgDispId)
{
    return ComAdminGetIDsOfNames(&IID_ICatalogCollection, rgszNames, cNames, rgDispId);
}

static
HRESULT
WINAPI
collection_Invoke(
    _In_ ICatalogCollection *iface,
    _In_ DISPID dispIdMember,
    _In_ REFIID riid,
    _In_ LCID lcid,
    _In_ WORD wFlags,
    _In_ DISPPARAMS *pDispParams,
    _Out_opt_ VARIANT *pVarResult,
    _Out_opt_ EXCEPINFO *pExcepInfo,
    _Out_opt_ UINT *puArgErr)
{
    return ComAdminInvoke(iface, &IID_ICatalogCollection, dispIdMember, wFlags,
                          pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static
HRESULT
WINAPI
collection_get__NewEnum(
    _In_ ICatalogCollection *iface,
    _Outptr_ IUnknown **ppEnumVariant)
{
    PCOMADMIN_COLLECTION This = impl_from_ICatalogCollection(iface);
    IEnumVARIANT *Enum;
    HRESULT hr;

    if (ppEnumVariant == NULL)
        return E_POINTER;

    hr = EnumCreate(This, 0, &Enum);
    if (FAILED(hr))
    {
        *ppEnumVariant = NULL;
        return hr;
    }

    *ppEnumVariant = (IUnknown *)Enum;
    return S_OK;
}

static
HRESULT
WINAPI
collection_get_Item(
    _In_ ICatalogCollection *iface,
    _In_ LONG lIndex,
    _Outptr_ IDispatch **ppCatalogObject)
{
    PCOMADMIN_COLLECTION This = impl_from_ICatalogCollection(iface);

    if (ppCatalogObject == NULL)
        return E_POINTER;

    *ppCatalogObject = NULL;
    if (lIndex < 0 || (ULONG)lIndex >= This->Count)
        return E_INVALIDARG;

    return CollectionCreateItem(This, lIndex, ppCatalogObject);
}

static
HRESULT
WINAPI
collection_get_Count(
    _In_ ICatalogCollection *iface,
    _Out_ LONG *plObjectCount)
{
    PCOMADMIN_COLLECTION This = impl_from_ICatalogCollection(iface);

    if (plObjectCount == NULL)
        return E_POINTER;

    *plObjectCount = This->Count;
    return S_OK;
}

static
HRESULT
WINAPI
collection_Remove(
    _In_ ICatalogCollection *iface,
    _In_ LONG lIndex)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
collection_Add(
    _In_ ICatalogCollection *iface,
    _Outptr_ IDispatch **ppCatalogObject)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
collection_Populate(
    _In_ ICatalogCollection *iface)
{
    PCOMADMIN_COLLECTION This = impl_from_ICatalogCollection(iface);
    PWSTR *Keys = NULL;
    ULONG Count = 0;
    HRESULT hr;

    if (This->Type != CollectionRoot)
    {
        hr = StoreEnumSubKeys(This->Path, &Keys, &Count);
        if (FAILED(hr))
            return hr;
    }

    StoreFreeKeys(This->Keys, This->Count);
    This->Keys = Keys;
    This->Count = Count;
    return S_OK;
}

static
HRESULT
WINAPI
collection_SaveChanges(
    _In_ ICatalogCollection *iface,
    _Out_ LONG *pcChanges)
{
    if (pcChanges == NULL)
        return E_POINTER;

    *pcChanges = 0;
    return S_OK;
}

static
HRESULT
WINAPI
collection_GetCollection(
    _In_ ICatalogCollection *iface,
    _In_ BSTR bstrCollName,
    _In_ VARIANT varObjectKey,
    _Outptr_ IDispatch **ppCatalogCollection)
{
    PCOMADMIN_COLLECTION This = impl_from_ICatalogCollection(iface);
    ICatalogCollection *Collection;
    COMADMIN_COLLECTION_TYPE Type;
    PCWSTR Name;
    PWSTR ObjectPath = NULL, CollectionPath = NULL;
    BSTR Key;
    HRESULT hr;

    if (bstrCollName == NULL || ppCatalogCollection == NULL)
        return E_INVALIDARG;

    *ppCatalogCollection = NULL;

    if (This->Type == CollectionRoot)
    {
        BSTR RootKey = NULL;

        if (V_VT(&varObjectKey) == VT_BSTR)
            RootKey = V_BSTR(&varObjectKey);
        else if (V_VT(&varObjectKey) == (VT_BSTR | VT_BYREF))
            RootKey = *V_BSTRREF(&varObjectKey);

        if (RootKey == NULL || RootKey[0] == L'\0')
            return E_INVALIDARG;

        return ComAdminGetTopLevelCollection(bstrCollName, ppCatalogCollection);
    }

    if (This->Type != CollectionApplications)
        return E_INVALIDARG;

    if (lstrcmpiW(bstrCollName, COMADMIN_COMPONENTS_NAME) == 0)
    {
        Type = CollectionComponents;
        Name = COMADMIN_COMPONENTS_NAME;
    }
    else if (lstrcmpiW(bstrCollName, COMADMIN_ROLES_NAME) == 0)
    {
        Type = CollectionRoles;
        Name = COMADMIN_ROLES_NAME;
    }
    else
    {
        return E_INVALIDARG;
    }

    if (V_VT(&varObjectKey) == (VT_BSTR | VT_BYREF))
        Key = *V_BSTRREF(&varObjectKey);
    else if (V_VT(&varObjectKey) == VT_BSTR)
        Key = V_BSTR(&varObjectKey);
    else
        return E_INVALIDARG;

    if (Key == NULL)
        return E_INVALIDARG;

    ObjectPath = StoreJoinPath(This->Path, Key);
    if (ObjectPath == NULL)
        return E_OUTOFMEMORY;

    CollectionPath = StoreJoinPath(ObjectPath, Name);
    if (CollectionPath == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    hr = ComAdminCreateCollection(Type, Name, CollectionPath, &Collection);
    if (FAILED(hr))
        goto Cleanup;

    hr = ICatalogCollection_QueryInterface(Collection, &IID_IDispatch, (PVOID *)ppCatalogCollection);
    ICatalogCollection_Release(Collection);

Cleanup:
    HeapFree(GetProcessHeap(), 0, ObjectPath);
    HeapFree(GetProcessHeap(), 0, CollectionPath);
    return hr;
}

static
HRESULT
WINAPI
collection_get_Name(
    _In_ ICatalogCollection *iface,
    _Out_ VARIANT *pVarNamel)
{
    PCOMADMIN_COLLECTION This = impl_from_ICatalogCollection(iface);

    if (pVarNamel == NULL)
        return E_POINTER;

    VariantInit(pVarNamel);
    V_BSTR(pVarNamel) = SysAllocString(This->Name);
    if (V_BSTR(pVarNamel) == NULL)
        return E_OUTOFMEMORY;

    V_VT(pVarNamel) = VT_BSTR;
    return S_OK;
}

static
HRESULT
WINAPI
collection_get_AddEnabled(
    _In_ ICatalogCollection *iface,
    _Out_ VARIANT_BOOL *pVarBool)
{
    if (pVarBool == NULL)
        return E_POINTER;

    *pVarBool = VARIANT_FALSE;
    return S_OK;
}

static
HRESULT
WINAPI
collection_get_RemoveEnabled(
    _In_ ICatalogCollection *iface,
    _Out_ VARIANT_BOOL *pVarBool)
{
    if (pVarBool == NULL)
        return E_POINTER;

    *pVarBool = VARIANT_FALSE;
    return S_OK;
}

static
HRESULT
WINAPI
collection_GetUtilInterface(
    _In_ ICatalogCollection *iface,
    _Outptr_ IDispatch **ppIDispatch)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
collection_get_DataStoreMajorVersion(
    _In_ ICatalogCollection *iface,
    _Out_ LONG *plMajorVersion)
{
    if (plMajorVersion == NULL)
        return E_POINTER;

    *plMajorVersion = COMADMIN_STORE_MAJOR_VERSION;
    return S_OK;
}

static
HRESULT
WINAPI
collection_get_DataStoreMinorVersion(
    _In_ ICatalogCollection *iface,
    _Out_ LONG *plMinorVersionl)
{
    if (plMinorVersionl == NULL)
        return E_POINTER;

    *plMinorVersionl = COMADMIN_STORE_MINOR_VERSION;
    return S_OK;
}

static
HRESULT
WINAPI
collection_PopulateByKey(
    _In_ ICatalogCollection *iface,
    _In_ SAFEARRAY *psaKeys)
{
    return E_NOTIMPL;
}

static
HRESULT
WINAPI
collection_PopulateByQuery(
    _In_ ICatalogCollection *iface,
    _In_ BSTR bstrQueryString,
    _In_ LONG lQueryType)
{
    return E_NOTIMPL;
}

static const ICatalogCollectionVtbl g_CollectionVtbl =
{
    collection_QueryInterface,
    collection_AddRef,
    collection_Release,
    collection_GetTypeInfoCount,
    collection_GetTypeInfo,
    collection_GetIDsOfNames,
    collection_Invoke,
    collection_get__NewEnum,
    collection_get_Item,
    collection_get_Count,
    collection_Remove,
    collection_Add,
    collection_Populate,
    collection_SaveChanges,
    collection_GetCollection,
    collection_get_Name,
    collection_get_AddEnabled,
    collection_get_RemoveEnabled,
    collection_GetUtilInterface,
    collection_get_DataStoreMajorVersion,
    collection_get_DataStoreMinorVersion,
    collection_PopulateByKey,
    collection_PopulateByQuery
};

HRESULT
ComAdminCreateCollection(
    _In_ COMADMIN_COLLECTION_TYPE Type,
    _In_ PCWSTR Name,
    _In_opt_ PCWSTR Path,
    _Outptr_ ICatalogCollection **Collection)
{
    PCOMADMIN_COLLECTION This;

    *Collection = NULL;

    This = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*This));
    if (This == NULL)
        return E_OUTOFMEMORY;

    This->Name = StoreDuplicateString(Name);
    if (Path != NULL)
        This->Path = StoreDuplicateString(Path);

    if (This->Name == NULL || (Path != NULL && This->Path == NULL))
    {
        HeapFree(GetProcessHeap(), 0, This->Name);
        HeapFree(GetProcessHeap(), 0, This->Path);
        HeapFree(GetProcessHeap(), 0, This);
        return E_OUTOFMEMORY;
    }

    This->ICatalogCollection_iface.lpVtbl = &g_CollectionVtbl;
    This->RefCount = 1;
    This->Type = Type;

    InterlockedIncrement(&g_ObjectCount);
    *Collection = &This->ICatalogCollection_iface;
    return S_OK;
}

HRESULT
ComAdminGetTopLevelCollection(
    _In_ PCWSTR Name,
    _Outptr_ IDispatch **Collection)
{
    ICatalogCollection *Created;
    PWSTR Path;
    HRESULT hr;

    *Collection = NULL;

    if (lstrcmpiW(Name, COMADMIN_APPLICATIONS_NAME) != 0)
        return E_INVALIDARG;

    Path = StoreJoinPath(COMADMIN_CATALOG_ROOT_PATH, COMADMIN_APPLICATIONS_NAME);
    if (Path == NULL)
        return E_OUTOFMEMORY;

    hr = ComAdminCreateCollection(CollectionApplications, COMADMIN_APPLICATIONS_NAME, Path, &Created);
    HeapFree(GetProcessHeap(), 0, Path);
    if (FAILED(hr))
        return hr;

    hr = ICatalogCollection_QueryInterface(Created, &IID_IDispatch, (PVOID *)Collection);
    ICatalogCollection_Release(Created);
    return hr;
}