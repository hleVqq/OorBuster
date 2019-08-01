#include <Windows.h>
#include <tchar.h>
#include <shellapi.h>
#include <lowlevelmonitorconfigurationapi.h>

#include "resource.h"

const BYTE VCP_OSD_LANGUAGE = 0xCC;
const BYTE VCP_PICTURE_MODE = 0xDC;
const BYTE VCP_AMA = 0xF0;

enum
{
	WM_APP_RELOAD = WM_APP + 1,
	WM_APP_EXIT
};

DWORD CachedOsdLanguage = 2;
DWORD CachedPictureMode = 0;
DWORD CachedAma = 1;

DWORD OorDelay = 2000;
DWORD PicDelay = 500;
DWORD AmaDelay = 250;
DWORD WakeDelay = 3000;

NOTIFYICONDATA TrayIcon;
HANDLE Monitor;

int GetRefreshRate()
{
	DEVMODE dm;
	dm.dmSize = sizeof(DEVMODE);
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm);

	return dm.dmDisplayFrequency;
}

HANDLE GetPhysicalMonitor()
{
	HMONITOR monitor = MonitorFromWindow(GetActiveWindow(), MONITOR_DEFAULTTOPRIMARY);
	LPPHYSICAL_MONITOR monitors = (LPPHYSICAL_MONITOR)malloc(sizeof(PHYSICAL_MONITOR));
	HANDLE pm = nullptr;

	if (monitors && GetPhysicalMonitorsFromHMONITOR(monitor, 1, monitors))
		pm = monitors[0].hPhysicalMonitor;

	free(monitors);

	return pm;
}

void CacheVcpValues()
{
	Monitor = GetPhysicalMonitor();

	GetVCPFeatureAndVCPFeatureReply(Monitor, VCP_OSD_LANGUAGE, NULL, &CachedOsdLanguage, NULL);
	GetVCPFeatureAndVCPFeatureReply(Monitor, VCP_PICTURE_MODE, NULL, &CachedPictureMode, NULL);
	GetVCPFeatureAndVCPFeatureReply(Monitor, VCP_AMA, NULL, &CachedAma, NULL);
}

void ReadLaunchParams()
{
	int argCount;
	LPWSTR* args = CommandLineToArgvW(GetCommandLine(), &argCount);

	if (args && argCount > 1)
	{
		OorDelay = (DWORD)wcstod(args[1], L'\0');
		PicDelay = (DWORD)wcstod(args[2], L'\0');
		AmaDelay = (DWORD)wcstod(args[3], L'\0');
		WakeDelay = (DWORD)wcstod(args[4], L'\0');
	}
}

inline void FixOor() {SetVCPFeature(Monitor, VCP_OSD_LANGUAGE, CachedOsdLanguage);}
inline void FixPic() {SetVCPFeature(Monitor, VCP_PICTURE_MODE, CachedPictureMode);}
inline void FixAma() {SetVCPFeature(Monitor, VCP_AMA, CachedAma);}

void ApplyVcpValues(bool wake = false)
{
	if (GetRefreshRate() <= 144)
		return;

	if (wake)
		Sleep(WakeDelay);

	Sleep(OorDelay); FixOor();
	Sleep(PicDelay); FixPic();
	Sleep(AmaDelay); FixAma();
}

void ShowTrayMenu(HWND wnd)
{
	POINT pos;
	GetCursorPos(&pos);

	HMENU menu = CreatePopupMenu();
	InsertMenu(menu, -1, MF_BYPOSITION, WM_APP_RELOAD, L"Reload");
	InsertMenu(menu, -1, MF_BYPOSITION, WM_APP_EXIT, L"Exit");
	SetForegroundWindow(wnd);
	TrackPopupMenu(menu, TPM_BOTTOMALIGN, pos.x, pos.y, 0, wnd, NULL);
	DestroyMenu(menu);
}

LRESULT CALLBACK WindowProc(_In_ HWND wnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	switch (msg)
	{
	case WM_DISPLAYCHANGE:
		ApplyVcpValues();

		break;
	case WM_POWERBROADCAST:
		if (wParam == PBT_POWERSETTINGCHANGE)
		{
			const DWORD DISPLAY_ON = 0x1;

			POWERBROADCAST_SETTING* pbs = (POWERBROADCAST_SETTING *)lParam;
			DWORD status = *(DWORD*)(pbs->Data);

			if (status == DISPLAY_ON)
				ApplyVcpValues(true);
		}

		break;
	case WM_APP:
		switch (lParam)
		{
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			ShowTrayMenu(wnd);

			break;
		}

		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case WM_APP_RELOAD:
			CacheVcpValues();

			break;
		case WM_APP_EXIT:
			Shell_NotifyIcon(NIM_DELETE, &TrayIcon);
			PostQuitMessage(0);

			break;
		}

		break;
	case WM_DESTROY:
		Shell_NotifyIcon(NIM_DELETE, &TrayIcon);
		PostQuitMessage(0);

		break;
	}

	return DefWindowProc(wnd, msg, wParam, lParam);
}

HWND CreateMainWindow(HINSTANCE instance)
{
	WNDCLASS wc = { };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = instance;
	wc.lpszClassName = L"Main";
	RegisterClass(&wc);

	HWND wnd = CreateWindow(L"Main", L"OOR Buster", WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, instance, NULL);

	return wnd;
}

void CreateTrayIcon(HWND wnd, HICON icon)
{
	ZeroMemory(&TrayIcon, sizeof(NOTIFYICONDATA));
	TrayIcon.cbSize = sizeof(NOTIFYICONDATA);
	TrayIcon.uID = 1;
	TrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	TrayIcon.hIcon = icon;
	TrayIcon.hWnd = wnd;
	TrayIcon.uCallbackMessage = WM_APP;
	wcscpy_s(TrayIcon.szTip, L"OOR Buster");
	Shell_NotifyIcon(NIM_ADD, &TrayIcon);
}

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int showFlag)
{
	CacheVcpValues();
	ReadLaunchParams();

	HWND wnd = CreateMainWindow(instance);
	CreateTrayIcon(wnd, LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON1)));
	RegisterPowerSettingNotification(wnd, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}