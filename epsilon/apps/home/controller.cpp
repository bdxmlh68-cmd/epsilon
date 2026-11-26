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

struct NexusTheme {
  const char* label;
  KDColor background;
  KDColor accent;
};

constexpr NexusTheme k_themes[] = {
    {"NEXUS // CYAN", KDColor::RGB24(0x0B1020), KDColor::RGB24(0x00F0FF)},
    {"NEXUS // NEON", KDColor::RGB24(0x140B24), KDColor::RGB24(0xFF2A85)},
    {"NEXUS // EMERALD", KDColor::RGB24(0x081814), KDColor::RGB24(0x00E676)},
};

constexpr int k_numThemes = sizeof(k_themes) / sizeof(k_themes[0]);

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

  // Bandeau NEXUS OS en bas de l'ecran
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

View* Controller::ContentView::subviewAtIndex(int index) {
  assert(index == 0);
  return &m_selectableTableView;
}

void Controller::ContentView::layoutSubviews(bool force) {
  KDRect frame =
      KDRect(KDPointZero, bounds().width(), bounds().height() - k_bottomMargin);
  setChildFrame(&m_selectableTableView, frame, force);
}

Controller::Controller(Responder* parentResponder,
                       SelectableTableViewDataSource* selectionDataSource)
    : ViewController(parentResponder), m_view(this, selectionDataSource) {}

bool Controller::handleEvent(Ion::Events::Event event) {
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
    m_view.selectableTableView()->selectCellAtLocation(0, 0);
    return true;
  }
  if (event == Ion::Events::Right &&
      selectionDataSource()->selectedRow() < numberOfRows() - 1) {
    m_view.selectableTableView()->selectCellAtLocation(
        0, selectionDataSource()->selectedRow() + 1);
    return true;
  }
  if (event == Ion::Events::Left && selectionDataSource()->selectedRow() > 0) {
    m_view.selectableTableView()->selectCellAtLocation(
        k_numberOfColumns - 1, selectionDataSource()->selectedRow() - 1);
    return true;
  }
  const char* eventTextPtr = event.text();
  if (eventTextPtr != nullptr && eventTextPtr[0] != 0 && eventTextPtr[1] == 0 &&
      eventTextPtr[0] >= '0' && eventTextPtr[0] <= '9') {
    int iconIndex;
    if (eventTextPtr[0] == '0') {
      iconIndex = numberOfIcons() - 1 -
                  AppsContainer::sharedAppsContainer()->numberOfExternalApps();
    } else {
      iconIndex = eventTextPtr[0] - '1';
    }
    if (iconIndex >= AppsContainer::sharedAppsContainer()->numberOfApps()) {
      return false;
    }
    int col = columnOfIconAtIndex(iconIndex);
    int row = rowOfIconAtIndex(iconIndex);
    if (col == m_view.selectableTableView()->selectedColumn() &&
        row == m_view.selectableTableView()->selectedRow()) {
      switchToSelectedApp();
      return true;
    }
    m_view.selectableTableView()->selectCellAtLocation(col, row);
    return true;
  }
  if (event == Ion::Events::Down &&
      selectionDataSource()->selectedRow() == numberOfRows() - 2) {
    assert(selectionDataSource()->selectedColumn() > columnOfLastIcon());
    m_view.selectableTableView()->selectCellAtLocation(columnOfLastIcon(),
                                                       numberOfRows() - 1);
    return true;
  }
  return false;
}

void Controller::handleResponderChainEvent(ResponderChainEvent event) {
  if (event.type == ResponderChainEventType::HasBecomeFirst) {
    if (selectionDataSource()->selectedRow() == -1) {
      selectionDataSource()->selectCellAtLocation(0, 0);
    }
    App::app()->setFirstResponder(m_view.selectableTableView());
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
  int appIdx = indexOfAppAtColumnAndRow(selectionDataSource()->selectedColumn(),
                                        selectionDataSource()->selectedRow());
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
