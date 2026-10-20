#include "launcher_platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellscalingapi.h>
#include <xinput.h>

#include <algorithm>
#include <string>

#include <imgui.h>

#include "trg/platform.h"

#include "launcher_placement.h"

// The NVIDIA Optimus and AMD switchable graphics drivers run programs exporting these on the discrete GPU.
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace gw2 {
namespace {

BOOL CALLBACK AddMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM user) {
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  if (!GetMonitorInfoW(monitor, &info)) {
    return TRUE;
  }
  MonitorInfo found;
  found.width = info.rcMonitor.right - info.rcMonitor.left;
  found.height = info.rcMonitor.bottom - info.rcMonitor.top;
  found.work_width = info.rcWork.right - info.rcWork.left;
  found.work_height = info.rcWork.bottom - info.rcWork.top;
  found.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
  UINT dpi_x = 0;
  UINT dpi_y = 0;
  if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)) && dpi_x) {
    found.scale = float(dpi_x) / 96.0f;
  }
  reinterpret_cast<std::vector<MonitorInfo>*>(user)->push_back(found);
  return TRUE;
}

BOOL CALLBACK AddWorkArea(HMONITOR monitor, HDC, LPRECT, LPARAM user) {
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  if (GetMonitorInfoW(monitor, &info)) {
    reinterpret_cast<std::vector<Rect>*>(user)->push_back(
        {info.rcWork.left, info.rcWork.top, info.rcWork.right, info.rcWork.bottom});
  }
  return TRUE;
}

using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

XInputGetStateFn LoadXInput() {
  for (const wchar_t* name : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
    if (HMODULE dll = LoadLibraryW(name)) {
      if (FARPROC proc = GetProcAddress(dll, "XInputGetState")) {
        return reinterpret_cast<XInputGetStateFn>(reinterpret_cast<void*>(proc));
      }
    }
  }
  return nullptr;
}

DWORD GetPadState(DWORD pad, XINPUT_STATE* state) {
  static const XInputGetStateFn get_state = LoadXInput();
  return get_state ? get_state(pad, state) : ERROR_DEVICE_NOT_CONNECTED;
}

struct ButtonKey {
  ImGuiKey key;
  WORD mask;
};
constexpr ButtonKey kButtonKeys[] = {
    {ImGuiKey_GamepadStart, XINPUT_GAMEPAD_START},
    {ImGuiKey_GamepadBack, XINPUT_GAMEPAD_BACK},
    {ImGuiKey_GamepadFaceDown, XINPUT_GAMEPAD_A},
    {ImGuiKey_GamepadFaceRight, XINPUT_GAMEPAD_B},
    {ImGuiKey_GamepadFaceLeft, XINPUT_GAMEPAD_X},
    {ImGuiKey_GamepadFaceUp, XINPUT_GAMEPAD_Y},
    {ImGuiKey_GamepadDpadLeft, XINPUT_GAMEPAD_DPAD_LEFT},
    {ImGuiKey_GamepadDpadRight, XINPUT_GAMEPAD_DPAD_RIGHT},
    {ImGuiKey_GamepadDpadUp, XINPUT_GAMEPAD_DPAD_UP},
    {ImGuiKey_GamepadDpadDown, XINPUT_GAMEPAD_DPAD_DOWN},
    {ImGuiKey_GamepadL1, XINPUT_GAMEPAD_LEFT_SHOULDER},
    {ImGuiKey_GamepadR1, XINPUT_GAMEPAD_RIGHT_SHOULDER},
    {ImGuiKey_GamepadL3, XINPUT_GAMEPAD_LEFT_THUMB},
    {ImGuiKey_GamepadR3, XINPUT_GAMEPAD_RIGHT_THUMB},
};

struct StickKey {
  ImGuiKey key;
  bool horizontal;
  int dead_zone;
  int full;
};
constexpr StickKey kStickKeys[] = {
    {ImGuiKey_GamepadLStickLeft, true, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768},
    {ImGuiKey_GamepadLStickRight, true, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, 32767},
    {ImGuiKey_GamepadLStickUp, false, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, 32767},
    {ImGuiKey_GamepadLStickDown, false, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768},
};

}  // namespace

std::vector<Rect> WorkAreas() {
  std::vector<Rect> areas;
  EnumDisplayMonitors(nullptr, nullptr, AddWorkArea, reinterpret_cast<LPARAM>(&areas));
  return areas;
}

std::optional<WindowState> QueryWindowState(void* native_handle) {
  const HWND window = static_cast<HWND>(native_handle);
  RECT rect;
  if (!window || !IsWindow(window) || !GetWindowRect(window, &rect)) {
    return std::nullopt;
  }
  WindowState state;
  state.rect = {rect.left, rect.top, rect.right, rect.bottom};
  state.minimized = IsIconic(window) != FALSE;
  state.maximized = IsZoomed(window) != FALSE;
  if (state.minimized) {
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    state.maximized = GetWindowPlacement(window, &placement) && (placement.flags & WPF_RESTORETOMAXIMIZED);
  }
  return state;
}

void ApplyWindowPlacement(void* native_handle, const WindowPlacement& placement) {
  const HWND window = static_cast<HWND>(native_handle);
  if (!window || !IsWindow(window)) {
    return;
  }
  ShowWindow(window, SW_RESTORE);
  SetWindowPos(window, nullptr, placement.normal.left, placement.normal.top, placement.normal.width(),
               placement.normal.height(), SWP_NOZORDER | SWP_NOACTIVATE);
  if (placement.maximized) {
    ShowWindow(window, SW_MAXIMIZE);
  }
}

void ApplyWindowIcon() {}

void SetApplicationId() {}

void RunLauncherWithoutGraphics(const GraphicsFallbackInfo&) {}

namespace {

PreShowPlan g_plan;
WindowPlacement g_saved;
HHOOK g_hook = nullptr;
std::optional<PreShowResult> g_result;

LRESULT CALLBACK PreShowProc(int code, WPARAM wparam, LPARAM lparam) {
  const auto* message = reinterpret_cast<const CWPSTRUCT*>(lparam);
  if (code == HC_ACTION && message->message == WM_SHOWWINDOW && message->wParam && message->lParam == 0 &&
      !GetParent(message->hwnd) && !g_result) {
    HHOOK hook = g_hook;
    g_hook = nullptr;
    const HWND window = message->hwnd;
    RECT before;
    GetWindowRect(window, &before);
    g_result = PreShowResult{{{before.left, before.top, before.right, before.bottom}, false}, g_saved};
    if (g_plan.normal) {
      SetWindowPos(window, nullptr, g_plan.normal->left, g_plan.normal->top, g_plan.normal->width(),
                   g_plan.normal->height(), SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (g_plan.maximized) {
      ShowWindow(window, SW_MAXIMIZE);
    }
    UnhookWindowsHookEx(hook);
    return 0;
  }
  return CallNextHookEx(g_hook, code, wparam, lparam);
}

}  // namespace

void InstallPreShowPlacement(const PreShowPlan& plan, const WindowPlacement& saved) {
  if (g_hook || !plan.Needed()) {
    return;
  }
  g_plan = plan;
  g_saved = saved;
  g_result.reset();
  g_hook = SetWindowsHookExW(WH_CALLWNDPROC, PreShowProc, nullptr, GetCurrentThreadId());
}

std::optional<PreShowResult> TakePreShowResult() {
  if (g_hook) {
    UnhookWindowsHookEx(g_hook);
    g_hook = nullptr;
  }
  std::optional<PreShowResult> result = g_result;
  g_result.reset();
  return result;
}

std::vector<MonitorInfo> ListMonitors() {
  std::vector<MonitorInfo> monitors;
  EnumDisplayMonitors(nullptr, nullptr, AddMonitor, reinterpret_cast<LPARAM>(&monitors));
  std::stable_partition(monitors.begin(), monitors.end(), [](const MonitorInfo& m) { return m.primary; });
  return monitors;
}

bool ShiftHeld() {
  return trg::ShiftHeld();
}

bool PackagePickerAvailable() {
  return true;
}

bool RelaunchSelf(std::wstring_view extra_args) {
  std::wstring command_line = GetCommandLineW();
  command_line += L' ';
  command_line += extra_args;
  wchar_t exe[MAX_PATH];
  if (!GetModuleFileNameW(nullptr, exe, MAX_PATH)) {
    return false;
  }
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(exe, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
    return false;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}

UiFontFiles FindUiFonts() {
  wchar_t windows[MAX_PATH];
  const UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
  const std::filesystem::path fonts =
      std::filesystem::path(length ? std::wstring(windows, length) : std::wstring(L"C:\\Windows")) / L"Fonts";
  return {fonts / "segoeui.ttf", fonts / "seguisb.ttf", fonts / "segoeuib.ttf"};
}

void LauncherGamepad::Update(ImGuiIO& io) {
  XINPUT_STATE state{};
  if (pad_ < 0 || GetPadState(DWORD(pad_), &state) != ERROR_SUCCESS) {
    pad_ = -1;
    held_ = 0;
    // Polling an empty slot is slow, so look for a controller once a second.
    if (ImGui::GetTime() < next_scan_) {
      return;
    }
    next_scan_ = ImGui::GetTime() + 1.0;
    for (DWORD i = 0; i < XUSER_MAX_COUNT && pad_ < 0; ++i) {
      if (GetPadState(i, &state) == ERROR_SUCCESS) {
        pad_ = int(i);
      }
    }
    if (pad_ < 0) {
      return;
    }
  }
  io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  const XINPUT_GAMEPAD& pad = state.Gamepad;
  held_ = 0;
  for (size_t i = 0; i < std::size(kButtonKeys); ++i) {
    const bool down = (pad.wButtons & kButtonKeys[i].mask) != 0;
    io.AddKeyEvent(kButtonKeys[i].key, down);
    held_ |= uint32_t(down) << i;
  }
  for (const StickKey& s : kStickKeys) {
    const int value = s.horizontal ? pad.sThumbLX : pad.sThumbLY;
    const float amount = std::clamp(float(value - s.dead_zone) / float(s.full - s.dead_zone), 0.0f, 1.0f);
    io.AddKeyAnalogEvent(s.key, amount > 0.1f, amount);
  }
}

bool LauncherGamepad::Held(ImGuiKey key) const {
  for (size_t i = 0; i < std::size(kButtonKeys); ++i) {
    if (kButtonKeys[i].key == key) {
      return (held_ >> i) & 1;
    }
  }
  return false;
}

void LauncherGamepad::Release(ImGuiIO& io) {
  held_ = 0;
  for (const ButtonKey& b : kButtonKeys) {
    io.AddKeyEvent(b.key, false);
  }
  for (const StickKey& s : kStickKeys) {
    io.AddKeyAnalogEvent(s.key, false, 0.0f);
  }
  pad_ = -1;
}

}  // namespace gw2
