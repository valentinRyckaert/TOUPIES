#include "ui/LobbyPage.h"

#include "core/PartCatalog.h"
#include "core/TopRepository.h"
#include "game/Stadium.h"
#include "net/ClientSession.h"
#include "net/HostSession.h"
#include "net/MatchCodec.h"
#include "ui/ArenaWidget.h"
#include "ui/StadiumPainter.h"
#include "ui/StatsWidget.h"

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

    m_title = new QLabel(this);
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.6);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    root->addWidget(m_title);
    m_subtitle = new QLabel(this);
    m_subtitle->setWordWrap(true);
    m_subtitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_subtitle);

    // --- Paramètres du lobby ----------------------------------------------------
    auto *settingsRow = new QHBoxLayout;
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
    settingsRow->addLayout(form);
    m_stadiumPreview = new StadiumWidget(this);
    settingsRow->addWidget(m_stadiumPreview, 1);
    root->addLayout(settingsRow);

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
    root->addWidget(m_table);

    // --- Ma toupie --------------------------------------------------------------
    auto *topBox = new QGroupBox(tr("Votre toupie (parmi vos toupies locales)"), this);
    auto *topLayout = new QVBoxLayout(topBox);
    m_topCombo = new QComboBox(topBox);
    topLayout->addWidget(m_topCombo);
    m_topPreview = new StatsWidget(topBox);
    topLayout->addWidget(m_topPreview);
    root->addWidget(topBox);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    root->addStretch();

    auto *buttons = new QHBoxLayout;
    auto *leaveButton = new QPushButton(tr("Quitter le lobby"), this);
    m_readyButton = new QPushButton(tr("Je suis prêt"), this);
    m_readyButton->setCheckable(true);
    m_startButton = new QPushButton(tr("Lancer la partie"), this);
    m_startButton->setMinimumHeight(38);
    buttons->addWidget(leaveButton);
    buttons->addStretch();
    buttons->addWidget(m_readyButton);
    buttons->addWidget(m_startButton);
    root->addLayout(buttons);

    connect(leaveButton, &QPushButton::clicked, this, &LobbyPage::leaveRequested);
    connect(m_topCombo, &QComboBox::currentIndexChanged, this, &LobbyPage::sendSelectedTop);
    connect(m_readyButton, &QPushButton::toggled, this, [this](bool ready) {
        m_readyButton->setText(ready ? tr("Prêt ✓") : tr("Je suis prêt"));
        if (m_client)
            m_client->setReady(ready);
    });
    connect(m_startButton, &QPushButton::clicked, this, &LobbyPage::startMatch);
    connect(m_playersCombo, &QComboBox::activated, this, [this] {
        if (m_server)
            m_server->setMaxPlayers(m_playersCombo->currentData().toInt());
        rebuild(); // Valeur refusée : on réaffiche la valeur réelle.
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
    sendSelectedTop();
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
        sendSelectedTop();
        rebuild();
        emit joined();
    });
    connect(client, &net::LobbyClient::joinFailed, this, [this](const QString &reason) {
        disposeLater(m_client);
        emit joinFailed(reason);
    });
    connect(client, &net::LobbyClient::connectionLost, this, [this](const QString &reason) {
        m_session.reset();
        disposeLater(m_client);
        emit connectionLost(reason);
    });
    connect(client, &net::LobbyClient::lobbyChanged, this, &LobbyPage::onLobbyChanged);
    connect(client, &net::LobbyClient::matchStarted, this, &LobbyPage::onClientMatchStarted);
    client->connectToHost(hostAddress, port, playerName);
}

void LobbyPage::leave()
{
    m_session.reset();
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
    const QString current = m_topCombo->currentText();
    m_tops = m_repository.allTops();
    {
        const QSignalBlocker blocker(m_topCombo);
        m_topCombo->clear();
        for (const Top &top : m_tops)
            m_topCombo->addItem(QStringLiteral("%1  [%2]").arg(top.name(), toString(top.type())));
        m_topCombo->setCurrentIndex(qMax(0, m_topCombo->findText(current)));
    }
    sendSelectedTop();
}

void LobbyPage::sendSelectedTop()
{
    const int index = m_topCombo->currentIndex();
    if (index < 0 || index >= m_tops.size())
        return;
    const Top &top = m_tops.at(index);
    m_topPreview->setTop(top);
    if (m_server) {
        m_server->setHostTop(top);
    } else if (m_client && m_client->isJoined()) {
        m_client->selectTop(top);
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
    if (m_client && m_wasInGame && !info->inGame)
        emit returnedToLobby();
    m_wasInGame = info->inGame;
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
            cells << name << (p.top ? p.top->name() : tr("—"))
                  << (p.top ? toString(p.top->type()) : QString())
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

void LobbyPage::onClientMatchStarted(const QJsonObject &start)
{
    const auto setup = net::parseStart(start, PartCatalog::instance());
    if (!setup || !m_client)
        return;
    m_wasInGame = true;
    m_session = std::make_unique<net::ClientSession>(m_client.get(), *setup);
    emit matchStarted();
}

} // namespace toupie
