#include <include/cef_app.h>

#include <Windows.h>

////////////////////////////////////////////////////////////////////////////////

extern bool MCCefCreateApp(CefRefPtr<CefApp> &r_app);

////////////////////////////////////////////////////////////////////////////////

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	CefMainArgs t_args(hInstance);
	
	CefRefPtr<CefApp> t_app;
	if (!MCCefCreateApp(t_app))
		return -1;
	
	return CefExecuteProcess(t_args, t_app, NULL);
}

////////////////////////////////////////////////////////////////////////////////
