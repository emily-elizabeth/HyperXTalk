/* Copyright (C) 2024 HyperXTalk contributors.

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

#include <core.h>

#include "libbrowser_webkitgtk.h"

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <cstring>
#include <cstdlib>

////////////////////////////////////////////////////////////////////////////////
// WebKitGTK type declarations — we use the stubs wrappers so these are
// declared as opaque pointers with the real WebKit/JSC functions resolved
// at runtime via dlsym.

typedef struct _WebKitWebView WebKitWebView;
typedef struct _WebKitSettings WebKitSettings;
typedef struct _WebKitUserContentManager WebKitUserContentManager;
typedef struct _WebKitUserScript WebKitUserScript;
typedef struct _WebKitPolicyDecision WebKitPolicyDecision;
typedef struct _WebKitNavigationPolicyDecision WebKitNavigationPolicyDecision;
typedef struct _WebKitNavigationAction WebKitNavigationAction;
typedef struct _WebKitURIRequest WebKitURIRequest;
typedef struct _JSCValue JSCValue;

// WebKitUserScriptInjectionTime
enum {
	WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START = 0,
	WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END = 1,
};

// WebKitUserContentInjectedFrames
enum {
	WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES = 0,
	WEBKIT_USER_CONTENT_INJECT_TOP_FRAME = 1,
};

// WebKitLoadEvent
enum {
	WEBKIT_LOAD_STARTED = 0,
	WEBKIT_LOAD_REDIRECTED = 1,
	WEBKIT_LOAD_COMMITTED = 2,
	WEBKIT_LOAD_FINISHED = 3,
};

// WebKitPolicyDecisionType
enum {
	WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION = 0,
	WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION = 1,
	WEBKIT_POLICY_DECISION_TYPE_RESPONSE = 2,
};

// WebKitNavigationType
enum {
	WEBKIT_NAVIGATION_TYPE_LINK_CLICKED = 0,
	WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED = 1,
	WEBKIT_NAVIGATION_TYPE_BACK_FORWARD = 2,
	WEBKIT_NAVIGATION_TYPE_RELOAD = 3,
	WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED = 4,
	WEBKIT_NAVIGATION_TYPE_OTHER = 5,
};

// GLib/GIO types are already defined via <gtk/gtk.h> includes

////////////////////////////////////////////////////////////////////////////////
// Extern declarations for stubs-generated wrapper functions

extern "C" {

void *webkit_web_view_new_with_user_content_manager(void *ucm);
void webkit_web_view_load_uri(void *web_view, void *uri);
void webkit_web_view_load_html(void *web_view, void *content, void *base_uri);
void webkit_web_view_go_back(void *web_view);
void webkit_web_view_go_forward(void *web_view);
int webkit_web_view_can_go_back(void *web_view);
int webkit_web_view_can_go_forward(void *web_view);
void webkit_web_view_stop_loading(void *web_view);
void webkit_web_view_reload(void *web_view);
void *webkit_web_view_get_uri(void *web_view);
void *webkit_web_view_get_settings(void *web_view);
double webkit_web_view_get_estimated_load_progress(void *web_view);
void *webkit_web_view_get_user_content_manager(void *web_view);
int webkit_web_view_get_tls_info(void *web_view, void *certificate, void *errors);
void webkit_web_view_evaluate_javascript(void *web_view, void *script, size_t length,
	void *world_name, void *source_uri, void *cancellable,
	void *callback, void *user_data);
void *webkit_web_view_evaluate_javascript_finish(void *web_view, void *result, void *error);

void *webkit_settings_new(void);
void *webkit_settings_get_user_agent(void *settings);
void webkit_settings_set_user_agent(void *settings, void *user_agent);

void *webkit_user_content_manager_new(void);
int webkit_user_content_manager_register_script_message_handler(void *manager, void *name);
void webkit_user_content_manager_unregister_script_message_handler(void *manager, void *name);
void webkit_user_content_manager_add_script(void *manager, void *script);
void webkit_user_content_manager_remove_all_scripts(void *manager);

void *webkit_user_script_new(void *source, int injected_frames, int injection_time,
	void *allow_list, void *block_list);
void webkit_user_script_unref(void *script);

void webkit_policy_decision_use(void *decision);
void webkit_policy_decision_ignore(void *decision);

void *webkit_navigation_policy_decision_get_navigation_action(void *decision);

void *webkit_navigation_action_get_request(void *navigation);
int webkit_navigation_action_get_navigation_type(void *navigation);

void *webkit_uri_request_get_uri(void *request);

void *jsc_value_to_string(void *value);
int jsc_value_is_string(void *value);
int jsc_value_is_undefined(void *value);
int jsc_value_is_null(void *value);

int initialise_weak_link_webkit2gtk(void);
int initialise_weak_link_javascriptcoregtk(void);

}

////////////////////////////////////////////////////////////////////////////////

#define LIBBROWSER_DUMMY_URL "http://libbrowser_dummy_url/"

////////////////////////////////////////////////////////////////////////////////
// Signal callbacks

static void on_load_changed(void *web_view, int load_event, void *user_data)
{
	MCWebKitGTKBrowser *browser = (MCWebKitGTKBrowser *)user_data;
	const char *uri = (const char *)webkit_web_view_get_uri(web_view);
	if (uri == nil)
		uri = "";

	switch (load_event)
	{
		case WEBKIT_LOAD_STARTED:
			browser->OnNavigationBegin(false, uri);
			break;
		case WEBKIT_LOAD_COMMITTED:
			browser->OnDocumentLoadBegin(false, uri);
			break;
		case WEBKIT_LOAD_FINISHED:
			browser->SyncJavaScriptHandlers();
			browser->OnDocumentLoadComplete(false, uri);
			browser->OnNavigationComplete(false, uri);
			break;
	}
}

static int on_load_failed(void *web_view, int load_event,
	char *failing_uri, GError *error, void *user_data)
{
	MCWebKitGTKBrowser *browser = (MCWebKitGTKBrowser *)user_data;
	const char *msg = (error && error->message) ? error->message : "unknown error";
	browser->OnDocumentLoadFailed(false, failing_uri, msg);
	browser->OnNavigationFailed(false, failing_uri, msg);
	return 0;
}

static bool uri_scheme_is_navigable(const char *p_uri)
{
	if (p_uri == nil)
		return false;

	static const char *s_navigable[] = {
		"http://", "https://", "file://", "about:", "data:", "blob:", "javascript:",
	};

	for (size_t i = 0; i < sizeof(s_navigable) / sizeof(s_navigable[0]); i++)
	{
		size_t len = strlen(s_navigable[i]);
		if (strncasecmp(p_uri, s_navigable[i], len) == 0)
			return true;
	}
	return false;
}

static int on_decide_policy(void *web_view, void *decision, int type, void *user_data)
{
	MCWebKitGTKBrowser *browser = (MCWebKitGTKBrowser *)user_data;

	if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
	{
		bool t_allow = false;
		browser->GetBoolProperty(kMCBrowserAllowNewWindows, t_allow);
		if (!t_allow)
		{
			webkit_policy_decision_ignore(decision);
			return 1;
		}
	}

	if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
	{
		void *action = webkit_navigation_policy_decision_get_navigation_action(decision);
		void *request = webkit_navigation_action_get_request(action);
		const char *uri = (const char *)webkit_uri_request_get_uri(request);

		if (!uri_scheme_is_navigable(uri))
		{
			browser->OnNavigationRequestUnhandled(false, uri);
			webkit_policy_decision_ignore(decision);
			return 1;
		}
	}

	return 0;
}

static int on_context_menu(void *web_view, void *menu,
	void *event, void *hit_test, void *user_data)
{
	MCWebKitGTKBrowser *browser = (MCWebKitGTKBrowser *)user_data;
	bool t_enabled = true;
	browser->GetBoolProperty(kMCBrowserEnableContextMenu, t_enabled);
	if (!t_enabled)
		return 1;
	return 0;
}

static void on_progress_changed(void *object, void *pspec, void *user_data)
{
	MCWebKitGTKBrowser *browser = (MCWebKitGTKBrowser *)user_data;
	double progress = webkit_web_view_get_estimated_load_progress(object);
	const char *uri = (const char *)webkit_web_view_get_uri(object);
	if (uri == nil)
		uri = "";
	browser->OnProgressChanged(uri, (uint32_t)(progress * 100));
}

static void on_script_message(void *manager, void *js_result, void *user_data)
{
	MCWebKitGTKBrowser *browser = (MCWebKitGTKBrowser *)user_data;

	// js_result is a WebKitJavascriptResult* — on webkit2gtk 4.1 this is a JSCValue*
	JSCValue *value = (JSCValue *)js_result;
	if (!jsc_value_is_string(value))
		return;

	char *json_str = (char *)jsc_value_to_string(value);
	if (json_str == nil)
		return;

	// Parse the JSON array: ["handlerName", [arg1, arg2, ...]]
	// Simple parsing: find the handler name between first pair of quotes
	char *p = json_str;

	// Skip to opening bracket
	while (*p && *p != '[') p++;
	if (!*p) { g_free(json_str); return; }
	p++;

	// Skip whitespace
	while (*p && (*p == ' ' || *p == '\t')) p++;

	// Expect opening quote for handler name
	if (*p != '"') { g_free(json_str); return; }
	p++;

	// Extract handler name
	char *name_start = p;
	while (*p && *p != '"') p++;
	if (!*p) { g_free(json_str); return; }

	size_t name_len = p - name_start;
	char *handler_name = nil;
	if (!MCCStringCloneSubstring(name_start, name_len, handler_name))
	{
		g_free(json_str);
		return;
	}

	// For now, pass an empty parameter list — full JSON arg parsing can be
	// added as a follow-up when needed.
	MCBrowserListRef t_args = nil;
	MCBrowserListCreate(t_args, 0);

	browser->OnJavaScriptCall(handler_name, t_args);

	MCBrowserListRelease(t_args);
	MCCStringFree(handler_name);
	g_free(json_str);
}

////////////////////////////////////////////////////////////////////////////////
// Synchronous JavaScript evaluation

struct EvalJSContext
{
	bool evaluating;
	char *result;
	bool success;
};

static void eval_js_finished(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	EvalJSContext *ctx = (EvalJSContext *)user_data;
	GError *error = nil;

	void *value = webkit_web_view_evaluate_javascript_finish(
		source_object, res, &error);

	if (error != nil)
	{
		ctx->success = false;
		ctx->result = nil;
		g_error_free(error);
	}
	else if (value != nil)
	{
		if (jsc_value_is_null(value) || jsc_value_is_undefined(value))
			ctx->result = nil;
		else
			ctx->result = (char *)jsc_value_to_string(value);
		ctx->success = true;
		g_object_unref(value);
	}
	else
	{
		ctx->success = true;
		ctx->result = nil;
	}

	ctx->evaluating = false;
	MCBrowserRunloopBreakWait();
}

////////////////////////////////////////////////////////////////////////////////

MCWebKitGTKBrowser::MCWebKitGTKBrowser()
{
	m_plug = nil;
	m_web_view = nil;
	m_content_manager = nil;

	m_js_handlers = nil;
	m_htmltext = nil;
	m_url = nil;

	m_allow_new_windows = false;
	m_enable_context_menu = true;

	m_load_changed_handler = 0;
	m_load_failed_handler = 0;
	m_decide_policy_handler = 0;
	m_context_menu_handler = 0;
	m_script_message_handler = 0;
	m_progress_handler = 0;
}

MCWebKitGTKBrowser::~MCWebKitGTKBrowser()
{
	if (m_web_view != nil)
	{
		if (m_load_changed_handler)
			g_signal_handler_disconnect(m_web_view, m_load_changed_handler);
		if (m_load_failed_handler)
			g_signal_handler_disconnect(m_web_view, m_load_failed_handler);
		if (m_decide_policy_handler)
			g_signal_handler_disconnect(m_web_view, m_decide_policy_handler);
		if (m_context_menu_handler)
			g_signal_handler_disconnect(m_web_view, m_context_menu_handler);
		if (m_progress_handler)
			g_signal_handler_disconnect(m_web_view, m_progress_handler);
	}

	if (m_content_manager != nil && m_script_message_handler)
	{
		g_signal_handler_disconnect(m_content_manager, m_script_message_handler);
		webkit_user_content_manager_unregister_script_message_handler(
			m_content_manager, (void *)"liveCode");
	}

	if (m_plug != nil)
		gtk_widget_destroy((GtkWidget *)m_plug);

	MCBrowserCStringAssign(m_js_handlers, nil);
	MCBrowserCStringAssign(m_htmltext, nil);
	MCBrowserCStringAssign(m_url, nil);
}

////////////////////////////////////////////////////////////////////////////////

bool MCWebKitGTKBrowser::Init(void)
{
	// Create the user content manager
	m_content_manager = webkit_user_content_manager_new();
	if (m_content_manager == nil)
		return false;

	// Register the "liveCode" script message handler for JS→engine calls
	webkit_user_content_manager_register_script_message_handler(
		m_content_manager, (void *)"liveCode");

	char signal_name[] = "script-message-received::liveCode";
	m_script_message_handler = g_signal_connect(
		m_content_manager, signal_name,
		G_CALLBACK(on_script_message), this);

	// Create the web view
	m_web_view = webkit_web_view_new_with_user_content_manager(m_content_manager);
	if (m_web_view == nil)
		return false;

	// Create a GtkPlug for XEMBED embedding
	m_plug = gtk_plug_new(0);
	if (m_plug == nil)
		return false;

	gtk_container_add(GTK_CONTAINER(m_plug), (GtkWidget *)m_web_view);
	gtk_widget_show((GtkWidget *)m_web_view);
	gtk_widget_show((GtkWidget *)m_plug);

	// Connect signals
	m_load_changed_handler = g_signal_connect(
		m_web_view, "load-changed",
		G_CALLBACK(on_load_changed), this);

	m_load_failed_handler = g_signal_connect(
		m_web_view, "load-failed",
		G_CALLBACK(on_load_failed), this);

	m_decide_policy_handler = g_signal_connect(
		m_web_view, "decide-policy",
		G_CALLBACK(on_decide_policy), this);

	m_context_menu_handler = g_signal_connect(
		m_web_view, "context-menu",
		G_CALLBACK(on_context_menu), this);

	m_progress_handler = g_signal_connect(
		m_web_view, "notify::estimated-load-progress",
		G_CALLBACK(on_progress_changed), this);

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void *MCWebKitGTKBrowser::GetNativeLayer()
{
	if (m_plug == nil)
		return nil;

	return (void *)(uintptr_t)gtk_plug_get_id(GTK_PLUG(m_plug));
}

////////////////////////////////////////////////////////////////////////////////

bool MCWebKitGTKBrowser::GetRect(MCBrowserRect &r_rect)
{
	if (m_plug == nil)
		return false;

	GtkAllocation alloc;
	gtk_widget_get_allocation((GtkWidget *)m_plug, &alloc);

	r_rect.left = alloc.x;
	r_rect.top = alloc.y;
	r_rect.right = alloc.x + alloc.width;
	r_rect.bottom = alloc.y + alloc.height;

	return true;
}

bool MCWebKitGTKBrowser::SetRect(const MCBrowserRect &p_rect)
{
	if (m_plug == nil)
		return false;

	int width = p_rect.right - p_rect.left;
	int height = p_rect.bottom - p_rect.top;

	gtk_widget_set_size_request((GtkWidget *)m_web_view, width, height);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Navigation

// WebKitGTK is single-threaded; all API calls must happen on the GTK main
// thread. The x-talk engine normally runs on that thread, but we defensively
// marshal navigation calls via the GLib main loop so secondary threads cannot
// crash the browser by invoking these methods directly.

enum MCWebKitGTKAction
{
    kMCWebKitGTKActionLoadURI,
    kMCWebKitGTKActionLoadHTML,
    kMCWebKitGTKActionGoBack,
    kMCWebKitGTKActionGoForward,
    kMCWebKitGTKActionStopLoading,
    kMCWebKitGTKActionReload
};

struct MCWebKitGTKDispatch
{
    void *web_view;
    MCWebKitGTKAction action;
    char *str1;
    char *str2;
    bool done;
};

static gboolean mcwebkitgtk_dispatch_idle(gpointer p_data)
{
    MCWebKitGTKDispatch *t_dispatch = (MCWebKitGTKDispatch*)p_data;

    switch (t_dispatch->action)
    {
        case kMCWebKitGTKActionLoadURI:
            webkit_web_view_load_uri(t_dispatch->web_view, (void *)t_dispatch->str1);
            break;
        case kMCWebKitGTKActionLoadHTML:
            webkit_web_view_load_html(t_dispatch->web_view, (void *)t_dispatch->str1, (void *)t_dispatch->str2);
            break;
        case kMCWebKitGTKActionGoBack:
            webkit_web_view_go_back(t_dispatch->web_view);
            break;
        case kMCWebKitGTKActionGoForward:
            webkit_web_view_go_forward(t_dispatch->web_view);
            break;
        case kMCWebKitGTKActionStopLoading:
            webkit_web_view_stop_loading(t_dispatch->web_view);
            break;
        case kMCWebKitGTKActionReload:
            webkit_web_view_reload(t_dispatch->web_view);
            break;
    }

    t_dispatch->done = true;
    return G_SOURCE_REMOVE;
}

static void mcwebkitgtk_dispatch(MCWebKitGTKDispatch *p_dispatch)
{
    p_dispatch->done = false;
    g_idle_add(mcwebkitgtk_dispatch_idle, p_dispatch);
    while (!p_dispatch->done)
        MCBrowserRunloopWait();
}

bool MCWebKitGTKBrowser::GoToURL(const char *p_url)
{
	if (m_web_view == nil)
		return false;

	MCWebKitGTKDispatch t_dispatch;
	t_dispatch.web_view = m_web_view;
	t_dispatch.str2 = nil;

	if (MCCStringIsEmpty(p_url))
	{
		t_dispatch.action = kMCWebKitGTKActionLoadHTML;
		t_dispatch.str1 = nil;
	MCCStringClone("<html><body></body></html>", t_dispatch.str1);
	}
	else
	{
		t_dispatch.action = kMCWebKitGTKActionLoadURI;
		t_dispatch.str1 = nil;
		MCCStringClone(p_url, t_dispatch.str1);
	}

	mcwebkitgtk_dispatch(&t_dispatch);
	MCCStringFree(t_dispatch.str1);

	MCBrowserCStringAssignCopy(m_url, p_url);
	MCBrowserCStringAssign(m_htmltext, nil);

	return true;
}

bool MCWebKitGTKBrowser::LoadHTMLText(const char *p_htmltext, const char *p_base_url)
{
	if (m_web_view == nil)
		return false;

	MCWebKitGTKDispatch t_dispatch;
	t_dispatch.web_view = m_web_view;
	t_dispatch.action = kMCWebKitGTKActionLoadHTML;
	t_dispatch.str1 = nil;
	t_dispatch.str2 = nil;
	MCCStringClone(p_htmltext, t_dispatch.str1);
	MCCStringClone(p_base_url, t_dispatch.str2);

	mcwebkitgtk_dispatch(&t_dispatch);
	MCCStringFree(t_dispatch.str1);
	MCCStringFree(t_dispatch.str2);

	MCBrowserCStringAssignCopy(m_htmltext, p_htmltext);
	MCBrowserCStringAssignCopy(m_url, p_base_url);

	return true;
}

bool MCWebKitGTKBrowser::GoBack()
{
	if (m_web_view == nil)
		return false;

	MCWebKitGTKDispatch t_dispatch;
	t_dispatch.web_view = m_web_view;
	t_dispatch.action = kMCWebKitGTKActionGoBack;
	t_dispatch.str1 = nil;
	t_dispatch.str2 = nil;

	mcwebkitgtk_dispatch(&t_dispatch);
	return true;
}

bool MCWebKitGTKBrowser::GoForward()
{
	if (m_web_view == nil)
		return false;

	MCWebKitGTKDispatch t_dispatch;
	t_dispatch.web_view = m_web_view;
	t_dispatch.action = kMCWebKitGTKActionGoForward;
	t_dispatch.str1 = nil;
	t_dispatch.str2 = nil;

	mcwebkitgtk_dispatch(&t_dispatch);
	return true;
}

bool MCWebKitGTKBrowser::StopLoading()
{
	if (m_web_view == nil)
		return false;

	MCWebKitGTKDispatch t_dispatch;
	t_dispatch.web_view = m_web_view;
	t_dispatch.action = kMCWebKitGTKActionStopLoading;
	t_dispatch.str1 = nil;
	t_dispatch.str2 = nil;

	mcwebkitgtk_dispatch(&t_dispatch);
	return true;
}

bool MCWebKitGTKBrowser::Reload()
{
	if (m_web_view == nil)
		return false;

	MCWebKitGTKDispatch t_dispatch;
	t_dispatch.web_view = m_web_view;
	t_dispatch.action = kMCWebKitGTKActionReload;
	t_dispatch.str1 = nil;
	t_dispatch.str2 = nil;

	mcwebkitgtk_dispatch(&t_dispatch);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// JavaScript

bool MCWebKitGTKBrowser::EvaluateJavaScript(const char *p_script, char *&r_result)
{
	if (m_web_view == nil)
		return false;

	EvalJSContext ctx;
	ctx.evaluating = true;
	ctx.result = nil;
	ctx.success = false;

	webkit_web_view_evaluate_javascript(
		m_web_view, (void *)p_script, -1,
		nil, nil, nil,
		(void *)eval_js_finished, &ctx);

	while (ctx.evaluating)
		MCBrowserRunloopWait();

	if (ctx.success)
	{
		MCCStringClone(ctx.result ? ctx.result : "", r_result);
		if (ctx.result)
			g_free(ctx.result);
		return true;
	}

	return false;
}

void MCWebKitGTKBrowser::SyncJavaScriptHandlers()
{
	if (m_web_view == nil || m_content_manager == nil)
		return;

	webkit_user_content_manager_remove_all_scripts(m_content_manager);

	if (MCCStringIsEmpty(m_js_handlers))
		return;

	// Build the injection script
	// Format: window.liveCode = {}; window.liveCode.<handler> = function() { ... };
	size_t script_size = 256;
	char *script = nil;
	MCBrowserMemoryAllocate(script_size, (void *&)script);
	if (script == nil)
		return;

	strcpy(script, "window.liveCode = {};");
	size_t pos = strlen(script);

	// Parse newline-delimited handler names
	const char *p = m_js_handlers;
	while (*p)
	{
		// Skip whitespace/newlines
		while (*p && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t'))
			p++;
		if (!*p)
			break;

		const char *name_start = p;
		while (*p && *p != '\n' && *p != '\r')
			p++;

		size_t name_len = p - name_start;
		if (name_len == 0)
			continue;

		// Build: window.liveCode.<name> = function() {
		//   window.webkit.messageHandlers.liveCode.postMessage(
		//     JSON.stringify(['<name>', Array.prototype.slice.call(arguments)]));
		// };
		size_t needed = pos + name_len * 3 + 256;
		if (needed > script_size)
		{
			script_size = needed * 2;
			void *t_old = script;
			MCBrowserMemoryReallocate(script, script_size, (void *&)script);
			if (script == nil)
			{
				MCBrowserMemoryDeallocate(t_old);
				return;
			}
		}

		pos += snprintf(script + pos, script_size - pos,
			"window.liveCode.%.*s = function() {"
			"window.webkit.messageHandlers.liveCode.postMessage("
			"JSON.stringify(['%.*s', Array.prototype.slice.call(arguments)]));"
			"};",
			(int)name_len, name_start,
			(int)name_len, name_start);
	}

	void *user_script = webkit_user_script_new(
		(void *)script,
		WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
		WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
		nil, nil);

	if (user_script != nil)
	{
		webkit_user_content_manager_add_script(m_content_manager, user_script);
		webkit_user_script_unref(user_script);
	}

	MCBrowserMemoryDeallocate(script);
}

////////////////////////////////////////////////////////////////////////////////
// Properties

bool MCWebKitGTKBrowser::GetUrl(char *&r_url)
{
	if (m_web_view == nil)
		return false;

	const char *uri = (const char *)webkit_web_view_get_uri(m_web_view);
	return MCCStringClone(uri ? uri : "", r_url);
}

bool MCWebKitGTKBrowser::GetHTMLText(char *&r_htmltext)
{
	return EvaluateJavaScript("document.documentElement.outerHTML", r_htmltext);
}

bool MCWebKitGTKBrowser::SetHTMLText(const char *p_htmltext)
{
	return LoadHTMLText(p_htmltext, LIBBROWSER_DUMMY_URL);
}

bool MCWebKitGTKBrowser::GetJavaScriptHandlers(char *&r_handlers)
{
	return MCCStringClone(m_js_handlers ? m_js_handlers : "", r_handlers);
}

bool MCWebKitGTKBrowser::SetJavaScriptHandlers(const char *p_handlers)
{
	char *t_handlers = nil;

	if (!MCCStringIsEmpty(p_handlers) && !MCCStringClone(p_handlers, t_handlers))
		return false;

	MCBrowserCStringAssign(m_js_handlers, t_handlers);
	SyncJavaScriptHandlers();
	return true;
}

bool MCWebKitGTKBrowser::GetUserAgent(char *&r_useragent)
{
	if (m_web_view == nil)
		return false;

	void *settings = webkit_web_view_get_settings(m_web_view);
	const char *ua = (const char *)webkit_settings_get_user_agent(settings);
	return MCCStringClone(ua ? ua : "", r_useragent);
}

bool MCWebKitGTKBrowser::SetUserAgent(const char *p_useragent)
{
	if (m_web_view == nil)
		return false;

	void *settings = webkit_web_view_get_settings(m_web_view);
	webkit_settings_set_user_agent(settings, (void *)p_useragent);
	return true;
}

bool MCWebKitGTKBrowser::GetIsSecure(bool &r_value)
{
	if (m_web_view == nil)
		return false;

	void *cert = nil;
	unsigned int errors = 0;

	if (!webkit_web_view_get_tls_info(m_web_view, &cert, &errors))
	{
		r_value = false;
		return true;
	}

	r_value = (errors == 0 && cert != nil);
	return true;
}

bool MCWebKitGTKBrowser::GetCanGoBack(bool &r_value)
{
	if (m_web_view == nil)
		return false;

	r_value = webkit_web_view_can_go_back(m_web_view) != 0;
	return true;
}

bool MCWebKitGTKBrowser::GetCanGoForward(bool &r_value)
{
	if (m_web_view == nil)
		return false;

	r_value = webkit_web_view_can_go_forward(m_web_view) != 0;
	return true;
}

bool MCWebKitGTKBrowser::GetAllowUserInteraction(bool &r_value)
{
	if (m_web_view == nil)
		return false;

	r_value = gtk_widget_is_sensitive((GtkWidget *)m_web_view) != 0;
	return true;
}

bool MCWebKitGTKBrowser::SetAllowUserInteraction(bool p_value)
{
	if (m_web_view == nil)
		return false;

	gtk_widget_set_sensitive((GtkWidget *)m_web_view, p_value ? 1 : 0);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Property dispatch

bool MCWebKitGTKBrowser::SetBoolProperty(MCBrowserProperty p_property, bool p_value)
{
	switch (p_property)
	{
		case kMCBrowserVerticalScrollbarEnabled:
		{
			const char *val = p_value ? "auto" : "hidden";
			char script[128];
			snprintf(script, sizeof(script),
				"document.body.style.overflowY = '%s'", val);
			char *result = nil;
			EvaluateJavaScript(script, result);
			MCCStringFree(result);
			return true;
		}
		case kMCBrowserHorizontalScrollbarEnabled:
		{
			const char *val = p_value ? "auto" : "hidden";
			char script[128];
			snprintf(script, sizeof(script),
				"document.body.style.overflowX = '%s'", val);
			char *result = nil;
			EvaluateJavaScript(script, result);
			MCCStringFree(result);
			return true;
		}
		case kMCBrowserAllowNewWindows:
			m_allow_new_windows = p_value;
			return true;
		case kMCBrowserEnableContextMenu:
			m_enable_context_menu = p_value;
			return true;
		case kMCBrowserAllowUserInteraction:
			return SetAllowUserInteraction(p_value);
		default:
			break;
	}
	return true;
}

bool MCWebKitGTKBrowser::GetBoolProperty(MCBrowserProperty p_property, bool &r_value)
{
	switch (p_property)
	{
		case kMCBrowserAllowNewWindows:
			r_value = m_allow_new_windows;
			return true;
		case kMCBrowserEnableContextMenu:
			r_value = m_enable_context_menu;
			return true;
		case kMCBrowserIsSecure:
			return GetIsSecure(r_value);
		case kMCBrowserCanGoForward:
			return GetCanGoForward(r_value);
		case kMCBrowserCanGoBack:
			return GetCanGoBack(r_value);
		case kMCBrowserAllowUserInteraction:
			return GetAllowUserInteraction(r_value);
		case kMCBrowserVerticalScrollbarEnabled:
		case kMCBrowserHorizontalScrollbarEnabled:
			r_value = true;
			return true;
		case kMCBrowserScrollEnabled:
			r_value = true;
			return true;
		case kMCBrowserScrollCanBounce:
			r_value = false;
			return true;
		default:
			break;
	}
	return true;
}

bool MCWebKitGTKBrowser::SetStringProperty(MCBrowserProperty p_property, const char *p_utf8_string)
{
	switch (p_property)
	{
		case kMCBrowserHTMLText:
			return SetHTMLText(p_utf8_string);
		case kMCBrowserJavaScriptHandlers:
			return SetJavaScriptHandlers(p_utf8_string);
		case kMCBrowserUserAgent:
			return SetUserAgent(p_utf8_string);
		default:
			break;
	}
	return true;
}

bool MCWebKitGTKBrowser::GetStringProperty(MCBrowserProperty p_property, char *&r_utf8_string)
{
	switch (p_property)
	{
		case kMCBrowserURL:
			return GetUrl(r_utf8_string);
		case kMCBrowserHTMLText:
			return GetHTMLText(r_utf8_string);
		case kMCBrowserJavaScriptHandlers:
			return GetJavaScriptHandlers(r_utf8_string);
		case kMCBrowserUserAgent:
			return GetUserAgent(r_utf8_string);
		default:
			break;
	}
	return true;
}

bool MCWebKitGTKBrowser::SetIntegerProperty(MCBrowserProperty p_property, int32_t p_value)
{
	return true;
}

bool MCWebKitGTKBrowser::GetIntegerProperty(MCBrowserProperty p_property, int32_t &r_value)
{
	switch (p_property)
	{
		case kMCBrowserDataDetectorTypes:
			r_value = 0;
			return true;
		default:
			break;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Factory

class MCWebKitGTKBrowserFactory : public MCBrowserFactory
{
public:
	virtual bool CreateBrowser(void *p_display, void *p_parent_view, MCBrowser *&r_browser)
	{
		MCWebKitGTKBrowser *t_browser;
		t_browser = new (nothrow) MCWebKitGTKBrowser();
		if (t_browser == nil)
			return false;

		if (!t_browser->Init())
		{
			delete t_browser;
			return false;
		}

		r_browser = t_browser;
		return true;
	}
};

static bool s_webkitgtk_initialized = false;

bool MCWebKitGTKBrowserFactoryCreate(MCBrowserFactoryRef &r_factory)
{
	if (!s_webkitgtk_initialized)
	{
		if (!initialise_weak_link_webkit2gtk())
			return false;
		if (!initialise_weak_link_javascriptcoregtk())
			return false;
		s_webkitgtk_initialized = true;
	}

	MCWebKitGTKBrowserFactory *t_factory;
	t_factory = new (nothrow) MCWebKitGTKBrowserFactory();
	if (t_factory == nil)
		return false;

	r_factory = (MCBrowserFactoryRef)t_factory;
	return true;
}

////////////////////////////////////////////////////////////////////////////////
