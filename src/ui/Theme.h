#pragma once

#include <QColor>
#include <QFont>
#include <QPainterPath>
#include <QRectF>

class QApplication;
class QLabel;
class QPainter;
class QWidget;

namespace toupie::Theme {

// Charte « Holo Combat » : fond quasi noir bleu-violet, reflets holographiques
// bleu → violet → magenta, vert fluo réservé aux actions, coins coupés à 45°.
inline const QColor kVoid{0x05, 0x04, 0x0f};
inline const QColor kBackground{0x0a, 0x08, 0x20};
inline const QColor kBackgroundDeep{0x05, 0x04, 0x0f};
inline const QColor kField{0x07, 0x06, 0x1a};
inline const QColor kPanel{0x15, 0x11, 0x3f};
inline const QColor kPanelAlt{0x1d, 0x18, 0x58};
inline const QColor kBorder{0x3a, 0x2f, 0x8f};
inline const QColor kBorderBright{0x5b, 0x4b, 0xd1};
inline const QColor kText{0xea, 0xf0, 0xff};
inline const QColor kMuted{0x92, 0x96, 0xd0};
inline const QColor kDim{0x58, 0x5c, 0x99};
inline const QColor kAccent{0x3d, 0xff, 0x8b};       ///< Vert plasma : actions.
inline const QColor kAccentBright{0xb8, 0xff, 0x2e}; ///< Vert acide (dégradé des actions).
inline const QColor kAccentDark{0x1f, 0x8f, 0x4f};
inline const QColor kBlue{0x2f, 0x6b, 0xff};
inline const QColor kCyan{0x34, 0xd6, 0xff};
inline const QColor kViolet{0x7b, 0x3c, 0xff};
inline const QColor kMagenta{0xc2, 0x3b, 0xff};
inline const QColor kDanger{0xff, 0x3b, 0x6b};
inline const QColor kWarning{0xff, 0xc2, 0x3d};

/// Police des titres, chiffres et boutons (Chakra Petch), taille en pixels.
QFont displayFont(double pixelSize, bool italic = false);
/// Police du texte courant (Rajdhani), taille en pixels.
QFont bodyFont(double pixelSize, int weight = QFont::DemiBold);

/// Rectangle aux coins coupés à 45° (haut-gauche, haut-droit, bas-droit, bas-gauche).
QPainterPath chamfer(const QRectF &rect, double topLeft, double topRight, double bottomRight,
                     double bottomLeft);
/// Coupe asymétrique de la charte : haut-gauche et bas-droit.
QPainterPath chamfer(const QRectF &rect, double cut);
/// Parallélogramme incliné vers la droite.
QPainterPath skewed(const QRectF &rect, double slant);

/// Halo néon : contour élargi et translucide, puis contour net.
void strokeGlow(QPainter &painter, const QPainterPath &path, const QColor &color, double width);
/// Trois barres obliques décoratives « /// ».
void paintSlashes(QPainter &painter, QPointF topLeft, double height, const QColor &color);
/// Fond d'écran : dégradé profond, halos de couleur et lignes de balayage.
void paintBackground(QPainter &painter, const QRectF &rect, bool floorGrid = false);
/// Panneau de la charte : chanfrein, contour holographique et coins accentués.
void paintPanel(QPainter &painter, const QRectF &rect, const QColor &accent, double cut = 18.0);
/// Étiquette inclinée (type de toupie, forme d'un coup…).
void paintTag(QPainter &painter, const QRectF &rect, const QString &text, const QColor &color);

/// Barre de stat segmentée (segments inclinés) ; avec un libellé, celui-ci et la valeur
/// occupent la moitié haute de `rect`.
void paintStatBar(QPainter &painter, const QRectF &rect, const QString &label, int value,
                  int segments, const QColor &color);
/// Icône vectorielle de la charte (grille 24 × 24, à tracer) : play, lan, trophy, build,
/// tuto, gear, help, update, quit, back, next, dash, timer, info, plus, trash, save, sliders, check, lock, user.
/// Un bouton l'affiche si sa propriété « glyph » porte ce nom.
QPainterPath iconPath(const QString &name);
void paintIcon(QPainter &painter, const QString &name, const QRectF &rect, const QColor &color);

/// Titre d'écran à la charte (police d'affichage, capitales).
QLabel *createTitle(const QString &text, QWidget *parent);

/// Applique le style, la palette, les polices et la feuille de style à l'application.
void apply(QApplication &app);

} // namespace toupie::Theme
