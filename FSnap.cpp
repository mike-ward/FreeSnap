// Copyright (c) 2008 Blue Onion Software
// All rights reserved


#include "stdafx.h"
#include <mmsystem.h>
#include <algorithm>
#include "FSnap.h"

// ----------------------------------------------------------------------------

HHOOK g_keyboard_hook = 0;
HINSTANCE g_module_handle;
bool g_use_alternate_keys = false;   // allows use of alternate keys suitable for laptops
bool g_use_undo_behavior = false;    // old/new behavior control
bool g_task_switch;
UINT g_help_free_snap = RegisterWindowMessage("HelpFreeSnap");
std::vector<SIZE> g_sizes;

// ----------------------------------------------------------------------------

BOOL APIENTRY DllMain(HINSTANCE module, DWORD reason_for_call, LPVOID reserved)
{
    switch (reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_module_handle = module;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    
    return TRUE;
}

// ----------------------------------------------------------------------------

bool StringCompare(const char* left, const char* right, int length)
{
    int result = CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, left, -1, right, length);
    return result == CSTR_EQUAL;
}

// ----------------------------------------------------------------------------

FSNAP_API bool snap_install(std::vector<SIZE> sizes, bool altKeys, bool undo, bool task_switch)
{
    if (sizes.size() == 0)
    {
        static SIZE default_sizes[] =
        {
            {640,   480},
            {800,   600},
            {1024,  768},
            {1152,  864},
            {1280, 1024}
        };

        for (int i = 0 ; i < sizeof(default_sizes)/sizeof(default_sizes[0]) ; ++i)
            g_sizes.push_back(default_sizes[i]);
    }

    else
    {
        g_sizes = sizes;
    }

    g_use_alternate_keys = altKeys;
    g_use_undo_behavior = undo;
    g_task_switch = task_switch;
    g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, FSnapKeyboardProc, g_module_handle, 0);
    return (g_keyboard_hook != NULL);
}

// ----------------------------------------------------------------------------

FSNAP_API bool snap_uninstall()
{
    return UnhookWindowsHookEx(g_keyboard_hook) ? true : false;  
}

// ----------------------------------------------------------------------------

struct MonitorInfo
{
	HMONITOR hMonitor;
	MONITORINFO mi;
};

struct MonitorInfo g_monitors[6];
int g_monitorCount = 0;

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor,
							  HDC hdcMonitor,
							  LPRECT lprcMonitor,
							  LPARAM dwData)
{
	g_monitors[g_monitorCount].hMonitor = hMonitor;
	g_monitors[g_monitorCount].mi.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(hMonitor, &g_monitors[g_monitorCount].mi);
	g_monitorCount += 1;

	return TRUE;
}

// ----------------------------------------------------------------------------
// http://stackoverflow.com/questions/210504/enumerate-windows-like-alt-tab-does

std::vector<HWND> g_windows;
HWND g_last_window = (HWND)INVALID_HANDLE_VALUE;

HWND GetLastVisibleActivePopupOfWindow(HWND hwnd)
{
    HWND last = GetLastActivePopup(hwnd);

    if (IsWindowVisible(last))
        return last;

    if (last == hwnd)
        return (HWND)INVALID_HANDLE_VALUE;

    return GetLastVisibleActivePopupOfWindow(last);
}

bool IsAltTabWindow(HWND hwnd)
{
    if (hwnd == GetShellWindow())
        return false;

    if (IsWindowVisible(hwnd) == FALSE)
        return false;

    HWND root = GetAncestor(hwnd, GA_ROOTOWNER);
    
    if (GetLastVisibleActivePopupOfWindow(root) == hwnd)
    {
        TCHAR className[200];

        if (GetClassName(hwnd, className, 200) > 0)
        {
            if (StringCompare(className, "Shell_TrayWnd", -1) ||
                StringCompare(className, "DV2ControlHost", -1) ||
                StringCompare(className, "SideBar_AppBarBullet", -1) ||
                StringCompare(className, "SideBar_AppBarWindow", -1) ||
                StringCompare(className, "SynTrackCursorWindowClass", -1) ||
                StringCompare(className, "MsgrIMEWindowClass", -1) ||
                StringCompare(className, "SysShadow", -1) ||
                StringCompare(className, "WMP9MediaBarFlyout", 18))
            {
                return false;
            }

            if (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
                return false;
        }

        return true;
    }

    return false;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lpar)
{
    if (IsAltTabWindow(hwnd))
    {
        if (std::find(g_windows.begin(), g_windows.end(), hwnd) == g_windows.end())
            g_windows.push_back(hwnd);
    }

    return TRUE;
}

// ----------------------------------------------------------------------------

FSNAP_API LRESULT CALLBACK FSnapKeyboardProc(int code, WPARAM wpar, LPARAM lpar)
{
    if (code == HC_ACTION)
    {
        if (wpar == WM_KEYDOWN)
        {
            const KBDLLHOOKSTRUCT& kb = *(PKBDLLHOOKSTRUCT)lpar;
            
            if (GetKeyState(VK_LWIN) & 0x80)
            {
                static enum boundary
                {
                    left   = 0x1,
                    right  = 0x2,
                    top    = 0x4,
                    bottom = 0x8
                };
                
                static enum command
                {
                    null,
                    size,
                    move,
                    center,
                    maximize,
                    shrink,
                    grow,
                    minimize,
					next,
                    bump,
                    help,
                    close,
                    cycle_forward,
                    cycle_back,
                    half,
                    center_no_override
                };

                unsigned edge = null;
                unsigned action = null;
                bool ctrl_key = (GetKeyState(VK_CONTROL) & 0x80) != 0;
                bool shift_key = (GetKeyState(VK_SHIFT) & 0x80) != 0;

                switch (kb.vkCode)
                {
                case VK_LEFT:   
                case VK_NUMPAD4:
                case 'J':
                    edge = left;
                    action = (ctrl_key || shift_key) ? bump : size;
                    break;
                    
                case VK_RIGHT:  
                case VK_NUMPAD6:
                case 'L':
                    edge = right;
                    action = (ctrl_key || shift_key) ? bump : size;
                    break;
                    
                case VK_UP:
                case VK_NUMPAD8:
                case 'I':
                    edge = top;
                    action = (ctrl_key || shift_key) ? bump : size;
                    break;
                    
                case VK_DOWN:
                case VK_NUMPAD2:
                case 'K':
                    edge = bottom;
                    action = (ctrl_key || shift_key) ? bump : size;
                    break;
                    
                case VK_HOME:
                case VK_NUMPAD7:
                case 'T':
                    edge = top + left;
                    action = move;
                    break;
                    
                case VK_END:
                case VK_NUMPAD1:
                case 'G':
                    edge = bottom + left;
                    action = move;
                    break;
                    
                case VK_PRIOR:
                case VK_NUMPAD9:
                case 'Y':
                    edge = top + right;
                    action = move;
                    break;
                    
                case VK_NEXT:
                case VK_NUMPAD3:
                case 'H':
                    edge = bottom + right;
                    action = move;
                    break;
                    
                case VK_CLEAR:
                case VK_NUMPAD5:
                case 'C':
                    action = center;
                    break;
                
                case VK_MULTIPLY:
                case '*':
                    action = center_no_override;
                    break;

                case VK_RETURN:
                    action = maximize;
                    break;

                case VK_ADD:
                case 'Z':
                    action = grow;
                    break;

                case VK_SUBTRACT:
                case 'X':
                    action = shrink;
                    break;

                case VK_INSERT:
                case VK_NUMPAD0:
                case VK_OEM_PERIOD:
                    action = minimize;
                    break;

                case VK_OEM_2: // /?
                    action = shift_key ? help : close;
                    break;

                case VK_TAB:
                    if (g_task_switch) 
                        action = (shift_key) ? cycle_back : cycle_forward;
                    break;

                case VK_OEM_3: // `~
                    if (g_task_switch) 
                        action = cycle_back;
                    break;

                default:
                    action = null;
                    break;
                }
                
                // 10/31/2004 - Allow use_alternate_keys to override keybindings for laptops
                //
                if (g_use_alternate_keys == false && kb.vkCode >= 'C' && kb.vkCode <= 'Z')
                    action = null;

				if (GetSystemMetrics(SM_CMONITORS) > 1 && action == center)
					action = next;

                HWND window = GetForegroundWindow();
                RECT rect;

                static struct
                {
                    HWND  window;
                    DWORD vkCode;
                    RECT  rect;
                } undo;

                if (g_use_undo_behavior) // old behavior undoes last size
                {
                    if (window == undo.window && kb.vkCode == undo.vkCode
                          && action != shrink && action != grow
                          && action != maximize && action != minimize
                          && action != bump)
                    {
                        SetWindowPos(window, HWND_TOP, 
                            undo.rect.left, 
                            undo.rect.top, 
                            undo.rect.right - undo.rect.left,
                            undo.rect.bottom - undo.rect.top,
                            SWP_SHOWWINDOW);

                        undo.window = NULL;
                        return 1;
                    }
                }

                else // new bahavior pulls opposite edge to mid point of the screen
                {
                    if (window == undo.window && kb.vkCode == undo.vkCode
                          && action != shrink && action != grow
                          && action != maximize && action != minimize
                          && action != bump
                          && (edge == top || edge == left || edge == bottom || edge == right))
                    {
                        action = half;

                        switch (edge)
                        {
                        case left: edge = right; break;
                        case right: edge = left; break;
                        case top: edge = bottom; break;
                        case bottom: edge = top; break;
                        }
                    }
                }

                GetWindowRect(window, &rect);
				HMONITOR hMonitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);

				MONITORINFO mi;
				mi.cbSize = sizeof(mi);
				GetMonitorInfo(hMonitor, &mi);
				RECT desktop = mi.rcWork;

                undo.window = window;
                undo.vkCode = kb.vkCode;
                undo.rect = rect;
                                    
                if (action == maximize)
                {
                    int state = SW_RESTORE;
                    
                    if (!IsZoomed(window) && !IsIconic(window))
                        state = SW_MAXIMIZE;
                    
                    ShowWindow(window, state);
                    return 1;
                }
                
                if (action == minimize)
                {
                    int state = SW_RESTORE;

                    if (!IsIconic(window))
                        state = SW_MINIMIZE;
                    
                    ShowWindow(window, state);
                    return 1;
                }
                
                if (action == move || action == size)
                {
                    if (edge & left)
                        rect.left = desktop.left;
                    
                    if (edge & right)
                    {
                        if (action == move)
                            rect.left += desktop.right - rect.right;
                        
                        else
                            rect.right = desktop.right;
                    }
                    
                    if (edge & top)
                        rect.top = desktop.top;
                    
                    if (edge & bottom)
                    {
                        if (action == move)
                            rect.top += desktop.bottom - rect.bottom;
                        
                        else
                            rect.bottom = desktop.bottom;
                    }
                    
                    SetWindowPos(window, HWND_TOP, rect.left, rect.top,
                        rect.right-rect.left, rect.bottom-rect.top,
                        (action == move) ? SWP_NOSIZE : SWP_SHOWWINDOW);
                    
                    return 1;
                }

                if (action == center || action == center_no_override)
                {
                    int wx = rect.right - rect.left;
                    int dx = desktop.right - desktop.left;
                    int wy = rect.bottom - rect.top;
                    int dy = desktop.bottom - desktop.top;

                    SetWindowPos(window, HWND_TOP, desktop.left + (dx - wx)/2, desktop.top + (dy - wy)/2,
                        0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
                    
                    return 1;
                }

                if (action == shrink || action == grow)
                {
                    static int current = 0;

                    if (action == shrink)
                    {
                        if (current > 0)
                            current -= 1;
                    }

                    else
                    {
                        if (current < ((int)g_sizes.size() - 1))
                            current += 1;
                    }

                    SetWindowPos(window, HWND_TOP, rect.left, rect.top,
                        g_sizes[current].cx, g_sizes[current].cy,
                        SWP_SHOWWINDOW);

                    return 1;
                }

				if (action == next)
				{
					g_monitorCount = 0;
					EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
					int currentMonitor;

					for (currentMonitor = 0; currentMonitor < g_monitorCount; ++currentMonitor)
					{
						if (hMonitor == g_monitors[currentMonitor].hMonitor)
							break;
					}

					if (currentMonitor < g_monitorCount)
					{
                        bool zoomed = false;
						int nextMonitor = (currentMonitor + 1) % g_monitorCount;

                        if (IsZoomed(window))
                        {
                            ShowWindow(window, SW_RESTORE);
                            zoomed = true;
                        }

						SetWindowPos(window, HWND_TOP, 
							g_monitors[nextMonitor].mi.rcWork.left + rect.left - g_monitors[currentMonitor].mi.rcWork.left, 
							g_monitors[nextMonitor].mi.rcWork.top + rect.top - g_monitors[currentMonitor].mi.rcWork.top, 
							0, 0, SWP_NOSIZE);

                        if (zoomed)
                            ShowWindow(window, SW_MAXIMIZE);
					}

					return 1;
				}

                if (action == bump)
                {
                    int bump = (shift_key) ? 10 : -10;

                    if (edge & left)
                        rect.left -= bump;

                    else if (edge & right)
                        rect.right += bump;

                    else if (edge & top)
                        rect.top -= bump;

                    else if (edge & bottom)
                        rect.bottom += bump;

                    SetWindowPos(window, HWND_TOP, rect.left, rect.top,
                        rect.right-rect.left, rect.bottom-rect.top, SWP_SHOWWINDOW);

                    return 1;
                }

                if (action == close)
                {
                    PostMessage(window, WM_CLOSE, 0, 0);
                    return 1;
                }

                if (action == help)
                {
			        PostMessage(HWND_BROADCAST, g_help_free_snap, 0, 0);
                    return 1;
                }

                if (action == cycle_forward || action == cycle_back)
                {
                    g_windows.clear();
                    
                    if (EnumDesktopWindows(NULL, EnumWindowsProc, 0) == TRUE)
                    {
                        if (g_windows.size() > 0)
                        {
                            std::sort(g_windows.begin(), g_windows.end());

                            if (std::find(g_windows.begin(), g_windows.end(), g_last_window) == g_windows.end())
                                g_last_window = g_windows[0];

                            for (int i = 0 ; i < (int)g_windows.size() ; ++i)
                            {
                                if (g_last_window == g_windows[i])
                                {
                                    int increment = (action == cycle_forward) ? 1 : -1;
                                    g_last_window = g_windows[(i + increment) % g_windows.size()];
                                    HWND hwnd_foreground = GetForegroundWindow();

                                    if (g_last_window == hwnd_foreground)
                                        g_last_window = g_windows[(i + 2) % g_windows.size()];

                                    BringWindowToTop(g_last_window);
                                    
                                    if (SetForegroundWindow(g_last_window) == FALSE)
                                    {
	                                    if (!hwnd_foreground)
		                                    hwnd_foreground = FindWindow("Shell_TrayWnd", NULL);

	                                    DWORD idFrgnd = GetWindowThreadProcessId(hwnd_foreground, NULL);
	                                    DWORD idSwitch = GetWindowThreadProcessId(g_last_window, NULL);
	                                    AttachThreadInput(idFrgnd, idSwitch, TRUE);

	                                    INPUT inp[4];
	                                    ZeroMemory(&inp, sizeof(inp));
	                                    inp[0].type = inp[1].type = inp[2].type = inp[3].type = INPUT_KEYBOARD;
	                                    inp[0].ki.wVk = inp[1].ki.wVk = inp[2].ki.wVk = inp[3].ki.wVk = VK_MENU;
	                                    inp[0].ki.dwFlags = inp[2].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
	                                    inp[1].ki.dwFlags = inp[3].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;
	                                    SendInput(4, inp, sizeof(INPUT));

	                                    AttachThreadInput(idFrgnd, idSwitch, FALSE);
                                    }

                                    if (IsIconic(g_last_window))
                                        PostMessage(g_last_window, WM_SYSCOMMAND, SC_RESTORE, 0);

                                    break;
                                }
                            }
                        }
                    }

                    return 1;
                }

                if (action == half)
                {
                    int halfWidth = (desktop.right - desktop.left) / 2;
                    int halfHeight = (desktop.bottom - desktop.top) / 2;

                    if (edge & left)
                        rect.left = rect.right - halfWidth;

                    else if (edge & right)
                        rect.right = rect.left + halfWidth;

                    else if (edge & top)
                        rect.top = halfHeight;

                    else if (edge & bottom)
                        rect.bottom = halfHeight;

                    SetWindowPos(window, HWND_TOP, rect.left, rect.top,
                        rect.right-rect.left, rect.bottom-rect.top, SWP_SHOWWINDOW);

                    return 1;
                }
            }
        }
    }
    
    return CallNextHookEx(g_keyboard_hook, code, wpar, lpar);
}
