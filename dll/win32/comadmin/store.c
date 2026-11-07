/*
 * PROJECT:     ReactOS COM+ Administration
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Registry backed catalog store
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#include "comadmin_private.h"

PWSTR
StoreDuplicateString(
    _In_ PCWSTR String)
{
    SIZE_T Size = (lstrlenW(String) + 1) * sizeof(WCHAR);
    PWSTR Copy = HeapAlloc(GetProcessHeap(), 0, Size);

    if (Copy != NULL)
        RtlCopyMemory(Copy, String, Size);

    return Copy;
}

PWSTR
StoreJoinPath(
    _In_ PCWSTR Base,
    _In_ PCWSTR Leaf)
{
    SIZE_T BaseLength = lstrlenW(Base);
    SIZE_T LeafLength = lstrlenW(Leaf);
    PWSTR Path = HeapAlloc(GetProcessHeap(), 0, (BaseLength + 1 + LeafLength + 1) * sizeof(WCHAR));

    if (Path == NULL)
        return NULL;

    RtlCopyMemory(Path, Base, BaseLength * sizeof(WCHAR));
    Path[BaseLength] = L'\\';
    RtlCopyMemory(Path + BaseLength + 1, Leaf, (LeafLength + 1) * sizeof(WCHAR));
    return Path;
}

VOID
StoreFreeKeys(
    _In_opt_ PWSTR *Keys,
    _In_ ULONG Count)
{
    ULONG i;

    if (Keys == NULL)
        return;

    for (i = 0; i < Count; i++)
        HeapFree(GetProcessHeap(), 0, Keys[i]);

    HeapFree(GetProcessHeap(), 0, Keys);
}

BOOL
StoreKeyExists(
    _In_ PCWSTR Path)
{
    HKEY hKey;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, Path, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;

    RegCloseKey(hKey);
    return TRUE;
}

HRESULT
StoreEnumSubKeys(
    _In_ PCWSTR Path,
    _Outptr_result_maybenull_ PWSTR **Keys,
    _Out_ PULONG Count)
{
    HKEY hKey;
    PWSTR *List = NULL;
    DWORD SubKeyCount, MaxLength, Length, i;
    LONG Result;
    HRESULT hr = S_OK;

    *Keys = NULL;
    *Count = 0;

    Result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, Path, 0, KEY_READ, &hKey);
    if (Result == ERROR_FILE_NOT_FOUND || Result == ERROR_PATH_NOT_FOUND)
        return S_OK;
    if (Result != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(Result);

    Result = RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &SubKeyCount, &MaxLength,
                              NULL, NULL, NULL, NULL, NULL, NULL);
    if (Result != ERROR_SUCCESS)
    {
        hr = HRESULT_FROM_WIN32(Result);
        goto Cleanup;
    }

    if (SubKeyCount == 0)
        goto Cleanup;

    List = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, SubKeyCount * sizeof(PWSTR));
    if (List == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    for (i = 0; i < SubKeyCount; i++)
    {
        Length = MaxLength + 1;
        List[i] = HeapAlloc(GetProcessHeap(), 0, Length * sizeof(WCHAR));
        if (List[i] == NULL)
        {
            hr = E_OUTOFMEMORY;
            break;
        }

        Result = RegEnumKeyExW(hKey, i, List[i], &Length, NULL, NULL, NULL, NULL);
        if (Result != ERROR_SUCCESS)
        {
            HeapFree(GetProcessHeap(), 0, List[i]);
            List[i] = NULL;
            if (Result != ERROR_NO_MORE_ITEMS)
                hr = HRESULT_FROM_WIN32(Result);
            break;
        }
    }

    if (FAILED(hr))
    {
        StoreFreeKeys(List, i);
        goto Cleanup;
    }

    *Keys = List;
    *Count = i;

Cleanup:
    RegCloseKey(hKey);
    return hr;
}

HRESULT
StoreReadValue(
    _In_ PCWSTR Path,
    _In_ PCWSTR ValueName,
    _Out_ VARIANT *Value)
{
    HKEY hKey;
    DWORD Type, Size = 0;
    PWSTR Buffer = NULL;
    LONG Result;
    HRESULT hr = S_OK;

    VariantInit(Value);

    Result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, Path, 0, KEY_READ, &hKey);
    if (Result != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(Result);

    Result = RegQueryValueExW(hKey, ValueName, NULL, &Type, NULL, &Size);
    if (Result != ERROR_SUCCESS)
    {
        hr = HRESULT_FROM_WIN32(Result);
        goto Cleanup;
    }

    if (Type == REG_DWORD)
    {
        LONG Number = 0;

        Size = sizeof(Number);
        Result = RegQueryValueExW(hKey, ValueName, NULL, &Type, (PBYTE)&Number, &Size);
        if (Result != ERROR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(Result);
            goto Cleanup;
        }

        V_VT(Value) = VT_I4;
        V_I4(Value) = Number;
    }
    else if (Type == REG_SZ || Type == REG_EXPAND_SZ)
    {
        Buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Size + sizeof(WCHAR));
        if (Buffer == NULL)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }

        Result = RegQueryValueExW(hKey, ValueName, NULL, &Type, (PBYTE)Buffer, &Size);
        if (Result != ERROR_SUCCESS)
        {
            hr = HRESULT_FROM_WIN32(Result);
            goto Cleanup;
        }

        V_BSTR(Value) = SysAllocString(Buffer);
        if (V_BSTR(Value) == NULL)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }

        V_VT(Value) = VT_BSTR;
    }
    else
    {
        hr = DISP_E_TYPEMISMATCH;
    }

Cleanup:
    if (Buffer != NULL)
        HeapFree(GetProcessHeap(), 0, Buffer);
    RegCloseKey(hKey);
    return hr;
}