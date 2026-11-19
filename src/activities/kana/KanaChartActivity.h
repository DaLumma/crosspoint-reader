#pragma once

#include <components/bars/tab-bar.h>

#include <cstdint>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "kana/KanaData.h"

// Kana lookup chart: hiragana and katakana tabs, each paged into the basic
// gojūon grid, the dakuten rows and the combinations, with romaji per cell.
class KanaChartActivity final : public Activity, private UiAppHost {
 public:
  KanaChartActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static void chartScreen(UiScreen& screen, void* user);
  static void onTab(const freeink::ui::ActionEvent& event, void* user);
  static void onPage(const freeink::ui::ActionEvent& event, void* user);

  void buildChartScreen(UiScreen& screen);
  void setScript(int script);
  void turnPage(int delta);

  int script = 0;  // kana::Script as int: tab index
  int page = 0;
  freeink::ui::TabItem tabs[kana::SCRIPT_COUNT]{};
  char pageLabel[32] = {};
};
