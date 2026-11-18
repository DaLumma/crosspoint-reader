#pragma once

#include <cstddef>
#include <cstdint>

// Kana practice data: the gojūon table grouped into the rows learners pick,
// romaji spellings, and the helpers the quiz and its 12-key pad share.
// Katakana is derived from hiragana (+0x60 per code point), so only one table
// is stored.
namespace kana {

enum class Script : uint8_t { Hiragana = 0, Katakana = 1 };
constexpr int SCRIPT_COUNT = 2;

struct Entry {
  const char* hiragana;  // UTF-8, one or two kana
  const char* romaji;    // Hepburn, shown and accepted
  const char* alts;      // other accepted spellings, space separated, or nullptr
};

struct Group {
  const char* name;   // short label for the setup list
  const char* hint;   // romaji of the members, for the setup list
  uint8_t first;      // index into entries()
  uint8_t count;
};

const Entry* entries();
size_t entryCount();
const Group* groups();
constexpr int GROUP_COUNT = 16;

// Group masks are one bit per Group, per script.
using GroupMask = uint16_t;
constexpr GroupMask ALL_GROUPS = 0xFFFF;
constexpr GroupMask BASIC_GROUPS = 0x03FF;  // gojūon without dakuten or yōon

// Writes the entry's kana in `script` into `out` (NUL terminated). Returns out.
char* entryText(uint8_t entry, Script script, char* out, size_t outSize);

// True when `typed` (any case, surrounding spaces ignored) is an accepted
// romaji spelling of the entry.
bool romajiMatches(uint8_t entry, const char* typed);

// Entry whose kana equals `typedUtf8` in either script, or -1.
int findEntryByKana(const char* typedUtf8);

// UTF-8 helpers for the kana pad (BMP code points only).
size_t encodeUtf8(uint32_t cp, char* out);
// Next form of a kana under the ゛゜小 key (か→が→か, は→ば→ぱ→は,
// つ→っ→づ→つ, や→ゃ→や, ...). Returns cp unchanged when it has no variant.
uint32_t modifierNext(uint32_t cp);
uint32_t toScript(uint32_t hiraganaCp, Script script);

// The ten multi-tap rows of the 12-key pad (あかさたなはまやらわ), as hiragana
// code points; the row ends at the first 0.
constexpr int PAD_ROWS = 10;
constexpr int PAD_ROW_MAX = 4;
const uint32_t* padRow(int row);

}  // namespace kana
