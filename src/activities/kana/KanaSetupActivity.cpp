#include "KanaSetupActivity.h"

#if CROSSPOINT_KANA_PRACTICE

#include <GfxRenderer.h>

#include <cstdio>
#include <memory>

#include "KanaQuizActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
const char* const SCRIPT_HEADINGS[kana::SCRIPT_COUNT] = {"Hiragana", "Katakana"};
const char* const SCRIPT_ALL_LABELS[kana::SCRIPT_COUNT] = {"All hiragana", "All katakana"};
}  // namespace

KanaSetupActivity::KanaSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("KanaSetup", renderer, mappedInput) {}

void KanaSetupActivity::onEnter() {
  UiListActivity::onEnter();
  config.load();
  rebuildRows();
  requestUpdate();
}

void KanaSetupActivity::onExit() {
  config.save();
  UiListActivity::onExit();
}

void KanaSetupActivity::rebuildRows() {
  const int selected = config.selectedKanaCount();
  if (selected == 0) {
    snprintf(startSubtitle, sizeof(startSubtitle), "Select at least one row below");
  } else {
    snprintf(startSubtitle, sizeof(startSubtitle), "%d kana selected", selected);
  }
  const uint8_t count = kana::QUESTION_COUNTS[config.countIndex];
  if (count == 0) {
    snprintf(countValue, sizeof(countValue), "All");
  } else {
    snprintf(countValue, sizeof(countValue), "%u", static_cast<unsigned>(count));
  }

  for (int i = 0; i < TOTAL_ROWS; ++i) {
    rows[i] = fui::ListItem{};
    rows[i].actionValue = static_cast<int16_t>(i);
  }
  rows[ROW_START].label = "Start practice";
  rows[ROW_START].subtitle = startSubtitle;
  rows[ROW_START].bold = true;
  rows[ROW_START].enabled = selected > 0;
  rows[ROW_DIRECTION].label = "Direction";
  rows[ROW_DIRECTION].value = kana::directionLabel(config.direction);
  rows[ROW_ANSWER].label = "Answer with";
  rows[ROW_ANSWER].value = kana::answerModeLabel(config.answer);
  rows[ROW_COUNT].label = "Questions";
  rows[ROW_COUNT].value = countValue;

  for (int s = 0; s < kana::SCRIPT_COUNT; ++s) {
    const int base = ROW_FIRST_SCRIPT + s * ROWS_PER_SCRIPT;
    const kana::GroupMask mask = config.masks[s];
    fui::ListItem& all = rows[base];
    all.sectionHeading = SCRIPT_HEADINGS[s];
    all.label = SCRIPT_ALL_LABELS[s];
    all.toggle = true;
    all.toggleChecked = mask == kana::ALL_GROUPS;
    for (int g = 0; g < kana::GROUP_COUNT; ++g) {
      fui::ListItem& row = rows[base + 1 + g];
      row.label = kana::groups()[g].name;
      row.subtitle = kana::groups()[g].hint;
      row.toggle = true;
      row.toggleChecked = (mask & (1u << g)) != 0;
    }
  }
}

void KanaSetupActivity::activateIndex(const int index) {
  switch (index) {
    case ROW_START:
      startQuiz();
      return;
    case ROW_DIRECTION:
      config.direction = static_cast<kana::Direction>((static_cast<int>(config.direction) + 1) % kana::DIRECTION_COUNT);
      break;
    case ROW_ANSWER:
      config.answer = static_cast<kana::AnswerMode>((static_cast<int>(config.answer) + 1) % kana::ANSWER_MODE_COUNT);
      break;
    case ROW_COUNT:
      config.countIndex = static_cast<uint8_t>((config.countIndex + 1) % kana::QUESTION_COUNT_OPTIONS);
      break;
    default: {
      if (index < ROW_FIRST_SCRIPT || index >= TOTAL_ROWS) return;
      const int script = (index - ROW_FIRST_SCRIPT) / ROWS_PER_SCRIPT;
      const int offset = (index - ROW_FIRST_SCRIPT) % ROWS_PER_SCRIPT;
      kana::GroupMask& mask = config.masks[script];
      if (offset == 0) {
        mask = mask == kana::ALL_GROUPS ? 0 : kana::ALL_GROUPS;
      } else {
        mask ^= static_cast<kana::GroupMask>(1u << (offset - 1));
      }
      break;
    }
  }
  rebuildRows();
  requestUpdate();
}

void KanaSetupActivity::startQuiz() {
  if (config.selectedKanaCount() == 0) return;
  app.clearTapFlash();  // the tap leaves this screen
  config.save();
  startActivityForResult(std::make_unique<KanaQuizActivity>(renderer, mappedInput, config),
                         [this](const ActivityResult&) { requestUpdate(); });
}

void KanaSetupActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rows;
  props.count = static_cast<uint16_t>(TOTAL_ROWS);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in the base loop
  syncListViewport(screen, props);
  screen.list(props);
}

#endif  // CROSSPOINT_KANA_PRACTICE
