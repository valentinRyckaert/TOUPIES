#pragma once

#include <QWidget>

class QDialog;

namespace toupie {

/// Fiches des sept coups spéciaux, reprises de la planche de la maquette
/// (design/svg/13-coups-speciaux.svg) : illustration de l'effet, emblème,
/// boulon, forme, point fort, point faible et jauges dégâts / recul / portée.
class MovesBoard : public QWidget
{
    Q_OBJECT

public:
    explicit MovesBoard(QWidget *parent = nullptr);

    QSize sizeHint() const override { return {1200, 940}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return width * 1750 / 2220; }

    /// Fenêtre (non bloquante) qui affiche la planche.
    static QDialog *createDialog(QWidget *parent);

protected:
    void paintEvent(QPaintEvent *event) override;
};

} // namespace toupie
