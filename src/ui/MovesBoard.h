#pragma once

#include <QWidget>

namespace toupie {

/// Fiches des sept coups spéciaux, reprises de la planche de la maquette
/// (design/svg/13-coups-speciaux.svg) : illustration de l'effet, emblème,
/// boulon, forme, point fort, point faible et jauges dégâts / recul / portée.
/// Les fiches se rangent en colonnes selon la largeur disponible.
class MovesBoard : public QWidget
{
    Q_OBJECT

public:
    explicit MovesBoard(QWidget *parent = nullptr);

    QSize sizeHint() const override { return {1100, heightForWidth(1100)}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    static int columns(int width);
};

/// Page des coups spéciaux (ouverte depuis l'atelier) : fiches et bouton retour.
class MovesPage : public QWidget
{
    Q_OBJECT

public:
    explicit MovesPage(QWidget *parent = nullptr);

signals:
    void backRequested();
};

} // namespace toupie
