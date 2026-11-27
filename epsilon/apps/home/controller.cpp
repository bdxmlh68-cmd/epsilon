#include "controller.h"

#include <apps/apps_container.h>
#include <apps/exam_mode_manager.h>
#include <apps/home/apps_layout.h>
#include <ion/keyboard/layout_events.h>
#include <poincare/preferences.h>

#include "app.h"

extern "C" {
#include <assert.h>
}

using namespace Escher;
using namespace Poincare;

namespace Home {

namespace {

constexpr int k_maxApps = 32;

struct NexusTheme {
  const char* label;
  KDColor background;
  KDColor accent;
};

constexpr NexusTheme k_themes[] = {
    {"NOVA // PS5", KDColor::RGB24(0x081E4D), KDColor::RGB24(0x0070D1)},
    {"NEXUS // CYAN", KDColor::RGB24(0x0B1020), KDColor::RGB24(0x00F0FF)},
    {"NEXUS // NEON", KDColor::RGB24(0x140B24), KDColor::RGB24(0xFF2A85)},
};

constexpr int k_numThemes = sizeof(k_themes) / sizeof(k_themes[0]);

const char* appNameAtIndex(int appIdx) {
  AppsContainer* container = AppsContainer::sharedAppsContainer();
  if (appIdx < container->numberOfBuiltinApps()) {
    I18n::Message msg =
        container->appSnapshotAtIndex(appIdx)->descriptor()->name();
    return I18n::translate(msg);
  }
  int extIdx = appIdx - container->numberOfBuiltinApps();
  if (extIdx >= 0 && extIdx < container->numberOfExternalApps()) {
    return container->externalAppAtIndex(extIdx).name();
  }
  return "App";
}

}  // namespace

Controller::ContentView::ContentView(
    Controller* controller, SelectableTableViewDataSource* selectionDataSource)
    : m_selectableTableView(controller, controller, selectionDataSource,
                            controller) {
  m_selectableTableView.setVerticalCellOverlap(0);
  m_selectableTableView.setMargins(k_margins);
  m_selectableTableView.setBackgroundColor(k_themes[0].background);
  m_selectableTableView.decorator()->setVerticalMargins(
      {k_indicatorMargin, k_indicatorMargin - k_bottomMargin});
}

SelectableTableView* Controller::ContentView::selectableTableView() {
  return &m_selectableTableView;
}

void Controller::ContentView::drawRect(KDContext* ctx, KDRect rect) const {
  const NexusTheme& theme = k_themes[m_themeIndex % k_numThemes];
  ctx->fillRect(bounds(), theme.background);

  KDCoordinate barY = bounds().height() - k_bottomMargin;
  ctx->fillRect(KDRect(0, barY, bounds().width(), 2), theme.accent);
  ctx->fillRect(KDRect(0, barY + 2, bounds().width(), k_bottomMargin - 2),
                KDColor::RGB24(0x111827));
  ctx->drawString(theme.label, KDPoint(8, barY + 3), KDFont::Size::Small,
                  theme.accent, KDColor::RGB24(0x111827));
  ctx->drawString("[TOOLBOX: THEME]", KDPoint(185, barY + 3),
                  KDFont::Size::Small, KDColorWhite, KDColor::RGB24(0x111827));
}

void Controller::ContentView::reloadBottomRow(
    SimpleTableViewDataSource* dataSource, int lastIconColumn) {
  for (int i = lastIconColumn + 1; i < dataSource->numberOfColumns(); i++) {
    markRectAsDirty(KDRect(dataSource->columnWidth(0) * i,
                           dataSource->rowHeight(0), dataSource->columnWidth(0),
                           dataSource->rowHeight(0)));
  }
}

int Controller::ContentView::numberOfSubviews() const { return 2; }

View* Controller::ContentView::subviewAtIndex(int index) {
  assert(0 <= index && index < 2);
  if (index == 0) {
    return &m_ps5View;
  }
  return &m_selectableTableView;
}

void Controller::ContentView::layoutSubviews(bool force) {
  KDRect frame =
      KDRect(KDPointZero, bounds().width(), bounds().height() - k_bottomMargin);
  setChildFrame(&m_ps5View, frame, force);
  setChildFrame(&m_selectableTableView, KDRectZero, force);
}

Controller::Controller(Responder* parentResponder,
                       SelectableTableViewDataSource* selectionDataSource)
    : ViewController(parentResponder), m_view(this, selectionDataSource) {}

void Controller::syncPS5View() {
  static const char* s_names[k_maxApps];
  AppsContainer* container = AppsContainer::sharedAppsContainer();
  int total = container->numberOfApps();
  if (total > k_maxApps) {
    total = k_maxApps;
  }
  for (int i = 0; i < total; i++) {
    s_names[i] = appNameAtIndex(i);
  }
  m_view.ps5View()->setApps(total, s_names);
  m_view.ps5View()->setSelected(m_ps5SelectedIndex);
}

bool Controller::handleEvent(Ion::Events::Event event) {
  AppsContainer* container = AppsContainer::sharedAppsContainer();
  int total = container->numberOfApps();
  if (total <= 0) {
    return false;
  }
  if (m_ps5SelectedIndex >= total) {
    m_ps5SelectedIndex = 0;
  }

  if (event == Ion::Events::Toolbox) {
    m_themeIndex = (m_themeIndex + 1) % k_numThemes;
    const NexusTheme& theme = k_themes[m_themeIndex];
    m_view.selectableTableView()->setBackgroundColor(theme.background);
    m_view.setThemeIndex(m_themeIndex);
    return true;
  }
  if (event == Ion::Events::OK || event == Ion::Events::EXE) {
    switchToSelectedApp();
    return true;
  }
  if (event == Ion::Events::Home || event == Ion::Events::Back) {
    m_ps5SelectedIndex = 0;
    m_view.ps5View()->setSelected(0);
    return true;
  }
  if (event == Ion::Events::Right) {
    m_ps5SelectedIndex = (m_ps5SelectedIndex + 1) % total;
    m_view.ps5View()->setSelected(m_ps5SelectedIndex);
    return true;
  }
  if (event == Ion::Events::Left) {
    m_ps5SelectedIndex = (m_ps5SelectedIndex - 1 + total) % total;
    m_view.ps5View()->setSelected(m_ps5SelectedIndex);
    return true;
  }
  const char* eventTextPtr = event.text();
  if (eventTextPtr != nullptr && eventTextPtr[0] != 0 && eventTextPtr[1] == 0 &&
      eventTextPtr[0] >= '0' && eventTextPtr[0] <= '9') {
    int iconIndex = (eventTextPtr[0] == '0') ? (total - 1) : (eventTextPtr[0] - '1');
    if (iconIndex < 0 || iconIndex >= total) {
      return false;
    }
    if (iconIndex == m_ps5SelectedIndex) {
      switchToSelectedApp();
      return true;
    }
    m_ps5SelectedIndex = iconIndex;
    m_view.ps5View()->setSelected(m_ps5SelectedIndex);
    return true;
  }
  return false;
}

void Controller::handleResponderChainEvent(ResponderChainEvent event) {
  if (event.type == ResponderChainEventType::HasBecomeFirst) {
    syncPS5View();
    App::app()->setFirstResponder(this);
  } else {
    ViewController::handleResponderChainEvent(event);
  }
}

HighlightCell* Controller::reusableCell(int index) {
  assert(0 <= index && index < reusableCellCount());
  return &m_reusableCells[index];
}

void Controller::fillCellForLocation(HighlightCell* cell, int column, int row) {
  AppCell* appCell = static_cast<AppCell*>(cell);
  AppsContainer* container = AppsContainer::sharedAppsContainer();
  int appIdx = indexOfAppAtColumnAndRow(column, row);
  if (appIdx >= container->numberOfApps()) {
    appCell->setVisible(false);
  } else {
    appCell->setVisible(true);
    if (appIdx < container->numberOfBuiltinApps()) {
      const ::App::Descriptor* descriptor =
          container->appSnapshotAtIndex(PermutedAppSnapshotIndex(appIdx))
              ->descriptor();
      appCell->setBuiltinAppDescriptor(descriptor);
    } else {
      Ion::ExternalApps::App a = container->externalAppAtIndex(
          appIdx - container->numberOfBuiltinApps());
      appCell->setExternalApp(a);
    }
  }
}

int Controller::numberOfIcons() const {
  AppsContainer* container = AppsContainer::sharedAppsContainer();
  assert(container->numberOfApps() > 0);
  return container->numberOfApps() - 1;
}

void Controller::tableViewDidChangeSelectionAndDidScroll(
    SelectableTableView* t, int previousSelectedCol, int previousSelectedRow,
    KDPoint previousOffset, bool withinTemporarySelection) {
  assert(t == m_view.selectableTableView());
  if (!withinTemporarySelection && t->selectedRow() == numberOfRows() - 1) {
    m_view.reloadBottomRow(this, columnOfLastIcon());
  }
}

SelectableTableViewDataSource* Controller::selectionDataSource() const {
  return App::app()->snapshot();
}

void Controller::switchToSelectedApp() {
  AppsContainer* container = AppsContainer::sharedAppsContainer();
  int appIdx = m_ps5SelectedIndex;
  if (appIdx < container->numberOfBuiltinApps()) {
    ::App::Snapshot* selectedSnapshot =
        container->appSnapshotAtIndex(PermutedAppSnapshotIndex(appIdx));
    if (appIsForbidden(selectedSnapshot->descriptor()->name())) {
      App::app()->displayWarning(forbiddenAppMessage());
    } else {
      container->switchToBuiltinApp(selectedSnapshot);
    }
  } else {
    assert(ExamModeManager::ExamMode().ruleset() == ExamMode::Ruleset::Off);
    m_view.reload();
    Ion::ExternalApps::App a = container->externalAppAtIndex(
        appIdx - container->numberOfBuiltinApps());
    container->switchToExternalApp(a);
  }
}

bool Controller::appIsForbidden(I18n::Message appName) const {
  ExamMode examMode = ExamModeManager::ExamMode();
  return (appName == I18n::Message::CodeApp && examMode.forbidCodeApp()) ||
         (appName == I18n::Message::ElementsApp &&
          examMode.forbidElementsApp()) ||
         (appName == I18n::Message::SolverApp && examMode.forbidSolverApp());
}

I18n::Message Controller::forbiddenAppMessage() const {
  ExamMode mode = ExamModeManager::ExamMode();
  if (mode.ruleset() == ExamMode::Ruleset::PressToTest) {
    return I18n::Message::ForbiddenAppInPressToTestMode;
  }
  assert(mode.isActive());
  return I18n::Message::ForbiddenAppInExamMode;
}

}  // namespace Home
