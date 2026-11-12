#include "ui/MenuPage.h"

#include "core/PartCatalog.h"
#include "game/Battle.h"
#include "game/BotController.h"
#include "game/MatchRules.h"
#include "game/Stadium.h"
#include "ui/ArenaWidget.h"
#include "ui/BattleEffects.h"
#include "ui/StadiumPainter.h"
#include "ui/Theme.h"
#include "ui/TopPainter.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <cmath>
#include <memory>
#include <random>
#include <vector>
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

/// Démonstration en fond de menu : un vrai combat entre deux bots normaux (toupies et
/// stadium tirés au hasard) ; dès qu'il se termine, un autre commence.
/// La simulation ne tourne que menu affiché, et le stadium est mis en cache :
/// chaque image ne redessine que les toupies et les effets du combat (BattleEffects,
/// les mêmes qu'en jeu : coups spéciaux, étincelles des chocs et des duels).
class BotDemo : public QWidget
{
public:
    explicit BotDemo(QWidget *parent)
        : QWidget(parent)
        , m_rng(std::random_device{}())
    {
        setMinimumSize(360, 360);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        restart();
    }

protected:
    void showEvent(QShowEvent *) override
    {
        m_clock.start();
        if (!m_timer)
            m_timer = startTimer(16);
    }

    void hideEvent(QHideEvent *) override
    {
        killTimer(m_timer);
        m_timer = 0;
    }

    void resizeEvent(QResizeEvent *) override { m_cache = QPixmap(); }

    void timerEvent(QTimerEvent *event) override
    {
        if (event->timerId() != m_timer)
            return;
        const double dt = std::min(0.05, m_clock.restart() / 1000.0);
        if (m_battle->isFinished()) {
            m_effects.update(dt, {}, m_battle->spinners(), m_battle->duel(), false); // Fin des étincelles.
            m_pause -= dt;
            if (m_pause <= 0.0)
                restart();
        } else {
            for (BotController &bot : m_bots)
                bot.update(*m_battle, dt);
            m_battle->advance(dt);
            m_effects.update(dt, m_battle->takeImpacts(), m_battle->spinners(), m_battle->duel(), true);
            if (m_battle->isFinished())
                m_pause = 1.5; // Le temps de voir qui a gagné.
            else if (m_battle->elapsed() > 60.0)
                restart(); // Deux toupies d'endurance : on passe au combat suivant.
        }
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF c(width() / 2.0, height() / 2.0);
        const double k = scale();
        if (m_cache.size() != size() * devicePixelRatioF() || m_cacheScale != k) {
            m_cacheScale = k;
            m_cache = QPixmap(size() * devicePixelRatioF());
            m_cache.setDevicePixelRatio(devicePixelRatioF());
            m_cache.fill(Qt::transparent);
            QPainter cache(&m_cache);
            cache.setRenderHint(QPainter::Antialiasing);
            StadiumPainter::paint(cache, m_battle->stadium(), m_battle->geometry(), c, k);
        }
        p.drawPixmap(0, 0, m_cache);
        auto toScreen = [&](Vec2 v) { return c + QPointF(v.x, v.y) * k; };

        const BattleConfig &config = m_battle->config();
        const double radius = config.topRadius * k;
        m_effects.paintTrails(p, m_battle->spinners(), toScreen, radius);
        BattleEffects::paintProjectiles(p, m_battle->projectiles(), toScreen, k, config.topRadius,
                                        m_battle->geometry().radius, m_battle->elapsed());
        const auto &spinners = m_battle->spinners();
        for (int i = 0; i < int(spinners.size()); ++i) {
            const Spinner &s = spinners[i];
            if (s.status == Spinner::Status::RingOut && m_battle->isFinished())
                continue; // Éjectée : sortie du stadium.
            QPointF pos = toScreen(s.position);
            if (m_battle->duel().sideOf(i) >= 0) { // Duel : les toupies tremblent.
                const double t = m_battle->elapsed() * 60.0 + i * 1.7;
                pos += QPointF(std::sin(t * 1.3), std::cos(t * 1.7)) * (radius * 0.06);
            }
            p.setOpacity(s.isActive() ? 1.0 : 0.5);
            TopPainter::paint(p, s.top, pos, radius, s.angle, s.isDashing() || s.isBoosted() ? 1.0 : 0.0);
            BattleEffects::paintStatus(p, s, pos, radius, m_battle->elapsed());
        }
        p.setOpacity(1.0);
        m_effects.paintSparks(p, toScreen, radius, k);
    }

private:
    double scale() const
    {
        // L'échelle de l'arène d'un vrai combat dans cette fenêtre : le stadium et les
        // toupies du menu ont la même taille qu'en jeu. La page de combat occupe la même
        // place que le menu, moins sa barre du bas (commandes, revanche).
        constexpr int kBattleBar = 48;
        const QSize page = parentWidget()->size();
        const double base = m_battle->config().stadiumRadius / std::max(0.1, m_battle->stadium().size);
        return ArenaWidget::unitScale(QSizeF(page.width(), page.height() - kBattleBar)) / std::max(0.1, base);
    }

    void restart()
    {
        const QList<Top> tops = PartCatalog::instance().presets();
        const QList<Stadium> &stadiums = Stadium::all();
        std::uniform_int_distribution<qsizetype> pickTop(0, tops.size() - 1), pickStadium(0, stadiums.size() - 1);
        const qsizetype a = pickTop(m_rng);
        qsizetype b = pickTop(m_rng);
        if (b == a)
            b = (a + 1) % tops.size();
        const Stadium &stadium = stadiums.at(pickStadium(m_rng));
        BattleConfig config = stadium.apply(BattleConfig{});
        config.duelMode = MatchRules{}.duelMode; // Les duels choisis dans les paramètres.
        m_battle = std::make_unique<Battle>(QList<Top>{tops.at(a), tops.at(b)}, stadium, config,
                                            static_cast<std::uint32_t>(m_rng()));
        m_bots = {BotController(0, Difficulty::Normal, static_cast<std::uint32_t>(m_rng())),
                  BotController(1, Difficulty::Normal, static_cast<std::uint32_t>(m_rng()))};
        m_battle->launch({m_bots[0].chooseLaunchAngle(*m_battle), m_bots[1].chooseLaunchAngle(*m_battle)},
                         {m_bots[0].chooseLaunchPower(*m_battle), m_bots[1].chooseLaunchPower(*m_battle)});
        m_cache = QPixmap();
        m_effects.reset();
    }

    std::mt19937 m_rng;
    std::unique_ptr<Battle> m_battle;
    std::vector<BotController> m_bots;
    BattleEffects m_effects;
    QPixmap m_cache;
    double m_cacheScale = 0.0;
    QElapsedTimer m_clock;
    double m_pause = 0.0;
    int m_timer = 0;
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
    root->addWidget(new BotDemo(this), 1);

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
