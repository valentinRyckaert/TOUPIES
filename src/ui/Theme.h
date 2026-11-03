#pragma once

#include <QColor>

class QApplication;

namespace toupie::Theme {

// Charte : gris sombres + vert fluo adouci.
inline const QColor kBackground{0x15, 0x18, 0x1c};
inline const QColor kBackgroundDeep{0x0e, 0x10, 0x13};
inline const QColor kPanel{0x1d, 0x21, 0x26};
inline const QColor kPanelAlt{0x25, 0x2a, 0x31};
inline const QColor kBorder{0x34, 0x3b, 0x44};
inline const QColor kText{0xe3, 0xe7, 0xec};
inline const QColor kMuted{0x8f, 0x99, 0xa3};
inline const QColor kAccent{0x8e, 0xe0, 0x5a};       ///< Vert fluo adouci.
inline const QColor kAccentBright{0xa9, 0xf2, 0x72};
inline const QColor kAccentDark{0x4c, 0x7d, 0x2e};
inline const QColor kDanger{0xff, 0x6b, 0x6b};
inline const QColor kWarning{0xff, 0xc1, 0x4d};

/// Applique la palette, la police et la feuille de style à l'application.
void apply(QApplication &app);

} // namespace toupie::Theme
