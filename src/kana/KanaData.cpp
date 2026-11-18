#include "KanaData.h"

#if CROSSPOINT_KANA_PRACTICE

#include <cctype>
#include <cstring>

namespace kana {
namespace {

// Order matters: Group::first/count index into this table.
const Entry ENTRIES[] = {
    // 0: vowels
    {"あ", "a", nullptr},
    {"い", "i", nullptr},
    {"う", "u", nullptr},
    {"え", "e", nullptr},
    {"お", "o", nullptr},
    // 5: k
    {"か", "ka", nullptr},
    {"き", "ki", nullptr},
    {"く", "ku", nullptr},
    {"け", "ke", nullptr},
    {"こ", "ko", nullptr},
    // 10: s
    {"さ", "sa", nullptr},
    {"し", "shi", "si"},
    {"す", "su", nullptr},
    {"せ", "se", nullptr},
    {"そ", "so", nullptr},
    // 15: t
    {"た", "ta", nullptr},
    {"ち", "chi", "ti"},
    {"つ", "tsu", "tu"},
    {"て", "te", nullptr},
    {"と", "to", nullptr},
    // 20: n
    {"な", "na", nullptr},
    {"に", "ni", nullptr},
    {"ぬ", "nu", nullptr},
    {"ね", "ne", nullptr},
    {"の", "no", nullptr},
    // 25: h
    {"は", "ha", nullptr},
    {"ひ", "hi", nullptr},
    {"ふ", "fu", "hu"},
    {"へ", "he", nullptr},
    {"ほ", "ho", nullptr},
    // 30: m
    {"ま", "ma", nullptr},
    {"み", "mi", nullptr},
    {"む", "mu", nullptr},
    {"め", "me", nullptr},
    {"も", "mo", nullptr},
    // 35: y
    {"や", "ya", nullptr},
    {"ゆ", "yu", nullptr},
    {"よ", "yo", nullptr},
    // 38: r
    {"ら", "ra", nullptr},
    {"り", "ri", nullptr},
    {"る", "ru", nullptr},
    {"れ", "re", nullptr},
    {"ろ", "ro", nullptr},
    // 43: w + n
    {"わ", "wa", nullptr},
    {"を", "wo", "o"},
    {"ん", "n", "nn"},
    // 46: g
    {"が", "ga", nullptr},
    {"ぎ", "gi", nullptr},
    {"ぐ", "gu", nullptr},
    {"げ", "ge", nullptr},
    {"ご", "go", nullptr},
    // 51: z
    {"ざ", "za", nullptr},
    {"じ", "ji", "zi"},
    {"ず", "zu", nullptr},
    {"ぜ", "ze", nullptr},
    {"ぞ", "zo", nullptr},
    // 56: d
    {"だ", "da", nullptr},
    {"ぢ", "ji", "di zi"},
    {"づ", "zu", "du"},
    {"で", "de", nullptr},
    {"ど", "do", nullptr},
    // 61: b
    {"ば", "ba", nullptr},
    {"び", "bi", nullptr},
    {"ぶ", "bu", nullptr},
    {"べ", "be", nullptr},
    {"ぼ", "bo", nullptr},
    // 66: p
    {"ぱ", "pa", nullptr},
    {"ぴ", "pi", nullptr},
    {"ぷ", "pu", nullptr},
    {"ぺ", "pe", nullptr},
    {"ぽ", "po", nullptr},
    // 71: yōon
    {"きゃ", "kya", nullptr},
    {"きゅ", "kyu", nullptr},
    {"きょ", "kyo", nullptr},
    {"しゃ", "sha", "sya"},
    {"しゅ", "shu", "syu"},
    {"しょ", "sho", "syo"},
    {"ちゃ", "cha", "tya cya"},
    {"ちゅ", "chu", "tyu cyu"},
    {"ちょ", "cho", "tyo cyo"},
    {"にゃ", "nya", nullptr},
    {"にゅ", "nyu", nullptr},
    {"にょ", "nyo", nullptr},
    {"ひゃ", "hya", nullptr},
    {"ひゅ", "hyu", nullptr},
    {"ひょ", "hyo", nullptr},
    {"みゃ", "mya", nullptr},
    {"みゅ", "myu", nullptr},
    {"みょ", "myo", nullptr},
    {"りゃ", "rya", nullptr},
    {"りゅ", "ryu", nullptr},
    {"りょ", "ryo", nullptr},
    {"ぎゃ", "gya", nullptr},
    {"ぎゅ", "gyu", nullptr},
    {"ぎょ", "gyo", nullptr},
    {"じゃ", "ja", "jya zya"},
    {"じゅ", "ju", "jyu zyu"},
    {"じょ", "jo", "jyo zyo"},
    {"びゃ", "bya", nullptr},
    {"びゅ", "byu", nullptr},
    {"びょ", "byo", nullptr},
    {"ぴゃ", "pya", nullptr},
    {"ぴゅ", "pyu", nullptr},
    {"ぴょ", "pyo", nullptr},
};
constexpr size_t ENTRY_COUNT = sizeof(ENTRIES) / sizeof(ENTRIES[0]);
static_assert(ENTRY_COUNT == 104, "group table below assumes 104 entries");

const Group GROUPS[GROUP_COUNT] = {
    {"Vowels", "a i u e o", 0, 5},
    {"K", "ka ki ku ke ko", 5, 5},
    {"S", "sa shi su se so", 10, 5},
    {"T", "ta chi tsu te to", 15, 5},
    {"N", "na ni nu ne no", 20, 5},
    {"H", "ha hi fu he ho", 25, 5},
    {"M", "ma mi mu me mo", 30, 5},
    {"Y", "ya yu yo", 35, 3},
    {"R", "ra ri ru re ro", 38, 5},
    {"W + n", "wa wo n", 43, 3},
    {"G (dakuten)", "ga gi gu ge go", 46, 5},
    {"Z (dakuten)", "za ji zu ze zo", 51, 5},
    {"D (dakuten)", "da ji zu de do", 56, 5},
    {"B (dakuten)", "ba bi bu be bo", 61, 5},
    {"P (handakuten)", "pa pi pu pe po", 66, 5},
    {"Combinations", "kya shu cho ...", 71, 33},
};

const uint32_t PAD[PAD_ROWS][PAD_ROW_MAX + 1] = {
    {0x3042, 0x3044, 0x3046, 0x3048, 0x304A},  // あいうえお
    {0x304B, 0x304D, 0x304F, 0x3051, 0x3053},  // かきくけこ
    {0x3055, 0x3057, 0x3059, 0x305B, 0x305D},  // さしすせそ
    {0x305F, 0x3061, 0x3064, 0x3066, 0x3068},  // たちつてと
    {0x306A, 0x306B, 0x306C, 0x306D, 0x306E},  // なにぬねの
    {0x306F, 0x3072, 0x3075, 0x3078, 0x307B},  // はひふへほ
    {0x307E, 0x307F, 0x3080, 0x3081, 0x3082},  // まみむめも
    {0x3084, 0x3086, 0x3088, 0, 0},            // やゆよ
    {0x3089, 0x308A, 0x308B, 0x308C, 0x308D},  // らりるれろ
    {0x308F, 0x3092, 0x3093, 0x30FC, 0},       // わをんー
};

constexpr uint32_t HIRA_FIRST = 0x3041;
constexpr uint32_t HIRA_LAST = 0x3096;
constexpr uint32_t KATA_OFFSET = 0x60;

uint32_t decodeUtf8(const unsigned char*& p) {
  const unsigned char c = *p;
  if (c < 0x80) {
    ++p;
    return c;
  }
  if ((c & 0xE0) == 0xC0 && p[1]) {
    const uint32_t cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);
    p += 2;
    return cp;
  }
  if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
    const uint32_t cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    p += 3;
    return cp;
  }
  ++p;  // malformed or outside the BMP: skip a byte
  return 0xFFFD;
}

uint32_t toHiragana(const uint32_t cp) {
  if (cp >= HIRA_FIRST + KATA_OFFSET && cp <= HIRA_LAST + KATA_OFFSET) return cp - KATA_OFFSET;
  return cp;
}

// Range of two-form pairs (first form, second form, ...): returns the other form.
bool flipPair(const uint32_t cp, const uint32_t first, const uint32_t last, uint32_t& out) {
  if (cp < first || cp > last) return false;
  out = ((cp - first) % 2 == 0) ? cp + 1 : cp - 1;
  return true;
}

// Compares a candidate spelling against `typed` (already trimmed/lowercased).
bool sameWord(const char* a, size_t aLen, const char* b) { return std::strlen(b) == aLen && std::strncmp(a, b, aLen) == 0; }

}  // namespace

const Entry* entries() { return ENTRIES; }
size_t entryCount() { return ENTRY_COUNT; }
const Group* groups() { return GROUPS; }
const uint32_t* padRow(const int row) { return PAD[row]; }

size_t encodeUtf8(const uint32_t cp, char* out) {
  if (cp < 0x80) {
    out[0] = static_cast<char>(cp);
    return 1;
  }
  if (cp < 0x800) {
    out[0] = static_cast<char>(0xC0 | (cp >> 6));
    out[1] = static_cast<char>(0x80 | (cp & 0x3F));
    return 2;
  }
  out[0] = static_cast<char>(0xE0 | (cp >> 12));
  out[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
  out[2] = static_cast<char>(0x80 | (cp & 0x3F));
  return 3;
}

uint32_t toScript(const uint32_t hiraganaCp, const Script script) {
  if (script == Script::Katakana && hiraganaCp >= HIRA_FIRST && hiraganaCp <= HIRA_LAST) {
    return hiraganaCp + KATA_OFFSET;
  }
  return hiraganaCp;
}

char* entryText(const uint8_t entry, const Script script, char* out, const size_t outSize) {
  size_t len = 0;
  if (entry < ENTRY_COUNT) {
    const auto* p = reinterpret_cast<const unsigned char*>(ENTRIES[entry].hiragana);
    while (*p && len + 4 < outSize) len += encodeUtf8(toScript(decodeUtf8(p), script), out + len);
  }
  if (outSize > 0) out[len < outSize ? len : outSize - 1] = '\0';
  return out;
}

bool romajiMatches(const uint8_t entry, const char* typed) {
  if (entry >= ENTRY_COUNT || !typed) return false;
  char norm[16];
  size_t len = 0;
  for (const char* p = typed; *p && len + 1 < sizeof(norm); ++p) {
    if (*p == ' ') continue;
    norm[len++] = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
  }
  norm[len] = '\0';
  if (len == 0) return false;
  const Entry& e = ENTRIES[entry];
  if (sameWord(norm, len, e.romaji)) return true;
  if (!e.alts) return false;
  const char* word = e.alts;
  while (*word) {
    const char* end = word;
    while (*end && *end != ' ') ++end;
    if (static_cast<size_t>(end - word) == len && std::strncmp(word, norm, len) == 0) return true;
    word = *end ? end + 1 : end;
  }
  return false;
}

int findEntryByKana(const char* typedUtf8) {
  if (!typedUtf8 || !*typedUtf8) return -1;
  char hira[16];
  size_t len = 0;
  const auto* p = reinterpret_cast<const unsigned char*>(typedUtf8);
  while (*p && len + 4 < sizeof(hira)) len += encodeUtf8(toHiragana(decodeUtf8(p)), hira + len);
  hira[len] = '\0';
  for (size_t i = 0; i < ENTRY_COUNT; ++i) {
    if (std::strcmp(ENTRIES[i].hiragana, hira) == 0) return static_cast<int>(i);
  }
  return -1;
}

uint32_t modifierNext(const uint32_t cp) {
  const bool kata = cp >= HIRA_FIRST + KATA_OFFSET && cp <= HIRA_LAST + KATA_OFFSET;
  const uint32_t h = kata ? cp - KATA_OFFSET : cp;
  uint32_t out = h;
  if (h == 0x3064) {
    out = 0x3063;  // つ → っ
  } else if (h == 0x3063) {
    out = 0x3065;  // っ → づ
  } else if (h == 0x3065) {
    out = 0x3064;  // づ → つ
  } else if (h >= 0x306F && h <= 0x307D) {
    // は ば ぱ triples
    const uint32_t base = 0x306F + ((h - 0x306F) / 3) * 3;
    out = base + ((h - base + 1) % 3);
  } else {
    // Each call leaves `out` untouched when h is outside its range.
    (void)(flipPair(h, 0x3041, 0x304A, out) ||  // ぁ あ ぃ い ...
           flipPair(h, 0x304B, 0x3062, out) ||  // か が ... ち ぢ
           flipPair(h, 0x3066, 0x3069, out) ||  // て で と ど
           flipPair(h, 0x3083, 0x3088, out) ||  // ゃ や ゅ ゆ ょ よ
           flipPair(h, 0x308E, 0x308F, out));   // ゎ わ
  }
  return kata ? out + KATA_OFFSET : out;
}

}  // namespace kana

#endif  // CROSSPOINT_KANA_PRACTICE
