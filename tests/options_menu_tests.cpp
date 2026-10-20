// Options menu tests: the L3 + R3 combo and how a row's value steps through its choices.
// Logic only: the menu itself needs the running runtime.

#include <cstdio>
#include <string>
#include <vector>

#include "options_menu_rules.h"

using namespace gw2;

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                 \
  do {                                                              \
    ++g_checks;                                                     \
    if (!(cond)) {                                                  \
      ++g_failures;                                                 \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                               \
  } while (0)

void TestCombo() {
  OptionsComboDetector combo;
  CHECK(!combo.Poll(0));
  CHECK(!combo.Poll(kXinputButtonLeftThumb));
  CHECK(combo.Poll(kOptionsMenuButtons));
  // Held: opens once.
  CHECK(!combo.Poll(kOptionsMenuButtons));
  CHECK(!combo.Poll(kOptionsMenuButtons | 0x1000));
  CHECK(combo.held());
  // One released and pressed again: a new press.
  CHECK(!combo.Poll(kXinputButtonRightThumb));
  CHECK(combo.Poll(kOptionsMenuButtons));
  // Other buttons don't matter.
  CHECK(!combo.Poll(0));
  CHECK(combo.Poll(kOptionsMenuButtons | 0x0001 | 0x2000));
}

void TestChoices() {
  const std::vector<std::string> values = {"1", "2", "3", "4"};
  CHECK(ChoiceIndex(values, "3") == 2);
  CHECK(ChoiceIndex(values, "7") == -1);
  CHECK(StepChoice(1, 4, 1) == 2);
  CHECK(StepChoice(1, 4, -1) == 0);
  CHECK(StepChoice(0, 4, -1) == 0);
  CHECK(StepChoice(3, 4, 1) == 3);
  CHECK(StepChoice(-1, 4, 1) == 0);
  CHECK(StepChoice(-1, 4, -1) == 0);
  CHECK(StepChoice(0, 0, 1) == -1);
  CHECK(NextChoiceWrapping(0, 2) == 1);
  CHECK(NextChoiceWrapping(1, 2) == 0);
  CHECK(NextChoiceWrapping(-1, 2) == 0);
  CHECK(NextChoiceWrapping(0, 0) == -1);
  CHECK(WrapIndex(0, 5, -1) == 4);
  CHECK(WrapIndex(4, 5, 1) == 0);
  CHECK(WrapIndex(2, 5, 1) == 3);
  CHECK(WrapIndex(0, 0, 1) == 0);
}

}  // namespace

int main() {
  TestCombo();
  TestChoices();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures ? 1 : 0;
}
