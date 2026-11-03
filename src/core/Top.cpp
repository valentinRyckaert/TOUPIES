#include "core/Top.h"

#include "core/PartCatalog.h"

#include <utility>

namespace toupie {

namespace {
constexpr auto kNameKey = "name";
constexpr auto kWheelKey = "wheel";
constexpr auto kMiddleKey = "middle";
constexpr auto kTipKey = "tip";
} // namespace

Top::Top(QString name, Part wheel, Part middle, Part tip)
    : m_name(std::move(name))
    , m_wheel(std::move(wheel))
    , m_middle(std::move(middle))
    , m_tip(std::move(tip))
{
}

const Part &Top::part(PartKind kind) const
{
    switch (kind) {
    case PartKind::Wheel:
        return m_wheel;
    case PartKind::Middle:
        return m_middle;
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
        && m_tip.isValid() && m_tip.kind == PartKind::Tip;
}

QJsonObject Top::toJson() const
{
    return {
        {kNameKey, m_name},
        {kWheelKey, m_wheel.id},
        {kMiddleKey, m_middle.id},
        {kTipKey, m_tip.id},
    };
}

std::optional<Top> Top::fromJson(const QJsonObject &json, const PartCatalog &catalog)
{
    const auto wheel = catalog.find(json.value(kWheelKey).toString());
    const auto middle = catalog.find(json.value(kMiddleKey).toString());
    const auto tip = catalog.find(json.value(kTipKey).toString());
    if (!wheel || !middle || !tip)
        return std::nullopt;

    Top top(json.value(kNameKey).toString(), *wheel, *middle, *tip);
    if (!top.isValid())
        return std::nullopt;
    return top;
}

} // namespace toupie
