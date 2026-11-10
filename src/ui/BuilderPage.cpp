#include "ui/BuilderPage.h"
#include "ui/Theme.h"

#include "core/PartCatalog.h"
#include "core/TopRepository.h"
#include "game/SpecialMove.h"
#include "ui/TopPainter.h"
#include "ui/Widgets.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <cmath>

namespace toupie {

namespace {
constexpr PartKind kKinds[4] = {PartKind::Bolt, PartKind::Wheel, PartKind::Middle, PartKind::Tip};

QColor slotColor(PartKind kind)
{
    switch (kind) {
    case PartKind::Bolt:
        return QColor(0xc9, 0xd8, 0xff);
    case PartKind::Wheel:
        return TopPainter::typeColor(TopType::Attack);
    case PartKind::Middle:
        return Theme::kWarning;
    case PartKind::Tip:
        return Theme::kMagenta;
    }
    return Theme::kCyan;
}

QString partTitle(const Part &part)
{
    if (part.kind == PartKind::Wheel)
        return part.name + (part.spin == SpinDirection::Clockwise ? QStringLiteral(" ↻") : QStringLiteral(" ↺"));
    return part.name;
}

QString partDetail(const Part &part)
{
    if (part.kind == PartKind::Bolt) {
        const auto move = SpecialMove::find(part.special);
        if (!move)
            return part.special;
        return QStringLiteral("%1 · %2").arg(move->name, move->hitsEveryone() ? PartSlot::tr("anneau")
                                                                               : PartSlot::tr("vague"));
    }
    return QStringLiteral("A %1 · D %2 · E %3")
        .arg(part.stats.attack)
        .arg(part.stats.defense)
        .arg(part.stats.endurance);
}

int maxStat()
{
    return PartCatalog::instance().maxStatValue();
}

QFont smallCaps(double size = 13)
{
    QFont f = Theme::bodyFont(size, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    f.setCapitalization(QFont::AllUppercase);
    return f;
}

QLabel *smallLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("muted"));
    label->setFont(smallCaps());
    return label;
}

/// Toupie de la liste : vignette, nom, type et étoile des toupies personnelles.
class TopRowDelegate : public QStyledItemDelegate
{
public:
    TopRowDelegate(const QList<Top> &tops, const int &presetCount, QObject *parent)
        : QStyledItemDelegate(parent)
        , m_tops(tops)
        , m_presetCount(presetCount)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        return {option.rect.width(), 92};
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const int row = index.row();
        if (row < 0 || row >= m_tops.size())
            return;
        const Top &top = m_tops.at(row);
        const bool on = option.state & QStyle::State_Selected;
        const bool hover = option.state & QStyle::State_MouseOver;
        const QRectF r = QRectF(option.rect).adjusted(1, 1, -2, -12);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QPainterPath card = Theme::chamfer(r, 14, 0, 14, 0);
        p->fillPath(card, on ? QColor(0x1b, 0x2a, 0x60) : QColor(0x0e, 0x0b, 0x30));
        p->setPen(QPen(on ? Theme::kCyan : hover ? Theme::kBorderBright : Theme::kBorder, on ? 2.0 : 1.0));
        p->drawPath(card);
        if (on)
            p->fillRect(QRectF(r.left(), r.top() + 16, 4, r.height() - 32), Theme::kCyan);
        TopPainter::paint(*p, top, QPointF(r.left() + 48, r.center().y()), 28, row);
        QFont name = Theme::displayFont(16);
        name.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        p->setFont(name);
        p->setPen(Theme::kText);
        const QRectF text(r.left() + 92, r.top() + 12, r.width() - (row >= m_presetCount ? 128 : 104), 26);
        p->drawText(text, Qt::AlignLeft | Qt::AlignVCenter,
                    p->fontMetrics().elidedText(top.name().toUpper(), Qt::ElideRight, int(text.width())));
        const QString type = toString(top.type());
        Theme::paintTag(*p, QRectF(r.left() + 92, r.top() + 44, 46 + type.size() * 10.5, 20), type,
                        TopPainter::typeColor(top.type()));
        if (row >= m_presetCount) {
            p->setFont(Theme::displayFont(16));
            p->setPen(Theme::kCyan);
            p->drawText(QRectF(r.right() - 34, r.top() + 10, 20, 24), Qt::AlignCenter, QStringLiteral("★"));
        }
        p->restore();
    }

private:
    const QList<Top> &m_tops;
    const int &m_presetCount;
};
} // namespace

// --- Carte d'emplacement --------------------------------------------------------

PartSlot::PartSlot(PartKind kind, const QList<Part> &parts, const QColor &color, QWidget *parent)
    : QWidget(parent)
    , m_kind(kind)
    , m_parts(parts)
    , m_color(color)
{
    Q_ASSERT(!m_parts.isEmpty());
    resize(sizeHint()); // L'établi la redimensionne (180 à 220 px de large).
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setToolTip(tr("‹ › ou molette : pièce précédente / suivante · clic : toutes les pièces"));
}

void PartSlot::setIndex(int index)
{
    const int n = m_parts.size();
    index = ((index % n) + n) % n;
    if (index == m_index)
        return;
    m_index = index;
    update();
    emit changed();
}

void PartSlot::selectId(const QString &id)
{
    for (int i = 0; i < m_parts.size(); ++i) {
        if (m_parts.at(i).id == id)
            m_index = i;
    }
    update();
}

QRectF PartSlot::arrowRect(int direction) const
{
    return {width() - (direction < 0 ? 72.0 : 36.0), 116.0, 24.0, 24.0};
}

void PartSlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool hover = underMouse();
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    const QPainterPath card = Theme::chamfer(r, 14, 0, 14, 0);
    p.fillPath(card, QColor(0x0b, 0x09, 0x30));
    if (hover || hasFocus()) {
        p.save();
        p.setClipPath(card);
        QColor glow = m_color;
        glow.setAlpha(60);
        p.setPen(QPen(glow, 7.0));
        p.drawPath(card);
        p.restore();
    }
    p.setPen(QPen(m_color, hover || hasFocus() ? 2.2 : 1.5));
    p.drawPath(card);

    const Part &current = part();
    p.setFont(smallCaps());
    p.setPen(m_color);
    p.drawText(QRectF(16, 12, width() - 70, 20), Qt::AlignLeft | Qt::AlignVCenter,
               m_kind == PartKind::Wheel    ? tr("Roue")
               : m_kind == PartKind::Middle ? tr("Intermédiaire")
                                            : toString(m_kind));
    p.setFont(Theme::displayFont(12));
    p.setPen(Theme::kDim);
    p.drawText(QRectF(16, 12, width() - 32, 20), Qt::AlignRight | Qt::AlignVCenter, current.id);

    const double radius = m_kind == PartKind::Wheel ? 32 : m_kind == PartKind::Middle ? 30 : m_kind == PartKind::Tip ? 31 : 26;
    TopPainter::paintPart(p, current, QPointF(50, m_kind == PartKind::Tip ? 58 : 60), radius);

    QFont name = Theme::displayFont(16);
    name.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    p.setFont(name);
    p.setPen(Theme::kText);
    p.drawText(QRectF(16, 94, width() - 32, 24), Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(partTitle(current).toUpper(), Qt::ElideRight, width() - 32));
    p.setFont(Theme::bodyFont(15, QFont::DemiBold));
    p.setPen(Theme::kMuted);
    QString detail = partDetail(current);
    if (m_kind == PartKind::Bolt && p.fontMetrics().horizontalAdvance(detail) > width() - 90)
        detail = detail.section(QStringLiteral(" · "), 0, 0); // Sans la forme du coup.
    p.drawText(QRectF(16, 118, width() - 90, 24), Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(detail, Qt::ElideRight, width() - 90));
    Theme::paintIcon(p, QStringLiteral("back"), arrowRect(-1).adjusted(4, 4, -4, -4), Theme::kCyan);
    Theme::paintIcon(p, QStringLiteral("next"), arrowRect(1).adjusted(4, 4, -4, -4), Theme::kCyan);
}

void PartSlot::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    if (event->button() != Qt::LeftButton)
        return;
    if (arrowRect(-1).contains(event->position()))
        setIndex(m_index - 1);
    else if (arrowRect(1).contains(event->position()))
        setIndex(m_index + 1);
    else
        openList();
}

void PartSlot::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() != 0)
        setIndex(m_index + (event->angleDelta().y() < 0 ? 1 : -1));
    event->accept();
}

void PartSlot::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Up:
        setIndex(m_index - 1);
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
        setIndex(m_index + 1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        openList();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void PartSlot::openList()
{
    QMenu menu(this);
    for (int i = 0; i < m_parts.size(); ++i) {
        const Part &part = m_parts.at(i);
        QPixmap icon(QSize(80, 80) * devicePixelRatioF());
        icon.setDevicePixelRatio(devicePixelRatioF());
        icon.fill(Qt::transparent);
        QPainter painter(&icon);
        TopPainter::paintPart(painter, part, QPointF(40, part.kind == PartKind::Tip ? 34 : 40), 37);
        painter.end();
        QAction *action = menu.addAction(QIcon(icon), QStringLiteral("%1    %2").arg(partTitle(part), partDetail(part)));
        action->setData(i);
        action->setCheckable(true);
        action->setChecked(i == m_index);
        if (i == m_index)
            menu.setActiveAction(action);
    }
    if (const QAction *chosen = menu.exec(mapToGlobal(QPoint(0, height()))))
        setIndex(chosen->data().toInt());
}

namespace {

// --- Établi : vue éclatée reliée aux quatre cartes ------------------------------

class Workbench : public QWidget
{
public:
    Workbench(const QList<PartSlot *> &cards, QWidget *parent)
        : QWidget(parent)
        , m_slots(cards)
    {
        for (PartSlot *slot : cards)
            slot->setParent(this);
        setMinimumSize(440, 310);
    }

    void setTop(const Top &top)
    {
        m_top = top;
        update();
    }

    QSize sizeHint() const override { return {780, 400}; }

protected:
    void resizeEvent(QResizeEvent *) override
    {
        const int w = std::clamp((width() - 100) / 2, 180, 220);
        const int h = m_slots.first()->height();
        const int second = std::clamp(height() - h, 160, 230);
        const QPoint at[4] = {{0, 0}, {width() - w, 0}, {0, second}, {width() - w, second}};
        for (int i = 0; i < m_slots.size(); ++i)
            m_slots.at(i)->setGeometry(QRect(at[i], QSize(w, h)));
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double cx = width() / 2.0;
        QRadialGradient glow(QPointF(cx, 210), 200);
        glow.setColorAt(0.0, QColor(0x7b, 0x3c, 0xff, 45));
        glow.setColorAt(1.0, QColor(0x7b, 0x3c, 0xff, 0));
        p.fillRect(rect(), glow);
        p.fillRect(QRectF(cx - 1, 20, 2, height() - 20), QColor(0x34, 0xd6, 0xff, 64));

        // Entre les cartes, et assez court pour tenir en hauteur (≈ 1 × la largeur).
        const double profileWidth = std::clamp(std::min(width() - 2.0 * m_slots.first()->width() - 20.0,
                                                        height() - 50.0), 100.0, 320.0);
        const auto anchors = TopPainter::paintProfile(p, m_top, QPointF(cx, 30), profileWidth);
        for (const PartSlot *slot : m_slots) {
            const QRect g = slot->geometry();
            const QPointF a = anchors[static_cast<int>(slot->kind())];
            const double lx = g.center().x() < cx ? g.right() + 1 : g.left();
            const double y = g.top() + 40;
            QPainterPath leader(QPointF(lx, y));
            leader.lineTo((lx + a.x()) / 2.0, y);
            leader.lineTo(a);
            const QColor color = slotColor(slot->kind());
            QPen pen(color, 1.5);
            pen.setDashPattern({2.7, 2.7});
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(leader);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(a, 4, 4);
        }
    }

private:
    QList<PartSlot *> m_slots;
    Top m_top;
};

// --- Aperçu en rotation -----------------------------------------------------------

class SpinPreview : public QWidget
{
public:
    explicit SpinPreview(QWidget *parent)
        : QWidget(parent)
    {
        setMinimumSize(420, 120);
        setMaximumHeight(250);
        m_timer = startTimer(16);
    }

    void setTop(const Top &top)
    {
        m_top = top;
        update();
    }

    /// Texte à droite de la toupie : titre en gras puis description.
    void setText(const QString &title, const QString &body)
    {
        m_title = title;
        m_body = body;
        update();
    }

    /// Message d'état sous la description (vert : succès, rouge : erreur).
    void setStatus(const QString &text, const QColor &color = Theme::kAccent)
    {
        m_status = text;
        m_statusColor = color;
        update();
    }

    QSize sizeHint() const override { return {780, 250}; }

protected:
    void timerEvent(QTimerEvent *event) override
    {
        if (event->timerId() != m_timer || !isVisible())
            return;
        m_angle += spinSign(m_top.spin()) * 0.12;
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double R = std::min({110.0, height() / 2.0 / 1.2, width() * 0.18});
        const QPointF c(R * 1.3, height() / 2.0);
        const QColor type = TopPainter::typeColor(m_top.type());
        QRadialGradient aura(c, R * 1.3);
        aura.setColorAt(0.6, QColor(type.red(), type.green(), type.blue(), 60));
        aura.setColorAt(1.0, QColor(type.red(), type.green(), type.blue(), 0));
        p.setPen(Qt::NoPen);
        p.setBrush(aura);
        p.drawEllipse(c, R * 1.3, R * 1.3);
        // Trois arcs d'aura qui tournent avec la toupie.
        const double sign = spinSign(m_top.spin());
        const QRectF ring(c.x() - R * 1.14, c.y() - R * 1.14, R * 2.28, R * 2.28);
        p.setPen(QPen(type, R * 0.035, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 3; ++i) {
            const double start = -qRadiansToDegrees(m_angle + i * 2.0 * M_PI / 3.0);
            p.drawArc(ring, int(start * 16), int(-sign * 80 * 16));
        }
        TopPainter::paint(p, m_top, c, R, m_angle);

        const QRectF text(c.x() + R * 1.3 + 24, 8, width() - c.x() - R * 1.3 - 24, height() - 16);
        const QFont bold = Theme::bodyFont(19, QFont::Bold), body = Theme::bodyFont(19, QFont::Medium);
        const QFont status = Theme::bodyFont(18, QFont::Bold);
        const QFontMetrics boldMetrics(bold), bodyMetrics(body);
        const int flags = Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap;
        const double statusHeight = m_status.isEmpty() ? 0.0 : QFontMetrics(status).boundingRect(text.toRect(), flags, m_status).height() + 16;
        double y = text.top();
        p.setFont(bold);
        p.setPen(Theme::kText);
        QRectF r = boldMetrics.boundingRect(QRectF(text.left(), y, text.width(), text.height()).toRect(), flags, m_title);
        p.drawText(r, flags, m_title);
        y = r.bottom() + 4;
        p.setFont(body);
        p.setPen(Theme::kMuted);
        // Description tronquée si la place manque (petite fenêtre).
        const QRectF bodyRect(text.left(), y, text.width(), std::max(0.0, text.bottom() - statusHeight - y));
        p.setClipRect(bodyRect);
        p.drawText(bodyRect, flags, m_body);
        p.setClipping(false);
        if (!m_status.isEmpty()) {
            p.setFont(status);
            p.setPen(m_statusColor);
            p.drawText(QRectF(text.left(), text.bottom() - statusHeight + 16, text.width(), statusHeight), flags, m_status);
        }
    }

private:
    QString m_title;
    QString m_body;
    QString m_status;
    QColor m_statusColor = Theme::kAccent;
    Top m_top;
    double m_angle = 0.0;
    int m_timer = 0;
};

// --- Statistiques : radar, type obtenu, barres et budget --------------------------

class BuildStats : public QWidget
{
public:
    explicit BuildStats(QWidget *parent)
        : QWidget(parent)
    {
        setMinimumSize(240, 480);
    }

    void setTop(const Top &top)
    {
        m_top = top;
        update();
    }

    QSize sizeHint() const override { return {460, 720}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Stats st = m_top.stats();
        const int values[3] = {st.attack, st.defense, st.endurance};
        const QColor colors[3] = {TopPainter::typeColor(TopType::Attack), TopPainter::typeColor(TopType::Defense),
                                  TopPainter::typeColor(TopType::Endurance)};
        const QColor type = TopPainter::typeColor(m_top.type());
        const double w = width();
        constexpr double kBottom = 310; // Type, trois barres et budget.

        // Le radar occupe 1,5 R plus ses étiquettes (≈ 92 px) au-dessus du bloc du bas.
        const double R = std::min({175.0, (height() - kBottom - 92) / 1.5, (w - 120) / 1.75});
        if (R > 40) {
            const QPointF c(w / 2.0, 34 + R);
            const double angles[3] = {-M_PI / 2, M_PI / 6, 5 * M_PI / 6};
            auto pt = [&](int i, double k) { return c + QPointF(std::cos(angles[i]), std::sin(angles[i])) * R * k; };
            p.setPen(QPen(Theme::kBorder, 1.0));
            p.setBrush(Qt::NoBrush);
            for (double k : {1.0, 0.66, 0.33})
                p.drawPolygon(QPolygonF{pt(0, k), pt(1, k), pt(2, k)});
            for (int i = 0; i < 3; ++i)
                p.drawLine(c, pt(i, 1));
            QPolygonF shape;
            for (int i = 0; i < 3; ++i)
                shape << pt(i, std::clamp(values[i] / double(maxStat()), 0.05, 1.0));
            p.setPen(QPen(QColor(type.red(), type.green(), type.blue(), 100), 6.0));
            p.drawPolygon(shape);
            p.setPen(QPen(type, 2.0));
            p.setBrush(QColor(type.red(), type.green(), type.blue(), 64));
            p.drawPolygon(shape);
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::white);
            for (const QPointF &q : shape)
                p.drawEllipse(q, 5, 5);
            p.setFont(Theme::displayFont(16));
            const QString names[3] = {tr("ATT %1"), tr("DÉF %1"), tr("END %1")};
            const QPointF at[3] = {pt(0, 1) - QPointF(0, 18), pt(1, 1) + QPointF(6, 26), pt(2, 1) + QPointF(-6, 26)};
            for (int i = 0; i < 3; ++i) {
                p.setPen(colors[i]);
                p.drawText(QRectF(at[i].x() - 60, at[i].y() - 12, 120, 24), Qt::AlignCenter,
                           names[i].arg(values[i]));
            }
        }

        double y = height() - kBottom;
        p.setFont(smallCaps());
        p.setPen(Theme::kMuted);
        p.drawText(QRectF(0, y, w, 20), Qt::AlignLeft | Qt::AlignVCenter, tr("Type obtenu"));
        QFont big = Theme::displayFont(38);
        big.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
        p.setFont(big);
        p.setPen(type);
        p.drawText(QRectF(0, y + 18, w, 48), Qt::AlignLeft | Qt::AlignVCenter, toString(m_top.type()).toUpper());
        p.setFont(Theme::bodyFont(17, QFont::DemiBold));
        p.setPen(Theme::kMuted);
        const int best = std::max({st.attack, st.defense, st.endurance});
        const QString bestName = best == st.attack    ? tr("Attaque")
                               : best == st.defense   ? tr("Défense")
                                                      : tr("Endurance");
        p.drawText(QRectF(0, y + 66, w, 22), Qt::AlignLeft | Qt::AlignVCenter,
                   m_top.type() == TopType::Balance ? tr("Stats équilibrées : aucune ne domine")
                                                    : tr("Stat la plus élevée : %1 %2").arg(bestName).arg(best));
        y += 104;
        const QString labels[3] = {tr("Attaque"), tr("Défense"), tr("Endurance")};
        for (int i = 0; i < 3; ++i) {
            Theme::paintStatBar(p, QRectF(0, y, w, 36), labels[i], values[i], maxStat(), colors[i]);
            y += 48;
        }
        const int budget = PartCatalog::budget(PartKind::Wheel) + PartCatalog::budget(PartKind::Middle)
                         + PartCatalog::budget(PartKind::Tip);
        p.setFont(smallCaps());
        p.setPen(Theme::kMuted);
        p.drawText(QRectF(0, y, w, 20), Qt::AlignLeft | Qt::AlignVCenter, tr("Budget"));
        p.setFont(Theme::displayFont(16));
        p.setPen(Theme::kAccent);
        p.drawText(QRectF(0, y, w, 20), Qt::AlignRight | Qt::AlignVCenter,
                   tr("%1 / %2 PTS").arg(st.total()).arg(budget));
        Theme::paintStatBar(p, QRectF(0, y + 26, w, 8), QString(), st.total(), budget, Theme::kAccent);
    }

private:
    Top m_top;
};

} // namespace

// --- Page ---------------------------------------------------------------------------

BuilderPage::BuilderPage(const PartCatalog &catalog, TopRepository &repository, QWidget *parent)
    : QWidget(parent)
    , m_catalog(catalog)
    , m_repository(repository)
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(22);

    auto *backButton = new QPushButton(tr("Menu"), this);
    backButton->setProperty("variant", QStringLiteral("ghost"));
    backButton->setProperty("glyph", QStringLiteral("back"));
    backButton->setProperty("key", tr("Échap"));
    root->addWidget(new TopBar(backButton, tr("Assembler une toupie"), tr("Atelier"), this));

    auto *body = new QHBoxLayout;
    body->setSpacing(24);
    root->addLayout(body, 1);

    // --- Mes toupies ----------------------------------------------------------------
    m_listBox = new QGroupBox(tr("Mes toupies"), this);
    m_listBox->setProperty("accent", Theme::kCyan);
    auto *listLayout = new QVBoxLayout(m_listBox);
    listLayout->setContentsMargins(14, 22, 14, 16);
    m_list = new QListWidget(m_listBox);
    m_list->setStyleSheet(QStringLiteral("QListWidget { background: transparent; border: none; }"));
    m_list->setItemDelegate(new TopRowDelegate(m_tops, m_presetCount, m_list));
    m_list->setMouseTracking(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listLayout->addWidget(m_list, 1);
    m_listButtons = new QBoxLayout(QBoxLayout::LeftToRight);
    auto *listButtons = m_listButtons;
    auto *newButton = new QPushButton(tr("Nouvelle"), m_listBox);
    newButton->setProperty("glyph", QStringLiteral("plus"));
    m_deleteButton = new QPushButton(tr("Supprimer"), m_listBox);
    m_deleteButton->setProperty("variant", QStringLiteral("danger"));
    m_deleteButton->setProperty("glyph", QStringLiteral("trash"));
    for (QPushButton *button : {newButton, m_deleteButton})
        button->setMinimumHeight(48);
    listButtons->addWidget(newButton);
    listButtons->addWidget(m_deleteButton);
    listLayout->addLayout(listButtons);
    m_listBox->setMinimumWidth(280);
    m_listBox->setMaximumWidth(380);
    body->addWidget(m_listBox, 38); // Proportions de la maquette : 380 / 860 / 540.

    // --- Établi ---------------------------------------------------------------------
    m_benchBox = new QGroupBox(tr("Établi"), this);
    m_benchBox->setProperty("accent", Theme::kAccent);
    auto *bench = new QVBoxLayout(m_benchBox);
    bench->setContentsMargins(18, 10, 18, 18);
    bench->setSpacing(8);
    bench->addWidget(smallLabel(tr("Nom"), m_benchBox));
    m_nameEdit = new QLineEdit(m_benchBox);
    m_nameEdit->setMaxLength(24);
    m_nameEdit->setPlaceholderText(tr("Nom de la toupie"));
    m_nameEdit->setFont(Theme::bodyFont(20, QFont::DemiBold));
    m_nameEdit->setMinimumHeight(50);
    bench->addWidget(m_nameEdit);
    for (PartKind kind : kKinds) {
        auto *slot = new PartSlot(kind, m_catalog.parts(kind), slotColor(kind), m_benchBox);
        connect(slot, &PartSlot::changed, this, &BuilderPage::updatePreview);
        m_slots.append(slot);
    }
    m_bench = new Workbench(m_slots, m_benchBox);
    bench->addWidget(m_bench, 1);

    bench->addWidget(smallLabel(tr("Aperçu en rotation"), m_benchBox));
    auto *rule = new QWidget(m_benchBox);
    rule->setFixedHeight(1);
    rule->setStyleSheet(QStringLiteral("background-color: %1;").arg(Theme::kBorder.name()));
    bench->addWidget(rule);
    m_spin = new SpinPreview(m_benchBox);
    bench->addWidget(m_spin);
    body->addWidget(m_benchBox, 86);

    // --- Statistiques ---------------------------------------------------------------
    auto *statsBox = new QGroupBox(tr("Statistiques"), this);
    statsBox->setProperty("accent", Theme::kMagenta);
    statsBox->setProperty("index", tr("TOTAL"));
    auto *statsLayout = new QVBoxLayout(statsBox);
    statsLayout->setContentsMargins(20, 10, 20, 20);
    m_stats = new BuildStats(statsBox);
    statsLayout->addWidget(m_stats, 1);
    statsLayout->addSpacing(16);
    auto *saveButton = new QPushButton(tr("Enregistrer"), statsBox);
    saveButton->setProperty("variant", QStringLiteral("primary"));
    saveButton->setProperty("glyph", QStringLiteral("save"));
    saveButton->setMinimumHeight(62);
    saveButton->setDefault(true);
    statsLayout->addWidget(saveButton);
    statsBox->setMinimumWidth(290);
    statsBox->setMaximumWidth(540);
    body->addWidget(statsBox, 54);

    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this,
            &BuilderPage::backRequested);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &BuilderPage::updatePreview);
    connect(newButton, &QPushButton::clicked, this, &BuilderPage::newTop);
    connect(m_deleteButton, &QPushButton::clicked, this, &BuilderPage::deleteSelected);
    connect(saveButton, &QPushButton::clicked, this, &BuilderPage::saveTop);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &BuilderPage::saveTop);
    connect(backButton, &QPushButton::clicked, this, &BuilderPage::backRequested);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        m_deleteButton->setEnabled(row >= m_presetCount && row < m_tops.size());
        if (row >= 0 && row < m_tops.size())
            loadTop(m_tops.at(row));
    });

    refresh();
    newTop();
}

void BuilderPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Liste étroite (petite fenêtre) : « Nouvelle » et « Supprimer » l'un sous l'autre.
    m_listButtons->setDirection(m_listBox->width() < 360 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
}

void BuilderPage::refresh()
{
    const QSignalBlocker blocker(m_list);
    m_tops = m_catalog.presets();
    m_presetCount = m_tops.size();
    m_tops += m_repository.customTops();
    m_list->clear();
    for (const Top &top : std::as_const(m_tops))
        m_list->addItem(top.name());
    m_listBox->setProperty("index", QString::number(m_tops.size()));
    m_listBox->update();
    m_deleteButton->setEnabled(false);
}

Top BuilderPage::currentTop() const
{
    Top top;
    top.setName(m_nameEdit->text().trimmed());
    for (const PartSlot *slot : m_slots)
        top.setPart(slot->part());
    return top;
}

void BuilderPage::updatePreview()
{
    const Top top = currentTop();
    static_cast<Workbench *>(m_bench)->setTop(top);
    auto *preview = static_cast<SpinPreview *>(m_spin);
    preview->setTop(top);
    static_cast<BuildStats *>(m_stats)->setTop(top);
    m_benchBox->setProperty("index", tr("%1 PTS").arg(top.stats().total()));
    m_benchBox->update();
    if (const auto move = SpecialMove::find(top.special()))
        preview->setText(tr("Coup spécial — %1").arg(move->name), move->description);
}

void BuilderPage::loadTop(const Top &top)
{
    m_nameEdit->setText(top.name());
    for (PartSlot *slot : m_slots)
        slot->selectId(top.part(slot->kind()).id);
    static_cast<SpinPreview *>(m_spin)->setStatus(QString());
    updatePreview();
}

void BuilderPage::newTop()
{
    {
        const QSignalBlocker blocker(m_list);
        m_list->setCurrentRow(-1);
    }
    m_deleteButton->setEnabled(false);
    m_nameEdit->clear();
    for (PartSlot *slot : m_slots)
        slot->selectId(m_catalog.parts(slot->kind()).first().id);
    static_cast<SpinPreview *>(m_spin)->setStatus(QString());
    updatePreview();
    m_nameEdit->setFocus();
}

void BuilderPage::saveTop()
{
    auto fail = [this](const QString &text) {
        static_cast<SpinPreview *>(m_spin)->setStatus(text, Theme::kDanger);
    };
    const Top top = currentTop();
    if (top.name().isEmpty()) {
        fail(tr("Donnez un nom à votre toupie."));
        m_nameEdit->setFocus();
        return;
    }
    for (const Top &preset : m_catalog.presets()) {
        if (preset.name().compare(top.name(), Qt::CaseInsensitive) == 0) {
            fail(tr("« %1 » est le nom d'une toupie pré-assemblée.").arg(top.name()));
            return;
        }
    }
    if (!top.isValid()) {
        fail(tr("Assemblage incomplet."));
        return;
    }

    const bool existed = m_repository.contains(top.name());
    m_repository.upsert(top);
    if (!m_repository.save()) {
        QMessageBox::warning(this, tr("Sauvegarde impossible"),
                             tr("Impossible d'écrire le fichier :\n%1").arg(m_repository.filePath()));
        return;
    }

    refresh();
    for (int i = m_presetCount; i < m_tops.size(); ++i) {
        if (m_tops.at(i).name().compare(top.name(), Qt::CaseInsensitive) == 0) {
            const QSignalBlocker blocker(m_list);
            m_list->setCurrentRow(i);
            m_deleteButton->setEnabled(true);
        }
    }
    static_cast<SpinPreview *>(m_spin)->setStatus(existed ? tr("« %1 » mise à jour.").arg(top.name())
                                                          : tr("« %1 » enregistrée (type %2).")
                                                                .arg(top.name(), toString(top.type()).toLower()));
    emit topsChanged();
}

void BuilderPage::deleteSelected()
{
    const int row = m_list->currentRow();
    if (row < m_presetCount || row >= m_tops.size())
        return;
    const QString name = m_tops.at(row).name();
    if (QMessageBox::question(this, tr("Supprimer"), tr("Supprimer la toupie « %1 » ?").arg(name))
        != QMessageBox::Yes)
        return;

    m_repository.remove(name);
    m_repository.save();
    refresh();
    newTop();
    static_cast<SpinPreview *>(m_spin)->setStatus(tr("« %1 » supprimée.").arg(name));
    emit topsChanged();
}

} // namespace toupie
