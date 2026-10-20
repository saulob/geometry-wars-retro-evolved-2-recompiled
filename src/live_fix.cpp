#include "live_fix.h"

#include <atomic>
#include <cstring>
#include <mutex>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include "achievements_overlay_rules.h"
#include "guest_memory.h"
#include "options_menu_rules.h"

REXCVAR_DEFINE_BOOL(gw2_local_live, true, "Geometry Wars",
                    "Serve the game's Xbox Live scores and leaderboards from a local file; nothing goes online")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace gw2::live {
namespace {

constexpr uint32_t kSignedInLocally = 1;
constexpr uint32_t kSignedInToLive = 2;
constexpr uint32_t kInputStateGamepadOffset = 4;
constexpr uint32_t kInputStateGamepadSize = 12;
constexpr uint32_t kThumbLeftYOffset = 6;

struct Services {
  Services(std::filesystem::path file, LocalUser user) : stats(std::move(file)), board(stats, std::move(user)) {}

  LocalStats stats;
  LocalLeaderboard board;
  EnumeratorRegistry enumerators;
};

// Never freed: hooks may still run while the process exits.
std::atomic<Services*> g_services{nullptr};
AppBridge g_bridge;
std::mutex g_gate_mutex;
AchievementsOverlayGate g_gate;  // guarded by g_gate_mutex
std::atomic<float> g_scroll_axis{0.0f};
std::atomic<bool> g_holds_input{false};

Services* Active() { return g_services.load(std::memory_order_acquire); }

bool Succeeded(PPCContext& ctx, uint8_t* base, uint32_t overlapped) {
  uint32_t status = ctx.r3.u32;
  if (status == kErrorIoPending && overlapped) status = LoadBE32(base, overlapped);
  return status == kErrorSuccess;
}

bool CreateStatsEnumerator(PPCContext& ctx, uint8_t* base, StatsQuery::Kind kind) {
  Services* services = Active();
  if (!services || ctx.r3.u32 != 0) return false;

  const bool ranked = kind == StatsQuery::Kind::kRanked;
  const uint64_t pivot = ranked ? ctx.r4.u32 : ctx.r4.u64;
  uint32_t handle = 0;
  ctx.r3.u64 = CreateEnumerator(base, services->enumerators, kind, pivot, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32,
                                ctx.r8.u32, ctx.r9.u32, &handle);
  return true;
}

// One detector per controller the game reads.
OptionsComboDetector g_options_combo[4];

void FilterGuestInput(PPCContext& ctx, uint8_t* base, uint32_t user, uint32_t state) {
  if (!state || ctx.r3.u32 != 0) return;
  uint16_t buttons = LoadBE16(base, state + kInputStateGamepadOffset);
  if (user < 4) {
    if (g_options_combo[user].Poll(buttons) && g_bridge.toggle_options) g_bridge.toggle_options();
    if (g_options_combo[user].held()) {
      // The game never sees the pair that opens the menu.
      buttons &= uint16_t(~kOptionsMenuButtons);
      StoreBE<uint16_t>(base, state + kInputStateGamepadOffset, buttons);
    }
  }
  const int thumb_y = static_cast<int16_t>(LoadBE16(base, state + kInputStateGamepadOffset + kThumbLeftYOffset));
  AchievementsOverlayGate::Verdict verdict;
  {
    std::lock_guard lock(g_gate_mutex);
    verdict = g_gate.Poll(user, buttons, thumb_y);
    g_holds_input.store(g_gate.HoldsInput(), std::memory_order_release);
  }
  g_scroll_axis.store(verdict.scroll, std::memory_order_relaxed);
  if (verdict.close && g_bridge.close_achievements) g_bridge.close_achievements();
  if (verdict.hide_input) {
    std::memset(base + state + kInputStateGamepadOffset, 0, kInputStateGamepadSize);
  } else if (verdict.clear_b) {
    StoreBE<uint16_t>(base, state + kInputStateGamepadOffset, buttons & ~kXinputButtonB);
  }
}

}  // namespace

void Install(InstallParams params) {
  g_bridge = std::move(params.bridge);
  if (!REXCVAR_GET(gw2_local_live)) {
    REXLOG_INFO("Local Live services are disabled (gw2_local_live=false)");
    return;
  }
  auto* services = new Services(std::move(params.stats_file), std::move(params.user));
  const LoadResult loaded = services->stats.Load();
  REXLOG_INFO("Local Live services ready, stats file {}",
              loaded == LoadResult::kLoaded    ? "loaded"
              : loaded == LoadResult::kMissing ? "not created yet"
                                               : "was corrupt");
  g_services.store(services, std::memory_order_release);
}

void SetAchievementsModal(bool modal) {
  {
    std::lock_guard lock(g_gate_mutex);
    g_gate.SetOpen(modal);
    g_holds_input.store(g_gate.HoldsInput(), std::memory_order_release);
  }
  g_scroll_axis.store(0.0f, std::memory_order_relaxed);
}

bool AchievementsHoldInput() { return g_holds_input.load(std::memory_order_acquire); }

float AchievementsScrollAxis() { return g_scroll_axis.load(std::memory_order_relaxed); }

}  // namespace gw2::live

using namespace gw2;
using namespace gw2::live;

// XUserGetSigninState: the game only treats a player as eligible for scores and leaderboards when it
// reports signed in to Live. ReXGlue's local profile reports signed in locally.
REX_EXTERN(__imp__sub_820955C8);
REX_HOOK_RAW(sub_820955C8) {
  const uint32_t user = ctx.r3.u32;
  __imp__sub_820955C8(ctx, base);
  if (user != 0 || ctx.r3.u32 != kSignedInLocally || !Active()) return;
  ctx.r3.u64 = kSignedInToLive;
}

// XSessionWriteStats(session, xuid, view count, views, overlapped): the score of a finished game.
REX_EXTERN(__imp__sub_820E88F8);
REX_HOOK_RAW(sub_820E88F8) {
  const uint64_t xuid = ctx.r4.u64;
  const uint32_t view_count = ctx.r5.u32;
  const uint32_t views = ctx.r6.u32;
  const uint32_t overlapped = ctx.r7.u32;
  __imp__sub_820E88F8(ctx, base);
  Services* services = Active();
  if (!services || !Succeeded(ctx, base, overlapped)) return;
  if (xuid != services->board.user().xuid) return;

  for (const ScoreWrite& write : ParseScoreWrites(base, views, view_count)) {
    if (services->stats.Record(xuid, write.view, write.score) == RecordResult::kNotSaved) {
      REXLOG_WARN("New best score in view {} kept in memory: the stats file could not be written", write.view);
    }
  }
}

// XUserCreateStatsEnumeratorByRank / ByXuid: ReXGlue only stubs the underlying call.
REX_EXTERN(__imp__sub_820956B8);
REX_HOOK_RAW(sub_820956B8) {
  if (!CreateStatsEnumerator(ctx, base, StatsQuery::Kind::kRanked)) __imp__sub_820956B8(ctx, base);
}

REX_EXTERN(__imp__sub_82095718);
REX_HOOK_RAW(sub_82095718) {
  if (!CreateStatsEnumerator(ctx, base, StatsQuery::Kind::kAround)) __imp__sub_82095718(ctx, base);
}

// XEnumerate(handle, buffer, size, items, overlapped). Other handles (friends, achievements) are
// ReXGlue's.
REX_EXTERN(__imp__sub_820958D0);
REX_HOOK_RAW(sub_820958D0) {
  if (Services* services = Active()) {
    const uint32_t handle = ctx.r3.u32;
    if (const auto query = services->enumerators.Take(handle)) {
      ctx.r3.u64 = ServeEnumeration(base, services->board, *query, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);
      return;
    }
  }
  __imp__sub_820958D0(ctx, base);
}

// XUserReadStats(title, xuid count, xuids, spec count, specs, size ptr, results, overlapped): the
// game's own scores and its friends'. ReXGlue accepts the request and leaves the results empty.
REX_EXTERN(__imp__sub_820955D0);
REX_HOOK_RAW(sub_820955D0) {
  const uint32_t xuid_count = ctx.r4.u32;
  const uint32_t xuids = ctx.r5.u32;
  const uint32_t spec_count = ctx.r6.u32;
  const uint32_t specs = ctx.r7.u32;
  const uint32_t size_ptr = ctx.r8.u32;
  const uint32_t results = ctx.r9.u32;
  const uint32_t overlapped = ctx.r10.u32;
  __imp__sub_820955D0(ctx, base);
  Services* services = Active();
  if (!services || !results || !size_ptr || !Succeeded(ctx, base, overlapped)) return;

  ServeReadStats(base, services->board, xuid_count, xuids, spec_count, specs, LoadBE32(base, size_ptr), results);
}

// XamShowAchievementsUI(user): opens the achievements overlay instead of ReXGlue's stub.
REX_EXTERN(__imp__sub_82094DF8);
REX_HOOK_RAW(sub_82094DF8) {
  if (!g_bridge.open_achievements) {
    __imp__sub_82094DF8(ctx, base);
    return;
  }
  g_bridge.open_achievements();
  ctx.r3.u64 = kErrorSuccess;
}

// XInputGetState(user, state): the game stays out of the way while the achievements overlay is up.
REX_EXTERN(__imp__sub_82094B60);
REX_HOOK_RAW(sub_82094B60) {
  const uint32_t user = ctx.r3.u32;
  const uint32_t state = ctx.r4.u32;
  __imp__sub_82094B60(ctx, base);
  FilterGuestInput(ctx, base, user, state);
}
