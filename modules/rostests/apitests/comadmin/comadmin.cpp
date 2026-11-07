/*
 * PROJECT:     ReactOS API tests
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Tests for the COM+ administration catalog (comadmin.dll)
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#ifdef COMADMIN_TEST_STANDALONE

#include <stdio.h>
#include <windows.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "user32.lib")

static int g_Total = 0;
static int g_Failed = 0;

#define ok(cond, ...) \
    do { \
        g_Total++; \
        if (!(cond)) \
        { \
            g_Failed++; \
            printf("%s:%d: Test failed: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__); \
        } \
    } while (0)
#define trace(...) do { printf("%s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); } while (0)
#define skip(...) do { printf("%s:%d: Tests skipped: ", __FILE__, __LINE__); printf(__VA_ARGS__); } while (0)
#define START_TEST(name) \
    extern "C" void func_##name(void); \
    int main(void) { func_##name(); printf("%d tests executed, %d failures\n", g_Total, g_Failed); return g_Failed != 0; } \
    extern "C" void func_##name(void)

#else

#include <wine/test.h>
#include <windows.h>

#endif // COMADMIN_TEST_STANDALONE

#include <ole2.h>
#include <oleauto.h>
#include <comadmin.h>

static const GUID kNullGuid = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
static const GUID kClsidCatalog = { 0xF618C514, 0xDFB8, 0x11D1, { 0xA2, 0xCF, 0x00, 0x80, 0x5F, 0xC7, 0x92, 0x35 } };
static const GUID kClsidCatalogObject = { 0xF618C515, 0xDFB8, 0x11D1, { 0xA2, 0xCF, 0x00, 0x80, 0x5F, 0xC7, 0x92, 0x35 } };
static const GUID kClsidCatalogCollection = { 0xF618C516, 0xDFB8, 0x11D1, { 0xA2, 0xCF, 0x00, 0x80, 0x5F, 0xC7, 0x92, 0x35 } };
static const GUID kIidCatalog = { 0xDD662187, 0xDFC2, 0x11D1, { 0xA2, 0xCF, 0x00, 0x80, 0x5F, 0xC7, 0x92, 0x35 } };
static const GUID kIidCatalog2 = { 0x790C6E0B, 0x9194, 0x4CC9, { 0x94, 0x26, 0xA4, 0x8A, 0x63, 0x18, 0x56, 0x96 } };
static const GUID kIidObject = { 0x6EB22871, 0x8A19, 0x11D0, { 0x81, 0xB6, 0x00, 0xA0, 0xC9, 0x23, 0x1C, 0x29 } };
static const GUID kIidCollection = { 0x6EB22872, 0x8A19, 0x11D0, { 0x81, 0xB6, 0x00, 0xA0, 0xC9, 0x23, 0x1C, 0x29 } };

#define test_hr(hr, expected) \
    ok((hr) == (expected), "hr = 0x%08lx, expected 0x%08lx\n", (unsigned long)(hr), (unsigned long)(expected))

static void check_dispid(IDispatch *disp, const WCHAR *name, DISPID expected)
{
    DISPID id = 0x12345;
    LPOLESTR names[1];
    HRESULT hr;

    names[0] = (LPOLESTR)name;
    hr = disp->GetIDsOfNames(kNullGuid, names, 1, LOCALE_USER_DEFAULT, &id);
    ok(hr == S_OK, "%ls: GetIDsOfNames hr = 0x%08lx\n", name, (unsigned long)hr);
    ok(id == expected, "%ls: dispid 0x%08lx, expected 0x%08lx\n", name, (unsigned long)id, (unsigned long)expected);
}

static BOOL is_braced_guid(const WCHAR *s)
{
    return s != NULL && lstrlenW(s) == 38 && s[0] == L'{' && s[37] == L'}';
}

static ICatalogCollection *get_collection(ICOMAdminCatalog2 *cat, const WCHAR *name)
{
    IDispatch *disp = NULL;
    ICatalogCollection *coll = NULL;
    BSTR bstr = SysAllocString(name);
    HRESULT hr = cat->GetCollection(bstr, &disp);

    SysFreeString(bstr);
    ok(hr == S_OK, "GetCollection(%ls) hr = 0x%08lx\n", name, (unsigned long)hr);
    if (SUCCEEDED(hr) && disp != NULL)
    {
        hr = disp->QueryInterface(kIidCollection, (void **)&coll);
        ok(hr == S_OK, "QueryInterface(ICatalogCollection) hr = 0x%08lx\n", (unsigned long)hr);
        disp->Release();
    }

    return coll;
}

static void test_creation(void)
{
    IUnknown *unk = NULL, *unk2 = NULL;
    IDispatch *disp = NULL;
    ICOMAdminCatalog *cat1 = NULL;
    ICOMAdminCatalog2 *cat2 = NULL;
    IClassFactory *factory = NULL;
    void *other = (void *)0x1234;
    ULONG ref, ref2;
    HRESULT hr;

    hr = CoCreateInstance(kClsidCatalog, NULL, CLSCTX_INPROC_SERVER, kIidCatalog2, (void **)&cat2);
    test_hr(hr, S_OK);
    if (FAILED(hr))
        return;

    hr = cat2->QueryInterface(IID_IUnknown, (void **)&unk);
    test_hr(hr, S_OK);
    hr = cat2->QueryInterface(IID_IDispatch, (void **)&disp);
    test_hr(hr, S_OK);
    hr = cat2->QueryInterface(kIidCatalog, (void **)&cat1);
    test_hr(hr, S_OK);

    hr = disp->QueryInterface(IID_IUnknown, (void **)&unk2);
    test_hr(hr, S_OK);
    ok(unk == unk2, "IUnknown identity differs: %p vs %p\n", unk, unk2);
    ok((void *)cat1 == (void *)cat2, "ICOMAdminCatalog and ICOMAdminCatalog2 pointers differ\n");

    hr = cat2->QueryInterface(IID_IStream, &other);
    test_hr(hr, E_NOINTERFACE);
    ok(other == NULL, "ppv = %p after failed QueryInterface\n", other);

    ref = cat2->AddRef();
    ref2 = cat2->Release();
    ok(ref2 == ref - 1, "Release returned %lu after AddRef returned %lu\n", ref2, ref);

    if (unk2 != NULL)
        unk2->Release();
    if (cat1 != NULL)
        cat1->Release();
    if (disp != NULL)
        disp->Release();
    if (unk != NULL)
        unk->Release();
    cat2->Release();

    other = (void *)0x1234;
    hr = CoCreateInstance(kClsidCatalog, NULL, CLSCTX_INPROC_SERVER, IID_IUnknown, &other);
    test_hr(hr, S_OK);
    if (SUCCEEDED(hr))
        ((IUnknown *)other)->Release();

    hr = CoGetClassObject(kClsidCatalog, CLSCTX_INPROC_SERVER, NULL, IID_IClassFactory, (void **)&factory);
    test_hr(hr, S_OK);
    if (SUCCEEDED(hr))
    {
        test_hr(factory->LockServer(TRUE), S_OK);
        test_hr(factory->LockServer(FALSE), S_OK);
        factory->Release();
    }

    other = NULL;
    hr = CoCreateInstance(kClsidCatalogObject, NULL, CLSCTX_INPROC_SERVER, IID_IUnknown, &other);
    test_hr(hr, REGDB_E_CLASSNOTREG);
    hr = CoCreateInstance(kClsidCatalogCollection, NULL, CLSCTX_INPROC_SERVER, IID_IUnknown, &other);
    test_hr(hr, REGDB_E_CLASSNOTREG);
}

class COuter : public IUnknown
{
public:
    LONG ref;
    LONG addRefs;
    LONG queries;
    IUnknown *inner;

    COuter() : ref(1), addRefs(0), queries(0), inner(NULL) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv)
    {
        queries++;
        if (IsEqualGUID(riid, IID_IUnknown))
        {
            *ppv = static_cast<IUnknown *>(this);
            AddRef();
            return S_OK;
        }

        if (inner != NULL)
            return inner->QueryInterface(riid, ppv);

        *ppv = NULL;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef()
    {
        addRefs++;
        return ++ref;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        return --ref;
    }
};

static void test_aggregation(void)
{
    COuter outer;
    IClassFactory *factory = NULL;
    IUnknown *inner = NULL;
    ICOMAdminCatalog2 *cat2 = NULL;
    IDispatch *disp = NULL;
    void *other = (void *)0x1234;
    LONG queries;
    HRESULT hr;

    hr = CoGetClassObject(kClsidCatalog, CLSCTX_INPROC_SERVER, NULL, IID_IClassFactory, (void **)&factory);
    test_hr(hr, S_OK);
    if (FAILED(hr))
        return;

    hr = factory->CreateInstance(&outer, kIidCatalog2, &other);
    test_hr(hr, CLASS_E_NOAGGREGATION);
    ok(other == NULL, "ppv = %p after failed aggregation\n", other);

    hr = factory->CreateInstance(&outer, IID_IUnknown, (void **)&inner);
    test_hr(hr, S_OK);
    if (SUCCEEDED(hr))
    {
        outer.inner = inner;

        hr = inner->QueryInterface(kIidCatalog2, (void **)&cat2);
        test_hr(hr, S_OK);
        ok(outer.addRefs > 0, "AddRef on the interface did not reach the outer object\n");

        queries = outer.queries;
        hr = cat2->QueryInterface(IID_IDispatch, (void **)&disp);
        test_hr(hr, S_OK);
        ok(outer.queries > queries, "QueryInterface on the interface did not reach the outer object\n");

        if (disp != NULL)
            disp->Release();
        if (cat2 != NULL)
            cat2->Release();

        ok(outer.ref == 1, "outer refcount = %ld after release, expected 1\n", outer.ref);
        inner->Release();
    }

    factory->Release();
}

static void test_catalog_dispatch(ICOMAdminCatalog2 *cat)
{
    IDispatch *disp = NULL;
    ITypeInfo *info = NULL;
    TYPEATTR *attr;
    UINT count = 0;
    DISPID id = 0;
    LPOLESTR names[1];
    DISPPARAMS params = { NULL, NULL, 0, 0 };
    VARIANT result, arg;
    HRESULT hr;

    hr = cat->QueryInterface(IID_IDispatch, (void **)&disp);
    test_hr(hr, S_OK);
    if (FAILED(hr))
        return;

    hr = disp->GetTypeInfoCount(&count);
    test_hr(hr, S_OK);
    ok(count == 1, "GetTypeInfoCount = %u\n", count);

    hr = disp->GetTypeInfo(0, LOCALE_USER_DEFAULT, &info);
    test_hr(hr, S_OK);
    if (SUCCEEDED(hr))
    {
        hr = info->GetTypeAttr(&attr);
        test_hr(hr, S_OK);
        if (SUCCEEDED(hr))
        {
            ok(IsEqualGUID(attr->guid, kIidCatalog2), "type info guid is not ICOMAdminCatalog2\n");
            ok(attr->typekind == TKIND_DISPATCH, "typekind = %d\n", (int)attr->typekind);
            ok((attr->wTypeFlags & TYPEFLAG_FDUAL) != 0, "type flags 0x%04x lack FDUAL\n", attr->wTypeFlags);
            info->ReleaseTypeAttr(attr);
        }
        info->Release();
    }

    info = (ITypeInfo *)0x1;
    hr = disp->GetTypeInfo(1, LOCALE_USER_DEFAULT, &info);
    test_hr(hr, DISP_E_BADINDEX);

    check_dispid(disp, L"GetCollection", 1);
    check_dispid(disp, L"Connect", 2);
    check_dispid(disp, L"MajorVersion", 3);
    check_dispid(disp, L"MinorVersion", 4);
    check_dispid(disp, L"GetCollectionByQuery", 5);
    check_dispid(disp, L"InstallApplication", 0xA);
    check_dispid(disp, L"ServiceCheck", 0x17);
    check_dispid(disp, L"GetCollectionByQuery2", 0x1B);
    check_dispid(disp, L"IsApplicationInstanceDumpSupported", 0x23);
    check_dispid(disp, L"CurrentPartition", 0x28);
    check_dispid(disp, L"Is64BitCatalogServer", 0x35);
    check_dispid(disp, L"GetComponentVersionCount", 0x39);
    check_dispid(disp, L"getcollection", 1);

    id = 0;
    names[0] = (LPOLESTR)L"NoSuchMember";
    hr = disp->GetIDsOfNames(kNullGuid, names, 1, LOCALE_USER_DEFAULT, &id);
    test_hr(hr, DISP_E_UNKNOWNNAME);
    ok(id == DISPID_UNKNOWN, "dispid = 0x%08lx for unknown name\n", (unsigned long)id);

    VariantInit(&result);
    hr = disp->Invoke(3, kNullGuid, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, &result, NULL, NULL);
    test_hr(hr, S_OK);
    ok(V_VT(&result) == VT_I4, "MajorVersion vt = %d\n", V_VT(&result));
    VariantClear(&result);

    VariantInit(&arg);
    V_VT(&arg) = VT_BSTR;
    V_BSTR(&arg) = SysAllocString(L"Applications");
    params.rgvarg = &arg;
    params.cArgs = 1;
    VariantInit(&result);
    hr = disp->Invoke(1, kNullGuid, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, &result, NULL, NULL);
    test_hr(hr, S_OK);
    ok(V_VT(&result) == VT_DISPATCH, "GetCollection vt = %d\n", V_VT(&result));
    VariantClear(&result);
    VariantClear(&arg);

    params.rgvarg = NULL;
    params.cArgs = 0;
    VariantInit(&result);
    hr = disp->Invoke(1, kNullGuid, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, &result, NULL, NULL);
    test_hr(hr, DISP_E_BADPARAMCOUNT);
    VariantClear(&result);

    disp->Release();
}

static void test_versions(ICOMAdminCatalog2 *cat)
{
    VARIANT_BOOL is64 = 0;
    LONG major = -1, minor = -1;
    HRESULT hr;

    hr = cat->get_MajorVersion(&major);
    test_hr(hr, S_OK);
    ok(major == 4, "MajorVersion = %ld\n", major);

    hr = cat->get_MinorVersion(&minor);
    test_hr(hr, S_OK);
    ok(minor == 0, "MinorVersion = %ld\n", minor);

    hr = cat->get_Is64BitCatalogServer(&is64);
    test_hr(hr, S_OK);
    if (sizeof(void *) == 8)
        ok(is64 == VARIANT_TRUE, "Is64BitCatalogServer = %d in a 64-bit process\n", (int)is64);
    else
        trace("Is64BitCatalogServer = %d in a 32-bit process\n", (int)is64);
}

static void test_collection_dispatch(ICatalogCollection *coll)
{
    IDispatch *disp = NULL;
    ITypeInfo *info = NULL;
    TYPEATTR *attr;
    HRESULT hr;

    hr = coll->QueryInterface(IID_IDispatch, (void **)&disp);
    test_hr(hr, S_OK);
    if (FAILED(hr))
        return;

    hr = disp->GetTypeInfo(0, LOCALE_USER_DEFAULT, &info);
    test_hr(hr, S_OK);
    if (SUCCEEDED(hr))
    {
        hr = info->GetTypeAttr(&attr);
        test_hr(hr, S_OK);
        if (SUCCEEDED(hr))
        {
            ok(IsEqualGUID(attr->guid, kIidCollection), "type info guid is not ICatalogCollection\n");
            info->ReleaseTypeAttr(attr);
        }
        info->Release();
    }

    check_dispid(disp, L"_NewEnum", DISPID_NEWENUM);
    check_dispid(disp, L"Item", 1);
    check_dispid(disp, L"Count", 0x60020002);
    check_dispid(disp, L"Remove", 0x60020003);
    check_dispid(disp, L"Add", 0x60020004);
    check_dispid(disp, L"Populate", 2);
    check_dispid(disp, L"SaveChanges", 3);
    check_dispid(disp, L"GetCollection", 4);
    check_dispid(disp, L"Name", 6);
    check_dispid(disp, L"AddEnabled", 7);
    check_dispid(disp, L"RemoveEnabled", 8);
    check_dispid(disp, L"GetUtilInterface", 9);
    check_dispid(disp, L"DataStoreMajorVersion", 0xA);
    check_dispid(disp, L"DataStoreMinorVersion", 0xB);
    check_dispid(disp, L"PopulateByKey", 0xC);
    check_dispid(disp, L"PopulateByQuery", 0xD);

    disp->Release();
}

static void test_object_dispatch(ICatalogObject *obj)
{
    IDispatch *disp = NULL;
    HRESULT hr = obj->QueryInterface(IID_IDispatch, (void **)&disp);

    test_hr(hr, S_OK);
    if (FAILED(hr))
        return;

    check_dispid(disp, L"Value", 1);
    check_dispid(disp, L"Key", 2);
    check_dispid(disp, L"Name", 3);
    check_dispid(disp, L"IsPropertyReadOnly", 4);
    check_dispid(disp, L"Valid", 5);
    check_dispid(disp, L"IsPropertyWriteOnly", 6);
    disp->Release();
}

static void test_object(ICatalogObject *obj, const WCHAR *keyProperty)
{
    VARIANT key, name, value;
    VARIANT_BOOL valid = 0;
    BSTR bstr;
    HRESULT hr;

    VariantInit(&key);
    VariantInit(&name);
    VariantInit(&value);

    hr = obj->get_Key(&key);
    test_hr(hr, S_OK);
    ok(V_VT(&key) == VT_BSTR, "Key vt = %d\n", V_VT(&key));
    if (V_VT(&key) == VT_BSTR)
        ok(is_braced_guid(V_BSTR(&key)), "Key = %ls is not a braced GUID\n", V_BSTR(&key));

    hr = obj->get_Name(&name);
    test_hr(hr, S_OK);
    ok(V_VT(&name) == VT_BSTR, "Name vt = %d\n", V_VT(&name));
    if (V_VT(&name) == VT_BSTR)
        ok(lstrlenW(V_BSTR(&name)) > 0, "Name is empty\n");

    hr = obj->get_Valid(&valid);
    test_hr(hr, S_OK);
    ok(valid == VARIANT_TRUE, "Valid = %d\n", (int)valid);

    bstr = SysAllocString(keyProperty);
    hr = obj->get_Value(bstr, &value);
    SysFreeString(bstr);
    test_hr(hr, S_OK);
    ok(V_VT(&value) == VT_BSTR, "Value(%ls) vt = %d\n", keyProperty, V_VT(&value));
    if (V_VT(&value) == VT_BSTR && V_VT(&key) == VT_BSTR)
        ok(lstrcmpiW(V_BSTR(&value), V_BSTR(&key)) == 0, "Value(%ls) = %ls, Key = %ls\n",
           keyProperty, V_BSTR(&value), V_BSTR(&key));
    VariantClear(&value);

    bstr = SysAllocString(L"Name");
    hr = obj->get_Value(bstr, &value);
    SysFreeString(bstr);
    test_hr(hr, S_OK);
    if (V_VT(&value) == VT_BSTR && V_VT(&name) == VT_BSTR)
        ok(lstrcmpW(V_BSTR(&value), V_BSTR(&name)) == 0, "Value(Name) = %ls, Name = %ls\n",
           V_BSTR(&value), V_BSTR(&name));
    VariantClear(&value);

    bstr = SysAllocString(L"NoSuchProperty");
    VariantInit(&value);
    hr = obj->get_Value(bstr, &value);
    SysFreeString(bstr);
    test_hr(hr, E_INVALIDARG);
    VariantClear(&value);

    VariantClear(&key);
    VariantClear(&name);
    test_object_dispatch(obj);
}

static void test_enumeration(ICatalogCollection *coll, LONG count)
{
    IUnknown *unk = NULL;
    IEnumVARIANT *enumerator = NULL, *clone = NULL;
    VARIANT var, first, second;
    ULONG fetched;
    LONG i;
    HRESULT hr;

    hr = coll->get__NewEnum(&unk);
    test_hr(hr, S_OK);
    if (FAILED(hr))
        return;

    hr = unk->QueryInterface(IID_IEnumVARIANT, (void **)&enumerator);
    test_hr(hr, S_OK);
    unk->Release();
    if (FAILED(hr))
        return;

    for (i = 0; i < count; i++)
    {
        IDispatch *item = NULL;
        ICatalogObject *obj = NULL;
        VARIANT enumKey, itemKey;

        VariantInit(&var);
        fetched = 0;
        hr = enumerator->Next(1, &var, &fetched);
        test_hr(hr, S_OK);
        ok(fetched == 1, "fetched = %lu\n", fetched);
        ok(V_VT(&var) == VT_DISPATCH, "enumerated vt = %d\n", V_VT(&var));
        if (V_VT(&var) != VT_DISPATCH)
        {
            VariantClear(&var);
            break;
        }

        VariantInit(&enumKey);
        VariantInit(&itemKey);
        if (SUCCEEDED(V_DISPATCH(&var)->QueryInterface(kIidObject, (void **)&obj)))
        {
            obj->get_Key(&enumKey);
            obj->Release();
        }

        if (SUCCEEDED(coll->get_Item(i, &item)) && item != NULL)
        {
            if (SUCCEEDED(item->QueryInterface(kIidObject, (void **)&obj)))
            {
                obj->get_Key(&itemKey);
                obj->Release();
            }
            item->Release();
        }

        if (V_VT(&enumKey) == VT_BSTR && V_VT(&itemKey) == VT_BSTR)
            ok(lstrcmpiW(V_BSTR(&enumKey), V_BSTR(&itemKey)) == 0, "item %ld: enumerated key %ls, Item key %ls\n",
               i, V_BSTR(&enumKey), V_BSTR(&itemKey));

        VariantClear(&enumKey);
        VariantClear(&itemKey);
        VariantClear(&var);
    }

    VariantInit(&var);
    fetched = 99;
    hr = enumerator->Next(1, &var, &fetched);
    test_hr(hr, S_FALSE);
    ok(fetched == 0, "fetched = %lu at end of enumeration\n", fetched);

    hr = enumerator->Reset();
    test_hr(hr, S_OK);

    if (count >= 2)
    {
        VariantInit(&first);
        VariantInit(&second);

        hr = enumerator->Skip(1);
        test_hr(hr, S_OK);

        hr = enumerator->Clone(&clone);
        test_hr(hr, S_OK);
        if (SUCCEEDED(hr))
        {
            ICatalogObject *obj = NULL;
            VARIANT cloneKey, orgKey;

            VariantInit(&cloneKey);
            VariantInit(&orgKey);

            hr = clone->Next(1, &first, &fetched);
            test_hr(hr, S_OK);
            hr = enumerator->Next(1, &second, &fetched);
            test_hr(hr, S_OK);

            if (V_VT(&first) == VT_DISPATCH && SUCCEEDED(V_DISPATCH(&first)->QueryInterface(kIidObject, (void **)&obj)))
            {
                obj->get_Key(&cloneKey);
                obj->Release();
            }
            if (V_VT(&second) == VT_DISPATCH && SUCCEEDED(V_DISPATCH(&second)->QueryInterface(kIidObject, (void **)&obj)))
            {
                obj->get_Key(&orgKey);
                obj->Release();
            }
            if (V_VT(&cloneKey) == VT_BSTR && V_VT(&orgKey) == VT_BSTR)
                ok(lstrcmpiW(V_BSTR(&cloneKey), V_BSTR(&orgKey)) == 0, "clone key %ls, original key %ls\n",
                   V_BSTR(&cloneKey), V_BSTR(&orgKey));

            VariantClear(&cloneKey);
            VariantClear(&orgKey);
            clone->Release();
        }

        VariantClear(&first);
        VariantClear(&second);
    }

    hr = enumerator->Reset();
    test_hr(hr, S_OK);
    hr = enumerator->Skip(count + 1);
    test_hr(hr, S_FALSE);

    enumerator->Release();
}

static ICatalogCollection *get_related(ICatalogCollection *apps, const WCHAR *collName, const WCHAR *appKey, HRESULT *phr)
{
    IDispatch *disp = NULL;
    ICatalogCollection *coll = NULL;
    VARIANT key;
    BSTR name = SysAllocString(collName);

    VariantInit(&key);
    V_VT(&key) = VT_BSTR;
    V_BSTR(&key) = SysAllocString(appKey);

    *phr = apps->GetCollection(name, key, &disp);
    SysFreeString(name);
    VariantClear(&key);

    if (SUCCEEDED(*phr) && disp != NULL)
    {
        if (FAILED(disp->QueryInterface(kIidCollection, (void **)&coll)))
            coll = NULL;
        disp->Release();
    }

    return coll;
}

static void test_related(ICatalogCollection *apps, const WCHAR *appKey)
{
    static const WCHAR *const names[2] = { L"Components", L"Roles" };
    ICatalogCollection *coll;
    HRESULT hr;
    ULONG n;

    for (n = 0; n < 2; n++)
    {
        VARIANT name;
        LONG count = -1;
        LONG i;

        coll = get_related(apps, names[n], appKey, &hr);
        test_hr(hr, S_OK);
        if (coll == NULL)
            continue;

        VariantInit(&name);
        hr = coll->get_Name(&name);
        test_hr(hr, S_OK);
        if (V_VT(&name) == VT_BSTR)
            ok(lstrcmpW(V_BSTR(&name), names[n]) == 0, "Name = %ls, expected %ls\n", V_BSTR(&name), names[n]);
        VariantClear(&name);

        hr = coll->get_Count(&count);
        test_hr(hr, S_OK);
        ok(count == 0, "%ls Count before Populate = %ld\n", names[n], count);

        hr = coll->Populate();
        test_hr(hr, S_OK);
        hr = coll->get_Count(&count);
        test_hr(hr, S_OK);
        ok(count >= 0, "%ls Count after Populate = %ld\n", names[n], count);

        for (i = 0; i < count; i++)
        {
            IDispatch *item = NULL;
            ICatalogObject *obj = NULL;
            VARIANT key, nameValue, byProp;

            hr = coll->get_Item(i, &item);
            test_hr(hr, S_OK);
            if (FAILED(hr) || item == NULL)
                continue;

            if (SUCCEEDED(item->QueryInterface(kIidObject, (void **)&obj)))
            {
                VariantInit(&key);
                VariantInit(&nameValue);
                VariantInit(&byProp);

                hr = obj->get_Key(&key);
                test_hr(hr, S_OK);
                ok(V_VT(&key) == VT_BSTR, "%ls Key vt = %d\n", names[n], V_VT(&key));
                obj->get_Name(&nameValue);

                if (n == 0 && V_VT(&key) == VT_BSTR)
                {
                    BSTR prop = SysAllocString(L"CLSID");

                    ok(is_braced_guid(V_BSTR(&key)), "component key %ls is not a braced GUID\n", V_BSTR(&key));
                    hr = obj->get_Value(prop, &byProp);
                    test_hr(hr, S_OK);
                    if (V_VT(&byProp) == VT_BSTR)
                        ok(lstrcmpiW(V_BSTR(&byProp), V_BSTR(&key)) == 0, "Value(CLSID) = %ls, Key = %ls\n",
                           V_BSTR(&byProp), V_BSTR(&key));
                    SysFreeString(prop);
                }
                else if (n == 1 && V_VT(&key) == VT_BSTR)
                {
                    ok(V_VT(&nameValue) == VT_BSTR, "role Name vt = %d\n", V_VT(&nameValue));
                    if (V_VT(&nameValue) == VT_BSTR)
                        ok(lstrcmpiW(V_BSTR(&nameValue), V_BSTR(&key)) == 0, "role Name = %ls, Key = %ls\n",
                           V_BSTR(&nameValue), V_BSTR(&key));
                }

                VariantClear(&key);
                VariantClear(&nameValue);
                VariantClear(&byProp);
                obj->Release();
            }
            item->Release();
        }

        trace("%ls: %ld item(s)\n", names[n], count);
        coll->Release();
    }

    coll = get_related(apps, L"NoSuchRelated", appKey, &hr);
    test_hr(hr, E_INVALIDARG);
    if (coll != NULL)
        coll->Release();

    coll = get_related(apps, L"Components", L"{00000000-0000-0000-0000-000000000000}", &hr);
    test_hr(hr, S_OK);
    if (coll != NULL)
    {
        LONG unknownCount = -1;

        hr = coll->Populate();
        trace("Populate for an unknown application key hr = 0x%08lx\n", (unsigned long)hr);
        if (SUCCEEDED(hr) && SUCCEEDED(coll->get_Count(&unknownCount)))
            trace("Components count for an unknown application key = %ld\n", unknownCount);
        coll->Release();
    }
}

static void test_applications(ICOMAdminCatalog2 *cat)
{
    ICatalogCollection *coll;
    ICatalogCollection *again;
    IDispatch *item = NULL;
    VARIANT name;
    VARIANT_BOOL enabled;
    LONG count = -1, count2 = -1, major = -1, minor = -1, changes = -1;
    HRESULT hr;
    LONG i;

    coll = get_collection(cat, L"Applications");
    if (coll == NULL)
        return;

    VariantInit(&name);
    hr = coll->get_Name(&name);
    test_hr(hr, S_OK);
    ok(V_VT(&name) == VT_BSTR, "Name vt = %d\n", V_VT(&name));
    if (V_VT(&name) == VT_BSTR)
        ok(lstrcmpW(V_BSTR(&name), L"Applications") == 0, "Name = %ls\n", V_BSTR(&name));
    VariantClear(&name);

    hr = coll->get_DataStoreMajorVersion(&major);
    test_hr(hr, S_OK);
    ok(major == 5, "DataStoreMajorVersion = %ld\n", major);
    hr = coll->get_DataStoreMinorVersion(&minor);
    test_hr(hr, S_OK);
    ok(minor == 0, "DataStoreMinorVersion = %ld\n", minor);

    hr = coll->get_AddEnabled(&enabled);
    test_hr(hr, S_OK);
    trace("AddEnabled = %d\n", (int)enabled);
    hr = coll->get_RemoveEnabled(&enabled);
    test_hr(hr, S_OK);
    trace("RemoveEnabled = %d\n", (int)enabled);

    hr = coll->get_Count(&count);
    test_hr(hr, S_OK);
    ok(count == 0, "Count before Populate = %ld\n", count);

    hr = coll->Populate();
    test_hr(hr, S_OK);
    hr = coll->get_Count(&count);
    test_hr(hr, S_OK);
    ok(count >= 0, "Count after Populate = %ld\n", count);

    hr = coll->Populate();
    test_hr(hr, S_OK);
    hr = coll->get_Count(&count2);
    test_hr(hr, S_OK);
    ok(count2 == count, "Count changed on second Populate: %ld vs %ld\n", count, count2);

    hr = coll->SaveChanges(&changes);
    test_hr(hr, S_OK);
    ok(changes == 0, "SaveChanges reported %ld changes\n", changes);

    hr = coll->get_Item(-1, &item);
    test_hr(hr, E_INVALIDARG);
    hr = coll->get_Item(count, &item);
    test_hr(hr, E_INVALIDARG);

    again = get_collection(cat, L"Applications");
    if (again != NULL)
    {
        LONG fresh = -1;

        hr = again->get_Count(&fresh);
        test_hr(hr, S_OK);
        ok(fresh == 0, "Count of a new, unpopulated collection = %ld\n", fresh);
        again->Release();
    }

    test_collection_dispatch(coll);

    if (count == 0)
    {
        skip("no applications in the catalog, item tests skipped\n");
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            ICatalogObject *obj = NULL;

            item = NULL;
            hr = coll->get_Item(i, &item);
            test_hr(hr, S_OK);
            if (FAILED(hr) || item == NULL)
                continue;

            hr = item->QueryInterface(kIidObject, (void **)&obj);
            test_hr(hr, S_OK);
            if (SUCCEEDED(hr))
            {
                VARIANT appKey;

                test_object(obj, L"ID");

                VariantInit(&appKey);
                if (SUCCEEDED(obj->get_Key(&appKey)) && V_VT(&appKey) == VT_BSTR)
                    test_related(coll, V_BSTR(&appKey));
                VariantClear(&appKey);

                obj->Release();
            }
            item->Release();
        }

        test_enumeration(coll, count);
    }

    coll->Release();
}

static void test_connect(ICOMAdminCatalog2 *cat)
{
    ICatalogCollection *root = NULL;
    IDispatch *disp = NULL, *sub = NULL;
    VARIANT name, key;
    LONG count = -1;
    BSTR bstr;
    HRESULT hr;

    bstr = SysAllocString(L"");
    hr = cat->Connect(bstr, &disp);
    SysFreeString(bstr);
    test_hr(hr, S_OK);
    if (SUCCEEDED(hr) && disp != NULL)
    {
        hr = disp->QueryInterface(kIidCollection, (void **)&root);
        test_hr(hr, S_OK);
        disp->Release();
    }

    if (root != NULL)
    {
        VariantInit(&name);
        hr = root->get_Name(&name);
        test_hr(hr, S_OK);
        if (V_VT(&name) == VT_BSTR)
            ok(lstrcmpW(V_BSTR(&name), L"Root") == 0, "Name = %ls\n", V_BSTR(&name));
        VariantClear(&name);

        hr = root->get_Count(&count);
        test_hr(hr, S_OK);
        ok(count == 0, "Root Count = %ld\n", count);

        VariantInit(&key);
        bstr = SysAllocString(L"Applications");
        hr = root->GetCollection(bstr, key, &sub);
        SysFreeString(bstr);
        test_hr(hr, E_INVALIDARG);
        if (SUCCEEDED(hr) && sub != NULL)
            sub->Release();

        root->Release();
    }

    disp = (IDispatch *)0x1;
    bstr = SysAllocString(L"NoSuchMachine");
    hr = cat->Connect(bstr, &disp);
    SysFreeString(bstr);
    test_hr(hr, HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE));
}

static void test_errors(ICOMAdminCatalog2 *cat)
{
    IDispatch *disp = (IDispatch *)0x1;
    BSTR bstr = SysAllocString(L"NoSuchCollection");
    HRESULT hr = cat->GetCollection(bstr, &disp);

    SysFreeString(bstr);
    test_hr(hr, E_INVALIDARG);
}

START_TEST(comadmin)
{
    ICOMAdminCatalog2 *cat = NULL;
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ok(SUCCEEDED(hr), "CoInitializeEx failed: 0x%08lx\n", (unsigned long)hr);

    test_creation();
    test_aggregation();

    hr = CoCreateInstance(kClsidCatalog, NULL, CLSCTX_INPROC_SERVER, kIidCatalog2, (void **)&cat);
    if (FAILED(hr))
    {
        skip("COM+ catalog is not available (0x%08lx)\n", (unsigned long)hr);
        CoUninitialize();
        return;
    }

    test_catalog_dispatch(cat);
    test_versions(cat);
    test_applications(cat);
    test_connect(cat);
    test_errors(cat);

    cat->Release();
    CoUninitialize();
}