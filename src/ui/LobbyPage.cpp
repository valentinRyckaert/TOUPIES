#include "ui/LobbyPage.h"
#include "ui/Theme.h"

#include "core/PartCatalog.h"
#include "core/TopRepository.h"
#include "game/Stadium.h"
#include "net/ClientSession.h"
#include "net/HostSession.h"
#include "net/MatchCodec.h"
#include "ui/ArenaWidget.h"
#include "ui/RulesWidget.h"
#include "ui/StadiumPainter.h"
#include "ui/StatsWidget.h"
#include "ui/TournamentSettingsWidget.h"
#include "ui/Widgets.h"

#include <QDialog>

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QNetworkInterface>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTableWidget>
#include <QVBoxLayout>

namespace toupie {

namespace {
QStringList localAddresses()
{
    QStringList result;
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback())
            result << address.toString();
    }
    return result;
}

template<typename T>
void disposeLater(std::unique_ptr<T> &object)
{
    // Destruction différée : l'objet peut être en train d'émettre un signal.
    if (object)
        object.release()->deleteLater();
}
} // namespace

LobbyPage::LobbyPage(TopRepository &repository, QWidget *parent)
    : QWidget(parent)
    , m_repository(repository)
{
    auto *root = new QVBoxLayout(this);

    m_title = Theme::createTitle(QString(), this);
    root->addWidget(m_title);
    m_subtitle = new QLabel(this);
    m_subtitle->setWordWrap(true);
    m_subtitle->setObjectName(QStringLiteral("muted"));
    m_subtitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_subtitle);

    // --- Paramètres du lobby (panneau latéral rétractable) -----------------------
    auto *settings = new QWidget;
    auto *settingsLayout = new QVBoxLayout(settings);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    auto *form = new QFormLayout;
    m_playersCombo = new QComboBox(this);
    for (int n = Battle::kMinPlayers; n <= Battle::kMaxPlayers; ++n)
        m_playersCombo->addItem(tr("%1 joueurs").arg(n), n);
    m_playersLabel = new QLabel(this);
    auto *playersRow = new QHBoxLayout;
    playersRow->addWidget(m_playersCombo);
    playersRow->addWidget(m_playersLabel);
    form->addRow(tr("Nombre de joueurs"), playersRow);
    m_stadiumCombo = new QComboBox(this);
    for (const Stadium &stadium : Stadium::all())
        m_stadiumCombo->addItem(stadium.name, stadium.id);
    m_stadiumLabel = new QLabel(this);
    auto *stadiumRow = new QHBoxLayout;
    stadiumRow->addWidget(m_stadiumCombo);
    stadiumRow->addWidget(m_stadiumLabel);
    form->addRow(tr("Stadium"), stadiumRow);
    settingsLayout->addLayout(form);
    m_stadiumPreview = new StadiumWidget(this);
    settingsLayout->addWidget(m_stadiumPreview);

    // Personnalisation et réglages du tournoi : dans des fenêtres surgissantes.
    m_rules = new RulesWidget;
    QDialog *rulesPopup = createPopup(this, tr("Personnalisation"), m_rules);
    m_rulesButton = new QPushButton(settings);
    settingsLayout->addWidget(m_rulesButton);
    m_tournamentSettings = new TournamentSettingsWidget;
    QDialog *tournamentPopup = createPopup(this, tr("Réglages du tournoi"), m_tournamentSettings);
    m_tournamentButton = new QPushButton(settings);
    settingsLayout->addWidget(m_tournamentButton);
    settingsLayout->addStretch();
    connect(m_rulesButton, &QPushButton::clicked, rulesPopup, &QDialog::open);
    connect(m_tournamentButton, &QPushButton::clicked, tournamentPopup, &QDialog::open);

    auto *columns = new QHBoxLayout;
    auto *sidePanel = new SidePanel(tr("Réglages du lobby"), settings, this);
    sidePanel->setMaximumWidth(360);
    columns->addWidget(sidePanel);
    auto *main = new QVBoxLayout;
    columns->addLayout(main, 1);
    root->addLayout(columns, 1);

    // --- Joueurs ----------------------------------------------------------------
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({tr("Joueur"), tr("Toupie"), tr("Type"), tr("Statut")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setFixedHeight(m_table->horizontalHeader()->sizeHint().height()
                            + Battle::kMaxPlayers * m_table->verticalHeader()->defaultSectionSize()
                            + 4);
    main->addWidget(m_table);

    // --- Ma toupie --------------------------------------------------------------
    auto *topBox = new QGroupBox(tr("Votre toupie (parmi vos toupies locales)"), this);
    auto *topLayout = new QVBoxLayout(topBox);
    auto *combos = new QHBoxLayout;
    for (int i = 0; i < 3; ++i) {
        auto *combo = new QComboBox(topBox);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(10);
        m_topCombos.append(combo);
        combos->addWidget(combo);
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo] {
            m_topPreview->setTop(m_tops.value(combo->currentIndex()));
            sendSelectedTops();
        });
    }
    topLayout->addLayout(combos);
    m_topPreview = new StatsWidget(topBox);
    topLayout->addWidget(m_topPreview);
    main->addWidget(topBox);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    main->addWidget(m_status);
    main->addStretch();

    auto *buttons = new QHBoxLayout;
    auto *leaveButton = new QPushButton(tr("Quitter le lobby"), this);
    leaveButton->setProperty("variant", QStringLiteral("danger"));
    leaveButton->setProperty("glyph", QStringLiteral("quit"));
    m_readyButton = new QPushButton(tr("Je suis prêt"), this);
    m_readyButton->setCheckable(true);
    m_startButton = new QPushButton(tr("Lancer la partie"), this);
    m_startButton->setObjectName(QStringLiteral("accent"));
    m_startButton->setMinimumHeight(38);
    buttons->addWidget(leaveButton);
    buttons->addStretch();
    buttons->addWidget(m_readyButton);
    buttons->addWidget(m_startButton);
    root->addLayout(buttons);

    connect(leaveButton, &QPushButton::clicked, this, &LobbyPage::leaveRequested);
    connect(m_readyButton, &QPushButton::toggled, this, [this](bool ready) {
        m_readyButton->setText(ready ? tr("Prêt ✓") : tr("Je suis prêt"));
        if (m_client)
            m_client->setReady(ready);
    });
    connect(m_startButton, &QPushButton::clicked, this, [this] {
        if (const net::LobbyInfo *info = lobby(); info && info->tournament)
            startTournament();
        else
            startMatch();
    });
    connect(m_tournamentSettings, &TournamentSettingsWidget::settingsChanged, this,
            [this](const TournamentSettings &settings) {
                if (m_server)
                    m_server->setTournamentSettings(settings);
            });
    connect(m_playersCombo, &QComboBox::activated, this, [this] {
        if (m_server)
            m_server->setMaxPlayers(m_playersCombo->currentData().toInt());
        rebuild(); // Valeur refusée : on réaffiche la valeur réelle.
    });
    connect(m_rules, &RulesWidget::rulesChanged, this, [this](const MatchRules &rules) {
        if (m_server)
            m_server->setRules(rules);
    });
    connect(m_stadiumCombo, &QComboBox::activated, this, [this] {
        if (m_server)
            m_server->setStadium(m_stadiumCombo->currentData().toString());
    });

    refreshTops();
}

LobbyPage::~LobbyPage()
{
    m_session.reset();
    m_tournament.reset();
}

// ---------------------------------------------------------------------------
// Connexion
// ---------------------------------------------------------------------------

bool LobbyPage::host(const net::LobbyServer::Settings &settings, QString *error)
{
    leave();
    m_server = std::make_unique<net::LobbyServer>(PartCatalog::instance());
    if (!m_server->start(settings)) {
        if (error)
            *error = m_server->errorString();
        m_server.reset();
        return false;
    }
    connect(m_server.get(), &net::LobbyServer::lobbyChanged, this, &LobbyPage::onLobbyChanged);
    m_wasInGame = false;
    sendSelectedTops();
    rebuild();
    return true;
}

void LobbyPage::join(const QString &hostAddress, quint16 port, const QString &playerName)
{
    leave();
    m_client = std::make_unique<net::LobbyClient>(PartCatalog::instance());
    net::LobbyClient *client = m_client.get();
    connect(client, &net::LobbyClient::joined, this, [this] {
        m_wasInGame = false;
        m_readyButton->setChecked(false);
        sendSelectedTops();
        rebuild();
        emit joined();
    });
    connect(client, &net::LobbyClient::joinFailed, this, [this](const QString &reason) {
        disposeLater(m_client);
        emit joinFailed(reason);
    });
    connect(client, &net::LobbyClient::connectionLost, this, [this](const QString &reason) {
        m_session.reset();
        m_tournament.reset();
        disposeLater(m_client);
        emit connectionLost(reason);
    });
    connect(client, &net::LobbyClient::lobbyChanged, this, &LobbyPage::onLobbyChanged);
    connect(client, &net::LobbyClient::matchStarted, this, &LobbyPage::onClientMatchStarted);
    connect(client, &net::LobbyClient::gameMessage, this, [this](const QJsonObject &message) {
        if (message.value("type").toString() != net::Msg::Tournament || m_tournament || !m_client)
            return;
        auto follower = std::make_unique<net::TournamentFollower>(m_client.get());
        follower->apply(message);
        m_tournament = std::move(follower);
        emit tournamentStarted();
    });
    client->connectToHost(hostAddress, port, playerName);
}

void LobbyPage::leave()
{
    m_session.reset();
    m_tournament.reset();
    if (m_server) {
        disconnect(m_server.get(), nullptr, this, nullptr);
        m_server->close(tr("L'hôte a fermé le lobby."));
        disposeLater(m_server);
    }
    if (m_client) {
        disconnect(m_client.get(), nullptr, this, nullptr);
        m_client->disconnectFromHost();
        disposeLater(m_client);
    }
    m_wasInGame = false;
}

void LobbyPage::returnToLobby()
{
    m_tournament.reset();
    if (m_server)
        m_server->returnToLobby();
}

std::unique_ptr<MatchSession> LobbyPage::takeSession()
{
    return std::move(m_session);
}

// ---------------------------------------------------------------------------
// Lobby
// ---------------------------------------------------------------------------

const net::LobbyInfo *LobbyPage::lobby() const
{
    if (m_server)
        return &m_server->lobby();
    if (m_client && m_client->isJoined())
        return &m_client->lobby();
    return nullptr;
}

int LobbyPage::localPlayerId() const
{
    if (m_server)
        return net::LobbyServer::kHostId;
    return m_client ? m_client->playerId() : -1;
}

void LobbyPage::refreshTops()
{
    m_tops = m_repository.allTops();
    for (int i = 0; i < m_topCombos.size(); ++i) {
        QComboBox *combo = m_topCombos.at(i);
        const QString current = combo->currentText();
        const QSignalBlocker blocker(combo);
        combo->clear();
        for (const Top &top : m_tops)
            combo->addItem(QStringLiteral("%1  [%2]").arg(top.name(), toString(top.type())));
        const int found = combo->findText(current);
        combo->setCurrentIndex(found >= 0 ? found : std::min(i, int(m_tops.size()) - 1));
    }
    m_topPreview->setTop(m_tops.value(m_topCombos.first()->currentIndex()));
    sendSelectedTops();
}

void LobbyPage::sendSelectedTops()
{
    const net::LobbyInfo *info = lobby();
    const int needed = info ? info->topsNeeded() : 1;
    QList<Top> tops;
    for (int i = 0; i < needed; ++i) {
        const int index = m_topCombos.at(i)->currentIndex();
        if (index < 0 || index >= m_tops.size())
            return;
        tops.append(m_tops.at(index));
    }
    if (m_server) {
        m_server->setHostTops(tops);
    } else if (m_client && m_client->isJoined()) {
        m_client->selectTops(tops);
        // Changer de toupie annule le statut « prêt » (côté hôte aussi).
        const QSignalBlocker blocker(m_readyButton);
        m_readyButton->setChecked(false);
        m_readyButton->setText(tr("Je suis prêt"));
    }
}

void LobbyPage::onLobbyChanged()
{
    const net::LobbyInfo *info = lobby();
    if (!info)
        return;
    if (m_client && m_wasInGame && !info->inGame) {
        m_tournament.reset();
        emit returnedToLobby();
    }
    m_wasInGame = info->inGame;
    // Le nombre de toupies demandé a changé (type de match du tournoi).
    const net::LobbyPlayer *me = info->player(localPlayerId());
    if (me && me->tops.size() != info->topsNeeded() && !info->inGame)
        sendSelectedTops();
    rebuild();
}

void LobbyPage::rebuild()
{
    const net::LobbyInfo *info = lobby();
    if (!info)
        return;
    const bool hosting = isHost();

    m_title->setText(tr("Lobby « %1 »").arg(info->name));
    if (hosting) {
        const QStringList addresses = localAddresses();
        const QString port = QString::number(m_server->port());
        QStringList endpoints;
        for (const QString &a : addresses)
            endpoints << QStringLiteral("%1:%2").arg(a, port);
        m_subtitle->setText(
            tr("Vous êtes l'hôte. Les autres joueurs peuvent rejoindre « %1 » depuis la liste des "
               "lobbys ou se connecter à : %2")
                .arg(info->name,
                     endpoints.isEmpty() ? tr("(aucune adresse réseau, port %1)").arg(port)
                                         : endpoints.join(QStringLiteral("  ·  "))));
    } else {
        m_subtitle->setText(tr("Connecté à l'hôte %1.").arg(m_client->hostLabel()));
    }

    // Paramètres : modifiables par l'hôte, affichés pour les clients.
    m_playersCombo->setVisible(hosting);
    m_stadiumCombo->setVisible(hosting);
    m_playersLabel->setVisible(!hosting);
    m_stadiumLabel->setVisible(!hosting);
    {
        const QSignalBlocker b1(m_playersCombo);
        const QSignalBlocker b2(m_stadiumCombo);
        m_playersCombo->setCurrentIndex(qMax(0, m_playersCombo->findData(info->maxPlayers)));
        m_stadiumCombo->setCurrentIndex(qMax(0, m_stadiumCombo->findData(info->stadiumId)));
    }
    m_playersLabel->setText(tr("%1 joueurs").arg(info->maxPlayers));
    const Stadium stadium = Stadium::find(info->stadiumId).value_or(Stadium::classic());
    m_stadiumLabel->setText(stadium.name);
    m_stadiumPreview->setStadium(stadium);
    m_rules->setReadOnly(!hosting);
    if (m_rules->rules() != info->rules)
        m_rules->setRules(info->rules);
    m_rulesButton->setText(tr("Personnalisation : %1%2")
                               .arg(info->rules.summary(), hosting ? tr(" — modifier…") : QString()));
    m_tournamentButton->setVisible(info->tournament.has_value());
    if (info->tournament) {
        m_tournamentButton->setText(
            tr("%1 · %2 · %3 participants%4")
                .arg(toString(info->tournament->format), toString(info->tournament->matchFormat))
                .arg(info->tournament->participants)
                .arg(hosting ? tr(" — modifier…") : tr(" — détails…")));
        m_tournamentSettings->setReadOnly(!hosting);
        m_tournamentSettings->setHumans(info->maxPlayers);
        if (m_tournamentSettings->settings() != *info->tournament)
            m_tournamentSettings->setSettings(*info->tournament);
    }
    const int needed = info->topsNeeded();
    for (int i = 0; i < m_topCombos.size(); ++i)
        m_topCombos.at(i)->setVisible(i < needed);

    // Tableau des places.
    m_table->setRowCount(info->maxPlayers);
    for (int row = 0; row < info->maxPlayers; ++row) {
        QStringList cells;
        QColor color = palette().color(QPalette::Disabled, QPalette::Text);
        if (row < info->players.size()) {
            const net::LobbyPlayer &p = info->players.at(row);
            QString name = p.name;
            if (p.host)
                name += tr(" (hôte)");
            if (p.id == localPlayerId())
                name += tr(" — vous");
            QStringList topNames;
            QStringList types;
            for (const Top &top : p.tops) {
                topNames << top.name();
                types << toString(top.type());
            }
            cells << name << (topNames.isEmpty() ? tr("—") : topNames.join(QStringLiteral(", ")))
                  << types.join(QStringLiteral(", "))
                  << (p.host ? tr("Hôte") : p.ready ? tr("Prêt") : tr("Pas prêt"));
            color = ArenaWidget::slotColor(row);
        } else {
            cells << tr("Place libre — en attente d'un joueur…") << QString() << QString()
                  << QString();
        }
        for (int col = 0; col < cells.size(); ++col) {
            auto *item = new QTableWidgetItem(cells.at(col));
            if (col == 0)
                item->setForeground(color.darker(110));
            m_table->setItem(row, col, item);
        }
    }

    // Boutons et statut.
    m_readyButton->setVisible(!hosting);
    m_startButton->setVisible(hosting);
    if (!hosting) {
        if (const net::LobbyPlayer *me = info->player(localPlayerId())) {
            const QSignalBlocker blocker(m_readyButton);
            m_readyButton->setChecked(me->ready);
            m_readyButton->setText(me->ready ? tr("Prêt ✓") : tr("Je suis prêt"));
        }
    }
    m_startButton->setEnabled(hosting && m_server->canStart());
    m_startButton->setText(info->tournament ? tr("Lancer le tournoi") : tr("Lancer la partie"));

    const int count = static_cast<int>(info->players.size());
    QString status;
    if (count < info->maxPlayers) {
        status = tr("En attente de joueurs (%1/%2)…").arg(count).arg(info->maxPlayers);
    } else if (hosting && !m_server->canStart()) {
        status = tr("Tous les joueurs doivent choisir une toupie et se déclarer prêts.");
    } else if (hosting) {
        status = tr("Tout le monde est prêt : vous pouvez lancer la partie !");
    } else {
        status = tr("Lobby complet. L'hôte lancera la partie quand tout le monde sera prêt.");
    }
    m_status->setText(status);
}

// ---------------------------------------------------------------------------
// Lancement
// ---------------------------------------------------------------------------

void LobbyPage::startMatch()
{
    if (!m_server || !m_server->canStart())
        return;
    const QJsonObject start = m_server->startMatch(QRandomGenerator::global()->generate());
    const auto setup = net::parseStart(start, PartCatalog::instance());
    if (!setup) {
        m_server->returnToLobby();
        return;
    }
    m_session = std::make_unique<net::HostSession>(m_server.get(), *setup);
    emit matchStarted();
}

void LobbyPage::startTournament()
{
    if (!m_server || !m_server->canStart() || !m_server->lobby().tournament)
        return;
    const net::LobbyInfo &info = m_server->lobby();
    TournamentSettings settings = *info.tournament;
    settings.stadiumId = info.stadiumId;
    settings.rules = info.rules;
    QList<Participant> humans;
    for (const net::LobbyPlayer &p : info.players)
        humans.append({p.name, p.id, Difficulty::Normal, p.tops.mid(0, info.topsNeeded())});
    auto *rng = QRandomGenerator::global();
    Tournament tournament(settings, addBots(humans, settings, m_tops, rng->generate()),
                          rng->generate());
    m_server->startTournament();
    m_tournament = std::make_unique<net::TournamentDirector>(m_server.get(), std::move(tournament),
                                                             rng->generate());
    emit tournamentStarted();
}

void LobbyPage::onClientMatchStarted(const QJsonObject &start)
{
    if (start.contains("match"))
        return; // Manche de tournoi : gérée par le suivi du tournoi.
    const auto setup = net::parseStart(start, PartCatalog::instance());
    if (!setup || !m_client)
        return;
    m_wasInGame = true;
    m_session = std::make_unique<net::ClientSession>(m_client.get(), *setup);
    emit matchStarted();
}

} // namespace toupie
