#include "core/Top.h"

#include "core/PartCatalog.h"

#include <utility>

namespace toupie {

namespace {
constexpr auto kNameKey = "name";
constexpr auto kWheelKey = "wheel";
constexpr auto kMiddleKey = "middle";
constexpr auto kTipKey = "tip";
constexpr auto kBoltKey = "bolt";
} // namespace

Top::Top(QString name, Part wheel, Part middle, Part tip, Part bolt)
    : m_name(std::move(name))
    , m_wheel(std::move(wheel))
    , m_middle(std::move(middle))
    , m_tip(std::move(tip))
    , m_bolt(std::move(bolt))
{
    if (!m_bolt.isValid()) {
        const QList<Part> bolts = PartCatalog::instance().parts(PartKind::Bolt);
        if (!bolts.isEmpty())
            m_bolt = bolts.first();
    }
}

const Part &Top::part(PartKind kind) const
{
    switch (kind) {
    case PartKind::Wheel:
        return m_wheel;
    case PartKind::Middle:
        return m_middle;
    case PartKind::Bolt:
        return m_bolt;
    case PartKind::Tip:
        break;
    }
    return m_tip;
}

void Top::setPart(const Part &part)
{
    switch (part.kind) {
    case PartKind::Wheel:
        m_wheel = part;
        break;
    case PartKind::Middle:
        m_middle = part;
        break;
    case PartKind::Tip:
        m_tip = part;
        break;
    case PartKind::Bolt:
        m_bolt = part;
        break;
    }
}

Stats Top::stats() const
{
    return m_wheel.stats + m_middle.stats + m_tip.stats;
}

bool Top::isValid() const
{
    return !m_name.trimmed().isEmpty()
        && m_wheel.isValid() && m_wheel.kind == PartKind::Wheel
        && m_middle.isValid() && m_middle.kind == PartKind::Middle
        && m_tip.isValid() && m_tip.kind == PartKind::Tip
        && m_bolt.isValid() && m_bolt.kind == PartKind::Bolt;
}

QJsonObject Top::toJson() const
{
    return {
        {kNameKey, m_name},
        {kWheelKey, m_wheel.id},
        {kMiddleKey, m_middle.id},
        {kTipKey, m_tip.id},
        {kBoltKey, m_bolt.id},
    };
}

std::optional<Top> Top::fromJson(const QJsonObject &json, const PartCatalog &catalog)
{
    const auto wheel = catalog.find(json.value(kWheelKey).toString());
    const auto middle = catalog.find(json.value(kMiddleKey).toString());
    const auto tip = catalog.find(json.value(kTipKey).toString());
    if (!wheel || !middle || !tip)
        return std::nullopt;

    Part bolt;
    if (json.contains(kBoltKey)) {
        const auto found = catalog.find(json.value(kBoltKey).toString());
        if (!found || found->kind != PartKind::Bolt)
            return std::nullopt;
        bolt = *found;
    } else if (const QList<Part> bolts = catalog.parts(PartKind::Bolt); !bolts.isEmpty()) {
        bolt = bolts.first(); // Toupie d'une version sans boulon.
    }
    Top top(json.value(kNameKey).toString(), *wheel, *middle, *tip, bolt);
    if (!top.isValid())
        return std::nullopt;
    return top;
}

} // namespace toupie
