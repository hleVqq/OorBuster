#include <Windows.h>
#include <tchar.h>
#include <shellapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <string>
#include <Tlhelp32.h>

#include "resource.h"

const BYTE VCP_OSD_LANGUAGE = 0xCC;
const BYTE VCP_PICTURE_MODE = 0xDC;
const BYTE VCP_AMA = 0xF0;

enum
{
	WM_APP_RELOAD = WM_APP + 1,
	WM_APP_EXIT,
	WM_APP_NEXT_MODE,
	WM_APP_PREV_MODE
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

void CachePhysicalMonitor()
{
	Monitor = GetPhysicalMonitor();
}

void CacheVcpValues()
{
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

void NextMode()
{
	CachePhysicalMonitor();
	CacheVcpValues();
	CachedPictureMode++;
	FixPic();
}

void PrevMode()
{
	CachePhysicalMonitor();
	CacheVcpValues();
	CachedPictureMode--;
	FixPic();
}

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
	InsertMenu(menu, -1, MF_BYPOSITION, WM_APP_NEXT_MODE, L"Next Mode");
	InsertMenu(menu, -1, MF_BYPOSITION, WM_APP_PREV_MODE, L"Prev Mode");
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
		CachePhysicalMonitor();
		ApplyVcpValues();

		break;
	case WM_POWERBROADCAST:
		if (wParam == PBT_POWERSETTINGCHANGE)
		{
			const DWORD DISPLAY_ON = 0x1;

			POWERBROADCAST_SETTING* pbs = (POWERBROADCAST_SETTING *)lParam;
			DWORD status = *(DWORD*)(pbs->Data);

			if (status == DISPLAY_ON)
			{
				CachePhysicalMonitor();
				ApplyVcpValues(true);
			}
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
			CachePhysicalMonitor();
			FixOor();
			CacheVcpValues();

			break;
		case WM_APP_NEXT_MODE:
			NextMode();
			break;
		case WM_APP_PREV_MODE:
			PrevMode();
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
	case WM_HOTKEY:
		switch (wParam)
		{
		case 1000:
			{
				OutputDebugString(L"[DEBUG] HOTKEY!\n");
				CachePhysicalMonitor();

				DWORD origOorDelay = OorDelay;
				OorDelay = 0;
				ApplyVcpValues();
				OorDelay = origOorDelay;
			}

			break;
		}
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

void KillOtherInstances()
{
	DWORD id = GetCurrentProcessId();

	WCHAR path[MAX_PATH]; 
	GetModuleFileName(NULL, path, MAX_PATH);

	WCHAR file[_MAX_FNAME]; 
	_wsplitpath_s(path, NULL, 0, NULL, 0, file, ARRAYSIZE(file), NULL, 0); 
	wcscat_s(file, ARRAYSIZE(file), L".exe");

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);

	PROCESSENTRY32 entry; 
	entry.dwSize = sizeof(entry);

	for (BOOL res = Process32First(snapshot, &entry); res; res = Process32Next(snapshot, &entry))
	{
		if (wcscmp(entry.szExeFile, file) == 0)
		{
			HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);

			if (process != nullptr && entry.th32ProcessID != id)
			{
				TerminateProcess(process, 9);
				CloseHandle(process);
			}
		}
	}

	CloseHandle(snapshot);
}

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	KillOtherInstances();
	CachePhysicalMonitor();
	CacheVcpValues();
	ReadLaunchParams();

	HWND wnd = CreateMainWindow(instance);
	CreateTrayIcon(wnd, LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON1)));
	RegisterPowerSettingNotification(wnd, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);
	RegisterHotKey(wnd, 1000, MOD_ALT, VK_HOME);

	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}