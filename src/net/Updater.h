#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

#include <optional>

namespace toupie::net {

/// Release plus récente que la version installée.
struct UpdateInfo
{
    QString version;  ///< Sans le « v » du tag.
    QUrl package;     ///< Paquet de cette plateforme.
    QUrl page;        ///< Page de la release.
};

/// Vrai si `candidate` (« v1.2.0 » ou « 1.2.0 ») est plus récente que `current`.
bool isNewerVersion(const QString &candidate, const QString &current);
/// Fin du nom du paquet de cette plateforme dans les releases (voir release.yml).
QString platformPackageSuffix();
/// Release GitHub (JSON de l'API) plus récente que `current` avec un paquet
/// se terminant par `suffix` ; nullopt sinon.
std::optional<UpdateInfo> parseLatestRelease(const QJsonObject &release, const QString &current,
                                             const QString &suffix);

/// Mises à jour depuis les releases GitHub : vérification, téléchargement,
/// remplacement de l'application installée et relance.
class Updater : public QObject
{
    Q_OBJECT

public:
    explicit Updater(QObject *parent = nullptr);

    void check();
    void download(const UpdateInfo &update);
    /// L'application peut se remplacer elle-même : AppImage (Linux), dossier
    /// accessible en écriture (Windows), bundle .app (macOS).
    static bool canSelfUpdate();
    /// Remplace l'application par le paquet téléchargé et lance la nouvelle
    /// version ; l'appelant doit ensuite quitter. False en cas d'échec.
    static bool installAndRestart(const QString &package, QString *error);

signals:
    void updateAvailable(const toupie::net::UpdateInfo &update);
    void upToDate();
    void progress(qint64 received, qint64 total);
    void downloaded(const QString &package);
    void failed(const QString &reason);

private:
    QNetworkAccessManager m_network;
};

} // namespace toupie::net
