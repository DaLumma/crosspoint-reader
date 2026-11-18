#pragma once

#include <components/keyboard/key-grid.h>

#include <cstdint>
#include <vector>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "kana/KanaPracticeConfig.h"

// One kana practice round. Each question shows a kana (answer in romaji) or a
// romaji syllable (answer with the kana), answered on an on-screen keyboard
// or by picking one of four options. Wrong answers are shown with the right
// one; the round ends with a score and an option to repeat the missed kana.
class KanaQuizActivity final : public Activity, private UiAppHost {
 public:
  KanaQuizActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const kana::PracticeConfig& config);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Phase : uint8_t { Asking, Feedback, Summary };
  // Which on-screen key set the current question uses.
  enum class Pad : uint8_t { Romaji, Kana, Choice };

  struct Question {
    uint8_t entry;
    kana::Script script;
    bool kanaPrompt;  // true: kana shown, answer in romaji
  };
  struct PoolItem {
    uint8_t entry;
    kana::Script script;
  };

  static constexpr int MAX_KEYS = 24;
  static constexpr int CHOICES = 4;
  static constexpr int PAD_TEXT_MAX = 4;
  static constexpr int SUMMARY_BUTTONS = 3;

  static void quizScreen(UiScreen& screen, void* user);
  static void onKey(const freeink::ui::ActionEvent& event, void* user);
  static void onNext(const freeink::ui::ActionEvent& event, void* user);

  void buildPool(bool missedOnly);
  void startRound(bool missedOnly);
  void prepareQuestion();
  void buildChoices();
  void buildKeys();
  void handleKey(int index);
  void handleKanaPadKey(int index);
  void handlePickerKey(int index);
  void closePicker();
  void submitAnswer();
  void advance();
  void showSummary();
  void activateSummaryButton(int index);
  int summaryButtonCount() const;
  void moveSelection(int delta);
  void updateTypedText();

  void buildQuizScreen(UiScreen& screen);
  void drawStatus(UiScreen& screen, freeink::ui::Rect rect);
  void drawPrompt(UiScreen& screen, freeink::ui::Rect rect);
  void drawSummary(UiScreen& screen, freeink::ui::Rect rect);

  kana::PracticeConfig config;
  std::vector<PoolItem> pool;
  std::vector<Question> questions;
  int current = 0;
  int correctCount = 0;
  int wrongCount = 0;
  Phase phase = Phase::Asking;
  Pad pad = Pad::Romaji;
  // Times each kana was missed this round, indexed [script][entry].
  uint8_t missed[kana::SCRIPT_COUNT][104] = {};

  // Current question, prepared once per question for the render task.
  char prompt[16] = {};
  char promptHint[24] = {};
  char typed[24] = {};  // what the keyboard has entered (romaji or kana)
  uint32_t padText[PAD_TEXT_MAX] = {};
  int padLen = 0;
  int padLastKey = -1;  // multi-tap key that may still cycle the last kana
  int padCycle = 0;
  int pickerRow = -1;  // pad row whose kana are listed ("pick from list"), or -1
  uint32_t pickerCps[MAX_KEYS] = {};
  char keyLabels[MAX_KEYS][8] = {};
  freeink::ui::KeyGridKey keys[MAX_KEYS]{};
  uint8_t keyRows = 0;
  uint8_t keyCols = 0;
  char choiceText[CHOICES][16] = {};
  uint8_t choiceEntry[CHOICES] = {};
  int selected = 0;  // focused choice or summary button (physical buttons)
  bool buttonFocus = false;  // choice focus is drawn only once buttons are used

  // Result of the last answer: banner after a right answer, panel after a wrong one.
  bool lastCorrect = false;
  char resultLine[48] = {};
  char yourAnswerLine[48] = {};

  char statusLeft[24] = {};
  char statusRight[24] = {};
  char summaryScore[48] = {};
  char summaryMissedButton[32] = {};
  // "か ka ×2 · き ki ×1 ..." for the summary; bounded by the 208 kana.
  char summaryMissed[640] = {};

  int rendersSinceClean = 0;
};
