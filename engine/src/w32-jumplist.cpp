/* Copyright (C) 2016 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "prefix.h"

#include "globals.h"
#include "platform.h"

// ── Windows Jump List via ICustomDestinationList (Windows 7+) ────────────────
//
// All COM interfaces are declared inline with explicit GUIDs to avoid
// taking a dependency on <shobjidl.h> / <shobjidl_core.h> under the
// project's _WIN32_WINNT=0x0601 setting.
//
// GUIDs from the published Windows SDK:
//   CLSID_DestinationList             {77F10CF0-3DB5-4966-B520-B7C54FD35ED6}
//   ICustomDestinationList            {6332DEBF-87B5-4670-90C0-5E57B408A49E}
//   CLSID_EnumerableObjectCollection  {2D3468C1-36A7-43B6-AC24-D3F02FD9607A}
//   IObjectCollection                 {5632B1A4-E38A-400A-928A-D4CD63230295}
//   IObjectArray                      {92CA9DCD-5622-4BBA-A805-5E9F541BD8C9}
//   CLSID_ShellLink                   {00021401-0000-0000-C000-000000000046}
//   IShellLinkW                       {000214F9-0000-0000-C000-000000000046}
//   IPropertyStore                    {886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99}
//
// Property keys:
//   PKEY_Title                              {F29F85E0-...}, pid=2
//   PKEY_AppUserModel_IsDestListSeparatorItem {9F4C2855-...}, pid=6

static const CLSID s_CLSID_DestinationList =
    {0x77f10cf0, 0x3db5, 0x4966, {0xb5, 0x20, 0xb7, 0xc5, 0x4f, 0xd3, 0x5e, 0xd6}};

static const CLSID s_CLSID_EnumerableObjectCollection =
    {0x2d3468c1, 0x36a7, 0x43b6, {0xac, 0x24, 0xd3, 0xf0, 0x2f, 0xd9, 0x60, 0x7a}};

static const CLSID s_CLSID_ShellLink =
    {0x00021401, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

static const PROPERTYKEY s_PKEY_Title =
    {{0xf29f85e0, 0x4ff9, 0x1068, {0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9}}, 2};

static const PROPERTYKEY s_PKEY_AppUserModel_IsDestListSeparatorItem =
    {{0x9f4c2855, 0x9f79, 0x4b39, {0xa8, 0xd0, 0xe1, 0xd4, 0x2d, 0xe1, 0xd5, 0xf3}}, 6};

// ── COM interface declarations ───────────────────────────────────────────────

// IObjectArray — read-only view of an object collection.
MIDL_INTERFACE("92CA9DCD-5622-4BBA-A805-5E9F541BD8C9")
IObjectArray : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT *pcObjects) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAt(UINT uiIndex, REFIID riid, void **ppv) = 0;
};

// IObjectCollection — mutable object collection; inherits IObjectArray.
MIDL_INTERFACE("5632B1A4-E38A-400A-928A-D4CD63230295")
IObjectCollection : public IObjectArray
{
public:
    virtual HRESULT STDMETHODCALLTYPE AddObject(IUnknown *punk) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddFromArray(IObjectArray *poaSource) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveObjectAt(UINT uiIndex) = 0;
    virtual HRESULT STDMETHODCALLTYPE Clear(void) = 0;
};

// IPropertyStore — key/value property bag attached to shell objects.
MIDL_INTERFACE("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99")
IPropertyStore : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD *cProps) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAt(DWORD iProp, PROPERTYKEY *pkey) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetValue(const PROPERTYKEY &key, PROPVARIANT *pv) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetValue(const PROPERTYKEY &key, const PROPVARIANT &propvar) = 0;
    virtual HRESULT STDMETHODCALLTYPE Commit(void) = 0;
};

// IShellLinkW — declare all methods up through SetPath to keep vtable correct.
// Only SetArguments, SetIconLocation, SetShowCmd and SetPath are called.
MIDL_INTERFACE("000214F9-0000-0000-C000-000000000046")
IShellLinkW : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetPath(LPWSTR, int, void *, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIDList(void **) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetIDList(void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDescription(LPWSTR, int) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDescription(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWorkingDirectory(LPWSTR, int) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetWorkingDirectory(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetArguments(LPWSTR, int) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetArguments(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHotkey(WORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetHotkey(WORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShowCmd(int *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShowCmd(int) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIconLocation(LPWSTR, int, int *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetIconLocation(LPCWSTR, int) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetRelativePath(LPCWSTR, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE Resolve(HWND, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPath(LPCWSTR) = 0;
};

// ICustomDestinationList — the Jump List builder interface.
MIDL_INTERFACE("6332DEBF-87B5-4670-90C0-5E57B408A49E")
ICustomDestinationList : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetAppID(LPCWSTR pszAppID) = 0;
    virtual HRESULT STDMETHODCALLTYPE BeginList(UINT *pcMaxSlots, REFIID riid, void **ppv) = 0;
    virtual HRESULT STDMETHODCALLTYPE AppendCategory(LPCWSTR pszCategory, IObjectArray *poa) = 0;
    virtual HRESULT STDMETHODCALLTYPE AppendKnownCategory(int category) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddUserTasks(IObjectArray *poa) = 0;
    virtual HRESULT STDMETHODCALLTYPE CommitList(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRemovedDestinations(REFIID riid, void **ppv) = 0;
    virtual HRESULT STDMETHODCALLTYPE DeleteList(LPCWSTR pszAppID) = 0;
    virtual HRESULT STDMETHODCALLTYPE AbortList(void) = 0;
};

////////////////////////////////////////////////////////////////////////////////
// Helpers

// Create a separator shell link (the thin divider line in the jump list).
static IShellLinkW *CreateSeparatorLink()
{
    IShellLinkW *t_link = NULL;
    if (FAILED(CoCreateInstance(s_CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                __uuidof(IShellLinkW), (void **)&t_link)))
        return NULL;

    IPropertyStore *t_store = NULL;
    if (FAILED(t_link->QueryInterface(__uuidof(IPropertyStore), (void **)&t_store)))
    {
        t_link->Release();
        return NULL;
    }

    PROPVARIANT t_pv;
    memset(&t_pv, 0, sizeof(t_pv));
    t_pv.vt      = VT_BOOL;
    t_pv.boolVal = (VARIANT_BOOL)-1; // VARIANT_TRUE
    t_store->SetValue(s_PKEY_AppUserModel_IsDestListSeparatorItem, t_pv);
    t_store->Commit();
    t_store->Release();

    return t_link;
}

// Create a task shell link: clicking it relaunches the exe with
//   --jumplist-task=<tag>
// appended to its argument list, which the relaunch handler can inspect.
static IShellLinkW *CreateTaskLink(LPCWSTR p_label, LPCWSTR p_tag,
                                   LPCWSTR p_exe_path)
{
    IShellLinkW *t_link = NULL;
    if (FAILED(CoCreateInstance(s_CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                __uuidof(IShellLinkW), (void **)&t_link)))
        return NULL;

    // Build the argument string: "--jumplist-task=<tag>"
    // Maximum tag length we'll accept is 256 wide chars.
    wchar_t t_args[300] = {};
    wcscpy_s(t_args, L"--jumplist-task=");
    wcsncat_s(t_args, p_tag, 256);

    t_link->SetPath(p_exe_path);
    t_link->SetArguments(t_args);
    t_link->SetIconLocation(p_exe_path, 0);
    t_link->SetShowCmd(1 /* SW_SHOWNORMAL */);

    // Set the display title via IPropertyStore.
    IPropertyStore *t_store = NULL;
    if (SUCCEEDED(t_link->QueryInterface(__uuidof(IPropertyStore), (void **)&t_store)))
    {
        PROPVARIANT t_pv;
        memset(&t_pv, 0, sizeof(t_pv));
        t_pv.vt      = VT_LPWSTR;
        t_pv.pwszVal = const_cast<LPWSTR>(p_label);
        t_store->SetValue(s_PKEY_Title, t_pv);
        t_store->Commit();
        t_store->Release();
    }

    return t_link;
}

////////////////////////////////////////////////////////////////////////////////
// MCPlatformSetJumpList

void MCPlatformSetJumpList(MCStringRef p_tasks, MCStringRef p_category)
{
    // Create the destination list COM object.
    ICustomDestinationList *t_list = NULL;
    if (FAILED(CoCreateInstance(s_CLSID_DestinationList, NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(ICustomDestinationList),
                                (void **)&t_list)))
        return;

    // Empty or nil tasks string — clear the jump list and return.
    if (p_tasks == nil || MCStringIsEmpty(p_tasks))
    {
        t_list->DeleteList(NULL);
        t_list->Release();
        return;
    }

    // Start the transaction; we ignore the removed-destinations list since
    // these are scripted tasks (not user-pinned destinations).
    UINT t_max_slots = 0;
    IObjectArray *t_removed = NULL;
    HRESULT t_hr = t_list->BeginList(&t_max_slots,
                                     __uuidof(IObjectArray),
                                     (void **)&t_removed);
    if (FAILED(t_hr))
    {
        t_list->Release();
        return;
    }
    if (t_removed != NULL)
        t_removed->Release();

    // Get the path of the running executable for use in each shell link.
    wchar_t t_exe_path[MAX_PATH] = {};
    GetModuleFileNameW(NULL, t_exe_path, MAX_PATH);

    // Create an object collection to hold the task links.
    IObjectCollection *t_collection = NULL;
    t_hr = CoCreateInstance(s_CLSID_EnumerableObjectCollection, NULL,
                            CLSCTX_INPROC_SERVER,
                            __uuidof(IObjectCollection),
                            (void **)&t_collection);
    if (FAILED(t_hr))
    {
        t_list->AbortList();
        t_list->Release();
        return;
    }

    // Convert the tasks MCStringRef to a wide string for parsing.
    unichar_t *t_wbuf = NULL;
    if (!MCStringConvertToWString(p_tasks, t_wbuf))
    {
        t_collection->Release();
        t_list->AbortList();
        t_list->Release();
        return;
    }

    // Parse "Label|tag,-,Label|tag,..." one comma-delimited item at a time.
    wchar_t *t_p = reinterpret_cast<wchar_t *>(t_wbuf);
    while (*t_p != L'\0')
    {
        // Find the end of this item.
        wchar_t *t_comma = wcschr(t_p, L',');
        size_t   t_len   = t_comma ? (size_t)(t_comma - t_p) : wcslen(t_p);

        // Temporarily NUL-terminate the item.
        wchar_t t_saved = t_p[t_len];
        t_p[t_len] = L'\0';

        IShellLinkW *t_link = NULL;
        if (wcscmp(t_p, L"-") == 0)
        {
            // Separator.
            t_link = CreateSeparatorLink();
        }
        else
        {
            // Split on '|' to get label and tag.
            wchar_t *t_pipe  = wcschr(t_p, L'|');
            wchar_t *t_label = t_p;
            wchar_t *t_tag   = t_pipe ? t_pipe + 1 : t_p;
            if (t_pipe)
                *t_pipe = L'\0';

            t_link = CreateTaskLink(t_label, t_tag, t_exe_path);

            if (t_pipe)
                *t_pipe = L'|';
        }

        if (t_link != NULL)
        {
            t_collection->AddObject(t_link);
            t_link->Release();
        }

        // Restore the comma and advance.
        t_p[t_len] = t_saved;
        t_p += t_len;
        if (*t_p == L',')
            t_p++;
    }

    MCMemoryDeallocate(t_wbuf);

    // Obtain IObjectArray view of the collection.
    IObjectArray *t_array = NULL;
    t_collection->QueryInterface(__uuidof(IObjectArray), (void **)&t_array);
    t_collection->Release();

    if (t_array == NULL)
    {
        t_list->AbortList();
        t_list->Release();
        return;
    }

    // Append the tasks — as a named custom category if one was given,
    // otherwise into the standard pinned "Tasks" section.
    if (p_category != nil && !MCStringIsEmpty(p_category))
    {
        unichar_t *t_cat_buf = NULL;
        if (MCStringConvertToWString(p_category, t_cat_buf))
        {
            t_list->AppendCategory(reinterpret_cast<LPCWSTR>(t_cat_buf), t_array);
            MCMemoryDeallocate(t_cat_buf);
        }
        else
        {
            t_list->AddUserTasks(t_array);
        }
    }
    else
    {
        t_list->AddUserTasks(t_array);
    }

    t_array->Release();
    t_list->CommitList();
    t_list->Release();
}
