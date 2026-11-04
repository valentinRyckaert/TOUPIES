#include "ui/LanMenuPage.h"

#include "game/Stadium.h"
#include "ui/StadiumPainter.h"
#include "ui/Widgets.h"

#include <QDialog>

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace toupie {

namespace {
const QString kNameSetting = QStringLiteral("lan/playerName");

QString defaultPlayerName()
{
    const QString user = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME"));
    return user.isEmpty() ? QStringLiteral("Joueur") : user.left(net::kMaxNameLength);
}

QString stadiumName(const QString &id)
{
    const auto stadium = Stadium::find(id);
    return stadium ? stadium->name : id;
}
} // namespace

LanMenuPage::LanMenuPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);

    m_title = new QLabel(tr("Multijoueur en réseau local"), this);
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.6);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setObjectName(QStringLiteral("title"));
    root->addWidget(m_title);

    auto *topRow = new QHBoxLayout;
    auto *nameForm = new QFormLayout;
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setMaxLength(net::kMaxNameLength);
    m_nameEdit->setText(QSettings().value(kNameSetting, defaultPlayerName()).toString());
    nameForm->addRow(tr("Votre pseudo"), m_nameEdit);
    topRow->addLayout(nameForm, 1);
    m_openCreateButton = new QPushButton(tr("＋  Créer un lobby…"), this);
    m_openCreateButton->setObjectName(QStringLiteral("accent"));
    m_openCreateButton->setMinimumHeight(36);
    topRow->addWidget(m_openCreateButton);
    root->addLayout(topRow);

    auto *columns = new QHBoxLayout;
    root->addLayout(columns, 1);

    // --- Créer ------------------------------------------------------------------
    auto *createBox = m_createBox = new QGroupBox(tr("Créer un lobby"));
    auto *createLayout = new QVBoxLayout(createBox);
    auto *createForm = new QFormLayout;
    m_lobbyNameEdit = new QLineEdit(createBox);
    m_lobbyNameEdit->setMaxLength(net::kMaxNameLength);
    m_lobbyNameEdit->setPlaceholderText(tr("Lobby de %1").arg(m_nameEdit->text()));
    createForm->addRow(tr("Nom du lobby"), m_lobbyNameEdit);
    m_playersCombo = new QComboBox(createBox);
    for (int n = 2; n <= 4; ++n)
        m_playersCombo->addItem(tr("%1 joueurs").arg(n), n);
    createForm->addRow(tr("Nombre de joueurs"), m_playersCombo);
    m_stadiumCombo = new QComboBox(createBox);
    for (const Stadium &stadium : Stadium::all())
        m_stadiumCombo->addItem(stadium.name, stadium.id);
    createForm->addRow(tr("Stadium"), m_stadiumCombo);
    createLayout->addLayout(createForm);
    m_stadiumPreview = new StadiumWidget(createBox);
    m_stadiumPreview->setMinimumHeight(130);
    createLayout->addWidget(m_stadiumPreview);
    createLayout->addStretch();
    m_createButton = new QPushButton(tr("Créer le lobby"), createBox);
    m_createButton->setObjectName(QStringLiteral("accent"));
    m_createButton->setMinimumHeight(36);
    createLayout->addWidget(m_createButton);
    QDialog *createPopupDialog = createPopup(this, tr("Créer un lobby"), createBox);
    connect(m_openCreateButton, &QPushButton::clicked, createPopupDialog, &QDialog::open);
    connect(m_createButton, &QPushButton::clicked, createPopupDialog, &QDialog::accept);

    // --- Rejoindre --------------------------------------------------------------
    auto *joinBox = new QGroupBox(tr("Rejoindre un lobby"), this);
    auto *joinLayout = new QVBoxLayout(joinBox);
    auto *discoveryRow = new QHBoxLayout;
    discoveryRow->addWidget(new QLabel(tr("Lobbys détectés sur le réseau :"), joinBox), 1);
    m_refreshButton = new QPushButton(tr("Actualiser"), joinBox);
    discoveryRow->addWidget(m_refreshButton);
    joinLayout->addLayout(discoveryRow);
    m_lobbyList = new QTreeWidget(joinBox);
    m_lobbyList->setHeaderLabels({tr("Lobby"), tr("Hôte"), tr("Joueurs"), tr("Stadium"),
                                  tr("Adresse")});
    m_lobbyList->setRootIsDecorated(false);
    m_lobbyList->setUniformRowHeights(true);
    m_lobbyList->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_lobbyList->header()->setStretchLastSection(true);
    joinLayout->addWidget(m_lobbyList, 1);
    m_discoveryStatus = new QLabel(joinBox);
    m_discoveryStatus->setWordWrap(true);
    m_discoveryStatus->setObjectName(QStringLiteral("hint"));
    joinLayout->addWidget(m_discoveryStatus);
    m_joinSelectedButton = new QPushButton(tr("Rejoindre le lobby sélectionné"), joinBox);
    joinLayout->addWidget(m_joinSelectedButton);

    joinLayout->addWidget(new QLabel(tr("Ou par nom de lobby / adresse IP[:port] :"), joinBox));
    auto *typedRow = new QHBoxLayout;
    m_joinEdit = new QLineEdit(joinBox);
    m_joinEdit->setPlaceholderText(tr("ex. « Lobby de Léa » ou 192.168.1.20:%1")
                                       .arg(net::kDefaultTcpPort));
    m_joinButton = new QPushButton(tr("Rejoindre"), joinBox);
    typedRow->addWidget(m_joinEdit, 1);
    typedRow->addWidget(m_joinButton);
    joinLayout->addLayout(typedRow);
    columns->addWidget(joinBox, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto *bottom = new QHBoxLayout;
    auto *backButton = new QPushButton(tr("Retour au menu"), this);
    bottom->addWidget(backButton);
    bottom->addStretch();
    root->addLayout(bottom);

    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        QSettings().setValue(kNameSetting, text);
        m_lobbyNameEdit->setPlaceholderText(tr("Lobby de %1").arg(playerName()));
    });
    connect(m_stadiumCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (const auto stadium = Stadium::find(m_stadiumCombo->currentData().toString()))
            m_stadiumPreview->setStadium(*stadium);
    });
    connect(&m_browser, &net::LobbyBrowser::lobbiesChanged, this, &LanMenuPage::refreshLobbies);
    connect(&m_browser, &net::LobbyBrowser::scanFinished, this,
            &LanMenuPage::updateDiscoveryStatus);
    connect(m_refreshButton, &QPushButton::clicked, this, [this] {
        m_browser.rescan();
        updateDiscoveryStatus();
    });
    connect(m_lobbyList, &QTreeWidget::itemDoubleClicked, this, &LanMenuPage::joinSelected);
    connect(m_lobbyList, &QTreeWidget::itemSelectionChanged, this, [this] {
        m_joinSelectedButton->setEnabled(!m_lobbyList->selectedItems().isEmpty());
    });
    connect(m_joinSelectedButton, &QPushButton::clicked, this, &LanMenuPage::joinSelected);
    connect(m_joinButton, &QPushButton::clicked, this, &LanMenuPage::joinTyped);
    connect(m_joinEdit, &QLineEdit::returnPressed, this, &LanMenuPage::joinTyped);
    connect(m_createButton, &QPushButton::clicked, this, &LanMenuPage::requestHost);
    connect(backButton, &QPushButton::clicked, this, &LanMenuPage::backRequested);

    m_joinSelectedButton->setEnabled(false);
}

QString LanMenuPage::playerName() const
{
    return net::sanitizeName(m_nameEdit->text(), defaultPlayerName());
}

void LanMenuPage::setTournament(const std::optional<TournamentSettings> &settings)
{
    m_tournament = settings;
    m_title->setText(settings ? tr("Tournoi en réseau local") : tr("Multijoueur en réseau local"));
    m_createBox->setTitle(settings ? tr("Créer un tournoi (lobby des joueurs humains)")
                                   : tr("Créer un lobby"));
    m_openCreateButton->setText(settings ? tr("＋  Créer un tournoi…") : tr("＋  Créer un lobby…"));
    m_createButton->setText(settings ? tr("Créer le lobby du tournoi") : tr("Créer le lobby"));
}

void LanMenuPage::activate()
{
    m_udpAvailable = m_browser.start();
    refreshLobbies();
    updateDiscoveryStatus();
}

void LanMenuPage::updateDiscoveryStatus()
{
    QStringList lines;
    if (m_browser.isScanning())
        lines << tr("Recherche des lobbys sur le réseau local…");
    if (!m_udpAvailable) {
        lines << tr("Port UDP %1 indisponible : seule la recherche par balayage est active.")
                     .arg(net::kDiscoveryPort);
    } else if (!m_browser.udpWorks() && !m_browser.lobbies().isEmpty()) {
        // Des lobbys sont trouvés par balayage mais aucune annonce n'arrive.
        lines << tr("Les annonces UDP n'arrivent pas (pare-feu ?). Pour une détection "
                    "instantanée, autorisez le port UDP %1 en entrée, par exemple : "
                    "sudo firewall-cmd --permanent --add-port=%1/udp && sudo firewall-cmd --reload")
                     .arg(net::kDiscoveryPort);
    }
    m_discoveryStatus->setText(lines.join('\n'));
    m_discoveryStatus->setVisible(!lines.isEmpty());
}

void LanMenuPage::deactivate()
{
    m_browser.stop();
}

void LanMenuPage::setBusy(bool busy, const QString &message)
{
    for (QWidget *w : {static_cast<QWidget *>(m_createButton),
                       static_cast<QWidget *>(m_openCreateButton),
                       static_cast<QWidget *>(m_joinButton),
                       static_cast<QWidget *>(m_lobbyList)})
        w->setEnabled(!busy);
    m_joinSelectedButton->setEnabled(!busy && !m_lobbyList->selectedItems().isEmpty());
    m_status->setText(message);
}

void LanMenuPage::refreshLobbies()
{
    const QString selectedId = m_lobbyList->currentItem()
                                 ? m_lobbyList->currentItem()->data(0, Qt::UserRole).toString()
                                 : QString();
    m_lobbyList->clear();
    for (const net::LobbyAnnouncement &lobby : m_browser.lobbies()) {
        const QString players = QStringLiteral("%1/%2%3")
                                    .arg(lobby.players)
                                    .arg(lobby.maxPlayers)
                                    .arg(lobby.inGame ? tr(" (en partie)") : QString());
        auto *item = new QTreeWidgetItem(
            m_lobbyList, {lobby.tournament ? tr("🏆 %1 (tournoi)").arg(lobby.name) : lobby.name,
                          lobby.hostName, players, stadiumName(lobby.stadiumId),
                          QStringLiteral("%1:%2").arg(lobby.address.toString()).arg(lobby.port)});
        item->setData(0, Qt::UserRole, lobby.lobbyId);
        item->setData(1, Qt::UserRole, lobby.address.toString());
        item->setData(2, Qt::UserRole, lobby.port);
        const bool joinable = !lobby.inGame && lobby.players < lobby.maxPlayers;
        if (!joinable) {
            for (int c = 0; c < item->columnCount(); ++c)
                item->setForeground(c, palette().color(QPalette::Disabled, QPalette::Text));
        }
        if (lobby.lobbyId == selectedId)
            m_lobbyList->setCurrentItem(item);
    }
    if (m_lobbyList->topLevelItemCount() == 0) {
        auto *item = new QTreeWidgetItem(
            m_lobbyList, {m_browser.isScanning() ? tr("Recherche de lobbys…")
                                                 : tr("Aucun lobby détecté pour l'instant")});
        item->setFlags(Qt::NoItemFlags);
    }
    updateDiscoveryStatus();
}

void LanMenuPage::joinSelected()
{
    QTreeWidgetItem *item = m_lobbyList->currentItem();
    if (!item || item->data(0, Qt::UserRole).toString().isEmpty())
        return;
    emit joinRequested(item->data(1, Qt::UserRole).toString(),
                       static_cast<quint16>(item->data(2, Qt::UserRole).toUInt()));
}

void LanMenuPage::joinTyped()
{
    const QString text = m_joinEdit->text().trimmed();
    if (text.isEmpty())
        return;

    // 1. Un lobby découvert portant ce nom.
    if (const auto lobby = m_browser.findByName(text)) {
        emit joinRequested(lobby->address.toString(), lobby->port);
        return;
    }

    // 2. Une adresse (IP ou nom de machine), avec un port optionnel.
    QString host = text;
    quint16 port = net::kDefaultTcpPort;
    const qsizetype colon = text.lastIndexOf(':');
    if (colon > 0 && text.count(':') == 1) {
        bool ok = false;
        const uint value = text.mid(colon + 1).toUInt(&ok);
        if (!ok || value == 0 || value > 65535) {
            m_status->setText(tr("Port invalide : « %1 ».").arg(text.mid(colon + 1)));
            return;
        }
        host = text.left(colon);
        port = static_cast<quint16>(value);
    }
    if (host.contains(' ')) {
        m_status->setText(tr("Aucun lobby nommé « %1 » n'a été détecté sur le réseau.").arg(text));
        return;
    }
    emit joinRequested(host, port);
}

void LanMenuPage::requestHost()
{
    net::LobbyServer::Settings settings;
    settings.hostName = playerName();
    settings.lobbyName = m_lobbyNameEdit->text().trimmed().isEmpty()
                           ? tr("Lobby de %1").arg(playerName())
                           : m_lobbyNameEdit->text().trimmed();
    settings.maxPlayers = m_playersCombo->currentData().toInt();
    settings.stadiumId = m_stadiumCombo->currentData().toString();
    settings.tournament = m_tournament;
    emit hostRequested(settings);
}

} // namespace toupie
