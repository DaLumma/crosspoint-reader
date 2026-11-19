#include "KanaChartActivity.h"

#if CROSSPOINT_KANA_PRACTICE

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "components/UIScale.h"
#include "components/UITheme.h"
#include "kana/KanaFonts.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_TAB = 1;
constexpr fui::ActionId ACTION_PAGE = 2;  // value -1 / +1
constexpr fui::FontId KANA_SLOT = fui::GfxRendererTarget::FONT_TITLE;
constexpr fui::FontId ROMAJI_SLOT = fui::GfxRendererTarget::FONT_BODY;
constexpr int8_t _ = -1;  // empty cell

struct ChartPage {
  const char* title;
  uint8_t cols;
  uint8_t rows;
  const int8_t* cells;  // kana::entries() indices, row-major; -1 = empty
};

// Traditional gojūon layout: や and わ rows keep their gaps.
const int8_t BASIC[] = {
    0,  1,  2,  3,  4,   //
    5,  6,  7,  8,  9,   //
    10, 11, 12, 13, 14,  //
    15, 16, 17, 18, 19,  //
    20, 21, 22, 23, 24,  //
    25, 26, 27, 28, 29,  //
    30, 31, 32, 33, 34,  //
    35, _,  36, _,  37,  //
    38, 39, 40, 41, 42,  //
    43, _,  _,  _,  44,  //
    45, _,  _,  _,  _,   //
};
const int8_t DAKUTEN[] = {
    46, 47, 48, 49, 50,  //
    51, 52, 53, 54, 55,  //
    56, 57, 58, 59, 60,  //
    61, 62, 63, 64, 65,  //
    66, 67, 68, 69, 70,  //
};
const int8_t COMBINATIONS[] = {
    71, 72,  73,  74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
    88, 89,  90,  91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103,
};

const ChartPage PAGES[] = {
    {"Basic", 5, 11, BASIC},
    {"Dakuten", 5, 5, DAKUTEN},
    {"Combinations", 3, 11, COMBINATIONS},
};
constexpr int PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);
static_assert(sizeof(BASIC) == 5 * 11, "BASIC is 5 x 11");
static_assert(sizeof(DAKUTEN) == 5 * 5, "DAKUTEN is 5 x 5");
static_assert(sizeof(COMBINATIONS) == 3 * 11, "COMBINATIONS is 3 x 11");

const char* const TAB_LABELS[kana::SCRIPT_COUNT] = {"Hiragana", "Katakana"};
}  // namespace

KanaChartActivity::KanaChartActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("KanaChart", renderer, mappedInput), UiAppHost(renderer) {}

void KanaChartActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  app.on(ACTION_TAB, &KanaChartActivity::onTab, this);
  app.on(ACTION_PAGE, &KanaChartActivity::onPage, this);
  app.setScreen(&KanaChartActivity::chartScreen, this);
  setScript(0);
}

void KanaChartActivity::setScript(const int value) {
  script = (value + kana::SCRIPT_COUNT) % kana::SCRIPT_COUNT;
  for (int i = 0; i < kana::SCRIPT_COUNT; ++i) {
    tabs[i] = fui::TabItem{};
    tabs[i].label = TAB_LABELS[i];
    tabs[i].value = static_cast<int16_t>(i);
    tabs[i].selected = i == script;
  }
  requestUpdate();
}

void KanaChartActivity::turnPage(const int delta) {
  page = (page + delta + PAGE_COUNT) % PAGE_COUNT;
  requestUpdate();
}

void KanaChartActivity::onTab(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<KanaChartActivity*>(user);
  RenderLock lock(*self);
  self->setScript(event.value);
}

void KanaChartActivity::onPage(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<KanaChartActivity*>(user);
  RenderLock lock(*self);
  self->turnPage(event.value < 0 ? -1 : 1);
}

void KanaChartActivity::loop() {
  const auto route = routeTouch(mappedInput);
  if (route.routed && app.invalidated()) requestUpdate();
  if (route) return;

  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  RenderLock lock(*this);
  if (mappedInput.wasPressed(Button::Left) || mappedInput.wasPressed(Button::Right)) {
    setScript(script + 1);  // two tabs: either direction switches
  } else if (mappedInput.wasPressed(Button::Up)) {
    turnPage(-1);
  } else if (mappedInput.wasPressed(Button::Down) || mappedInput.wasReleased(Button::Confirm)) {
    turnPage(1);
  } else {
    const auto swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Left) turnPage(1);
    if (swipe == MappedInputManager::SwipeDir::Right) turnPage(-1);
  }
}

void KanaChartActivity::chartScreen(UiScreen& screen, void* user) {
  static_cast<KanaChartActivity*>(user)->buildChartScreen(screen);
}

void KanaChartActivity::buildChartScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  const int16_t side = static_cast<int16_t>(metrics.contentSidePadding);
  screen.insetContent(fui::Insets{0, side, 0, side});
  const auto ink = fui::Paint::solid(fui::Color::Black);

  // Tabs: the selected one is a filled pill.
  fui::TabBarProps tabProps;
  tabProps.tabs = tabs;
  tabProps.count = kana::SCRIPT_COUNT;
  tabProps.action = ACTION_TAB;
  tabProps.inputMask = fui::InputTouch;
  tabProps.text = screen.theme().bodyText;
  fui::StyleSet tabStyles;
  tabStyles.explicitlySet = true;
  tabStyles.normal.foreground = ink;
  tabStyles.selected.background = ink;
  tabStyles.selected.foreground = fui::Paint::solid(fui::Color::White);
  tabStyles.selected.radius = screen.theme().listRowRadius;
  tabStyles.focused = tabStyles.selected;
  tabStyles.active = tabStyles.selected;
  tabProps.tabStyles = tabStyles;
  const int16_t bodyLh = screen.target().lineHeight(fui::GfxRendererTarget::FONT_BODY);
  fui::tabBar(screen.frame(), screen.takeTop(static_cast<int16_t>(bodyLh + 24), screen.theme().spaceMd), tabProps);

  // Pager at the bottom: [<]  Basic (1/3)  [>]
  const ChartPage& p = PAGES[page];
  snprintf(pageLabel, sizeof(pageLabel), "%s (%d/%d)", p.title, page + 1, PAGE_COUNT);
  const fui::Rect pager = screen.takeBottom(static_cast<int16_t>(bodyLh + 24), screen.theme().spaceMd);
  const int16_t arrowW = static_cast<int16_t>(pager.height * 3 / 2);
  for (int dir = -1; dir <= 1; dir += 2) {
    fui::ButtonProps b;
    b.label = dir < 0 ? "<" : ">";
    b.action = ACTION_PAGE;
    b.value = static_cast<int16_t>(dir);
    b.text = fui::TextStyle{fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Center};
    b.radius = 6;
    b.styles = fui::defaultKeyStyles();
    const int16_t x = dir < 0 ? pager.x : static_cast<int16_t>(pager.right() - arrowW);
    screen.button(b, fui::Rect{x, pager.y, arrowW, pager.height});
  }
  screen.target().text(pager, pageLabel, fui::TextStyle{fui::GfxRendererTarget::FONT_BODY, fui::TextAlign::Center});

  // Grid: kana (kana font) on the left of each cell, romaji (UI font) on the right.
  uiTarget.setFont(KANA_SLOT, KANA_20_FONT_ID);
  const fui::Rect grid = screen.body();
  const int16_t cellW = static_cast<int16_t>(grid.width / p.cols);
  const int16_t cellH = static_cast<int16_t>(grid.height / p.rows);
  const auto kanaScript = static_cast<kana::Script>(script);
  const int16_t kanaLh = uiTarget.lineHeight(KANA_SLOT);
  const int16_t romajiLh = uiTarget.lineHeight(ROMAJI_SLOT);
  const int16_t kanaAscent = static_cast<int16_t>(renderer.getFontAscenderSize(KANA_20_FONT_ID));
  const int16_t romajiAscent = static_cast<int16_t>(renderer.getFontAscenderSize(uiScaleSpec().bodyFontId));
  for (int r = 0; r < p.rows; ++r) {
    const int16_t y = static_cast<int16_t>(grid.y + r * cellH);
    if (r > 0) screen.target().fill(fui::Rect{grid.x, y, grid.width, 1}, fui::Paint::dither(fui::Color::DarkGray));
    for (int c = 0; c < p.cols; ++c) {
      const int entry = p.cells[r * p.cols + c];
      if (entry < 0) continue;
      const int16_t x = static_cast<int16_t>(grid.x + c * cellW);
      char text[16];
      kana::entryText(static_cast<uint8_t>(entry), kanaScript, text, sizeof(text));
      // Combinations are two kana wide; give the kana half the cell there.
      const int16_t kanaW = static_cast<int16_t>(p.cols <= 3 ? cellW * 3 / 5 : cellW / 2);
      // Kana centered on the row's midline by its ink (its line box is taller
      // than a row).
      const int16_t midY = static_cast<int16_t>(y + cellH / 2);
      const fui::TextStyle kanaStyle{KANA_SLOT, fui::TextAlign::Right};
      const fui::Rect ink = screen.target().inkBounds(KANA_SLOT, text, kanaStyle);
      const int16_t kanaY = static_cast<int16_t>(midY - ink.y - ink.height / 2);
      screen.target().text(fui::Rect{x, kanaY, kanaW, kanaLh}, text, kanaStyle);
      const char* romaji = kana::entries()[entry].romaji;
      const fui::TextStyle romajiStyle{ROMAJI_SLOT, fui::TextAlign::Left};
      // Romaji on the kana's baseline (line top + ascender in each font).
      const int16_t romajiY = static_cast<int16_t>(kanaY + kanaAscent - romajiAscent);
      screen.target().text(fui::Rect{static_cast<int16_t>(x + kanaW + 3), romajiY,
                                     static_cast<int16_t>(cellW - kanaW - 3), romajiLh},
                           romaji, romajiStyle);
    }
  }
  uiTarget.setFont(KANA_SLOT, uiScaleSpec().titleFontId);
}

void KanaChartActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, "Kana chart");
  renderUi();
  renderer.displayBuffer();
}

#endif  // CROSSPOINT_KANA_PRACTICE
