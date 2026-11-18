#include <gtest/gtest.h>

#include <string>

#include "kana/KanaData.h"

namespace {

using kana::Script;

std::string text(const uint8_t entry, const Script script) {
  char buffer[16];
  return kana::entryText(entry, script, buffer, sizeof(buffer));
}

std::string utf8(const uint32_t cp) {
  char buffer[4];
  return std::string(buffer, kana::encodeUtf8(cp, buffer));
}

std::string modified(const char* kanaUtf8) {
  // Single-kana inputs only: decode three UTF-8 bytes.
  const auto* p = reinterpret_cast<const unsigned char*>(kanaUtf8);
  const uint32_t cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
  return utf8(kana::modifierNext(cp));
}

TEST(KanaData, GroupsCoverTheTableInOrder) {
  int next = 0;
  for (int g = 0; g < kana::GROUP_COUNT; ++g) {
    EXPECT_EQ(kana::groups()[g].first, next);
    next += kana::groups()[g].count;
  }
  EXPECT_EQ(next, static_cast<int>(kana::entryCount()));
}

TEST(KanaData, KatakanaIsDerivedFromHiragana) {
  EXPECT_EQ(text(0, Script::Hiragana), "あ");
  EXPECT_EQ(text(0, Script::Katakana), "ア");
  EXPECT_EQ(text(45, Script::Katakana), "ン");
  EXPECT_EQ(text(71, Script::Katakana), "キャ");
}

TEST(KanaData, RomajiAcceptsHepburnAndCommonAlternatives) {
  EXPECT_TRUE(kana::romajiMatches(11, "shi"));
  EXPECT_TRUE(kana::romajiMatches(11, " SI "));
  EXPECT_FALSE(kana::romajiMatches(11, "sh"));
  EXPECT_TRUE(kana::romajiMatches(57, "ji"));  // ぢ
  EXPECT_TRUE(kana::romajiMatches(57, "di"));
  EXPECT_TRUE(kana::romajiMatches(95, "jya"));  // じゃ
  EXPECT_FALSE(kana::romajiMatches(0, ""));
}

TEST(KanaData, FindsEntriesFromEitherScript) {
  EXPECT_EQ(kana::findEntryByKana("きゃ"), 71);
  EXPECT_EQ(kana::findEntryByKana("キャ"), 71);
  EXPECT_EQ(kana::findEntryByKana("ん"), 45);
  EXPECT_EQ(kana::findEntryByKana("x"), -1);
  EXPECT_EQ(kana::findEntryByKana(""), -1);
}

TEST(KanaData, ModifierKeyCyclesLikeAPhonePad) {
  EXPECT_EQ(modified("か"), "が");
  EXPECT_EQ(modified("が"), "か");
  EXPECT_EQ(modified("つ"), "っ");
  EXPECT_EQ(modified("っ"), "づ");
  EXPECT_EQ(modified("づ"), "つ");
  EXPECT_EQ(modified("は"), "ば");
  EXPECT_EQ(modified("ば"), "ぱ");
  EXPECT_EQ(modified("ぱ"), "は");
  EXPECT_EQ(modified("や"), "ゃ");
  EXPECT_EQ(modified("あ"), "ぁ");
  EXPECT_EQ(modified("ト"), "ド");
  EXPECT_EQ(modified("ホ"), "ボ");
  EXPECT_EQ(modified("ん"), "ん");
}

}  // namespace
