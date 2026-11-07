/*
 * PROJECT:     ReactOS COM+ Administration
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Private definitions for the COM+ catalog
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#ifndef _COMADMIN_PRIVATE_H
#define _COMADMIN_PRIVATE_H

#include <stdarg.h>

#define WIN32_NO_STATUS
#define COBJMACROS
#define CONST_VTABLE

#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <ole2.h>
#include <oleauto.h>
#include <comadmin.h>

#define COMADMIN_CATALOG_ROOT_PATH      L"SOFTWARE\\Microsoft\\COM3\\Catalog"
#define COMADMIN_APPLICATIONS_NAME      L"Applications"
#define COMADMIN_COMPONENTS_NAME        L"Components"
#define COMADMIN_ROLES_NAME             L"Roles"

#define COMADMIN_CATALOG_MAJOR_VERSION  4
#define COMADMIN_CATALOG_MINOR_VERSION  0
#define COMADMIN_STORE_MAJOR_VERSION    5
#define COMADMIN_STORE_MINOR_VERSION    0

typedef enum _COMADMIN_COLLECTION_TYPE
{
    CollectionRoot,
    CollectionApplications,
    CollectionComponents,
    CollectionRoles
} COMADMIN_COLLECTION_TYPE;

extern HINSTANCE g_hInstance;
extern LONG g_ObjectCount;

/* main.c */
_Check_return_
HRESULT
ComAdminCreateCatalog(
    _In_opt_ IUnknown *Outer,
    _In_ REFIID Riid,
    _Outptr_ PVOID *Object);

/* typelib.c */
_Check_return_
HRESULT
ComAdminGetTypeInfo(
    _In_ REFIID Iid,
    _Outptr_ ITypeInfo **TypeInfo);

_Check_return_
HRESULT
ComAdminGetIDsOfNames(
    _In_ REFIID Iid,
    _In_reads_(NameCount) LPOLESTR *Names,
    _In_ UINT NameCount,
    _Out_writes_(NameCount) DISPID *DispIds);

_Check_return_
HRESULT
ComAdminInvoke(
    _In_ PVOID Instance,
    _In_ REFIID Iid,
    _In_ DISPID DispId,
    _In_ WORD Flags,
    _In_ DISPPARAMS *Params,
    _Out_opt_ VARIANT *Result,
    _Out_opt_ EXCEPINFO *ExcepInfo,
    _Out_opt_ UINT *ArgErr);

VOID
ComAdminReleaseTypeLib(VOID);

/* store.c */
_Check_return_
PWSTR
StoreDuplicateString(
    _In_ PCWSTR String);

_Check_return_
PWSTR
StoreJoinPath(
    _In_ PCWSTR Base,
    _In_ PCWSTR Leaf);

_Check_return_
HRESULT
StoreEnumSubKeys(
    _In_ PCWSTR Path,
    _Outptr_result_maybenull_ PWSTR **Keys,
    _Out_ PULONG Count);

VOID
StoreFreeKeys(
    _In_opt_ PWSTR *Keys,
    _In_ ULONG Count);

_Check_return_
HRESULT
StoreReadValue(
    _In_ PCWSTR Path,
    _In_ PCWSTR ValueName,
    _Out_ VARIANT *Value);

/* collection.c */
_Check_return_
HRESULT
ComAdminCreateCollection(
    _In_ COMADMIN_COLLECTION_TYPE Type,
    _In_ PCWSTR Name,
    _In_opt_ PCWSTR Path,
    _Outptr_ ICatalogCollection **Collection);

_Check_return_
HRESULT
ComAdminGetTopLevelCollection(
    _In_ PCWSTR Name,
    _Outptr_ IDispatch **Collection);

/* object.c */
_Check_return_
HRESULT
ComAdminCreateObject(
    _In_ COMADMIN_COLLECTION_TYPE Type,
    _In_ PCWSTR Key,
    _In_ PCWSTR Path,
    _Outptr_ ICatalogObject **Object);

#endif // _COMADMIN_PRIVATE_H