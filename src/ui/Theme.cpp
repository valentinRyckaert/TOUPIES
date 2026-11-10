#include "ui/Theme.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPalette>
#include <QProxyStyle>
#include <QPushButton>
#include <QStyleFactory>
#include <QStyleOption>

#include <algorithm>
#include <cmath>

namespace toupie::Theme {

namespace {

QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

QColor alpha(QColor c, int a)
{
    c.setAlpha(a);
    return c;
}

// --- Polices embarquées (licence OFL, voir fonts/) ---------------------------

struct FontFace
{
    const char *file;
    int weight;
    bool italic;
    bool display;
    QString family; ///< Famille déclarée par le fichier (renseignée au chargement).
};

FontFace g_faces[] = {
    {"ChakraPetch-SemiBold.ttf", QFont::DemiBold, false, true, {}},
    {"ChakraPetch-Bold.ttf", QFont::Bold, false, true, {}},
    {"ChakraPetch-BoldItalic.ttf", QFont::Bold, true, true, {}},
    {"Rajdhani-Medium.ttf", QFont::Medium, false, false, {}},
    {"Rajdhani-SemiBold.ttf", QFont::DemiBold, false, false, {}},
    {"Rajdhani-Bold.ttf", QFont::Bold, false, false, {}},
};

void loadFonts()
{
    static bool loaded = false;
    if (loaded || !qApp)
        return;
    loaded = true;
    for (FontFace &face : g_faces) {
        const int id = QFontDatabase::addApplicationFont(
            QStringLiteral(":/fonts/%1").arg(QLatin1String(face.file)));
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty())
            face.family = families.first();
    }
}

/// Selon la plateforme, chaque graisse peut être déclarée comme une famille
/// distincte (« Rajdhani SemiBold ») : on prend la famille du fichier le plus proche.
QString familyFor(bool display, int weight, bool italic)
{
    loadFonts();
    const FontFace *best = nullptr;
    int bestScore = 0;
    for (const FontFace &face : g_faces) {
        if (face.display != display || face.family.isEmpty())
            continue;
        const int score = std::abs(face.weight - weight) + (face.italic != italic ? 1000 : 0);
        if (!best || score < bestScore) {
            best = &face;
            bestScore = score;
        }
    }
    if (best)
        return best->family;
    return display ? QStringLiteral("Chakra Petch") : QStringLiteral("Rajdhani");
}

QFont groupTitleFont()
{
    QFont f = displayFont(14);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    return f;
}

// --- Style ------------------------------------------------------------------

enum class Variant { Secondary, Primary, Holo, Danger, Ghost, Card };

Variant variantOf(const QWidget *w, const QStyleOption *opt)
{
    const QString name = w ? w->objectName() : QString();
    const QString variant = w ? w->property("variant").toString() : QString();
    if (name == QLatin1String("card"))
        return Variant::Card;
    if (variant == QLatin1String("holo"))
        return Variant::Holo;
    if (variant == QLatin1String("danger"))
        return Variant::Danger;
    if (variant == QLatin1String("ghost"))
        return Variant::Ghost;
    const auto *button = qstyleoption_cast<const QStyleOptionButton *>(opt);
    if (name == QLatin1String("accent") || variant == QLatin1String("primary")
        || (button && (button->features & QStyleOptionButton::DefaultButton)))
        return Variant::Primary;
    return Variant::Secondary;
}

bool isMenuButton(const QWidget *w)
{
    return w && w->objectName() == QLatin1String("menu");
}

/// Police d'un libellé de bouton : celle du widget, espacée.
QFont labelFont(const QWidget *w)
{
    QFont font = w ? w->font() : QApplication::font("QPushButton");
    font.setLetterSpacing(QFont::AbsoluteSpacing, isMenuButton(w) ? 2.4 : 1.6);
    return font;
}

double buttonCut(const QRectF &r)
{
    return std::clamp(r.height() * 0.3, 6.0, 16.0);
}

/// Crochets d'angle (focus clavier, sélection).
void paintBrackets(QPainter &p, const QRectF &r, const QColor &color, double len)
{
    QPainterPath path;
    path.moveTo(r.left(), r.top() + len);
    path.lineTo(r.topLeft());
    path.lineTo(r.left() + len, r.top());
    path.moveTo(r.right() - len, r.bottom());
    path.lineTo(r.bottomRight());
    path.lineTo(r.right(), r.bottom() - len);
    p.setPen(QPen(color, 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

/// Champ de saisie : chanfrein sombre, contour cyan au focus.
void paintField(QPainter &p, const QStyleOption *opt)
{
    const QRectF r = QRectF(opt->rect).adjusted(1, 1, -1, -1);
    const QPainterPath path = chamfer(r, std::min(10.0, r.height() * 0.25));
    const bool enabled = opt->state & QStyle::State_Enabled;
    const bool focus = opt->state & (QStyle::State_HasFocus | QStyle::State_On);
    const bool hover = enabled && (opt->state & QStyle::State_MouseOver);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.fillPath(path, enabled ? kField : alpha(kField, 150));
    if (focus) {
        p.save();
        p.setClipPath(path);
        p.setPen(QPen(alpha(kCyan, 60), 6.0));
        p.drawPath(path);
        p.restore();
    }
    p.setPen(QPen(focus ? kCyan : hover ? kBorderBright : kBorder, focus ? 1.8 : 1.2));
    p.drawPath(path);
    p.restore();
}

class HoloStyle : public QProxyStyle
{
public:
    HoloStyle()
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {
    }

    void polish(QWidget *w) override
    {
        QProxyStyle::polish(w);
        if (qobject_cast<QPushButton *>(w) || qobject_cast<QComboBox *>(w)
            || qobject_cast<QLineEdit *>(w))
            w->setAttribute(Qt::WA_Hover, true);
        // Le texte ne doit pas toucher les coins coupés des champs.
        if (auto *edit = qobject_cast<QLineEdit *>(w); edit && edit->hasFrame())
            edit->setTextMargins(8, 0, 8, 0);
    }
    using QProxyStyle::polish;

    int pixelMetric(PixelMetric metric, const QStyleOption *opt, const QWidget *w) const override
    {
        switch (metric) {
        case PM_IndicatorWidth:
        case PM_IndicatorHeight:
        case PM_ExclusiveIndicatorWidth:
        case PM_ExclusiveIndicatorHeight:
            return 18;
        default:
            return QProxyStyle::pixelMetric(metric, opt, w);
        }
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption *opt, const QSize &size,
                           const QWidget *w) const override
    {
        QSize s = QProxyStyle::sizeFromContents(type, opt, size, w);
        if (type == CT_PushButton) {
            if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt);
                b && variantOf(w, opt) != Variant::Card) {
                const QFont font = labelFont(w);
                s.rwidth() += QFontMetrics(font).horizontalAdvance(b->text.toUpper())
                            - b->fontMetrics.horizontalAdvance(b->text);
            }
            if (w && !w->property("glyph").toString().isEmpty())
                s.rwidth() += 28;
            if (isMenuButton(w))
                return {std::max(s.width() + 90, 380), std::max(s.height(), 54)};
            if (variantOf(w, opt) == Variant::Card)
                return s + QSize(40, 20);
            return {s.width() + 26, std::max(s.height(), 38)};
        }
        if (type == CT_LineEdit || type == CT_ComboBox)
            return {s.width(), std::max(s.height(), 34)};
        return s;
    }

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p,
                       const QWidget *w) const override
    {
        switch (pe) {
        case PE_PanelButtonCommand:
            paintButtonPanel(opt, p, w);
            return;
        case PE_FrameDefaultButton:
        case PE_FrameFocusRect:
        case PE_FrameLineEdit:
            return;
        case PE_PanelLineEdit:
            if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(opt);
                frame && frame->lineWidth > 0) {
                paintField(*p, opt);
                return;
            }
            break;
        case PE_IndicatorCheckBox:
        case PE_IndicatorRadioButton:
            paintIndicator(pe == PE_IndicatorRadioButton, opt, p);
            return;
        default:
            break;
        }
        QProxyStyle::drawPrimitive(pe, opt, p, w);
    }

    void drawControl(ControlElement ce, const QStyleOption *opt, QPainter *p,
                     const QWidget *w) const override
    {
        switch (ce) {
        case CE_PushButtonBevel:
            proxy()->drawPrimitive(PE_PanelButtonCommand, opt, p, w);
            return;
        case CE_PushButtonLabel:
            if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt)) {
                paintButtonLabel(b, p, w);
                return;
            }
            break;
        default:
            break;
        }
        QProxyStyle::drawControl(ce, opt, p, w);
    }

    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt, QPainter *p,
                            const QWidget *w) const override
    {
        if (cc == CC_ComboBox) {
            if (const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(opt)) {
                paintField(*p, opt);
                const QRectF arrow = subControlRect(CC_ComboBox, combo, SC_ComboBoxArrow, w);
                const QPointF c = arrow.center();
                p->save();
                p->setRenderHint(QPainter::Antialiasing);
                p->setPen(QPen(combo->state & State_Enabled ? kCyan : kDim, 2.0, Qt::SolidLine,
                               Qt::SquareCap, Qt::MiterJoin));
                p->drawPolyline(QPolygonF{c + QPointF(-5, -2.5), c + QPointF(0, 2.5),
                                          c + QPointF(5, -2.5)});
                p->restore();
                return;
            }
        }
        if (cc == CC_GroupBox) {
            if (const auto *box = qstyleoption_cast<const QStyleOptionGroupBox *>(opt)) {
                paintGroupBox(box, p, w);
                return;
            }
        }
        QProxyStyle::drawComplexControl(cc, opt, p, w);
    }

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc,
                         const QWidget *w) const override
    {
        if (cc == CC_ComboBox && sc == SC_ComboBoxEditField)
            return QProxyStyle::subControlRect(cc, opt, sc, w).adjusted(8, 0, 0, 0);
        if (cc == CC_GroupBox) {
            if (const auto *box = qstyleoption_cast<const QStyleOptionGroupBox *>(opt)) {
                const QRect r = box->rect;
                const QFontMetrics fm(groupTitleFont());
                const int x = r.left() + 30;
                const int indicator = 18;
                const bool checkable = box->subControls & SC_GroupBoxCheckBox;
                const int title = box->text.isEmpty() && !checkable ? 0 : fm.height() + 22;
                switch (sc) {
                case SC_GroupBoxFrame:
                    return r;
                case SC_GroupBoxCheckBox:
                    return checkable ? QRect(x, r.top() + 12 + (fm.height() - indicator) / 2,
                                             indicator, indicator)
                                     : QRect();
                case SC_GroupBoxLabel: {
                    const int left = checkable ? x + indicator + 8 : x;
                    return {left, r.top() + 12, fm.horizontalAdvance(box->text.toUpper()) + 6,
                            fm.height()};
                }
                case SC_GroupBoxContents:
                    return r.adjusted(14, std::max(title, 14) + 4, -14, -14);
                default:
                    break;
                }
            }
        }
        return QProxyStyle::subControlRect(cc, opt, sc, w);
    }

private:
    void paintButtonPanel(const QStyleOption *opt, QPainter *p, const QWidget *w) const
    {
        const Variant v = variantOf(w, opt);
        const bool enabled = opt->state & State_Enabled;
        const bool hover = enabled && (opt->state & State_MouseOver);
        const bool down = opt->state & State_Sunken;
        const bool checked = opt->state & State_On;
        const QRectF r = QRectF(opt->rect).adjusted(1.5, 1.5, -1.5, -1.5);
        const double cut = buttonCut(r);
        const QPainterPath path = chamfer(r, cut);

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (!enabled)
            p->setOpacity(0.4);
        QLinearGradient fill(r.topLeft(), v == Variant::Primary ? r.topRight() : r.bottomRight());
        QColor border;
        switch (v) {
        case Variant::Primary:
            fill.setColorAt(0.0, kAccent);
            fill.setColorAt(1.0, kAccentBright);
            border = QColor(0xb9, 0xff, 0xd6);
            break;
        case Variant::Holo:
            fill.setColorAt(0.0, kBlue);
            fill.setColorAt(0.55, kViolet);
            fill.setColorAt(1.0, kMagenta);
            border = QColor(0xc9, 0xb8, 0xff);
            break;
        case Variant::Danger:
            fill.setColorAt(0.0, QColor(0x3a, 0x0f, 0x2a));
            fill.setColorAt(1.0, QColor(0x22, 0x08, 0x20));
            border = kDanger;
            break;
        case Variant::Ghost:
            fill.setColorAt(0.0, alpha(kVoid, 120));
            fill.setColorAt(1.0, alpha(kVoid, 120));
            border = kBorder;
            break;
        case Variant::Card:
        case Variant::Secondary:
            fill.setColorAt(0.0, checked ? QColor(0x12, 0x30, 0x1f) : QColor(0x23, 0x1c, 0x6b));
            fill.setColorAt(1.0, checked ? QColor(0x0c, 0x22, 0x18) : QColor(0x15, 0x10, 0x49));
            border = kBorderBright;
            break;
        }
        p->fillPath(path, fill);
        if (v == Variant::Primary || v == Variant::Holo) {
            const QRectF top(r.left() + 3, r.top() + 3, r.width() - 6, r.height() * 0.42);
            p->fillPath(chamfer(top, cut - 2, 0, 0, 0), QColor(255, 255, 255, down ? 18 : 44));
        }
        if (down)
            p->fillPath(path, QColor(0, 0, 0, 60));
        if (hover || checked) {
            p->save();
            p->setClipPath(path);
            p->setPen(QPen(alpha(v == Variant::Primary ? QColor(Qt::white) : kAccent, 70), 7.0));
            p->drawPath(path);
            p->restore();
            if (v != Variant::Primary)
                border = kAccent;
        }
        p->setPen(QPen(border, hover ? 2.0 : 1.3));
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);
        if ((opt->state & State_HasFocus) && (opt->state & State_KeyboardFocusChange))
            paintBrackets(*p, r.adjusted(-0.5, -0.5, 0.5, 0.5), kCyan, std::min(14.0, r.height() * 0.4));
        p->restore();
    }

    void paintButtonLabel(const QStyleOptionButton *b, QPainter *p, const QWidget *w) const
    {
        const Variant v = variantOf(w, b);
        const bool enabled = b->state & State_Enabled;
        const bool hover = enabled && (b->state & State_MouseOver);
        QColor color = v == Variant::Primary ? QColor(0x03, 0x14, 0x0a)
                     : v == Variant::Danger  ? QColor(0xff, 0xc3, 0xd2)
                     : v == Variant::Ghost   ? (hover ? kText : kMuted)
                                             : kText;
        if (!enabled) // Le fond est déjà estompé : encre foncée sur vert, gris sinon.
            color = v == Variant::Primary ? QColor(0x03, 0x14, 0x0a, 200) : kDim;
        const QRect r = b->rect;
        const double cut = buttonCut(r);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(color);

        if (v == Variant::Card) {
            // Carte de choix : titre + description.
            const QStringList lines = b->text.split(QLatin1Char('\n'));
            const QRect inner = r.adjusted(int(cut) + 18, 10, -22, -10);
            QFont title = displayFont(18);
            title.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
            p->setFont(title);
            const int titleHeight = QFontMetrics(title).height();
            const int bodyTop = inner.top() + (inner.height() - titleHeight - 44) / 2;
            int titleLeft = inner.left();
            if (const QString glyph = w ? w->property("glyph").toString() : QString(); !glyph.isEmpty()) {
                paintIcon(*p, glyph, QRectF(titleLeft, bodyTop + (titleHeight - 22) / 2.0, 22, 22),
                          hover ? kAccent : kCyan);
                titleLeft += 34;
            }
            p->drawText(QRect(titleLeft, bodyTop, inner.width(), titleHeight),
                        Qt::AlignLeft | Qt::AlignVCenter, lines.value(0).toUpper());
            p->setFont(bodyFont(15, QFont::Medium));
            p->setPen(hover ? kText : kMuted);
            p->drawText(QRect(inner.left(), bodyTop + titleHeight + 4, inner.width(), 44),
                        Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                        lines.mid(1).join(QLatin1Char(' ')));
            paintSlashes(*p, QPointF(inner.left(), bodyTop - 10), 6, hover ? kAccent : kCyan);
            p->restore();
            return;
        }

        const QFont font = labelFont(w);
        p->setFont(font);
        const QString text = b->text.toUpper();
        int textLeft = r.left();
        int flags = Qt::AlignCenter | Qt::TextShowMnemonic;
        if (isMenuButton(w)) {
            textLeft = r.left() + int(cut) + 22;
            flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic;
            paintSlashes(*p, QPointF(r.right() - cut - 46, r.center().y() - 7), 14,
                         v == Variant::Primary ? QColor(0x03, 0x14, 0x0a) : hover ? kAccent : kBorderBright);
        }
        QRect textRect(textLeft, r.top(), r.right() - textLeft, r.height());
        const QString iconName = w ? w->property("glyph").toString() : QString();
        if (!iconName.isEmpty()) {
            const int size = isMenuButton(w) ? 20 : 16;
            const int textWidth = QFontMetrics(font).horizontalAdvance(text);
            const int x = isMenuButton(w) ? textLeft : r.center().x() - (size + 10 + textWidth) / 2;
            const QColor iconColor = v == Variant::Primary ? color : hover ? kAccent : kCyan;
            paintIcon(*p, iconName, QRectF(x, r.center().y() - size / 2.0, size, size),
                      enabled ? iconColor : color);
            textRect.setLeft(x + size + 12);
            flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic;
        } else if (!b->icon.isNull()) {
            const QSize iconSize = b->iconSize;
            const QPixmap pm = b->icon.pixmap(iconSize, enabled ? QIcon::Normal : QIcon::Disabled);
            const int textWidth = QFontMetrics(font).horizontalAdvance(text);
            const int x = isMenuButton(w) ? textLeft
                                          : r.center().x() - (iconSize.width() + 8 + textWidth) / 2;
            p->drawPixmap(QPoint(x, r.center().y() - iconSize.height() / 2), pm);
            textRect.setLeft(x + iconSize.width() + 8);
            if (!isMenuButton(w))
                flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic;
        }
        p->drawText(textRect, flags,
                    QFontMetrics(font).elidedText(text, Qt::ElideRight, std::max(0, textRect.width() - 4)));
        p->restore();
    }

    void paintIndicator(bool radio, const QStyleOption *opt, QPainter *p) const
    {
        const QRectF r = QRectF(opt->rect).adjusted(1, 1, -1, -1);
        const bool enabled = opt->state & State_Enabled;
        const bool hover = enabled && (opt->state & State_MouseOver);
        const bool on = opt->state & State_On;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (!enabled)
            p->setOpacity(0.45);
        const QPointF c = r.center();
        QPainterPath path;
        if (radio) {
            const double h = r.width() / 2.0;
            path.addPolygon(QPolygonF{c + QPointF(0, -h), c + QPointF(h, 0), c + QPointF(0, h),
                                      c + QPointF(-h, 0), c + QPointF(0, -h)});
        } else {
            path = chamfer(r, r.width() * 0.3);
        }
        p->fillPath(path, on ? alpha(kAccent, 230) : kField);
        p->setPen(QPen(on ? kAccent : hover ? kCyan : kBorderBright, 1.3));
        p->drawPath(path);
        if (on) {
            p->setPen(QPen(QColor(0x03, 0x14, 0x0a), 2.2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
            if (radio) {
                p->setBrush(QColor(0x03, 0x14, 0x0a));
                p->setPen(Qt::NoPen);
                const double h = r.width() * 0.2;
                p->drawPolygon(QPolygonF{c + QPointF(0, -h), c + QPointF(h, 0), c + QPointF(0, h),
                                         c + QPointF(-h, 0)});
            } else {
                p->drawPolyline(QPolygonF{c + QPointF(-4.5, 0), c + QPointF(-1.5, 3.5),
                                          c + QPointF(4.5, -3.5)});
            }
        } else if (opt->state & State_NoChange) {
            p->fillRect(QRectF(c.x() - 4, c.y() - 1, 8, 2), kCyan);
        }
        p->restore();
    }

    void paintGroupBox(const QStyleOptionGroupBox *box, QPainter *p, const QWidget *w) const
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRectF frame = QRectF(box->rect).adjusted(1, 1, -1, -1);
        paintPanel(*p, frame, kCyan, 18.0);
        const QRect label = subControlRect(CC_GroupBox, box, SC_GroupBoxLabel, w);
        if (!box->text.isEmpty()) {
            p->setFont(groupTitleFont());
            p->setPen(box->state & State_Enabled ? kText : kMuted);
            p->drawText(label, Qt::AlignLeft | Qt::AlignVCenter, box->text.toUpper());
        }
        if (!box->text.isEmpty() || (box->subControls & SC_GroupBoxCheckBox)) {
            const double y = label.bottom() + 7.0;
            p->setPen(QPen(kBorder, 1.0));
            p->drawLine(QPointF(frame.left() + 30, y), QPointF(frame.right() - 18, y));
            paintSlashes(*p, QPointF(frame.left() + 30, y - 3), 6, kCyan);
        }
        if (box->subControls & SC_GroupBoxCheckBox) {
            QStyleOptionButton check;
            check.state = box->state;
            check.palette = box->palette;
            check.rect = subControlRect(CC_GroupBox, box, SC_GroupBoxCheckBox, w);
            proxy()->drawPrimitive(PE_IndicatorCheckBox, &check, p, w);
        }
        p->restore();
    }
};

QString styleSheet()
{
    QString css = QStringLiteral(R"(
QPushButton { font-family: "@display"; font-size: 10pt; font-weight: bold; }
QPushButton#menu { font-size: 12pt; }
QComboBox, QLineEdit { font-weight: 600; }
QToolTip { background-color: @panelAlt; color: @text; border: 1px solid @cyan; padding: 6px; }
QDialog { background-color: @bg; }

QLabel { background: transparent; }
QLabel#title { color: @text; }
QLabel#muted { color: @muted; }
QLabel#hint { color: @muted; font-style: italic; }
QMessageBox QLabel { color: @text; }

QSpinBox, QDoubleSpinBox {
    background-color: @field; color: @text; border: 1px solid @border; padding: 5px 8px;
    selection-background-color: @violet; selection-color: @text;
}
QSpinBox:focus, QDoubleSpinBox:focus { border-color: @cyan; }
QSpinBox:disabled, QDoubleSpinBox:disabled { color: @dim; }
QComboBox QAbstractItemView {
    background-color: @panel; color: @text; border: 1px solid @borderBright;
    selection-background-color: #1b2a60; selection-color: @accent; outline: 0;
}

QSlider::groove:horizontal { height: 6px; background: @field; border: 1px solid @border; }
QSlider::sub-page:horizontal {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @blue, stop:1 @cyan);
}
QSlider::sub-page:horizontal:disabled { background: @dim; }
QSlider::handle:horizontal {
    background: @text; border: 2px solid @cyan; width: 12px; margin: -8px 0;
}
QSlider::handle:horizontal:disabled { border-color: @dim; background: @muted; }

QTableWidget, QTreeWidget, QListWidget {
    background-color: @field; alternate-background-color: #0e0b30; color: @text;
    border: 1px solid @border; gridline-color: #1d1858;
    selection-background-color: #1b2a60; selection-color: @accent; outline: 0;
}
QHeaderView::section {
    background-color: @panel; color: @cyan; border: none; border-bottom: 1px solid @border;
    padding: 6px; font-family: "@display"; font-weight: bold;
}
QTableCornerButton::section { background-color: @panel; border: none; }

QProgressBar { background: @field; border: 1px solid @border; color: @text; text-align: center; }
QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @accent, stop:1 @acid);
}

QToolButton#sideToggle {
    color: @cyan; font-family: "@display"; font-weight: bold; padding: 6px 10px; text-align: left;
    border: 1px solid @border; background-color: @panel;
}
QToolButton#sideToggle:hover { border-color: @accent; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @border; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: @violet; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: @border; min-width: 24px; }
)");
    const std::pair<const char *, QString> tokens[] = {
        {"@panelAlt", hex(kPanelAlt)},     {"@panel", hex(kPanel)},
        {"@borderBright", hex(kBorderBright)}, {"@border", hex(kBorder)},
        {"@field", hex(kField)},           {"@text", hex(kText)},
        {"@muted", hex(kMuted)},           {"@dim", hex(kDim)},
        {"@accent", hex(kAccent)},         {"@acid", hex(kAccentBright)},
        {"@cyan", hex(kCyan)},             {"@blue", hex(kBlue)},
        {"@violet", hex(kViolet)},         {"@bg", hex(kBackground)},
        {"@display", familyFor(true, QFont::Bold, false)},
    };
    for (const auto &[token, value] : tokens)
        css.replace(QLatin1String(token), value);
    return css;
}

} // namespace

QFont displayFont(double pixelSize, bool italic)
{
    QFont f(familyFor(true, QFont::Bold, italic));
    f.setPixelSize(std::max(1, qRound(pixelSize)));
    f.setWeight(QFont::Bold);
    f.setItalic(italic);
    return f;
}

QFont bodyFont(double pixelSize, int weight)
{
    QFont f(familyFor(false, weight, false));
    f.setPixelSize(std::max(1, qRound(pixelSize)));
    f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

QPainterPath chamfer(const QRectF &r, double tl, double tr, double br, double bl)
{
    const double limit = std::min(r.width(), r.height()) / 2.0;
    tl = std::min(tl, limit);
    tr = std::min(tr, limit);
    br = std::min(br, limit);
    bl = std::min(bl, limit);
    QPainterPath path;
    path.moveTo(r.left() + tl, r.top());
    path.lineTo(r.right() - tr, r.top());
    path.lineTo(r.right(), r.top() + tr);
    path.lineTo(r.right(), r.bottom() - br);
    path.lineTo(r.right() - br, r.bottom());
    path.lineTo(r.left() + bl, r.bottom());
    path.lineTo(r.left(), r.bottom() - bl);
    path.lineTo(r.left(), r.top() + tl);
    path.closeSubpath();
    return path;
}

QPainterPath chamfer(const QRectF &rect, double cut)
{
    return chamfer(rect, cut, 0.0, cut, 0.0);
}

QPainterPath skewed(const QRectF &r, double slant)
{
    QPainterPath path;
    path.moveTo(r.left() + slant, r.top());
    path.lineTo(r.right(), r.top());
    path.lineTo(r.right() - slant, r.bottom());
    path.lineTo(r.left(), r.bottom());
    path.closeSubpath();
    return path;
}

void strokeGlow(QPainter &p, const QPainterPath &path, const QColor &color, double width)
{
    p.save();
    p.setBrush(Qt::NoBrush);
    for (int k = 3; k >= 1; --k)
        p.strokePath(path, QPen(alpha(color, 26 + 10 * (3 - k)), width + k * width * 1.4,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.strokePath(path, QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.restore();
}

void paintSlashes(QPainter &p, QPointF topLeft, double height, const QColor &color)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const double slant = height * 0.35;
    const double gap = std::max(4.0, height * 0.55);
    for (int i = 0; i < 3; ++i) {
        QColor c = color;
        c.setAlphaF(color.alphaF() * (1.0 - i * 0.22));
        p.fillPath(skewed(QRectF(topLeft.x() + i * gap, topLeft.y(), 3.0 + slant, height), slant), c);
    }
    p.restore();
}

void paintBackground(QPainter &p, const QRectF &r, bool floorGrid)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(r, kVoid);
    const double big = std::max(r.width(), r.height());
    QRadialGradient core(r.left() + r.width() * 0.5, r.top() + r.height() * 0.35, big * 0.75);
    core.setColorAt(0.0, QColor(0x1b, 0x12, 0x60, 240));
    core.setColorAt(0.55, QColor(0x0c, 0x0a, 0x2c, 230));
    core.setColorAt(1.0, kVoid);
    p.fillRect(r, core);
    auto halo = [&](double fx, double fy, double fr, const QColor &color, int a) {
        QRadialGradient g(r.left() + r.width() * fx, r.top() + r.height() * fy, big * fr);
        g.setColorAt(0.0, alpha(color, a));
        g.setColorAt(1.0, alpha(color, 0));
        p.fillRect(r, g);
    };
    halo(0.1, 0.08, 0.42, kBlue, 48);
    halo(0.92, 0.96, 0.38, kMagenta, 42);
    halo(0.78, 0.12, 0.2, kAccent, 16);

    if (floorGrid) {
        // Sol quadrillé en perspective sous l'horizon.
        const double horizon = r.top() + r.height() * 0.68;
        const double vx = r.center().x();
        QPainterPath grid;
        for (int i = -24; i <= 24; ++i) {
            grid.moveTo(vx + i * 18.0, horizon);
            grid.lineTo(vx + i * r.width() / 10.0, r.bottom());
        }
        for (int k = 1; k <= 12; ++k) {
            const double y = horizon + (r.bottom() - horizon) * std::pow(k / 12.0, 2.2);
            grid.moveTo(r.left(), y);
            grid.lineTo(r.right(), y);
        }
        p.strokePath(grid, QPen(alpha(kViolet, 56), 1.0));
        p.fillRect(QRectF(r.left(), horizon - 1.5, r.width(), 3), alpha(kCyan, 70));
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen scan(QColor(255, 255, 255, 6), 1.0);
    p.setPen(scan);
    for (double y = r.top(); y < r.bottom(); y += 4.0)
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    p.restore();
}

void paintPanel(QPainter &p, const QRectF &r, const QColor &accent, double cut)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QPainterPath path = chamfer(r, cut);
    QLinearGradient fill(r.topLeft(), r.bottomLeft());
    fill.setColorAt(0.0, QColor(0x1a, 0x15, 0x56, 235));
    fill.setColorAt(1.0, QColor(0x0d, 0x0a, 0x2e, 235));
    p.fillPath(path, fill);
    p.setPen(QPen(kBorder, 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    QLinearGradient holo(r.topLeft(), r.bottomRight());
    holo.setColorAt(0.0, alpha(accent, 220));
    holo.setColorAt(0.35, alpha(accent, 0));
    holo.setColorAt(0.7, alpha(kViolet, 0));
    holo.setColorAt(1.0, alpha(kViolet, 180));
    p.setPen(QPen(QBrush(holo), 1.4));
    p.drawPath(chamfer(r.adjusted(1.5, 1.5, -1.5, -1.5), cut - 0.5));
    // Coins accentués : haut-gauche (accent) et bas-droit (violet).
    const double arm = std::min(60.0, r.width() * 0.25);
    QPainterPath tl;
    tl.moveTo(r.left(), r.top() + cut + std::min(26.0, r.height() * 0.2));
    tl.lineTo(r.left(), r.top() + cut);
    tl.lineTo(r.left() + cut, r.top());
    tl.lineTo(r.left() + cut + arm, r.top());
    p.setPen(QPen(accent, 3.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    p.drawPath(tl);
    QPainterPath br;
    br.moveTo(r.right(), r.bottom() - cut - std::min(26.0, r.height() * 0.2));
    br.lineTo(r.right(), r.bottom() - cut);
    br.lineTo(r.right() - cut, r.bottom());
    br.lineTo(r.right() - cut - arm, r.bottom());
    p.setPen(QPen(kViolet, 3.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    p.drawPath(br);
    p.restore();
}

void paintTag(QPainter &p, const QRectF &r, const QString &text, const QColor &color)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const double slant = r.height() * 0.33;
    const QPainterPath path = skewed(r, slant);
    p.fillPath(path, alpha(color, 40));
    p.setPen(QPen(color, 1.2));
    p.drawPath(path);
    p.fillPath(skewed(QRectF(r.left() + 4, r.top() + 5, 8, r.height() - 10), 3), color);
    QFont f = displayFont(r.height() * 0.5);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    p.setFont(f);
    p.setPen(color);
    p.drawText(r.adjusted(slant + 12, 0, -slant, 0), Qt::AlignLeft | Qt::AlignVCenter, text.toUpper());
    p.restore();
}

QPainterPath iconPath(const QString &name)
{
    QPainterPath p;
    auto poly = [&p](std::initializer_list<QPointF> pts, bool close = false) {
        const QPolygonF polygon(pts);
        p.moveTo(polygon.first());
        for (int i = 1; i < polygon.size(); ++i)
            p.lineTo(polygon.at(i));
        if (close)
            p.closeSubpath();
    };
    if (name == QLatin1String("play")) {
        poly({{7, 4}, {19, 12}, {7, 20}}, true);
    } else if (name == QLatin1String("lan")) {
        poly({{3, 8}, {15, 8}, {12, 5}});
        poly({{21, 16}, {9, 16}, {12, 19}});
    } else if (name == QLatin1String("trophy")) {
        p.moveTo(7, 4);
        p.lineTo(17, 4);
        p.lineTo(17, 9);
        p.cubicTo(17, 12, 15, 14, 12, 14);
        p.cubicTo(9, 14, 7, 12, 7, 9);
        p.closeSubpath();
        poly({{7, 6}, {4, 6}, {4, 8}});
        p.cubicTo(4, 10, 5.5, 11, 7, 11);
        poly({{17, 6}, {20, 6}, {20, 8}});
        p.cubicTo(20, 10, 18.5, 11, 17, 11);
        poly({{12, 14}, {12, 18}});
        poly({{8, 20}, {16, 20}, {15, 18}, {9, 18}}, true);
    } else if (name == QLatin1String("build")) {
        poly({{12, 3}, {20, 7.5}, {20, 16.5}, {12, 21}, {4, 16.5}, {4, 7.5}}, true);
        poly({{4, 7.5}, {12, 12}, {20, 7.5}});
        poly({{12, 12}, {12, 21}});
    } else if (name == QLatin1String("tuto")) {
        poly({{2, 9}, {12, 4}, {22, 9}, {12, 14}}, true);
        p.moveTo(6, 11);
        p.lineTo(6, 16);
        p.cubicTo(8, 18, 16, 18, 18, 16);
        p.lineTo(18, 11);
        poly({{22, 9}, {22, 15}});
    } else if (name == QLatin1String("gear")) {
        p.addEllipse(QPointF(12, 12), 3.5, 3.5);
        QPolygonF teeth;
        for (int i = 0; i < 16; ++i) {
            const double a = i * M_PI / 8 + M_PI / 16;
            const double r = i % 2 ? 7.0 : 9.5;
            teeth << QPointF(12 + std::cos(a) * r, 12 + std::sin(a) * r);
        }
        teeth << teeth.first();
        p.addPolygon(teeth);
    } else if (name == QLatin1String("help")) {
        p.moveTo(9, 9);
        p.cubicTo(9, 6, 15, 6, 15, 9.5);
        p.cubicTo(15, 12, 12, 12, 12, 15);
        p.addEllipse(QPointF(12, 19), 0.6, 0.6);
    } else if (name == QLatin1String("update")) {
        const QRectF circle(4, 4, 16, 16);
        p.arcMoveTo(circle, 40);
        p.arcTo(circle, 40, 290);
        poly({{20, 4}, {20, 8.5}, {15.5, 8.5}});
    } else if (name == QLatin1String("quit")) {
        poly({{6, 6}, {18, 18}});
        poly({{18, 6}, {6, 18}});
    } else if (name == QLatin1String("back")) {
        poly({{15, 5}, {8, 12}, {15, 19}});
    } else if (name == QLatin1String("plus")) {
        poly({{12, 5}, {12, 19}});
        poly({{5, 12}, {19, 12}});
    } else if (name == QLatin1String("trash")) {
        poly({{4, 7}, {20, 7}});
        poly({{9, 7}, {9, 4}, {15, 4}, {15, 7}});
        poly({{6, 7}, {7, 21}, {17, 21}, {18, 7}});
    } else if (name == QLatin1String("save")) {
        poly({{5, 3}, {16, 3}, {20, 7}, {20, 21}, {5, 21}}, true);
        poly({{8, 3}, {8, 8}, {15, 8}, {15, 3}});
        poly({{8, 21}, {8, 14}, {17, 14}, {17, 21}});
    } else if (name == QLatin1String("sliders")) {
        for (double y : {6.0, 12.0, 18.0})
            poly({{4, y}, {20, y}});
        poly({{9, 4}, {9, 8}});
        poly({{15, 10}, {15, 14}});
        poly({{7, 16}, {7, 20}});
    } else if (name == QLatin1String("check")) {
        poly({{4, 12.5}, {9.5, 18}, {20, 6}});
    } else if (name == QLatin1String("lock")) {
        poly({{6, 11}, {18, 11}, {18, 21}, {6, 21}}, true);
        p.moveTo(8.5, 11);
        p.lineTo(8.5, 7.5);
        p.arcTo(QRectF(8.5, 4, 7, 7), 180, -180);
        p.lineTo(15.5, 11);
    } else if (name == QLatin1String("user")) {
        p.addEllipse(QPointF(12, 8), 4, 4);
        p.moveTo(4, 21);
        p.cubicTo(5, 16, 19, 16, 20, 21);
    }
    return p;
}

void paintIcon(QPainter &p, const QString &name, const QRectF &rect, const QColor &color)
{
    const QPainterPath path = iconPath(name);
    if (path.isEmpty())
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const double s = std::min(rect.width(), rect.height()) / 24.0;
    p.translate(rect.center() - QPointF(12, 12) * s);
    p.scale(s, s);
    p.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(name == QLatin1String("play") ? QBrush(color) : QBrush(Qt::NoBrush));
    p.drawPath(path);
    p.restore();
}

QLabel *createTitle(const QString &text, QWidget *parent)
{
    auto *title = new QLabel(text, parent);
    title->setObjectName(QStringLiteral("title"));
    QFont f = displayFont(30);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
    title->setFont(f);
    return title;
}

void apply(QApplication &app)
{
    loadFonts();
    app.setStyle(new HoloStyle);

    QPalette palette;
    palette.setColor(QPalette::Window, kBackground);
    palette.setColor(QPalette::WindowText, kText);
    palette.setColor(QPalette::Base, kField);
    palette.setColor(QPalette::AlternateBase, QColor(0x0e, 0x0b, 0x30));
    palette.setColor(QPalette::Text, kText);
    palette.setColor(QPalette::Button, kPanelAlt);
    palette.setColor(QPalette::ButtonText, kText);
    palette.setColor(QPalette::Highlight, QColor(0x1b, 0x2a, 0x60));
    palette.setColor(QPalette::HighlightedText, kAccent);
    palette.setColor(QPalette::ToolTipBase, kPanelAlt);
    palette.setColor(QPalette::ToolTipText, kText);
    palette.setColor(QPalette::PlaceholderText, kDim);
    palette.setColor(QPalette::Link, kCyan);
    palette.setColor(QPalette::Light, kBorderBright);
    palette.setColor(QPalette::Mid, kBorder);
    palette.setColor(QPalette::Dark, kField);
    for (QPalette::ColorRole role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, kDim);
    app.setPalette(palette);

    // En points (et non en pixels) : les pages agrandissent la police de l'application.
    QFont base = bodyFont(16, QFont::Medium);
    base.setPointSizeF(12.0);
    app.setFont(base);
    app.setStyleSheet(styleSheet());
}

} // namespace toupie::Theme
