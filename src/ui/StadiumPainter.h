#pragma once

#include "game/Stadium.h"

#include <QPointF>
#include <QString>
#include <QWidget>

class QPainter;

namespace toupie {

namespace StadiumPainter {

/// Dessine le stadium (sol, motif de surface, rebord ou vide) centré sur
/// `center`, avec `scale` pixels par unité de monde.
void paint(QPainter &painter, const Stadium &stadium, const StadiumGeometry &geometry,
           QPointF center, double scale);

/// Résumé lisible des caractéristiques (« Vitesse +15 % · Rebord haut… »).
QString describeTraits(const Stadium &stadium);

} // namespace StadiumPainter

/// Aperçu d'un stadium : miniature, nom, description et caractéristiques.
class StadiumWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StadiumWidget(QWidget *parent = nullptr);

    void setStadium(const Stadium &stadium);
    QSize sizeHint() const override { return {420, 120}; }
    QSize minimumSizeHint() const override { return {300, 110}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Stadium m_stadium;
};

} // namespace toupie
