#pragma once

#include "core/Top.h"

#include <QWidget>

#include <optional>

namespace toupie {

/// Fiche d'une toupie : aperçu animé, type, sens de rotation et barres de stats.
class StatsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatsWidget(QWidget *parent = nullptr);

    void setTop(const std::optional<Top> &top);
    /// Affiche « ? » à la place de la toupie (adversaire aléatoire).
    void setMystery(const QString &caption);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    std::optional<Top> m_top;
    QString m_mysteryCaption;
    double m_angle = 0.0;
    int m_timerId = 0;
};

} // namespace toupie
