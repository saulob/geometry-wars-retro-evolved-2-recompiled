// Extracts an Xbox 360 STFS (LIVE, PIRS or CON) package into an empty folder with the launcher's own reader
// (src/stfs.cpp). The package must carry the Geometry Wars: Retro Evolved 2 title ID.
//   extract_stfs <package> <out dir>
// Built and run by tools/extract_stfs.sh; the Windows counterpart is tools/extract_stfs.ps1.
#include <cstdio>
#include <filesystem>
#include <system_error>

#include "stfs.h"

namespace {

constexpr uint32_t kTitleId = 0x584108FF;

int Fail(const char* what, const std::string& detail) {
  std::fprintf(stderr, "%s%s%s\n", what, detail.empty() ? "" : ": ", detail.c_str());
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: extract_stfs <package> <out dir>\n");
    return 2;
  }
  gw2::stfs::Package package;
  if (auto result = package.Open(argv[1]); !result.ok()) {
    return Fail("cannot open package", result.detail);
  }
  if (package.title_id() != kTitleId) {
    std::fprintf(stderr, "Package title ID 0x%08X is not the expected 0x%08X.\n", package.title_id(), kTitleId);
    return 1;
  }
  if (auto result = package.LoadFiles(); !result.ok()) {
    return Fail("cannot read the file table", result.detail);
  }

  const std::filesystem::path out = argv[2];
  std::error_code ec;
  if (std::filesystem::exists(out, ec) && !std::filesystem::is_empty(out, ec)) {
    return Fail((out.string() + " is not empty; extract into a new folder").c_str(), {});
  }
  std::filesystem::create_directories(out, ec);
  if (ec) {
    return Fail(("cannot create " + out.string()).c_str(), ec.message());
  }

  if (auto result = package.ExtractTo(out, {}); !result.ok()) {
    return Fail("extraction failed", result.detail);
  }
  std::printf("Extracted %zu files (%llu bytes) to %s\n", package.file_count(),
              static_cast<unsigned long long>(package.total_bytes()), out.string().c_str());
  return 0;
}
