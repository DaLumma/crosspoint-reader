#pragma once

#include "activities/UiListActivity.h"
#include "kana/KanaPracticeConfig.h"

// Kana practice setup: pick hiragana/katakana rows, direction, answer method
// and question count, then start a quiz. Choices persist between visits.
class KanaSetupActivity final : public UiListActivity {
 public:
  KanaSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;

 private:
  // Row layout: fixed option rows, then per script an "all" row and one row
  // per group.
  enum Row : int { ROW_START = 0, ROW_DIRECTION, ROW_ANSWER, ROW_KANA_KEYS, ROW_COUNT, ROW_FIRST_SCRIPT };
  static constexpr int ROWS_PER_SCRIPT = 1 + kana::GROUP_COUNT;
  static constexpr int TOTAL_ROWS = ROW_FIRST_SCRIPT + kana::SCRIPT_COUNT * ROWS_PER_SCRIPT;

  int listCount() const override { return TOTAL_ROWS; }
  const char* headerTitle() const override { return "Kana practice"; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;

  void rebuildRows();
  void startQuiz();

  kana::PracticeConfig config;
  freeink::ui::ListItem rows[TOTAL_ROWS]{};
  char startSubtitle[40] = {};
  char countValue[8] = {};
};
