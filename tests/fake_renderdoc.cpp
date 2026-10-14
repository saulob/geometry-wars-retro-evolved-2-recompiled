// Stand-in for librenderdoc.so in renderdoc_detect_tests.cpp: exports the one entry point ReXGlue looks up.

namespace {
char g_api[256];
}

extern "C" __attribute__((visibility("default"))) int RENDERDOC_GetAPI(int /*version*/, void** out_api) {
  *out_api = g_api;
  return 1;
}
