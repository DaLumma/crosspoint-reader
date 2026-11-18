#include "KanaQuizActivity.h"

#if CROSSPOINT_KANA_PRACTICE

#include <GfxRenderer.h>
#include <esp_random.h>

#include <cstdio>
#include <cstring>

#include "components/UIScale.h"
#include "components/UITheme.h"
#include "kana/KanaFonts.h"

namespace fui = freeink::ui;
using kana::Script;

namespace {
constexpr fui::ActionId ACTION_KEY = 1;
constexpr fui::ActionId ACTION_NEXT = 2;
constexpr fui::FontId KANA_SLOT = fui::GfxRendererTarget::FONT_TITLE;

// Romaji pad: the letters Hepburn romaji uses, in gojūon order.
constexpr uint8_t ROMAJI_ROWS = 4;
constexpr uint8_t ROMAJI_COLS = 6;
constexpr int ROMAJI_DELETE = 5;
constexpr int ROMAJI_OK = 23;
const char ROMAJI_LETTERS[] = "aiueo-kstnhmyrwgzdbpjcf-";

// 12-key kana pad, phone style. Values >= 0 are multi-tap rows (kana::padRow).
constexpr uint8_t KANA_ROWS = 4;
constexpr uint8_t KANA_COLS = 4;
constexpr int KANA_DELETE = 3;
constexpr int KANA_NEXT = 7;  // ends the multi-tap cycle so the same row can follow
constexpr int KANA_CLEAR = 11;
constexpr int KANA_MODIFIER = 12;
constexpr int KANA_OK = 15;
const int8_t KANA_PAD_ROW[KANA_ROWS * KANA_COLS] = {0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, -1, 9, -1, -1};

// Summary buttons, by id.
constexpr int SUMMARY_MISSED = 0;
constexpr int SUMMARY_NEW = 1;
constexpr int SUMMARY_DONE = 2;

// Full (HALF) refresh every so often: fast refreshes leave e-ink ghosting.
constexpr int RENDERS_PER_CLEAN = 15;

uint32_t randomBelow(const uint32_t n) { return n == 0 ? 0 : esp_random() % n; }

template <typename T>
void shuffle(std::vector<T>& items) {
  for (size_t i = items.size(); i > 1; --i) {
    const size_t j = randomBelow(static_cast<uint32_t>(i));
    const T tmp = items[i - 1];
    items[i - 1] = items[j];
    items[j] = tmp;
  }
}

const char* romajiOf(const uint8_t entry) { return kana::entries()[entry].romaji; }

bool sameRomaji(const uint8_t a, const uint8_t b) { return std::strcmp(romajiOf(a), romajiOf(b)) == 0; }

void appendf(char* buffer, const size_t size, const char* format, const char* a, const char* b) {
  const size_t len = std::strlen(buffer);
  if (len + 1 < size) snprintf(buffer + len, size - len, format, a, b);
}
}  // namespace

KanaQuizActivity::KanaQuizActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   const kana::PracticeConfig& config)
    : Activity("KanaQuiz", renderer, mappedInput), UiAppHost(renderer), config(config) {}

void KanaQuizActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  app.on(ACTION_KEY, &KanaQuizActivity::onKey, this);
  app.on(ACTION_NEXT, &KanaQuizActivity::onNext, this);
  app.setScreen(&KanaQuizActivity::quizScreen, this);
  // At most 2 x 104 kana; reserved once so rounds never reallocate.
  pool.reserve(kana::SCRIPT_COUNT * kana::entryCount());
  questions.reserve(kana::SCRIPT_COUNT * kana::entryCount());
  startRound(false);
  requestUpdate();
}

void KanaQuizActivity::onExit() {
  closeRouting();
  pool.clear();
  pool.shrink_to_fit();
  questions.clear();
  questions.shrink_to_fit();
  Activity::onExit();
}

// --- round and question flow -------------------------------------------------

void KanaQuizActivity::buildPool(const bool missedOnly) {
  pool.clear();
  for (int s = 0; s < kana::SCRIPT_COUNT; ++s) {
    for (int g = 0; g < kana::GROUP_COUNT; ++g) {
      if (!missedOnly && !(config.masks[s] & (1u << g))) continue;
      const kana::Group& group = kana::groups()[g];
      for (int e = group.first; e < group.first + group.count; ++e) {
        if (missedOnly && missed[s][e] == 0) continue;
        pool.push_back(PoolItem{static_cast<uint8_t>(e), static_cast<Script>(s)});
      }
    }
  }
}

void KanaQuizActivity::startRound(const bool missedOnly) {
  buildPool(missedOnly);
  std::memset(missed, 0, sizeof(missed));
  questions.clear();
  current = 0;
  correctCount = 0;
  wrongCount = 0;
  lastCorrect = false;
  if (pool.empty()) {
    showSummary();
    return;
  }

  const uint8_t requested = missedOnly ? 0 : kana::QUESTION_COUNTS[config.countIndex];
  const size_t total = requested == 0 ? pool.size() : requested;
  // Deal from reshuffled copies of the pool so every kana comes up before any
  // repeats; avoid the same kana twice in a row across a reshuffle.
  std::vector<PoolItem> deck(pool);
  while (questions.size() < total) {
    shuffle(deck);
    if (!questions.empty() && deck.size() > 1 && deck[0].entry == questions.back().entry &&
        deck[0].script == questions.back().script) {
      const PoolItem tmp = deck[0];
      deck[0] = deck[1];
      deck[1] = tmp;
    }
    for (const PoolItem& item : deck) {
      if (questions.size() >= total) break;
      bool kanaPrompt = config.direction == kana::Direction::KanaToRomaji;
      if (config.direction == kana::Direction::Both) kanaPrompt = randomBelow(2) == 0;
      questions.push_back(Question{item.entry, item.script, kanaPrompt});
    }
  }
  phase = Phase::Asking;
  prepareQuestion();
}

void KanaQuizActivity::prepareQuestion() {
  const Question& q = questions[current];
  if (q.kanaPrompt) {
    kana::entryText(q.entry, q.script, prompt, sizeof(prompt));
    promptHint[0] = '\0';
  } else {
    snprintf(prompt, sizeof(prompt), "%s", romajiOf(q.entry));
    snprintf(promptHint, sizeof(promptHint), "in %s", q.script == Script::Hiragana ? "hiragana" : "katakana");
  }
  if (config.answer == kana::AnswerMode::Choice) {
    pad = Pad::Choice;
  } else {
    pad = q.kanaPrompt ? Pad::Romaji : Pad::Kana;
  }
  typed[0] = '\0';
  padLen = 0;
  padLastKey = -1;
  padCycle = 0;
  selected = 0;
  buttonFocus = false;
  if (pad == Pad::Choice) buildChoices();
  buildKeys();
  snprintf(statusLeft, sizeof(statusLeft), "%d / %d", current + 1, static_cast<int>(questions.size()));
  snprintf(statusRight, sizeof(statusRight), "\xE2\x97\x8B %d   \xC3\x97 %d", correctCount, wrongCount);
}

void KanaQuizActivity::buildChoices() {
  const Question& q = questions[current];
  const int correctSlot = static_cast<int>(randomBelow(CHOICES));
  int filled = 0;
  uint8_t picked[CHOICES] = {};
  // Distractors come from the learner's selection first, then the whole table,
  // never sharing the answer's romaji (じ/ぢ, ず/づ would be ambiguous).
  const auto accept = [&](const uint8_t entry) {
    if (sameRomaji(entry, q.entry)) return false;
    for (int i = 0; i < filled; ++i) {
      if (sameRomaji(entry, picked[i])) return false;
    }
    picked[filled++] = entry;
    return true;
  };
  for (int tries = 0; filled < CHOICES - 1 && tries < 64; ++tries) {
    accept(pool[randomBelow(static_cast<uint32_t>(pool.size()))].entry);
  }
  while (filled < CHOICES - 1) {
    accept(static_cast<uint8_t>(randomBelow(static_cast<uint32_t>(kana::entryCount()))));
  }
  for (int slot = 0, d = 0; slot < CHOICES; ++slot) {
    choiceEntry[slot] = slot == correctSlot ? q.entry : picked[d++];
    if (q.kanaPrompt) {
      snprintf(choiceText[slot], sizeof(choiceText[slot]), "%s", romajiOf(choiceEntry[slot]));
    } else {
      kana::entryText(choiceEntry[slot], q.script, choiceText[slot], sizeof(choiceText[slot]));
    }
  }
}

void KanaQuizActivity::buildKeys() {
  for (auto& key : keys) key = fui::KeyGridKey{};
  const Script script = questions.empty() ? Script::Hiragana : questions[current].script;
  if (pad == Pad::Choice) {
    keyRows = 2;
    keyCols = 2;
    for (int i = 0; i < CHOICES; ++i) {
      keys[i].label = choiceText[i];
      keys[i].value = static_cast<int16_t>(i);
    }
  } else if (pad == Pad::Romaji) {
    keyRows = ROMAJI_ROWS;
    keyCols = ROMAJI_COLS;
    for (int i = 0; i < ROMAJI_ROWS * ROMAJI_COLS; ++i) {
      keyLabels[i][0] = ROMAJI_LETTERS[i];
      keyLabels[i][1] = '\0';
      keys[i].label = keyLabels[i];
      keys[i].value = static_cast<int16_t>(i);
    }
    keys[ROMAJI_DELETE].kind = fui::KeyKind::Delete;
    keys[ROMAJI_OK].label = "OK";
    keys[ROMAJI_OK].kind = fui::KeyKind::Ok;
  } else {
    keyRows = KANA_ROWS;
    keyCols = KANA_COLS;
    for (int i = 0; i < KANA_ROWS * KANA_COLS; ++i) {
      keys[i].value = static_cast<int16_t>(i);
      if (KANA_PAD_ROW[i] >= 0) {
        const size_t n = kana::encodeUtf8(kana::toScript(kana::padRow(KANA_PAD_ROW[i])[0], script), keyLabels[i]);
        keyLabels[i][n] = '\0';
        keys[i].label = keyLabels[i];
      }
    }
    keys[KANA_DELETE].kind = fui::KeyKind::Delete;
    keys[KANA_NEXT].label = "\xE2\x86\x92";               // →
    keys[KANA_CLEAR].label = "C";
    keys[KANA_MODIFIER].label = "\xE3\x82\x9B\xE3\x82\x9C\xE5\xB0\x8F";  // ゛゜小
    keys[14].kind = fui::KeyKind::Disabled;
    keys[KANA_OK].label = "OK";
    keys[KANA_OK].kind = fui::KeyKind::Ok;
  }
}

void KanaQuizActivity::updateTypedText() {
  size_t len = 0;
  for (int i = 0; i < padLen && len + 4 < sizeof(typed); ++i) len += kana::encodeUtf8(padText[i], typed + len);
  typed[len] = '\0';
}

void KanaQuizActivity::handleKey(const int index) {
  if (phase != Phase::Asking) return;
  if (pad == Pad::Choice) {
    if (index < 0 || index >= CHOICES) return;
    selected = index;
    submitAnswer();
    return;
  }
  if (pad == Pad::Kana) {
    handleKanaPadKey(index);
    return;
  }
  if (index == ROMAJI_OK) {
    submitAnswer();
    return;
  }
  const size_t len = std::strlen(typed);
  if (index == ROMAJI_DELETE) {
    if (len > 0) typed[len - 1] = '\0';
  } else if (index >= 0 && index < ROMAJI_ROWS * ROMAJI_COLS && len + 1 < 8) {
    typed[len] = ROMAJI_LETTERS[index];
    typed[len + 1] = '\0';
  }
  requestUpdate();
}

void KanaQuizActivity::handleKanaPadKey(const int index) {
  if (index < 0 || index >= KANA_ROWS * KANA_COLS) return;
  const Script script = questions[current].script;
  switch (index) {
    case KANA_OK:
      submitAnswer();
      return;
    case KANA_DELETE:
      if (padLen > 0) --padLen;
      padLastKey = -1;
      break;
    case KANA_CLEAR:
      padLen = 0;
      padLastKey = -1;
      break;
    case KANA_NEXT:
      padLastKey = -1;
      break;
    case KANA_MODIFIER:
      if (padLen > 0) padText[padLen - 1] = kana::modifierNext(padText[padLen - 1]);
      padLastKey = -1;
      break;
    default: {
      const int row = KANA_PAD_ROW[index];
      if (row < 0) return;
      const uint32_t* cycle = kana::padRow(row);
      int cycleLen = 0;
      while (cycleLen <= kana::PAD_ROW_MAX && cycle[cycleLen] != 0) ++cycleLen;
      if (padLastKey == index && padLen > 0) {
        // Same key again: step to the next kana in its row, like a phone pad.
        padCycle = (padCycle + 1) % cycleLen;
        padText[padLen - 1] = kana::toScript(cycle[padCycle], script);
      } else if (padLen < PAD_TEXT_MAX) {
        padCycle = 0;
        padText[padLen++] = kana::toScript(cycle[0], script);
        padLastKey = index;
      }
      break;
    }
  }
  updateTypedText();
  requestUpdate();
}

void KanaQuizActivity::submitAnswer() {
  const Question& q = questions[current];
  bool correct = false;
  const char* answered = typed;
  if (pad == Pad::Choice) {
    correct = sameRomaji(choiceEntry[selected], q.entry);
    answered = choiceText[selected];
  } else if (pad == Pad::Romaji) {
    if (typed[0] == '\0') return;
    correct = kana::romajiMatches(q.entry, typed);
  } else {
    if (padLen == 0) return;
    const int entry = kana::findEntryByKana(typed);
    correct = entry >= 0 && sameRomaji(static_cast<uint8_t>(entry), q.entry);
  }

  char kanaText[16];
  kana::entryText(q.entry, q.script, kanaText, sizeof(kanaText));
  lastCorrect = correct;
  if (correct) {
    ++correctCount;
    snprintf(resultLine, sizeof(resultLine), "\xE2\x97\x8B %s = %s", kanaText, romajiOf(q.entry));
    advance();
    return;
  }
  ++wrongCount;
  if (missed[static_cast<int>(q.script)][q.entry] < 255) ++missed[static_cast<int>(q.script)][q.entry];
  snprintf(resultLine, sizeof(resultLine), "\xC3\x97  %s = %s", kanaText, romajiOf(q.entry));
  snprintf(yourAnswerLine, sizeof(yourAnswerLine), "You answered: %s", answered);
  snprintf(statusRight, sizeof(statusRight), "\xE2\x97\x8B %d   \xC3\x97 %d", correctCount, wrongCount);
  phase = Phase::Feedback;
  requestUpdate();
}

void KanaQuizActivity::advance() {
  if (current + 1 >= static_cast<int>(questions.size())) {
    showSummary();
    return;
  }
  ++current;
  phase = Phase::Asking;
  prepareQuestion();
  requestUpdate();
}

void KanaQuizActivity::showSummary() {
  phase = Phase::Summary;
  selected = 0;
  const int answered = correctCount + wrongCount;
  if (answered == 0) {
    snprintf(summaryScore, sizeof(summaryScore), "No questions answered");
  } else {
    snprintf(summaryScore, sizeof(summaryScore), "%d of %d right (%d%%)", correctCount, answered,
             correctCount * 100 / answered);
  }
  summaryMissed[0] = '\0';
  int missedKana = 0;
  for (int s = 0; s < kana::SCRIPT_COUNT; ++s) {
    for (size_t e = 0; e < kana::entryCount(); ++e) {
      if (missed[s][e] == 0) continue;
      ++missedKana;
      char kanaText[16];
      kana::entryText(static_cast<uint8_t>(e), static_cast<Script>(s), kanaText, sizeof(kanaText));
      appendf(summaryMissed, sizeof(summaryMissed), "%s %s   ", kanaText, romajiOf(static_cast<uint8_t>(e)));
    }
  }
  snprintf(summaryMissedButton, sizeof(summaryMissedButton), "Practice missed (%d)", missedKana);
  requestUpdate();
}

int KanaQuizActivity::summaryButtonCount() const { return summaryMissed[0] ? SUMMARY_BUTTONS : SUMMARY_BUTTONS - 1; }

void KanaQuizActivity::activateSummaryButton(int index) {
  // Without missed kana the first button is hidden; ids shift by one.
  if (!summaryMissed[0]) ++index;
  app.clearTapFlash();
  switch (index) {
    case SUMMARY_MISSED:
      startRound(true);
      break;
    case SUMMARY_NEW:
      startRound(false);
      break;
    case SUMMARY_DONE:
      finish();
      return;
    default:
      return;
  }
  requestUpdate();
}

void KanaQuizActivity::moveSelection(const int delta) {
  int count = 0;
  if (phase == Phase::Summary) {
    count = summaryButtonCount();
  } else if (phase == Phase::Asking && pad == Pad::Choice) {
    count = CHOICES;
  }
  if (count == 0) return;
  if (phase == Phase::Asking && !buttonFocus) {
    buttonFocus = true;  // the first press only reveals the focus
  } else {
    selected = (selected + delta + count) % count;
  }
  requestUpdate();
}

// --- input -------------------------------------------------------------------

void KanaQuizActivity::onKey(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<KanaQuizActivity*>(user);
  RenderLock lock(*self);
  self->handleKey(event.value);
}

void KanaQuizActivity::onNext(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<KanaQuizActivity*>(user);
  RenderLock lock(*self);
  if (self->phase == Phase::Feedback) {
    self->advance();
  } else if (self->phase == Phase::Summary) {
    self->activateSummaryButton(event.value);
  }
}

void KanaQuizActivity::loop() {
  const auto route = routeTouch(mappedInput);
  if (route.routed && app.invalidated()) requestUpdate();
  if (route) return;

  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    RenderLock lock(*this);
    if (phase == Phase::Summary || correctCount + wrongCount == 0) {
      finish();
    } else {
      showSummary();
    }
    return;
  }
  if (mappedInput.wasReleased(Button::Confirm)) {
    RenderLock lock(*this);
    if (phase == Phase::Feedback) {
      advance();
    } else if (phase == Phase::Summary) {
      activateSummaryButton(selected);
    } else if (pad == Pad::Choice && !buttonFocus) {
      buttonFocus = true;  // first press shows the focus; the next one answers
      requestUpdate();
    } else {
      submitAnswer();
    }
    return;
  }
  if (mappedInput.wasPressed(Button::Left) || mappedInput.wasPressed(Button::Up)) {
    RenderLock lock(*this);
    moveSelection(-1);
  } else if (mappedInput.wasPressed(Button::Right) || mappedInput.wasPressed(Button::Down)) {
    RenderLock lock(*this);
    moveSelection(1);
  }
}

// --- rendering -----------------------------------------------------------------

void KanaQuizActivity::quizScreen(UiScreen& screen, void* user) {
  static_cast<KanaQuizActivity*>(user)->buildQuizScreen(screen);
}

void KanaQuizActivity::drawStatus(UiScreen& screen, const fui::Rect rect) {
  fui::TextStyle left{KANA_SLOT, fui::TextAlign::Left};
  fui::TextStyle right{KANA_SLOT, fui::TextAlign::Right};
  if (phase != Phase::Summary) screen.target().text(rect, statusLeft, left);
  screen.target().text(rect, statusRight, right);
}

void KanaQuizActivity::drawPrompt(UiScreen& screen, const fui::Rect rect) {
  const int16_t hintH = promptHint[0] ? uiTarget.lineHeight(KANA_SLOT) : 0;
  uiTarget.setFont(KANA_SLOT, KANA_44_FONT_ID);
  const fui::Rect big{rect.x, rect.y, rect.width, static_cast<int16_t>(rect.height - hintH)};
  screen.target().text(big, prompt, fui::TextStyle{KANA_SLOT, fui::TextAlign::Center});
  uiTarget.setFont(KANA_SLOT, KANA_20_FONT_ID);
  if (hintH > 0) {
    const fui::Rect hint{rect.x, static_cast<int16_t>(rect.bottom() - hintH), rect.width, hintH};
    screen.target().text(hint, promptHint, fui::TextStyle{KANA_SLOT, fui::TextAlign::Center});
  }
}

void KanaQuizActivity::drawSummary(UiScreen& screen, fui::Rect) {
  const int16_t lh = uiTarget.lineHeight(KANA_SLOT);
  const fui::TextStyle center{KANA_SLOT, fui::TextAlign::Center};
  screen.target().text(screen.takeTop(lh, screen.theme().spaceLg), "Round complete", center);
  screen.target().text(screen.takeTop(lh, screen.theme().spaceLg), summaryScore, center);

  // Buttons stack from the bottom; the missed list takes what remains.
  const int16_t buttonH = static_cast<int16_t>(lh + 28);
  const int count = summaryButtonCount();
  const char* labels[SUMMARY_BUTTONS] = {summaryMissedButton, "New round", "Back to setup"};
  const int firstId = summaryMissed[0] ? SUMMARY_MISSED : SUMMARY_NEW;
  for (int i = count - 1; i >= 0; --i) {
    fui::ButtonProps props;
    props.label = labels[firstId + i];
    props.action = ACTION_NEXT;
    props.value = static_cast<int16_t>(i);
    props.text = center;
    props.radius = 6;
    if (i == selected) props.state = fui::StateSelected;
    screen.button(props, screen.takeBottom(buttonH, screen.theme().spaceMd));
  }

  if (summaryMissed[0]) {
    screen.target().text(screen.takeTop(lh, screen.theme().spaceSm), "Missed:",
                         fui::TextStyle{KANA_SLOT, fui::TextAlign::Left});
    const fui::Rect list = screen.body();
    fui::TextStyle wrap{KANA_SLOT, fui::TextAlign::Left};
    wrap.maxLines = static_cast<uint8_t>(list.height / (lh > 0 ? lh : 1));
    screen.target().text(list, summaryMissed, wrap);
  } else if (correctCount > 0) {
    screen.target().text(screen.body(), "No mistakes!", center);
  }
}

void KanaQuizActivity::buildQuizScreen(UiScreen& screen) {
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, false, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y), static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  const int16_t pad16 = screen.theme().spaceLg;
  screen.insetContent(fui::Insets{pad16, pad16, pad16, pad16});

  // The title slot carries the kana fonts on this screen only.
  uiTarget.setFont(KANA_SLOT, KANA_20_FONT_ID);
  const int16_t lh = uiTarget.lineHeight(KANA_SLOT);
  const fui::TextStyle center{KANA_SLOT, fui::TextAlign::Center};

  drawStatus(screen, screen.takeTop(lh, screen.theme().spaceMd));
  if (phase == Phase::Summary) {
    drawSummary(screen, screen.body());
    uiTarget.setFont(KANA_SLOT, uiScaleSpec().titleFontId);
    return;
  }

  // Banner: the previous answer when it was right (wrong ones get a panel).
  const fui::Rect banner = screen.takeTop(lh, screen.theme().spaceMd);
  if (phase == Phase::Asking && lastCorrect && current > 0) screen.target().text(banner, resultLine, center);

  const int16_t bodyH = screen.body().height;
  drawPrompt(screen, screen.takeTop(static_cast<int16_t>(bodyH * (pad == Pad::Choice ? 32 : 24) / 100),
                                    screen.theme().spaceLg));

  if (phase == Phase::Feedback) {
    screen.target().text(screen.takeTop(lh, screen.theme().spaceMd), resultLine, center);
    screen.target().text(screen.takeTop(lh, screen.theme().spaceMd), yourAnswerLine, center);
    fui::ButtonProps next;
    next.label = "Next";
    next.action = ACTION_NEXT;
    next.text = center;
    next.radius = 6;
    next.state = fui::StateSelected;
    screen.button(next, screen.takeBottom(static_cast<int16_t>(lh * 3), 0));
    uiTarget.setFont(KANA_SLOT, uiScaleSpec().titleFontId);
    return;
  }

  if (pad != Pad::Choice) {
    const fui::Rect field = screen.takeTop(static_cast<int16_t>(lh + 20), screen.theme().spaceLg);
    screen.target().stroke(field, fui::Paint::solid(fui::Color::Black), 2, 6);
    char shown[sizeof(typed) + 2];
    snprintf(shown, sizeof(shown), "%s_", typed);
    screen.target().text(field, shown, center);
  }

  fui::KeyGridProps props;
  props.keys = keys;
  props.rows = keyRows;
  props.cols = keyCols;
  props.action = ACTION_KEY;
  props.labelText = center;
  props.gap = static_cast<int16_t>(pad == Pad::Choice ? screen.theme().spaceLg : screen.theme().spaceMd);
  props.radius = 6;
  if (pad == Pad::Choice) {
    props.selectedIndex = static_cast<int16_t>(buttonFocus ? selected : -1);
    uiTarget.setFont(KANA_SLOT, KANA_44_FONT_ID);
  }
  fui::keyGrid(screen.frame(), screen.body(), props);
  uiTarget.setFont(KANA_SLOT, uiScaleSpec().titleFontId);
}

void KanaQuizActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  if (++rendersSinceClean >= RENDERS_PER_CLEAN) {
    rendersSinceClean = 0;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer();
  }
}

#endif  // CROSSPOINT_KANA_PRACTICE
