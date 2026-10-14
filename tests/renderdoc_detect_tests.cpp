// RenderDocAPI::CreateIfConnected must use RenderDoc only when the process already loaded it (patch 0003, see
// docs/rexglue-runtime-patch.md). Linux only. The fake librenderdoc.so is in renderdoc/ next to this executable, out of
// reach of the runtime's $ORIGIN RUNPATH, so it is absent until the re-executed child puts that folder on
// LD_LIBRARY_PATH, where a plain dlopen would find it.
// Run: gw2_renderdoc_detect_tests   (exit code 0 = pass)

#include <dlfcn.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <rex/logging.h>
#include <rex/ui/renderdoc_api.h>

namespace {

constexpr const char* kLibrary = "librenderdoc.so";
constexpr const char* kChildMarker = "GW2_RENDERDOC_TEST_CHILD";

int g_failures = 0;

void Check(bool ok, const char* what) {
  std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) {
    ++g_failures;
  }
}

bool IsLoaded() {
  void* handle = dlopen(kLibrary, RTLD_NOLOAD | RTLD_LAZY);
  if (handle) {
    dlclose(handle);
  }
  return handle != nullptr;
}

int ReexecWithLibraryPath(char** argv) {
  char exe[4096];
  const ssize_t length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
  if (length <= 0) {
    std::printf("cannot resolve /proc/self/exe\n");
    return 2;
  }
  exe[length] = '\0';
  std::string dir(exe);
  dir.resize(dir.rfind('/'));
  dir += "/renderdoc";
  if (const char* old_path = std::getenv("LD_LIBRARY_PATH"); old_path && *old_path) {
    dir += std::string(":") + old_path;
  }
  setenv("LD_LIBRARY_PATH", dir.c_str(), 1);
  setenv(kChildMarker, "1", 1);
  execv(exe, argv);
  std::perror("execv");
  return 2;
}

}  // namespace

int main(int /*argc*/, char** argv) {
  rex::InitLogging();

  if (!std::getenv(kChildMarker)) {
    // RenderDoc absent: nothing to find, nothing loaded.
    {
      auto api = rex::ui::RenderDocAPI::CreateIfConnected();
      Check(api == nullptr, "absent: CreateIfConnected returns nullptr");
      Check(!IsLoaded(), "absent: nothing was loaded");
    }
    if (g_failures) {
      std::printf("FAIL\n");
      return 1;
    }
    return ReexecWithLibraryPath(argv);
  }

  Check(!IsLoaded(), "fake RenderDoc is not loaded at start");
  void* probe = dlopen(kLibrary, RTLD_LAZY);
  Check(probe != nullptr, "fake RenderDoc is discoverable through the loader path");
  if (probe) {
    dlclose(probe);
  }
  Check(!IsLoaded(), "fake RenderDoc unloads again after the probe");

  // Discoverable but not loaded: no API, and nothing gets loaded.
  {
    auto api = rex::ui::RenderDocAPI::CreateIfConnected();
    Check(api == nullptr, "not loaded: CreateIfConnected returns nullptr");
    Check(!IsLoaded(), "not loaded: CreateIfConnected did not load the library");
  }

  // Loaded the way an attached RenderDoc would be: the API is found, and the reference it takes is released.
  void* attached = dlopen(kLibrary, RTLD_NOW);
  Check(attached != nullptr, "RenderDoc loaded intentionally");
  {
    auto api = rex::ui::RenderDocAPI::CreateIfConnected();
    Check(api != nullptr && api->api_1_0_0() != nullptr, "loaded: CreateIfConnected finds the API");
  }
  Check(IsLoaded(), "loaded: the library is still loaded after RenderDocAPI is released");

  // RenderDocAPI holds its own reference: the library outlives the intentional owner and goes with the last one.
  {
    auto api = rex::ui::RenderDocAPI::CreateIfConnected();
    Check(api != nullptr, "loaded: CreateIfConnected finds the API again");
    if (attached) {
      dlclose(attached);
      attached = nullptr;
    }
    Check(IsLoaded(), "loaded: RenderDocAPI keeps the library loaded after the other owner released it");
  }
  Check(!IsLoaded(), "loaded: references are balanced, the library unloads with the last owner");

  std::printf(g_failures ? "FAIL\n" : "PASS\n");
  return g_failures ? 1 : 0;
}
