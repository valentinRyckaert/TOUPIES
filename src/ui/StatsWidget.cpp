#include "ui/StatsWidget.h"

#include "core/PartCatalog.h"
#include "game/SpecialMove.h"
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

void drawBar(QPainter &p, const QRectF &rect, const QString &label, int value, const QColor &color,
             const QColor &textColor)
{
    const double labelWidth = 82.0;
    p.setPen(textColor);
    p.drawText(QRectF(rect.left(), rect.top(), labelWidth, rect.height()),
               Qt::AlignVCenter | Qt::AlignLeft, label);

    const QRectF track(rect.left() + labelWidth, rect.top() + rect.height() * 0.25,
                       rect.width() - labelWidth - 30, rect.height() * 0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(127, 127, 127, 60));
    p.drawRoundedRect(track, 4, 4);
    QRectF fill = track;
    fill.setWidth(track.width() * qBound(0, value, maxStat()) / maxStat());
    p.setBrush(color);
    p.drawRoundedRect(fill, 4, 4);

    p.setPen(textColor);
    p.drawText(QRectF(track.right() + 6, rect.top(), 24, rect.height()),
               Qt::AlignVCenter | Qt::AlignRight, QString::number(value));
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
    return {380, 140};
}

QSize StatsWidget::minimumSizeHint() const
{
    return {300, 130};
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
    const QColor text = palette().color(QPalette::WindowText);
    p.setPen(text);

    const double previewRadius = qMin(height() * 0.36, 50.0);
    const QPointF previewCenter(previewRadius + 12, height() / 2.0);
    const double left = previewCenter.x() + previewRadius + 18;
    const double width = this->width() - left - 8;

    if (!m_top) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(127, 127, 127, 60));
        p.drawEllipse(previewCenter, previewRadius, previewRadius);
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 2.2);
        f.setBold(true);
        p.setFont(f);
        p.setPen(text);
        p.drawText(QRectF(previewCenter.x() - previewRadius, previewCenter.y() - previewRadius,
                          2 * previewRadius, 2 * previewRadius),
                   Qt::AlignCenter, QStringLiteral("?"));
        p.setFont(font());
        p.drawText(QRectF(left, 0, width, height()),
                   Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
                   m_mysteryCaption.isEmpty() ? tr("Aucune toupie") : m_mysteryCaption);
        return;
    }

    const Top &top = *m_top;
    TopPainter::paint(p, top, previewCenter, previewRadius, m_angle);

    const Stats stats = top.stats();
    const QColor accent = TopPainter::typeColor(top.type());
    const double line = qMax(18.0, (height() - 12) / 5.0);
    double y = 6;

    QFont bold = font();
    bold.setBold(true);
    bold.setPointSizeF(bold.pointSizeF() * 1.15);
    p.setFont(bold);
    p.setPen(text);
    p.drawText(QRectF(left, y, width, line), Qt::AlignVCenter | Qt::AlignLeft,
               top.name().isEmpty() ? tr("(sans nom)") : top.name());
    y += line;

    p.setFont(font());
    const auto move = SpecialMove::find(top.special());
    const QString info = tr("%1  ·  %2  ·  ✦ %3")
                             .arg(toString(top.type()), toString(top.spin()).toLower(),
                                  move ? move->name : tr("aucun"));
    p.setPen(accent);
    p.drawText(QRectF(left, y, width, line), Qt::AlignVCenter | Qt::AlignLeft, info);
    y += line;

    drawBar(p, QRectF(left, y, width, line), tr("Attaque"), stats.attack,
            TopPainter::typeColor(TopType::Attack), text);
    y += line;
    drawBar(p, QRectF(left, y, width, line), tr("Défense"), stats.defense,
            TopPainter::typeColor(TopType::Defense), text);
    y += line;
    drawBar(p, QRectF(left, y, width, line), tr("Endurance"), stats.endurance,
            TopPainter::typeColor(TopType::Endurance), text);
}

} // namespace toupie
