#include "ui/MenuPage.h"

#include "core/PartCatalog.h"
#include "ui/Theme.h"
#include "ui/TopPainter.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <cmath>
#include <QtMath>

namespace toupie {

namespace {

/// Logo « TOP ARENA » : capitales holographiques à décalage chromatique,
/// bandeau vert incliné. Le nom du jeu n'est jamais traduit.
class LogoWidget : public QWidget
{
public:
    explicit LogoWidget(QWidget *parent)
        : QWidget(parent)
    {
        setFixedHeight(220);
        setMinimumWidth(480);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double s = std::min(1.0, width() / 560.0);
        const QPointF base(6, 124 * s);
        const QString top = QStringLiteral("TOP");
        QFont big = Theme::displayFont(138 * s, true);
        big.setLetterSpacing(QFont::AbsoluteSpacing, 6 * s);
        p.setFont(big);
        QColor magenta = Theme::kMagenta, cyan = Theme::kCyan;
        magenta.setAlpha(190);
        cyan.setAlpha(190);
        p.setPen(magenta);
        p.drawText(base + QPointF(-5 * s, 0), top);
        p.setPen(cyan);
        p.drawText(base + QPointF(5 * s, 2 * s), top);
        QLinearGradient holo(base + QPointF(0, -100 * s), base + QPointF(260 * s, 0));
        holo.setColorAt(0.0, Qt::white);
        holo.setColorAt(0.45, QColor(0xbf, 0xd3, 0xff));
        holo.setColorAt(0.7, QColor(0xc9, 0xa5, 0xff));
        holo.setColorAt(1.0, QColor(0x9d, 0xff, 0xcb));
        p.setPen(QPen(QBrush(holo), 1));
        p.drawText(base, top);

        // Bandeau « ARENA ».
        const QRectF band(base.x() + 6 * s, base.y() + 14 * s, 360 * s, 48 * s);
        QLinearGradient green(band.topLeft(), band.topRight());
        green.setColorAt(0.0, Theme::kAccent);
        green.setColorAt(1.0, Theme::kAccentBright);
        p.fillPath(Theme::skewed(band, 16 * s), green);
        QFont arena = Theme::displayFont(36 * s, true);
        arena.setLetterSpacing(QFont::AbsoluteSpacing, 18 * s);
        p.setFont(arena);
        const QColor ink(0x03, 0x14, 0x0a);
        p.setPen(ink);
        p.drawText(band.adjusted(28 * s, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("ARENA"));
        Theme::paintSlashes(p, QPointF(band.left() + 290 * s, band.top() + 9 * s), 30 * s, ink);

        p.setPen(QPen(Theme::kCyan, 2 * s));
        p.drawLine(QPointF(band.left() + 2 * s, band.bottom() + 14 * s),
                   QPointF(band.left() + 40 * s, band.bottom() + 14 * s));
        QFont tag = Theme::bodyFont(15 * s, QFont::Bold);
        tag.setLetterSpacing(QFont::AbsoluteSpacing, 4 * s);
        p.setFont(tag);
        p.setPen(Theme::kCyan);
        p.drawText(QRectF(band.left() + 50 * s, band.bottom() + 2 * s, 420 * s, 24 * s),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QCoreApplication::translate("toupie::MenuPage", "COMBAT DE TOUPIES"));
    }
};

/// Héros animé : une toupie en vedette, ses orbites et deux rivales qui tournent autour.
class HeroWidget : public QWidget
{
public:
    explicit HeroWidget(QWidget *parent)
        : QWidget(parent)
        , m_tops(PartCatalog::instance().presets())
    {
        setMinimumSize(360, 360);
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
        if (m_tops.size() < 3)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double R = std::min(width(), height()) * 0.28;
        const QPointF c(width() / 2.0, height() * 0.46);

        // Sol lumineux et orbites.
        QRadialGradient ground(c + QPointF(0, R * 1.25), R * 1.6);
        QColor cyan = Theme::kCyan;
        cyan.setAlpha(70);
        ground.setColorAt(0.0, cyan);
        cyan.setAlpha(0);
        ground.setColorAt(1.0, cyan);
        p.setPen(Qt::NoPen);
        p.setBrush(ground);
        p.drawEllipse(c + QPointF(0, R * 1.25), R * 1.6, R * 0.32);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Theme::kCyan, 2.0));
        p.drawEllipse(c + QPointF(0, R * 1.25), R * 1.25, R * 0.18);
        QPen dashed(Theme::kViolet, 1.5);
        dashed.setDashPattern({3, 7});
        p.setPen(dashed);
        p.drawEllipse(c + QPointF(0, R * 1.25), R * 1.6, R * 0.25);
        QLinearGradient ring(c - QPointF(R, R), c + QPointF(R, R));
        ring.setColorAt(0.0, Theme::kCyan);
        ring.setColorAt(0.5, Theme::kViolet);
        ring.setColorAt(1.0, Theme::kAccent);
        p.setPen(QPen(QBrush(ring), 1.5));
        p.setOpacity(0.55);
        p.drawEllipse(c, R * 1.42, R * 1.42);
        p.setOpacity(1.0);
        QPen sweep(Theme::kAccent, 3.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(sweep);
        p.drawArc(QRectF(c.x() - R * 1.62, c.y() - R * 1.62, R * 3.24, R * 3.24),
                  int(-m_time * 40 * 16) % (360 * 16), 60 * 16);

        // Rivales en orbite (perspective).
        for (int i = 0; i < 2; ++i) {
            const double a = m_time * (0.5 + 0.2 * i) + i * M_PI;
            const QPointF pos = c + QPointF(std::cos(a) * R * 1.4, std::sin(a) * R * 0.95);
            const double size = R * (0.2 + 0.06 * std::sin(a));
            TopPainter::paint(p, m_tops.at(1 + i), pos, size, m_time * (i ? -8.0 : 8.0));
        }

        // Toupie vedette et arcs de vitesse.
        const Top &hero = m_tops.first();
        const QColor type = TopPainter::typeColor(hero.type());
        for (int i = 0; i < 3; ++i) {
            QColor arc = type;
            arc.setAlpha(200);
            p.setPen(QPen(arc, std::max(2.0, R * 0.035), Qt::SolidLine, Qt::RoundCap));
            const double start = -m_time * 300 + i * 120;
            p.drawArc(QRectF(c.x() - R * 1.14, c.y() - R * 1.14, R * 2.28, R * 2.28),
                      int(start * 16), 80 * 16);
        }
        QRadialGradient halo(c, R * 1.45);
        QColor glow = type;
        glow.setAlpha(90);
        halo.setColorAt(0.5, glow);
        glow.setAlpha(0);
        halo.setColorAt(1.0, glow);
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(c, R * 1.45, R * 1.45);
        TopPainter::paint(p, hero, c, R, m_time * spinSign(hero.spin()) * 7.0);
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
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(40, 20, 20, 20);
    auto *column = new QVBoxLayout;
    column->setSpacing(12);
    root->addLayout(column, 0);
    root->addWidget(new HeroWidget(this), 1);

    column->addStretch(1);
    column->addWidget(new LogoWidget(this));
    auto *tagline
        = new QLabel(tr("Assemblez votre toupie · affrontez les bots ou vos amis en réseau local"), this);
    tagline->setObjectName(QStringLiteral("muted"));
    tagline->setWordWrap(true);
    column->addWidget(tagline);
    column->addSpacing(18);

    auto addButton = [&](const QString &text, const char *icon, auto signal) {
        auto *button = new QPushButton(text, this);
        button->setObjectName(QStringLiteral("menu"));
        button->setProperty("glyph", QString::fromLatin1(icon));
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, signal);
        column->addWidget(button);
        return button;
    };
    QPushButton *play = addButton(tr("Jouer contre les bots"), "play", &MenuPage::playRequested);
    play->setDefault(true);
    addButton(tr("Multijoueur en réseau local"), "lan", &MenuPage::lanRequested);
    addButton(tr("Tournois"), "trophy", &MenuPage::tournamentRequested);
    addButton(tr("Assembler une toupie"), "build", &MenuPage::builderRequested);
    addButton(tr("Tutoriel"), "tuto", &MenuPage::tutorialRequested);

    // Rangée secondaire : paramètres, aide, mises à jour, quitter.
    auto *extras = new QHBoxLayout;
    extras->setSpacing(8);
    for (auto [text, icon, signal] :
         {std::tuple{tr("Paramètres"), "gear", &MenuPage::settingsRequested},
          std::tuple{tr("Aide"), "help", &MenuPage::helpRequested},
          std::tuple{tr("Mises à jour"), "update", &MenuPage::updateRequested},
          std::tuple{tr("Quitter"), "quit", &MenuPage::quitRequested}}) {
        auto *button = new QPushButton(text, this);
        button->setProperty("variant", QStringLiteral("ghost"));
        button->setProperty("glyph", QString::fromLatin1(icon));
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, signal);
        extras->addWidget(button);
    }
    extras->addStretch();
    column->addSpacing(6);
    column->addLayout(extras);
    column->addStretch(1);

    if (const QString number = QCoreApplication::applicationVersion(); !number.isEmpty()) {
        auto *version = new QLabel(QStringLiteral("Top Arena · v%1").arg(number), this);
        version->setObjectName(QStringLiteral("muted"));
        column->addWidget(version);
    }
}

} // namespace toupie
