#include "Style.h"

QString portMonitorStyle()
{
    return QStringLiteral(R"(
QMainWindow, QWidget {
  background: #0f1216;
  color: #d7dde5;
  font-family: "Segoe UI", "Microsoft YaHei UI", "Microsoft YaHei";
  font-size: 13px;
}
QMenuBar, QToolBar, QStatusBar {
  background: #171b21;
  color: #d7dde5;
  border: none;
}
QMenuBar::item:selected, QMenu::item:selected { background: #232b35; }
QMenu { background: #171b21; border: 1px solid #313b47; }
QToolBar { spacing: 4px; padding: 4px 6px; border-bottom: 1px solid #232a33; }
QStatusBar { border-top: 1px solid #232a33; }
QStatusBar::item { border: none; }
QTabWidget::pane { border: none; background: #0f1216; }
QTabBar::tab {
  background: transparent;
  color: #9aa7b3;
  padding: 6px 12px;
  border: none;
  border-bottom: 2px solid transparent;
}
QTabBar::tab:selected { color: #eef2f6; border-bottom: 2px solid #2f7fd0; background: #171b21; }
QTabBar::tab:hover { color: #eef2f6; }
QTreeWidget, QTableView, QPlainTextEdit, QTextEdit {
  background: #0f1216;
  color: #d7dde5;
  border: none;
  selection-background-color: rgba(47, 127, 208, 0.28);
  selection-color: #eef2f6;
}
QTreeWidget { background: #12151a; }
QTableView { gridline-color: #1d232b; alternate-background-color: #12151a; }
QHeaderView::section {
  background: #1c222a;
  color: #9aa7b3;
  border: none;
  border-right: 1px solid #232a33;
  border-bottom: 1px solid #232a33;
  padding: 4px 6px;
}
QGroupBox {
  background: #171b21;
  border: 1px solid #232a33;
  border-radius: 6px;
  margin-top: 8px;
  padding: 8px 6px 6px 6px;
}
QGroupBox::title { subcontrol-origin: margin; left: 8px; color: #9aa7b3; }
QLineEdit, QComboBox, QPlainTextEdit, QTextEdit {
  background: #1b2027;
  border: 1px solid #33404d;
  border-radius: 4px;
  padding: 3px 6px;
  selection-background-color: rgba(47, 127, 208, 0.35);
}
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView { background: #171b21; color: #d7dde5; selection-background-color: #2f7fd0; }
QPushButton {
  background: #1b2027;
  color: #d7dde5;
  border: 1px solid #33404d;
  border-radius: 4px;
  padding: 4px 10px;
}
QPushButton:hover { background: #232b35; }
QPushButton:disabled { color: #7e8a97; background: #15191e; }
QPushButton#startButton { background: #2f7fd0; color: white; border: none; }
QPushButton#startButton:hover { background: #3b8fe2; }
QPushButton#startButton:disabled { background: #1b3a55; color: #9aa7b3; }
QCheckBox { spacing: 6px; }
QSplitter::handle { background: #232a33; }
QScrollBar:vertical { background: transparent; width: 10px; }
QScrollBar:horizontal { background: transparent; height: 10px; }
QScrollBar::handle { background: #313b47; border-radius: 4px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QLabel#hintLabel { color: #7e8a97; font-size: 11px; }
QLabel#errorLabel { color: #ef6a6a; }
QLabel#sectionLabel { color: #9aa7b3; font-size: 12px; }
)");
}
