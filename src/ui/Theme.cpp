#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace toupie::Theme {

namespace {
QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

QString styleSheet()
{
    QString css = QStringLiteral(R"(
QWidget { background-color: @bg; color: @text; }
QToolTip { background-color: @panelAlt; color: @text; border: 1px solid @accent; padding: 4px; }

QLabel { background: transparent; }
QLabel#title { color: @accentBright; }
QLabel#muted { color: @muted; }
QLabel#hint { color: @muted; font-style: italic; }

QGroupBox {
    background-color: @panel; border: 1px solid @border; border-radius: 12px;
    margin-top: 16px; padding: 14px 10px 10px 10px; font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin; subcontrol-position: top left; left: 14px; padding: 0 8px;
    color: @accent; background-color: @bg; border-radius: 4px;
}

QPushButton {
    background-color: @panelAlt; color: @text; border: 1px solid #3f4852; border-radius: 9px;
    padding: 8px 18px; font-weight: 600;
}
QPushButton:hover { border-color: @accent; color: @accentBright; background-color: #283225; }
QPushButton:pressed { background-color: #1e2a19; }
QPushButton:checked { background-color: #2c4022; border-color: @accent; color: @accentBright; }
QPushButton:disabled { color: #5d656e; border-color: #2c3239; background-color: #1a1e22; }
QPushButton:default, QPushButton#accent {
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accentBright, stop:1 @accent);
    color: #10140d; border: 1px solid @accentDark;
}
QPushButton:default:hover, QPushButton#accent:hover { background-color: @accentBright; color: #0b0e09; }
QPushButton:default:disabled, QPushButton#accent:disabled {
    background-color: #263021; color: #5f7a4e; border-color: #2c3a25;
}
QPushButton#menu {
    font-size: 13pt; padding: 12px 28px; border-radius: 12px; min-width: 320px;
    background-color: rgba(37, 42, 49, 220);
}
QPushButton#menu:default {
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accentBright, stop:1 @accent);
    color: #10140d; border: 1px solid @accentDark;
}
QPushButton#menu:default:hover { background-color: @accentBright; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background-color: #111417; color: @text; border: 1px solid @border; border-radius: 7px;
    padding: 5px 8px; selection-background-color: @accentDark; selection-color: @text;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: @accent; }
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled { color: #6b737c; background-color: #16191c; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background-color: @panel; color: @text; border: 1px solid @border;
    selection-background-color: #34502a; selection-color: @accentBright; outline: 0;
}

QSlider::groove:horizontal { height: 6px; background: #2c3239; border-radius: 3px; }
QSlider::sub-page:horizontal { background: @accent; border-radius: 3px; }
QSlider::sub-page:horizontal:disabled { background: #4b5a44; }
QSlider::handle:horizontal {
    background: @text; border: 2px solid @accent; width: 14px; height: 14px;
    margin: -6px 0; border-radius: 9px;
}
QSlider::handle:horizontal:disabled { border-color: #4b5a44; background: #8f99a3; }

QTableWidget, QTreeWidget, QListWidget {
    background-color: #111417; alternate-background-color: #171b1f; color: @text;
    border: 1px solid @border; border-radius: 9px; gridline-color: #262b31;
    selection-background-color: #34502a; selection-color: @accentBright; outline: 0;
}
QHeaderView::section {
    background-color: @panel; color: @accent; border: none; border-bottom: 1px solid @border;
    padding: 6px; font-weight: bold;
}
QTableCornerButton::section { background-color: @panel; border: none; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #3a424b; border-radius: 4px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: @accentDark; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #3a424b; border-radius: 4px; min-width: 24px; }

QMessageBox QLabel { color: @text; }
)");
    css.replace(QStringLiteral("@bg"), hex(kBackground));
    css.replace(QStringLiteral("@panelAlt"), hex(kPanelAlt));
    css.replace(QStringLiteral("@panel"), hex(kPanel));
    css.replace(QStringLiteral("@border"), hex(kBorder));
    css.replace(QStringLiteral("@text"), hex(kText));
    css.replace(QStringLiteral("@muted"), hex(kMuted));
    css.replace(QStringLiteral("@accentBright"), hex(kAccentBright));
    css.replace(QStringLiteral("@accentDark"), hex(kAccentDark));
    css.replace(QStringLiteral("@accent"), hex(kAccent));
    return css;
}
} // namespace

void apply(QApplication &app)
{
    if (QStyleFactory::keys().contains(QStringLiteral("Fusion")))
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    palette.setColor(QPalette::Window, kBackground);
    palette.setColor(QPalette::WindowText, kText);
    palette.setColor(QPalette::Base, QColor(0x11, 0x14, 0x17));
    palette.setColor(QPalette::AlternateBase, QColor(0x17, 0x1b, 0x1f));
    palette.setColor(QPalette::Text, kText);
    palette.setColor(QPalette::Button, kPanelAlt);
    palette.setColor(QPalette::ButtonText, kText);
    palette.setColor(QPalette::Highlight, kAccentDark);
    palette.setColor(QPalette::HighlightedText, kText);
    palette.setColor(QPalette::ToolTipBase, kPanelAlt);
    palette.setColor(QPalette::ToolTipText, kText);
    palette.setColor(QPalette::PlaceholderText, kMuted);
    palette.setColor(QPalette::Link, kAccent);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x5d, 0x65, 0x6e));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x5d, 0x65, 0x6e));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x5d, 0x65, 0x6e));
    app.setPalette(palette);

    QFont font = app.font();
    font.setPointSizeF(std::max(10.0, font.pointSizeF()));
    app.setFont(font);
    app.setStyleSheet(styleSheet());
}

} // namespace toupie::Theme
