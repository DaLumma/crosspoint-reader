#include "KanaPracticeConfig.h"

#if CROSSPOINT_KANA_PRACTICE

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>

namespace kana {
namespace {
constexpr char CONFIG_DIR[] = "/.crosspoint";
constexpr char CONFIG_PATH[] = "/.crosspoint/kana.txt";
constexpr int CONFIG_VERSION = 2;  // 2 added the kana key style; 1 still loads
}  // namespace

int PracticeConfig::selectedKanaCount() const {
  int total = 0;
  for (int s = 0; s < SCRIPT_COUNT; ++s) {
    for (int g = 0; g < GROUP_COUNT; ++g) {
      if (masks[s] & (1u << g)) total += groups()[g].count;
    }
  }
  return total;
}

void PracticeConfig::load() {
  char buffer[64];
  if (Storage.readFileToBuffer(CONFIG_PATH, buffer, sizeof(buffer)) == 0) return;
  int version = 0, dir = 0, ans = 0, count = 0, keyStyle = 0;
  unsigned hira = 0, kata = 0;
  const int fields = sscanf(buffer, "%d %d %d %d %x %x %d", &version, &dir, &ans, &count, &hira, &kata, &keyStyle);
  if (!((version == 1 && fields >= 6) || (version == CONFIG_VERSION && fields == 7))) {
    LOG_ERR("KANA", "Ignoring unreadable %s", CONFIG_PATH);
    return;
  }
  if (dir >= 0 && dir < DIRECTION_COUNT) direction = static_cast<Direction>(dir);
  if (ans >= 0 && ans < ANSWER_MODE_COUNT) answer = static_cast<AnswerMode>(ans);
  if (count >= 0 && count < QUESTION_COUNT_OPTIONS) countIndex = static_cast<uint8_t>(count);
  masks[0] = static_cast<GroupMask>(hira);
  masks[1] = static_cast<GroupMask>(kata);
  if (keyStyle >= 0 && keyStyle < KANA_KEY_STYLE_COUNT) kanaKeys = static_cast<KanaKeyStyle>(keyStyle);
}

void PracticeConfig::save() const {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%d %d %d %d %x %x %d\n", CONFIG_VERSION, static_cast<int>(direction),
           static_cast<int>(answer), static_cast<int>(countIndex), static_cast<unsigned>(masks[0]),
           static_cast<unsigned>(masks[1]), static_cast<int>(kanaKeys));
  Storage.ensureDirectoryExists(CONFIG_DIR);
  if (!Storage.writeFile(CONFIG_PATH, String(buffer))) LOG_ERR("KANA", "Could not save %s", CONFIG_PATH);
}

const char* directionLabel(const Direction direction) {
  switch (direction) {
    case Direction::KanaToRomaji:
      return "Kana to romaji";
    case Direction::RomajiToKana:
      return "Romaji to kana";
    case Direction::Both:
      return "Both (mixed)";
  }
  return "";
}

const char* kanaKeyStyleLabel(const KanaKeyStyle style) {
  return style == KanaKeyStyle::Cycle ? "Tap to cycle" : "Pick from list";
}

const char* answerModeLabel(const AnswerMode mode) {
  return mode == AnswerMode::Keyboard ? "Keyboard" : "Multiple choice";
}

}  // namespace kana

#endif  // CROSSPOINT_KANA_PRACTICE
