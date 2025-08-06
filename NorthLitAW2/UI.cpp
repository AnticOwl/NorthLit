import UI;
import Log;
import Northlight;
import HookUtil;

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static HWND		g_hwnd = NULL;
static LONG_PTR	g_origWndProc = 0;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef LRESULT(WINAPI* tWndProc)(HWND, UINT, WPARAM, LPARAM);
tWndProc oWndProc;

static bool s_HotkeyCapture = false;
static int s_CapturedHotkey = 0;

LRESULT WINAPI hWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (s_HotkeyCapture && msg == WM_KEYDOWN)
	{
		if (wParam == VK_ESCAPE)
		{
			s_HotkeyCapture = false;
		}
		else
		{
			if (wParam == VK_LCONTROL || wParam == VK_RCONTROL || wParam == VK_CONTROL ||
				wParam == VK_LSHIFT || wParam == VK_RSHIFT || wParam == VK_SHIFT ||
				wParam == VK_MENU)
			{
				const int modifier = static_cast<int>(wParam) << 8;
				s_CapturedHotkey = (s_CapturedHotkey & 0xFFFFFF00) == modifier ? 0 : modifier;
			}
			else
			{
				s_CapturedHotkey |= wParam;
				s_HotkeyCapture = false;
			}
		}

		return TRUE;
	}

	if (UI::GetInstance().IsVisible() && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return TRUE;

	return oWndProc(hWnd, msg, wParam, lParam);
}


UI& UI::GetInstance()
{
	static UI ui = UI();
	return ui;
}

UI::UI() :
	m_Visible(false),
	m_HasKeyboardFocus(false)
{

}

UI::~UI()
{

}

bool UI::Init()
{
	g_hwnd = FindWindowA(0, "Alan Wake 2");
	if (g_hwnd == NULL)
	{
		g_hwnd = FindWindowA("AppWindowClass", 0);
		if (g_hwnd == NULL)
		{
			Log::Error("Could not find handle for game window");
			return false;
		}
	}

	// Subclass the window with a new WndProc to catch messages
	//g_origWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)&WndProc);

	//Log::Write("Original WNDPROC %I64X", g_origWndProc);
	//Log::Write("New WNDPROC %I64X", (LONG_PTR)&WndProc);
	//Log::Write("&g_origWndProc %I64X", &g_origWndProc);

	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
	io.MouseDrawCursor = true;
	io.FontAllowUserScaling = true;

	if (!ImGui_ImplWin32_Init(g_hwnd))
	{
		Log::Error("Failed to initialize ImGui Win32 backend");
		return false;
	}

	// SetWindowLongPtr is causing a crash for some reason when shutting down and trying to reset GWLP_WNDPROC
	// So just use a normal hook
	g_origWndProc = GetWindowLongPtr(g_hwnd, GWLP_WNDPROC);
	CreateHook((void*)g_origWndProc, hWndProc, &oWndProc);

	Log::Success("ImGui initialized");
	return true;
}

void UI::Draw()
{
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (m_Visible)
	{
		for (auto& cb : m_DrawCbs)
		{
			cb();
		}
	}

	ImGui::Render();
}

void UI::SetVisible(bool visible)
{
	m_Visible = visible;

	auto pInputSystem = Northlight::InputSystem::GetInstance();
	pInputSystem->m_KeyboardEnabled		= !m_Visible;
	pInputSystem->m_MouseEnabled		= !m_Visible;
	pInputSystem->m_ControllerEnabled1	= !m_Visible;
	pInputSystem->m_ControllerEnabled2	= !m_Visible;

	auto pGameWindow = Northlight::coregame::GameWindow::GetInstance();
	pGameWindow->m_ForceShowMouse = m_Visible;
}


void UI::StartHotkeyCapture(int oldKey)
{
	s_HotkeyCapture = true;
	s_CapturedHotkey = oldKey;
}

bool UI::GetCapturedHotkey(int& outHotkey)
{
	if (!s_HotkeyCapture)
	{
		outHotkey = s_CapturedHotkey;
		return true;
	}

	return false;
}

void UI::Shutdown()
{
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	RemoveHook((void*)g_origWndProc);
}