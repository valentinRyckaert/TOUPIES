#include "ui/MenuPage.h"

#include "core/PartCatalog.h"
#include "ui/Theme.h"
#include "ui/TopPainter.h"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <cmath>
#include <algorithm>
#include <QtMath>

namespace toupie {

namespace {

/// Bandeau animé : titre lumineux et toupies qui tournent dans un stadium.
class HeroWidget : public QWidget
{
public:
    explicit HeroWidget(QWidget *parent)
        : QWidget(parent)
        , m_tops(PartCatalog::instance().presets())
    {
        setMinimumHeight(300);
        m_timer = startTimer(16);
    }

protected:
    void timerEvent(QTimerEvent *event) override
    {
        if (event->timerId() == m_timer) {
            m_time += 0.016;
            update();
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF c(width() / 2.0, height() * 0.62);
        const double r = std::min(width() * 0.32, height() * 0.42);

        // Halo vert et anneau de stadium stylisé.
        QRadialGradient halo(c, r * 1.6);
        QColor green = Theme::kAccent;
        green.setAlpha(45);
        halo.setColorAt(0.0, green);
        green.setAlpha(0);
        halo.setColorAt(1.0, green);
        p.fillRect(rect(), halo);

        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 3; ++i) {
            QColor ring = Theme::kAccent;
            ring.setAlpha(30 + i * 25);
            p.setPen(QPen(ring, 1.5 + i));
            p.drawEllipse(c, r * (0.55 + 0.2 * i), r * (0.22 + 0.08 * i));
        }

        // Toupies en orbite (ellipse = perspective).
        const int count = std::min<int>(3, m_tops.size());
        for (int i = 0; i < count; ++i) {
            const double a = m_time * (0.8 + 0.15 * i) + i * 2.0 * M_PI / count;
            const QPointF pos = c + QPointF(std::cos(a) * r * 0.75, std::sin(a) * r * 0.3);
            const double size = 26 + 8 * (std::sin(a) + 1.0) / 2.0;
            TopPainter::paint(p, m_tops.at(i * 2 % m_tops.size()), pos, size,
                              m_time * (i % 2 ? -9.0 : 9.0), 0.4);
        }

        // Titre lumineux.
        QFont title = font();
        title.setPointSizeF(font().pointSizeF() * 4.2);
        title.setBold(true);
        title.setLetterSpacing(QFont::PercentageSpacing, 108);
        p.setFont(title);
        const QRectF titleRect(0, height() * 0.06, width(), height() * 0.3);
        const QString text = tr("TOUPIE ARENA");
        for (int k = 6; k >= 1; --k) {
            QColor glow = Theme::kAccent;
            glow.setAlpha(18);
            p.setPen(glow);
            for (const QPointF &d : {QPointF(k, 0), QPointF(-k, 0), QPointF(0, k), QPointF(0, -k)})
                p.drawText(titleRect.translated(d), Qt::AlignCenter, text);
        }
        p.setPen(Theme::kAccentBright);
        p.drawText(titleRect, Qt::AlignCenter, text);

        QFont sub = font();
        sub.setPointSizeF(font().pointSizeF() * 1.15);
        p.setFont(sub);
        p.setPen(Theme::kMuted);
        p.drawText(QRectF(0, titleRect.bottom() - 4, width(), 28), Qt::AlignCenter,
                   tr("Assemblez votre toupie · affrontez le bot ou vos amis en réseau local"));
    }

private:
    QList<Top> m_tops;
    int m_timer = 0;
    double m_time = 0.0;
};

} // namespace

MenuPage::MenuPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->addWidget(new HeroWidget(this), 3);

    auto addButton = [&](const QString &text, auto signal) {
        auto *button = new QPushButton(text, this);
        button->setObjectName(QStringLiteral("menu"));
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, signal);
        layout->addWidget(button, 0, Qt::AlignHCenter);
        return button;
    };

    QPushButton *play = addButton(tr("▶  Jouer contre le bot"), &MenuPage::playRequested);
    play->setDefault(true);
    addButton(tr("⇄  Multijoueur en réseau local"), &MenuPage::lanRequested);
    addButton(tr("🏆  Tournois"), &MenuPage::tournamentRequested);
    addButton(tr("⚙  Assembler une toupie"), &MenuPage::builderRequested);
    addButton(tr("✕  Quitter"), &MenuPage::quitRequested);

    // Rangée secondaire : paramètres, aide et mises à jour.
    auto *extras = new QHBoxLayout;
    extras->addStretch();
    for (auto [text, signal] : {std::pair{tr("⚙  Paramètres"), &MenuPage::settingsRequested},
                                std::pair{tr("?  Aide"), &MenuPage::helpRequested},
                                std::pair{tr("⟳  Mises à jour"), &MenuPage::updateRequested}}) {
        auto *button = new QPushButton(text, this);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, signal);
        extras->addWidget(button);
    }
    extras->addStretch();
    layout->addLayout(extras);
    layout->addStretch(2);
}

} // namespace toupie
