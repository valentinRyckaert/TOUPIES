#include "ui/MovesBoard.h"

#include "core/PartCatalog.h"
#include "game/SpecialMove.h"
#include "ui/Theme.h"
#include "ui/TopPainter.h"
#include "ui/Widgets.h"

#include <QDialog>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace toupie {

namespace {
constexpr double kTau = 2.0 * M_PI;
// Contenu de la planche de la maquette (2400 × 1880), sans ses marges.
const QRectF kContent(90, 80, 2220, 1750);

/// Textes et jauges de la planche, à l'identique.
struct MoveSheet
{
    const char *id;
    const char *plus;
    const char *minus;
    int damage, knockback, range;
};
const MoveSheet kSheets[] = {
    {"onde", QT_TRANSLATE_NOOP("toupie::MovesBoard", "Gros recul : éjecte une cible déjà au bord"),
     QT_TRANSLATE_NOOP("toupie::MovesBoard", "Portée courte"), 14, 5, 2},
    {"lance", QT_TRANSLATE_NOOP("toupie::MovesBoard", "Les plus lourds dégâts (24)"),
     QT_TRANSLATE_NOOP("toupie::MovesBoard", "Aucun effet, presque aucun recul, esquivable de loin"), 24, 1, 4},
    {"givre", QT_TRANSLATE_NOOP("toupie::MovesBoard", "3 s gelée : ralentie, sans pilotage, usée ×3"),
     QT_TRANSLATE_NOOP("toupie::MovesBoard", "Dégâts moyens"), 12, 2, 4},
    {"vortex", QT_TRANSLATE_NOOP("toupie::MovesBoard", "Aspire violemment tout le monde vers le centre"),
     QT_TRANSLATE_NOOP("toupie::MovesBoard", "Dégâts modérés"), 10, 4, 4},
    {"distorsion", QT_TRANSLATE_NOOP("toupie::MovesBoard", "Téléporte la cible près du bord, lancée vers l’extérieur"),
     QT_TRANSLATE_NOOP("toupie::MovesBoard", "Lente, facile à esquiver"), 8, 4, 3},
    {"siphon", QT_TRANSLATE_NOOP("toupie::MovesBoard", "Rend 60 % de l’endurance volée"),
     QT_TRANSLATE_NOOP("toupie::MovesBoard", "Dégâts modérés, peu de recul"), 11, 1, 4},
    {"seisme", QT_TRANSLATE_NOOP("toupie::MovesBoard", "Blesse et bloque dash + coup spécial 6 s"),
     QT_TRANSLATE_NOOP("toupie::MovesBoard", "Portée très courte"), 13, 2, 1},
};

QColor alpha(QColor c, double a)
{
    c.setAlphaF(a);
    return c;
}

QFont display(double size, double spacing = 0.0)
{
    QFont f = Theme::displayFont(size);
    f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    return f;
}

QFont body(double size, int weight = QFont::DemiBold, double spacing = 0.0)
{
    QFont f = Theme::bodyFont(size, weight);
    f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    return f;
}

/// Générateur pseudo-aléatoire de la maquette (mêmes tirages, même dessin).
class Lcg
{
public:
    explicit Lcg(long long seed) : m_s(seed) {}
    double operator()() { return double(m_s = (m_s * 16807) % 2147483647) / 2147483647.0; }

private:
    long long m_s;
};

QPolygonF regular(QPointF c, double r, int n, double rot)
{
    QPolygonF poly;
    for (int i = 0; i < n; ++i)
        poly << c + QPointF(std::cos(rot + i * kTau / n), std::sin(rot + i * kTau / n)) * r;
    return poly;
}

/// Trait néon : halo large et translucide, puis trait net.
void glowLine(QPainter &p, const QPainterPath &path, const QColor &color, double width, double opacity = 1.0)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(alpha(color, 0.3 * opacity), width * 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
    p.setPen(QPen(alpha(color, opacity), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
}

void softDisc(QPainter &p, QPointF c, double r, const QColor &color, double opacity)
{
    QRadialGradient g(c, r);
    g.setColorAt(0.0, alpha(color, opacity));
    g.setColorAt(1.0, alpha(color, 0.0));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(c, r, r);
}

QPainterPath line(QPointF a, QPointF b)
{
    QPainterPath path(a);
    path.lineTo(b);
    return path;
}

/// Illustration de l'effet (design/gen/art.js, moveFx) : lanceur à gauche, cible à droite.
void paintEffect(QPainter &p, const QString &key, const QColor &col, const QRectF &frame)
{
    const QList<Top> presets = PartCatalog::instance().presets();
    if (presets.size() < 3)
        return;
    int casterIndex = 0;
    for (int i = 0; i < presets.size(); ++i) {
        if (presets.at(i).special() == key) {
            casterIndex = i;
            break;
        }
    }
    const Top &caster = presets.at(casterIndex);
    const Top &target = presets.at((casterIndex + 3) % presets.size());
    const double x = frame.left(), y = frame.top(), w = frame.width(), h = frame.height();
    const double cy = y + h / 2, u = h, ax = x + w * 0.2, bx = x + w * 0.78;
    Lcg rnd(key.size() * 31 + 7);

    p.save();
    p.setClipRect(frame);
    const auto move = SpecialMove::find(key);
    if (move && move->hitsEveryone()) {
        const double cx = x + w * 0.42;
        if (key == QLatin1String("onde")) {
            const double rings[3] = {0.16, 0.27, 0.38};
            for (int i = 0; i < 3; ++i) {
                QPainterPath ring;
                ring.addEllipse(QPointF(cx, cy), u * rings[i], u * rings[i]);
                glowLine(p, ring, col, 5 - i * 1.4, 1 - i * 0.25);
            }
            p.setPen(QPen(alpha(Theme::kMagenta, 0.6), 1.5));
            p.drawEllipse(QPointF(cx - 4, cy), u * 0.38, u * 0.38);
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            for (int i = 0; i < 16; ++i) {
                const double a = i * kTau / 16 + rnd() * 0.2, r = u * (0.4 + rnd() * 0.05);
                p.drawPolygon(QPolygonF{QPointF(cx + std::cos(a) * r, cy + std::sin(a) * r),
                                        QPointF(cx + std::cos(a + 0.03) * (r + 14), cy + std::sin(a + 0.03) * (r + 14)),
                                        QPointF(cx + std::cos(a - 0.03) * (r + 10), cy + std::sin(a - 0.03) * (r + 10))});
            }
            TopPainter::paint(p, caster, QPointF(cx, cy), 34, 0.4);
            TopPainter::paint(p, target, QPointF(x + w * 0.88, cy - 20), 26, 1);
            glowLine(p, line({x + w * 0.8, cy - 20}, {x + w * 0.95, cy - 32}), col, 2);
            glowLine(p, line({x + w * 0.8, cy - 10}, {x + w * 0.97, cy - 14}), col, 2);
        } else if (key == QLatin1String("vortex")) {
            for (int k = 0; k < 4; ++k) {
                QPainterPath arm;
                for (int i = 0; i <= 40; ++i) {
                    const double a = k * kTau / 4 + i * 0.13, r = u * 0.46 * (1 - i / 44.0);
                    const QPointF pt(cx + std::cos(a) * r, cy + std::sin(a) * r * 0.9);
                    i ? arm.lineTo(pt) : arm.moveTo(pt);
                }
                glowLine(p, arm, col, 3.2, 0.9);
            }
            softDisc(p, QPointF(cx, cy), u * 0.14, Theme::kViolet, 0.5);
            p.setPen(QPen(Theme::kViolet, 2));
            p.setBrush(Qt::black);
            p.drawEllipse(QPointF(cx, cy), u * 0.1, u * 0.1);
            const QPointF pulled[2] = {{x + w * 0.85, cy - 40}, {x + w * 0.1, cy + 50}};
            QPen dash(Theme::kViolet, 3);
            dash.setDashPattern({2, 2});
            p.setPen(dash);
            for (const QPointF &t : pulled) {
                const double a = std::atan2(cy - t.y(), cx - t.x());
                const QPointF d(std::cos(a), std::sin(a));
                p.drawLine(t + d * 34, t + d * 70);
            }
            TopPainter::paint(p, caster, QPointF(cx, cy), 30, 2);
            TopPainter::paint(p, target, pulled[0], 22, 0.3);
            TopPainter::paint(p, presets.at(2), pulled[1], 22, 1.4);
        } else { // Séisme
            for (int k = 0; k < 10; ++k) {
                double a = k * kTau / 10, r = 30;
                QPainterPath crack(QPointF(cx + std::cos(a) * r, cy + std::sin(a) * r));
                while (r < u * 0.4) {
                    r += 10 + rnd() * 14;
                    a += (rnd() - 0.5) * 0.5;
                    crack.lineTo(cx + std::cos(a) * r, cy + std::sin(a) * r);
                }
                glowLine(p, crack, col, 2, 0.95);
            }
            p.setPen(QPen(col, 1));
            p.setBrush(QColor(0x1d, 0x2a, 0x10));
            for (int i = 0; i < 12; ++i) {
                const double a = rnd() * kTau, r = u * (0.15 + rnd() * 0.22);
                const double size = 4 + rnd() * 6, rot = rnd() * 3;
                p.drawPolygon(regular(QPointF(cx + std::cos(a) * r, cy + std::sin(a) * r), size, 4, rot));
            }
            QPen dotted(col, 1.5);
            dotted.setDashPattern({2, 4});
            p.setPen(dotted);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(cx, cy), u * 0.4, u * 0.4);
            TopPainter::paint(p, caster, QPointF(cx, cy), 32, 0.9);
            const double tx = cx + w * 0.24;
            TopPainter::paint(p, target, QPointF(tx, cy - 28), 24, 0.2);
            const QRectF lock(tx + 14, cy - 76, 34, 30);
            p.fillPath(Theme::chamfer(lock, 6), QColor(0x0a, 0x08, 0x26));
            p.setPen(QPen(col, 1.5));
            p.drawPath(Theme::chamfer(lock, 6));
            Theme::paintIcon(p, QStringLiteral("lock"), QRectF(tx + 22, cy - 70, 18, 18), col);
            p.setFont(display(15));
            p.setPen(col);
            p.drawText(QPointF(tx + 54, cy - 55), MovesBoard::tr("6 S"));
        }
    } else {
        if (key == QLatin1String("lance")) {
            glowLine(p, line({ax + 40, cy}, {bx - 30, cy}), col, 4);
            p.setPen(QPen(col, 2));
            p.setBrush(Qt::white);
            p.drawPolygon(QPolygonF{{bx - 30, cy}, {bx - 76, cy - 18}, {bx - 64, cy}, {bx - 76, cy + 18}});
            p.setPen(QPen(alpha(col, 0.6), 1.3));
            for (int i = 0; i < 9; ++i) {
                const double yy = cy + (rnd() - 0.5) * 50, x0 = ax + 50 + rnd() * w * 0.3;
                p.drawLine(QPointF(x0, yy), QPointF(x0 + 30 + rnd() * 60, yy));
            }
            p.setPen(QPen(Theme::kWarning, 1.5));
            for (int i = 0; i < 12; ++i) {
                const double a = (rnd() - 0.5) * 2.4 + M_PI, r = 18 + rnd() * 30;
                p.drawLine(QPointF(bx - 24, cy), QPointF(bx - 24 + std::cos(a + M_PI) * r, cy + std::sin(a + M_PI) * r));
            }
        } else if (key == QLatin1String("givre")) {
            QLinearGradient cone(ax + 30, 0, bx - 10, 0);
            cone.setColorAt(0.0, alpha(col, 0.55));
            cone.setColorAt(1.0, alpha(col, 0.05));
            p.setPen(Qt::NoPen);
            p.setBrush(cone);
            p.drawPolygon(QPolygonF{{ax + 30, cy - 8}, {bx - 10, cy - 70}, {bx - 10, cy + 70}, {ax + 30, cy + 8}});
            for (int i = 0; i < 18; ++i) {
                const double t = rnd(), xx = ax + 40 + t * (bx - ax - 40), yy = cy + (rnd() - 0.5) * 120 * t;
                TopPainter::paintEmblem(p, key, QPointF(xx, yy), 4 + rnd() * 6, Qt::white, 1.1);
            }
            p.setPen(QPen(Qt::white, 1.5));
            p.setBrush(alpha(col, 0.18));
            p.drawPolygon(regular(QPointF(bx, cy), 48, 6, 0));
            p.setPen(QPen(col, 1));
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(regular(QPointF(bx, cy), 40, 6, 0.5));
        } else if (key == QLatin1String("distorsion")) {
            p.setPen(Qt::NoPen);
            for (int i = 0; i < 7; ++i) {
                const double yy = cy - 36 + i * 12, off = (rnd() - 0.5) * 40;
                p.setBrush(alpha(i % 2 ? Theme::kMagenta : Theme::kCyan, 0.55 + rnd() * 0.4));
                p.drawRect(QRectF(ax + 50 + off, yy, w * 0.42, 6));
            }
            QPainterPath jump(QPointF(bx - 10, cy));
            jump.cubicTo(bx, cy - 60, bx + 30, cy - 70, x + w * 0.93, y + 28);
            QPen dash(Theme::kMagenta, 2);
            dash.setDashPattern({1.5, 3});
            p.setPen(dash);
            p.setBrush(Qt::NoBrush);
            p.drawPath(jump);
            TopPainter::paint(p, target, QPointF(x + w * 0.93, y + 30), 20, 2);
            p.setPen(QPen(Theme::kMagenta, 1.5));
            p.drawEllipse(QPointF(x + w * 0.93, y + 30), 26, 26);
        } else { // Siphon
            for (int k = -1; k <= 1; ++k) {
                QPainterPath stream(QPointF(bx - 30, cy + k * 6));
                stream.cubicTo(bx - 120, cy - 70 + k * 30, ax + 120, cy + 70 + k * 30, ax + 40, cy + k * 4);
                glowLine(p, stream, col, k ? 1.4 : 3, k ? 0.6 : 1);
            }
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            for (int i = 0; i < 7; ++i) {
                const double t = (i + 0.5) / 7, xx = bx - 30 - t * (bx - ax - 70), yy = cy - std::sin(t * kTau) * 26;
                QPainterPath drop(QPointF(xx, yy - 7));
                drop.cubicTo(xx + 4, yy - 2, xx + 5, yy + 2, xx, yy + 5);
                drop.cubicTo(xx - 5, yy + 2, xx - 4, yy - 2, xx, yy - 7);
                p.drawPath(drop);
            }
            p.setFont(display(17));
            p.setPen(Theme::kAccent);
            p.drawText(QRectF(ax - 60, cy - 70, 120, 24), Qt::AlignCenter, MovesBoard::tr("+60 %"));
        }
        TopPainter::paint(p, caster, QPointF(ax, cy), 34, 0.6);
        p.setOpacity(key == QLatin1String("distorsion") ? 0.35 : 1.0); // La cible a disparu.
        TopPainter::paint(p, target, QPointF(bx, cy), 28, 1.7);
        p.setOpacity(1.0);
    }
    p.restore();
}

/// Jauge segmentée de la planche (libellé, valeur, 8 segments).
void statBar(QPainter &p, double x, double y, double w, const QString &label, int value, int max, const QColor &color)
{
    p.setFont(body(13, QFont::Bold, 1.6));
    p.setPen(Theme::kMuted);
    p.drawText(QPointF(x, y - 8), label.toUpper());
    p.setFont(display(15));
    p.setPen(color);
    p.drawText(QRectF(x, y - 26, w, 20), Qt::AlignRight | Qt::AlignBottom, QString::number(value));
    constexpr int kSegments = 8;
    const double gap = 3, sw = (w - (kSegments - 1) * gap) / kSegments;
    const int on = qRound(double(value) / max * kSegments);
    for (int i = 0; i < kSegments; ++i)
        p.fillPath(Theme::skewed(QRectF(x + i * (sw + gap), y, sw + 3, 10), 3),
                   i < on ? color : alpha(Theme::kBorder, 0.5));
}

void paintSheet(QPainter &p, const MoveSheet &sheet, const QRectF &r)
{
    const QString key = QString::fromLatin1(sheet.id);
    const auto move = SpecialMove::find(key);
    if (!move)
        return;
    const QColor col = QColor::fromRgb(move->color);
    QString boltName;
    for (const Part &bolt : PartCatalog::instance().parts(PartKind::Bolt)) {
        if (bolt.special == key)
            boltName = bolt.name;
    }
    const double x = r.left(), y = r.top(), w = r.width();

    QLinearGradient cardFill(0, y, 0, r.bottom());
    cardFill.setColorAt(0.0, QColor(0x17, 0x12, 0x4a, 242));
    cardFill.setColorAt(1.0, QColor(0x0c, 0x0a, 0x2a, 242));
    const QPainterPath card = Theme::chamfer(r, 28, 0, 28, 0);
    p.fillPath(card, cardFill);
    p.setPen(QPen(Theme::kBorder, 1.2));
    p.drawPath(card);

    const QRectF frame(x + 16, y + 16, w - 32, 340);
    const QPainterPath framePath = Theme::chamfer(frame, 20, 0, 0, 0);
    p.fillPath(framePath, QColor(0x06, 0x05, 0x1a));
    QRadialGradient tint(QPointF(frame.left() + frame.width() * 0.45, frame.center().y()), frame.width() * 0.6);
    tint.setColorAt(0.0, alpha(col, 0.14));
    tint.setColorAt(1.0, alpha(col, 0.0));
    p.fillPath(framePath, tint);
    p.setPen(QPen(Theme::kBorder, 1.0));
    p.drawPath(framePath);
    paintEffect(p, key, col, frame);

    const QRectF icon(x + 30, y + 384, 76, 76);
    softDisc(p, icon.center(), 52, col, 0.3);
    p.fillPath(Theme::chamfer(icon, 16, 0, 16, 0), QColor(0x07, 0x06, 0x1a));
    p.setPen(QPen(col, 2));
    p.drawPath(Theme::chamfer(icon, 16, 0, 16, 0));
    TopPainter::paintEmblem(p, key, icon.center(), 24, col, 2.4);

    p.setFont(body(13, QFont::Bold, 2.0));
    p.setPen(Theme::kMuted);
    p.drawText(QPointF(x + 126, y + 404), boltName.toUpper());
    p.setFont(display(30, 2.0));
    p.setPen(Theme::kText);
    p.drawText(QPointF(x + 126, y + 442), move->name.toUpper());
    Theme::paintTag(p, QRectF(x + w - 140, y + 386, 110, 24),
                    move->hitsEveryone() ? MovesBoard::tr("Anneau") : MovesBoard::tr("Vague"), col);

    const QRectF text(x + 52, 0, w - 82, 52);
    p.setFont(body(19));
    p.fillPath(Theme::skewed(QRectF(x + 30, y + 486, 10, 14), 4), Theme::kAccent);
    p.setPen(Theme::kText);
    p.drawText(text.translated(0, y + 480), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               MovesBoard::tr(sheet.plus));
    p.fillPath(Theme::skewed(QRectF(x + 30, y + 548, 10, 14), 4), Theme::kDanger);
    p.setPen(Theme::kMuted);
    p.drawText(text.translated(0, y + 542), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               MovesBoard::tr(sheet.minus));

    const double third = (w - 60) / 3;
    statBar(p, x + 30, y + 660, third - 24, MovesBoard::tr("Dégâts"), sheet.damage, 24,
            TopPainter::typeColor(TopType::Attack));
    statBar(p, x + 30 + third, y + 660, third - 24, MovesBoard::tr("Recul"), sheet.knockback, 5, Theme::kCyan);
    statBar(p, x + 30 + 2 * third, y + 660, third - 24, MovesBoard::tr("Portée"), sheet.range, 4, Theme::kViolet);
    p.setFont(body(15, QFont::Medium));
    p.setPen(Theme::kDim);
    p.drawText(QPointF(x + 30, y + 712), MovesBoard::tr("Coûte ¼ de l’endurance du moment · une fois par partie"));
}
} // namespace

MovesBoard::MovesBoard(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(720, 564);
}

QDialog *MovesBoard::createDialog(QWidget *parent)
{
    QDialog *dialog = createPopup(parent, tr("Coups spéciaux"), new MovesBoard);
    if (parent)
        dialog->resize(parent->window()->size() * 0.92);
    return dialog;
}

void MovesBoard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    Theme::paintBackground(p, rect());
    // Coordonnées de la planche, mises à l'échelle de la fenêtre.
    const double k = std::min(width() / kContent.width(), height() / kContent.height());
    p.translate((width() - kContent.width() * k) / 2.0, (height() - kContent.height() * k) / 2.0);
    p.scale(k, k);
    p.translate(-kContent.topLeft());

    Theme::paintSlashes(p, QPointF(120, 115), 16, Theme::kAccent);
    p.setFont(display(15, 4.0));
    p.setPen(Theme::kAccent);
    p.drawText(QPointF(160, 130), tr("Coups spéciaux").toUpper());
    p.setFont(display(54, 3.0));
    p.setPen(Theme::kText);
    p.drawText(QPointF(120, 188), tr("Sept signatures visuelles").toUpper());
    p.setFont(body(21, QFont::Medium));
    p.setPen(Theme::kMuted);
    p.drawText(QPointF(120, 226), tr("Vague = projectile qui file vers l’adversaire le plus proche (esquivable). "
                                     "Anneau = onde qui s’élargit autour du lanceur."));

    for (int i = 0; i < int(std::size(kSheets)); ++i) {
        const bool top = i < 4;
        const QRectF r(120 + (top ? i * 545 : (i - 4) * 725), top ? 290 : 1060, top ? 520 : 700, 740);
        paintSheet(p, kSheets[i], r);
    }
}

} // namespace toupie
