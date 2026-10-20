#include "launcher_platform.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include <spawn.h>
#include <unistd.h>

#include <SDL3/SDL.h>
#include <imgui.h>

#include <rex/filesystem.h>
#include <rex/logging.h>

#include "launcher_placement.h"

extern char** environ;

namespace gw2 {
namespace {

constexpr char kApplicationId[] = "io.github.saulob.geometry-wars-retro-evolved-2";

struct ButtonKey {
  ImGuiKey key;
  SDL_GamepadButton button;
};
constexpr ButtonKey kButtonKeys[] = {
    {ImGuiKey_GamepadStart, SDL_GAMEPAD_BUTTON_START},
    {ImGuiKey_GamepadBack, SDL_GAMEPAD_BUTTON_BACK},
    {ImGuiKey_GamepadFaceDown, SDL_GAMEPAD_BUTTON_SOUTH},
    {ImGuiKey_GamepadFaceRight, SDL_GAMEPAD_BUTTON_EAST},
    {ImGuiKey_GamepadFaceLeft, SDL_GAMEPAD_BUTTON_WEST},
    {ImGuiKey_GamepadFaceUp, SDL_GAMEPAD_BUTTON_NORTH},
    {ImGuiKey_GamepadDpadLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {ImGuiKey_GamepadDpadRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {ImGuiKey_GamepadDpadUp, SDL_GAMEPAD_BUTTON_DPAD_UP},
    {ImGuiKey_GamepadDpadDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {ImGuiKey_GamepadL1, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {ImGuiKey_GamepadR1, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {ImGuiKey_GamepadL3, SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {ImGuiKey_GamepadR3, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
};

// The left stick dead zone XInput uses. SDL's Y axis points down.
constexpr int kStickDeadZone = 7849;
struct StickKey {
  ImGuiKey key;
  SDL_GamepadAxis axis;
  int direction;
};
constexpr StickKey kStickKeys[] = {
    {ImGuiKey_GamepadLStickLeft, SDL_GAMEPAD_AXIS_LEFTX, -1},
    {ImGuiKey_GamepadLStickRight, SDL_GAMEPAD_AXIS_LEFTX, 1},
    {ImGuiKey_GamepadLStickUp, SDL_GAMEPAD_AXIS_LEFTY, -1},
    {ImGuiKey_GamepadLStickDown, SDL_GAMEPAD_AXIS_LEFTY, 1},
};

SDL_Gamepad* OpenFirstGamepad() {
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  SDL_Gamepad* pad = ids && count > 0 ? SDL_OpenGamepad(ids[0]) : nullptr;
  SDL_free(ids);
  return pad;
}

struct FontFamily {
  const char* regular;
  const char* semibold;
  const char* bold;
};
// The first family whose regular face exists wins.
constexpr FontFamily kFontFamilies[] = {
    {"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-SemiBold.ttf",
     "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf"},
    {"/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/noto/NotoSans-SemiBold.ttf",
     "/usr/share/fonts/noto/NotoSans-Bold.ttf"},
    {"/usr/share/fonts/google-noto/NotoSans-Regular.ttf", "/usr/share/fonts/google-noto/NotoSans-SemiBold.ttf",
     "/usr/share/fonts/google-noto/NotoSans-Bold.ttf"},
    {"/usr/share/fonts/truetype/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/NotoSans-SemiBold.ttf",
     "/usr/share/fonts/truetype/NotoSans-Bold.ttf"},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", nullptr, "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
    {"/usr/share/fonts/TTF/DejaVuSans.ttf", nullptr, "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"},
    {"/usr/share/fonts/dejavu/DejaVuSans.ttf", nullptr, "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"},
    {"/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf", nullptr,
     "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf"},
    {"/usr/share/fonts/truetype/DejaVuSans.ttf", nullptr, "/usr/share/fonts/truetype/DejaVuSans-Bold.ttf"},
    {"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", nullptr,
     "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"},
    {"/usr/share/fonts/liberation/LiberationSans-Regular.ttf", nullptr,
     "/usr/share/fonts/liberation/LiberationSans-Bold.ttf"},
};

// ReXGlue's native window handle is empty here, and the launcher is its only window.
SDL_Window* LauncherWindow() {
  int count = 0;
  SDL_Window** windows = SDL_GetWindows(&count);
  SDL_Window* window = windows && count > 0 ? windows[0] : nullptr;
  SDL_free(windows);
  return window;
}

bool OnPath(const char* program) {
  const char* path = std::getenv("PATH");
  if (!path) {
    return false;
  }
  const std::string_view list(path);
  for (size_t begin = 0; begin <= list.size();) {
    size_t end = list.find(':', begin);
    if (end == std::string_view::npos) {
      end = list.size();
    }
    const std::string candidate = std::string(list.substr(begin, end - begin)) + "/" + program;
    if (access(candidate.c_str(), X_OK) == 0) {
      return true;
    }
    begin = end + 1;
  }
  return false;
}

SDL_Surface* LoadApplicationIcon() {
  const std::filesystem::path icon_file = rex::filesystem::GetExecutableFolder() / ".." / "share" / "icons" /
                                          "hicolor" / "256x256" / "apps" / "geometry_wars_retro_evolved_2.png";
  return SDL_LoadPNG(Utf8(icon_file).c_str());
}

constexpr char kSoftwareWindowTitle[] = "Geometry Wars: Retro Evolved 2 Recompiled";

struct Color {
  int r, g, b;
};

constexpr int kWidth = 640;
constexpr int kHeight = 400;
constexpr int kMargin = 24;
constexpr int kHeaderHeight = 96;
constexpr int kIconSize = 72;
constexpr int kLargeScale = 2;
constexpr int kRowStep = 34;
constexpr int kLabelWidth = 120;
constexpr int kLineStep = 22;
constexpr Color kBackground{0x12, 0x14, 0x22};
constexpr Color kHeader{0x1c, 0x14, 0x38};
constexpr Color kAccent{0x94, 0x3c, 0xff};
constexpr Color kAccentHover{0xb0, 0x6e, 0xff};
constexpr Color kButton{0x2c, 0x2e, 0x44};
constexpr Color kButtonHover{0x40, 0x42, 0x5e};
constexpr Color kText{0xf0, 0xf0, 0xf0};
constexpr Color kDimText{0x9a, 0x9a, 0xb0};
constexpr Color kReadyText{0x5c, 0xc9, 0x6b};
constexpr Color kWarningText{0xf2, 0xb1, 0x3a};

constexpr SDL_Rect kPlayButton{kMargin, kHeight - 76, 200, 48};
constexpr SDL_Rect kQuitButton{kMargin + 216, kHeight - 76, 140, 48};

constexpr const char* kGraphicsFailureLines[] = {
    "A Vulkan-compatible graphics driver is required,",
    "such as Mesa (AMD and Intel) or the NVIDIA driver.",
    "Install or update it and start the game again.",
    "The logs folder next to the AppImage has the details.",
};

enum class Button { kNone, kPlay, kQuit };

// Draws with ImGui's built-in font from a private atlas, so it needs neither a GPU nor an ImGui context.
class SoftwareText {
 public:
  SoftwareText() {
    ImFont* font = atlas_.AddFontDefault();
    baked_ = font->GetFontBaked(font->LegacySize);
    // Loading a glyph rewrites the atlas pixels, so every glyph is loaded before they are read.
    for (ImWchar c = kFirstGlyph; c <= kLastGlyph; ++c) {
      baked_->FindGlyph(c);
    }
    atlas_.GetTexDataAsAlpha8(&pixels_, &width_, &height_);
  }

  int Width(std::string_view text, int scale) const {
    float width = 0.0f;
    for (const char c : text) {
      width += baked_->FindGlyph(Printable(c))->AdvanceX;
    }
    return int(width) * scale;
  }

  int Height(int scale) const { return int(baked_->Size) * scale; }

  void Draw(SDL_Surface* surface, std::string_view text, int x, int y, int scale, Color color) const {
    float pen_x = float(x);
    for (const char c : text) {
      const ImFontGlyph* glyph = baked_->FindGlyph(Printable(c));
      DrawGlyph(surface, *glyph, int(pen_x), y, scale, color);
      pen_x += glyph->AdvanceX * float(scale);
    }
  }

 private:
  static constexpr ImWchar kFirstGlyph = 0x20;
  static constexpr ImWchar kLastGlyph = 0x7e;

  static ImWchar Printable(char c) {
    const auto code = static_cast<unsigned char>(c);
    return code >= kFirstGlyph && code <= kLastGlyph ? ImWchar(code) : ImWchar('?');
  }

  void DrawGlyph(SDL_Surface* surface, const ImFontGlyph& glyph, int x, int y, int scale, Color color) const {
    if (!glyph.Visible) {
      return;
    }
    const int source_x = int(glyph.U0 * float(width_) + 0.5f);
    const int source_y = int(glyph.V0 * float(height_) + 0.5f);
    const int width = int(glyph.X1 - glyph.X0 + 0.5f);
    const int height = int(glyph.Y1 - glyph.Y0 + 0.5f);
    const int left = x + int(glyph.X0 * float(scale) + 0.5f);
    const int top = y + int(glyph.Y0 * float(scale) + 0.5f);
    for (int row = 0; row < height; ++row) {
      for (int column = 0; column < width; ++column) {
        const int alpha = pixels_[(source_y + row) * width_ + source_x + column];
        if (alpha == 0) {
          continue;
        }
        for (int dy = 0; dy < scale; ++dy) {
          for (int dx = 0; dx < scale; ++dx) {
            Blend(surface, left + column * scale + dx, top + row * scale + dy, color, alpha);
          }
        }
      }
    }
  }

  static void Blend(SDL_Surface* surface, int x, int y, Color color, int alpha) {
    Uint8 r, g, b, a;
    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h ||
        !SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a)) {
      return;
    }
    SDL_WriteSurfacePixel(surface, x, y, Uint8(r + (color.r - r) * alpha / 255),
                          Uint8(g + (color.g - g) * alpha / 255), Uint8(b + (color.b - b) * alpha / 255), 0xff);
  }

  ImFontAtlas atlas_;
  ImFontBaked* baked_ = nullptr;
  unsigned char* pixels_ = nullptr;
  int width_ = 0;
  int height_ = 0;
};

void FillRect(SDL_Surface* surface, const SDL_Rect& rect, Color color) {
  SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, Uint8(color.r), Uint8(color.g), Uint8(color.b)));
}

Button ButtonAt(float x, float y) {
  const SDL_Point point{int(x), int(y)};
  if (SDL_PointInRect(&point, &kPlayButton)) {
    return Button::kPlay;
  }
  return SDL_PointInRect(&point, &kQuitButton) ? Button::kQuit : Button::kNone;
}

std::string FitToWidth(const SoftwareText& text, std::string value, int max_width) {
  if (text.Width(value, 1) <= max_width) {
    return value;
  }
  while (value.size() > 1 && text.Width("..." + value, 1) > max_width) {
    value.erase(0, 1);
  }
  return "..." + value;
}

void DrawButton(SDL_Surface* surface, const SoftwareText& text, const SDL_Rect& rect, const char* label,
                bool primary, bool hovered) {
  FillRect(surface, rect, primary ? (hovered ? kAccentHover : kAccent) : (hovered ? kButtonHover : kButton));
  const int width = text.Width(label, kLargeScale);
  text.Draw(surface, label, rect.x + (rect.w - width) / 2, rect.y + (rect.h - text.Height(kLargeScale)) / 2,
            kLargeScale, kText);
}

void DrawStatusRow(SDL_Surface* surface, const SoftwareText& text, int y, const char* label, const std::string& value,
                   Color value_color) {
  text.Draw(surface, label, kMargin, y, 1, kDimText);
  text.Draw(surface, value, kMargin + kLabelWidth, y, 1, value_color);
}

void DrawLauncher(SDL_Surface* surface, const SoftwareText& text, SDL_Surface* icon, const GraphicsFallbackInfo& info,
                  Button hovered) {
  FillRect(surface, {0, 0, surface->w, surface->h}, kBackground);
  FillRect(surface, {0, 0, surface->w, kHeaderHeight}, kHeader);
  FillRect(surface, {0, kHeaderHeight, surface->w, 3}, kAccent);
  int title_x = kMargin;
  if (icon) {
    SDL_Rect target{kMargin, (kHeaderHeight - kIconSize) / 2, kIconSize, kIconSize};
    SDL_BlitSurfaceScaled(icon, nullptr, surface, &target, SDL_SCALEMODE_LINEAR);
    title_x += kIconSize + kMargin;
  }
  text.Draw(surface, "GEOMETRY WARS: RETRO EVOLVED 2", title_x, 24, kLargeScale, kText);
  text.Draw(surface, "RECOMPILED", title_x, 32 + text.Height(kLargeScale), kLargeScale, kAccentHover);

  const int max_value_width = surface->w - 2 * kMargin - kLabelWidth;
  int row_y = kHeaderHeight + 32;
  DrawStatusRow(surface, text, row_y, "Game files", info.game_files_status,
                info.game_files_ready ? kReadyText : kWarningText);
  row_y += kRowStep;
  DrawStatusRow(surface, text, row_y, "Folder", FitToWidth(text, info.game_dir, max_value_width), kDimText);
  row_y += kRowStep;
  DrawStatusRow(surface, text, row_y, "Graphics", "Vulkan setup required", kWarningText);
  text.Draw(surface, "Press PLAY for details about the graphics setup.", kMargin, row_y + kRowStep, 1, kDimText);

  DrawButton(surface, text, kPlayButton, "PLAY", true, hovered == Button::kPlay);
  DrawButton(surface, text, kQuitButton, "QUIT", false, hovered == Button::kQuit);
}

void DrawGraphicsFailure(SDL_Surface* surface, const SoftwareText& text) {
  FillRect(surface, {0, 0, surface->w, surface->h}, kBackground);
  FillRect(surface, {0, 0, surface->w, 3}, kAccent);
  int y = kMargin;
  text.Draw(surface, "The graphics system could not start.", kMargin, y, kLargeScale, kWarningText);
  y += text.Height(kLargeScale) + kLineStep;
  for (const char* line : kGraphicsFailureLines) {
    text.Draw(surface, line, kMargin, y, 1, kText);
    y += kLineStep;
  }
  text.Draw(surface, "Press a key or click to go back.", kMargin, kHeight - kMargin - text.Height(1), 1, kDimText);
}

}  // namespace

void RunLauncherWithoutGraphics(const GraphicsFallbackInfo& info) {
  SDL_Window* window = SDL_CreateWindow(kSoftwareWindowTitle, kWidth, kHeight, 0);
  if (!window || !SDL_GetWindowSurface(window)) {
    REXLOG_ERROR("Could not open the launcher window without graphics: {}", SDL_GetError());
    std::fprintf(stderr, "Could not open the launcher window without graphics: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    return;
  }
  const SoftwareText text;
  SDL_Surface* icon = LoadApplicationIcon();
  if (icon) {
    SDL_SetWindowIcon(window, icon);
  }
  Button hovered = Button::kNone;
  bool showing_failure = false;
  const auto redraw = [&] {
    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if (!surface) {
      return;
    }
    if (showing_failure) {
      DrawGraphicsFailure(surface, text);
    } else {
      DrawLauncher(surface, text, icon, info, hovered);
    }
    SDL_UpdateWindowSurface(window);
  };
  redraw();
  for (bool open = true; open;) {
    SDL_Event event;
    if (!SDL_WaitEvent(&event)) {
      break;
    }
    Button pressed = Button::kNone;
    switch (event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        open = false;
        break;
      case SDL_EVENT_KEY_DOWN:
        if (showing_failure) {
          showing_failure = false;
          redraw();
        } else if (event.key.key == SDLK_ESCAPE) {
          open = false;
        } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
          pressed = Button::kPlay;
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (showing_failure) {
          showing_failure = false;
          redraw();
        } else {
          pressed = ButtonAt(event.button.x, event.button.y);
        }
        break;
      case SDL_EVENT_MOUSE_MOTION:
        if (const Button over = showing_failure ? Button::kNone : ButtonAt(event.motion.x, event.motion.y);
            over != hovered) {
          hovered = over;
          redraw();
        }
        break;
      case SDL_EVENT_WINDOW_EXPOSED:
        redraw();
        break;
      default:
        break;
    }
    if (pressed == Button::kQuit) {
      open = false;
    } else if (pressed == Button::kPlay) {
      showing_failure = true;
      redraw();
    }
  }
  SDL_DestroySurface(icon);
  SDL_DestroyWindow(window);
}

std::vector<Rect> WorkAreas() {
  std::vector<Rect> areas;
  int count = 0;
  SDL_DisplayID* displays = SDL_GetDisplays(&count);
  for (int i = 0; displays && i < count; ++i) {
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(displays[i], &usable) && usable.w > 0 && usable.h > 0) {
      areas.push_back({usable.x, usable.y, usable.x + usable.w, usable.y + usable.h});
    }
  }
  SDL_free(displays);
  return areas;
}

std::optional<WindowState> QueryWindowState(void*) {
  SDL_Window* window = LauncherWindow();
  if (!window) {
    return std::nullopt;
  }
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &width, &height);
  const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
  WindowState state;
  state.rect = {x, y, x + width, y + height};
  state.minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
  state.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
  return state;
}

void ApplyWindowPlacement(void*, const WindowPlacement& placement) {
  SDL_Window* window = LauncherWindow();
  if (!window) {
    return;
  }
  SDL_RestoreWindow(window);
  SDL_SyncWindow(window);
  SDL_SetWindowSize(window, placement.normal.width(), placement.normal.height());
  SDL_SetWindowPosition(window, placement.normal.left, placement.normal.top);
  if (placement.maximized) {
    SDL_MaximizeWindow(window);
  }
}

void SetApplicationId() {
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, kApplicationId);
}

void ApplyWindowIcon() {
  SDL_Window* window = LauncherWindow();
  if (!window) {
    return;
  }
  if (SDL_Surface* icon = LoadApplicationIcon()) {
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
  }
}

void InstallPreShowPlacement(const PreShowPlan&, const WindowPlacement&) {}

std::optional<PreShowResult> TakePreShowResult() {
  return std::nullopt;
}

// Sizes are SDL window coordinates, the units ReXGlue sizes its window in (logical size times the scale).
std::vector<MonitorInfo> ListMonitors() {
  std::vector<MonitorInfo> monitors;
  int count = 0;
  SDL_DisplayID* displays = SDL_GetDisplays(&count);
  const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
  for (int i = 0; displays && i < count; ++i) {
    MonitorInfo found;
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(displays[i], &bounds)) {
      found.width = found.work_width = bounds.w;
      found.height = found.work_height = bounds.h;
    }
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(displays[i], &usable) && usable.w > 0 && usable.h > 0) {
      found.work_width = usable.w;
      found.work_height = usable.h;
    }
    if (const float scale = SDL_GetDisplayContentScale(displays[i]); scale > 0.0f) {
      found.scale = scale;
    }
    found.primary = displays[i] == primary;
    monitors.push_back(found);
  }
  SDL_free(displays);
  return monitors;
}

bool ShiftHeld() {
  SDL_PumpEvents();
  return (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
}

bool PackagePickerAvailable() {
  return OnPath("zenity") || OnPath("kdialog");
}

bool RelaunchSelf(std::wstring_view extra_args) {
  std::vector<std::string> args;
  std::ifstream cmdline("/proc/self/cmdline", std::ios::binary);
  for (std::string arg; std::getline(cmdline, arg, '\0');) {
    args.push_back(arg);
  }
  std::string extra;
  for (const wchar_t c : extra_args) {
    extra += char(c);  // ASCII flags
  }
  args.push_back(extra);
  // Inside an AppImage, start the AppImage again: this process's mount closes when it exits.
  std::error_code ec;
  const char* appimage = std::getenv("APPIMAGE");
  const std::string exe =
      appimage && *appimage ? std::string(appimage) : std::filesystem::read_symlink("/proc/self/exe", ec).string();
  if (exe.empty() || args.size() < 2) {
    REXLOG_ERROR("Relaunch failed: cannot read this process's command line");
    return false;
  }
  args.front() = exe;
  std::vector<char*> argv;
  for (std::string& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);
  pid_t pid = 0;
  if (const int error = posix_spawn(&pid, exe.c_str(), nullptr, nullptr, argv.data(), environ); error != 0) {
    REXLOG_ERROR("Relaunch failed ({})", error);
    return false;
  }
  return true;
}

UiFontFiles FindUiFonts() {
  for (const FontFamily& family : kFontFamilies) {
    std::error_code ec;
    if (std::filesystem::exists(family.regular, ec)) {
      return {family.regular, family.semibold ? family.semibold : family.bold, family.bold};
    }
  }
  return {};
}

void LauncherGamepad::Update(ImGuiIO& io) {
  SDL_Gamepad* pad = pad_ >= 0 ? SDL_GetGamepadFromID(SDL_JoystickID(pad_)) : nullptr;
  if (!pad || !SDL_GamepadConnected(pad)) {
    if (pad) {
      SDL_CloseGamepad(pad);
    }
    pad_ = -1;
    held_ = 0;
    if (ImGui::GetTime() < next_scan_) {
      return;
    }
    next_scan_ = ImGui::GetTime() + 1.0;
    // Started here and quit in Release; SDL counts the starts, so the game's own input is unaffected.
    if (!subsystem_ && !(subsystem_ = SDL_InitSubSystem(SDL_INIT_GAMEPAD))) {
      return;
    }
    pad = OpenFirstGamepad();
    if (!pad) {
      return;
    }
    pad_ = int(SDL_GetGamepadID(pad));
  }
  io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  held_ = 0;
  for (size_t i = 0; i < std::size(kButtonKeys); ++i) {
    const bool down = SDL_GetGamepadButton(pad, kButtonKeys[i].button);
    io.AddKeyEvent(kButtonKeys[i].key, down);
    held_ |= uint32_t(down) << i;
  }
  for (const StickKey& s : kStickKeys) {
    const int value = s.direction * int(SDL_GetGamepadAxis(pad, s.axis));
    const float amount = std::clamp(float(value - kStickDeadZone) / float(32767 - kStickDeadZone), 0.0f, 1.0f);
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
  if (pad_ >= 0) {
    if (SDL_Gamepad* pad = SDL_GetGamepadFromID(SDL_JoystickID(pad_))) {
      SDL_CloseGamepad(pad);
    }
  }
  pad_ = -1;
  if (subsystem_) {
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    subsystem_ = false;
  }
}

}  // namespace gw2
