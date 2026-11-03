#include "core/TopRepository.h"

#include "core/PartCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace toupie {

TopRepository::TopRepository(const PartCatalog &catalog, QString filePath)
    : m_catalog(catalog)
    , m_filePath(filePath.isEmpty() ? defaultFilePath() : std::move(filePath))
{
}

QString TopRepository::defaultFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("tops.json"));
}

bool TopRepository::load()
{
    m_tops.clear();

    QFile file(m_filePath);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return false;

    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (auto top = Top::fromJson(value.toObject(), m_catalog))
            upsert(*top);
    }
    return true;
}

bool TopRepository::save() const
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    QJsonArray array;
    for (const Top &top : m_tops)
        array.append(top.toJson());

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    return file.commit();
}

QList<Top> TopRepository::allTops() const
{
    QList<Top> result = m_catalog.presets();
    result.append(m_tops);
    return result;
}

void TopRepository::upsert(const Top &top)
{
    const int index = indexOf(top.name());
    if (index >= 0)
        m_tops[index] = top;
    else
        m_tops.append(top);
}

bool TopRepository::remove(const QString &name)
{
    const int index = indexOf(name);
    if (index < 0)
        return false;
    m_tops.removeAt(index);
    return true;
}

bool TopRepository::contains(const QString &name) const
{
    return indexOf(name) >= 0;
}

int TopRepository::indexOf(const QString &name) const
{
    for (int i = 0; i < m_tops.size(); ++i) {
        if (m_tops.at(i).name().compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

} // namespace toupie
