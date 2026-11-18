#pragma once

#include <cstdint>

#include "KanaData.h"

namespace kana {

enum class Direction : uint8_t { KanaToRomaji = 0, RomajiToKana = 1, Both = 2 };
constexpr int DIRECTION_COUNT = 3;

enum class AnswerMode : uint8_t { Keyboard = 0, Choice = 1 };
constexpr int ANSWER_MODE_COUNT = 2;

// How the 12-key kana pad enters a kana: tap a row key repeatedly to cycle
// (phone multi-tap), or tap it once and pick from all of the row's kana.
enum class KanaKeyStyle : uint8_t { Cycle = 0, Picker = 1 };
constexpr int KANA_KEY_STYLE_COUNT = 2;

// Question counts offered in setup; 0 means "every selected kana once".
constexpr uint8_t QUESTION_COUNTS[] = {10, 20, 30, 50, 0};
constexpr int QUESTION_COUNT_OPTIONS = sizeof(QUESTION_COUNTS) / sizeof(QUESTION_COUNTS[0]);

// Last setup choices, kept in /.crosspoint/kana.txt so practice resumes with
// the rows the learner had picked.
struct PracticeConfig {
  Direction direction = Direction::KanaToRomaji;
  AnswerMode answer = AnswerMode::Keyboard;
  uint8_t countIndex = 1;
  KanaKeyStyle kanaKeys = KanaKeyStyle::Cycle;
  GroupMask masks[SCRIPT_COUNT] = {0x0001, 0};  // hiragana vowels

  int selectedKanaCount() const;
  void load();
  void save() const;
};

const char* directionLabel(Direction direction);
const char* answerModeLabel(AnswerMode mode);
const char* kanaKeyStyleLabel(KanaKeyStyle style);

}  // namespace kana
