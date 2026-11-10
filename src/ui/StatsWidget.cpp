#include "ui/StatsWidget.h"

#include "core/PartCatalog.h"
#include "game/SpecialMove.h"
#include "ui/Theme.h"
#include "ui/TopPainter.h"

#include <QPainter>
#include <QTimerEvent>

namespace toupie {

namespace {
int maxStat()
{
    static const int value = PartCatalog::instance().maxStatValue();
    return value;
}

} // namespace

StatsWidget::StatsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_timerId = startTimer(16);
}

void StatsWidget::setTop(const std::optional<Top> &top)
{
    m_top = top;
    m_mysteryCaption.clear();
    update();
}

void StatsWidget::setMystery(const QString &caption)
{
    m_top.reset();
    m_mysteryCaption = caption;
    update();
}

QSize StatsWidget::sizeHint() const
{
    return {420, 190};
}

QSize StatsWidget::minimumSizeHint() const
{
    return {320, 180};
}

void StatsWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != m_timerId)
        return;
    if (m_top) {
        m_angle += spinSign(m_top->spin()) * 0.12;
        update();
    }
}

void StatsWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double previewRadius = qMin(height() * 0.4, 62.0);
    const QPointF previewCenter(previewRadius + 14, height() / 2.0);
    const double left = previewCenter.x() + previewRadius + 24;
    const double width = this->width() - left - 8;

    if (!m_top) {
        QPen dashed(Theme::kBorderBright, 1.5);
        dashed.setDashPattern({3, 5});
        p.setPen(dashed);
        p.setBrush(Theme::kField);
        p.drawEllipse(previewCenter, previewRadius, previewRadius);
        p.setFont(Theme::displayFont(previewRadius * 0.9));
        p.setPen(Theme::kCyan);
        p.drawText(QRectF(previewCenter.x() - previewRadius, previewCenter.y() - previewRadius,
                          2 * previewRadius, 2 * previewRadius),
                   Qt::AlignCenter, QStringLiteral("?"));
        p.setFont(Theme::bodyFont(16, QFont::DemiBold));
        p.setPen(Theme::kMuted);
        p.drawText(QRectF(left, 0, width, height()),
                   Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
                   m_mysteryCaption.isEmpty() ? tr("Aucune toupie") : m_mysteryCaption);
        return;
    }

    const Top &top = *m_top;
    const QColor accent = TopPainter::typeColor(top.type());
    QRadialGradient halo(previewCenter, previewRadius * 1.35);
    QColor glow = accent;
    glow.setAlpha(60);
    halo.setColorAt(0.6, glow);
    glow.setAlpha(0);
    halo.setColorAt(1.0, glow);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(previewCenter, previewRadius * 1.35, previewRadius * 1.35);
    QPen ring(accent, 1.2);
    ring.setDashPattern({2, 6});
    p.setPen(ring);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(previewCenter, previewRadius * 1.14, previewRadius * 1.14);
    TopPainter::paint(p, top, previewCenter, previewRadius, m_angle);

    const Stats stats = top.stats();
    double y = 4;
    QFont name = Theme::displayFont(20);
    name.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    p.setFont(name);
    p.setPen(Theme::kText);
    const QString title = top.name().isEmpty() ? tr("(sans nom)") : top.name();
    p.drawText(QRectF(left, y, width, 28), Qt::AlignVCenter | Qt::AlignLeft,
               p.fontMetrics().elidedText(title.toUpper(), Qt::ElideRight, int(width)));
    y += 32;

    const QString type = toString(top.type());
    const double tagWidth = std::min(width * 0.45, 46.0 + type.size() * 10.5);
    Theme::paintTag(p, QRectF(left, y, tagWidth, 22), type, accent);
    const auto move = SpecialMove::find(top.special());
    p.setFont(Theme::bodyFont(15, QFont::Bold));
    const QColor moveColor = TopPainter::moveColor(top.special());
    const double infoLeft = left + tagWidth + 12;
    TopPainter::paintEmblem(p, top.special(), QPointF(infoLeft + 9, y + 11), 8, moveColor, 1.6);
    p.setPen(moveColor);
    const QString info = tr("%1  ·  %2").arg(move ? move->name : tr("aucun"),
                                             toString(top.spin()).toLower());
    p.drawText(QRectF(infoLeft + 24, y, width - tagWidth - 36, 22), Qt::AlignVCenter | Qt::AlignLeft,
               p.fontMetrics().elidedText(info, Qt::ElideRight, int(width - tagWidth - 36)));
    y += 30;

    const double line = std::max(30.0, (height() - y - 4) / 3.0);
    Theme::paintStatBar(p, QRectF(left, y, width, line - 4), tr("Attaque"), stats.attack, maxStat(),
            TopPainter::typeColor(TopType::Attack));
    y += line;
    Theme::paintStatBar(p, QRectF(left, y, width, line - 4), tr("Défense"), stats.defense, maxStat(),
            TopPainter::typeColor(TopType::Defense));
    y += line;
    Theme::paintStatBar(p, QRectF(left, y, width, line - 4), tr("Endurance"), stats.endurance, maxStat(),
            TopPainter::typeColor(TopType::Endurance));
}

} // namespace toupie
