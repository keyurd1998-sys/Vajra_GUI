#pragma once

namespace vajra::gui {

inline constexpr const char* DARK_THEME_QSS = R"(
QMainWindow, QWidget {
    background-color: #121212;
    color: #FFFFFF;
    font-family: 'Segoe UI', 'Ubuntu', 'Helvetica Neue', sans-serif;
    font-size: 12px;
}
QMenuBar {
    background-color: #181818;
    color: #FFFFFF;
    border-bottom: 1px solid #282828;
}
QMenuBar::item:selected {
    background-color: #282828;
    color: #FFFFFF;
}
QMenu {
    background-color: #181818;
    color: #FFFFFF;
    border: 1px solid #333333;
}
QMenu::item:selected {
    background-color: #2E2E2E;
    color: #FFFFFF;
}
QToolBar {
    background-color: #181818;
    border-bottom: 1px solid #282828;
    spacing: 6px;
    padding: 3px;
}
QToolButton {
    background-color: #202020;
    color: #FFFFFF;
    border: 1px solid #333333;
    border-radius: 3px;
    padding: 4px 8px;
    font-weight: bold;
}
QToolButton:hover {
    background-color: #303030;
    border-color: #555555;
    color: #FFFFFF;
}
QToolButton:pressed {
    background-color: #141414;
}
QToolButton:checked {
    background-color: #383838;
    border-color: #777777;
    color: #FFFFFF;
}
QLineEdit {
    background-color: #1C1C1C;
    color: #FFFFFF;
    border: 1px solid #383838;
    border-radius: 3px;
    padding: 4px 8px;
    selection-background-color: #404040;
    selection-color: #FFFFFF;
}
QLineEdit:focus {
    border: 1px solid #666666;
}
QDockWidget {
    background-color: #161616;
    color: #FFFFFF;
}
QDockWidget::title {
    background-color: #1C1C1C;
    border-bottom: 1px solid #282828;
    padding: 6px;
    font-weight: bold;
    color: #FFFFFF;
}
QTreeWidget, QTableWidget, QListWidget {
    background-color: #161616;
    alternate-background-color: #1F1F1F;
    color: #FFFFFF;
    border: 1px solid #282828;
    gridline-color: #242424;
    selection-background-color: #333333;
    selection-color: #FFFFFF;
}
QTreeWidget::item:selected, QTableWidget::item:selected {
    background-color: #333333;
    color: #FFFFFF;
}
QTreeWidget::item:hover, QTableWidget::item:hover {
    background-color: #262626;
    color: #FFFFFF;
}
QHeaderView::section {
    background-color: #1C1C1C;
    color: #FFFFFF;
    padding: 5px;
    border: 1px solid #282828;
    font-weight: bold;
}
QScrollBar:horizontal {
    border: none;
    background: #121212;
    height: 10px;
    margin: 0px;
}
QScrollBar::handle:horizontal {
    background: #2D2D2D;
    min-width: 24px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal:hover {
    background: #444444;
}
QScrollBar::handle:horizontal:pressed {
    background: #666666;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0px;
}
QScrollBar:vertical {
    border: none;
    background: #121212;
    width: 10px;
    margin: 0px;
}
QScrollBar::handle:vertical {
    background: #2D2D2D;
    min-height: 24px;
    border-radius: 4px;
}
QScrollBar::handle:vertical:hover {
    background: #444444;
}
QScrollBar::handle:vertical:pressed {
    background: #666666;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}
QStatusBar {
    background-color: #161616;
    color: #CCCCCC;
    border-top: 1px solid #282828;
}
QLabel {
    color: #FFFFFF;
}
)";

} // namespace vajra::gui
