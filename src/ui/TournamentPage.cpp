#include "ui/TournamentPage.h"

#include "game/Stadium.h"
#include "net/TournamentSession.h"
#include "ui/Theme.h"
#include "ui/TopPainter.h"
#include "ui/Widgets.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

#include <optional>

namespace toupie {

namespace {
QColor withAlpha(QColor c, int a)
{
    c.setAlpha(a);
    return c;
}

QFont smallCaps(double size = 13)
{
    QFont f = Theme::bodyFont(size, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    f.setCapitalization(QFont::AllUppercase);
    return f;
}

QFont display(double size, double spacing = 1.0, bool italic = false)
{
    QFont f = Theme::displayFont(size, italic);
    f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    return f;
}

/// Nom court d'un tour pour la liste des résultats (« Quart », « J3 »…).
QString shortRoundName(const Tournament &t, int round)
{
    if (t.settings().format == TournamentFormat::League)
        return TournamentPage::tr("J%1").arg(round + 1);
    switch (t.roundCount() - round) {
    case 1:
        return TournamentPage::tr("Finale");
    case 2:
        return TournamentPage::tr("Demi");
    case 3:
        return TournamentPage::tr("Quart");
    case 4:
        return TournamentPage::tr("8e");
    default:
        return TournamentPage::tr("16e");
    }
}

/// Puce inclinée de la barre du haut (format, participants, manches).
class Chip : public QWidget
{
public:
    Chip(const QColor &color, const QString &icon, QWidget *parent)
        : QWidget(parent)
        , m_color(color)
        , m_icon(icon)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setText(const QString &text)
    {
        m_text = text.toUpper();
        updateGeometry();
        update();
    }

    QSize sizeHint() const override
    {
        return {QFontMetrics(display(13, 1.6)).horizontalAdvance(m_text) + 66, 34};
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        const QPainterPath path = Theme::skewed(r, 10);
        p.fillPath(path, withAlpha(m_color, 26));
        p.setPen(QPen(withAlpha(m_color, 180), 1.0));
        p.drawPath(path);
        Theme::paintIcon(p, m_icon, QRectF(22, r.center().y() - 8, 16, 16), m_color);
        p.setFont(display(13, 1.6));
        p.setPen(m_color);
        p.drawText(r.adjusted(46, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, m_text);
    }

private:
    QColor m_color;
    QString m_icon;
    QString m_text;
};

QWidget *transparentScroll(QScrollArea *scroll, QWidget *content)
{
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }"));
    return scroll;
}
} // namespace

namespace {

// --- Tableau à élimination directe -------------------------------------------------

class Bracket : public QWidget
{
public:
    explicit Bracket(QWidget *parent)
        : QWidget(parent)
    {
    }

    void setTournament(const Tournament &tournament, int local, const net::TournamentController *controller)
    {
        m_t = tournament;
        m_local = local;
        m_live.clear();
        for (int i = 0; i < m_t.rounds().value(m_t.currentRound()).size(); ++i) {
            if (const auto *live = controller->liveOf(i); live && live->series.rounds > 0)
                m_live.insert(i, live->series.score);
        }
        updateGeometry();
        update();
    }

    QSize sizeHint() const override { return {columns() * 380 + 150, 50 + firstRound() * 210}; }
    // Cases lisibles (pas moins de 190 px) : au-delà, le tableau défile dans les deux sens.
    QSize minimumSizeHint() const override { return {columns() * 220 + 130, 50 + firstRound() * 112}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const int cols = columns();
        if (cols == 0)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double pitchX = std::min(380.0, (width() - 130.0) / cols);
        const double boxW = std::min(250.0, pitchX - 30.0); // Maquette : cases de 250, pas de 380.
        const double top = 44.0;
        const double pitchY = std::max(112.0, (height() - top - 10.0) / firstRound());
        const double boxH = std::min(100.0, pitchY - 12.0);
        auto colX = [&](int r) { return 4.0 + r * pitchX; };
        // Ordonnée de chaque match : au milieu des deux matchs qui l'alimentent.
        QList<QList<double>> ys;
        for (int r = 0; r < cols; ++r) {
            QList<double> col;
            for (int i = 0; i < (firstRound() >> r); ++i)
                col << (r == 0 ? top + i * pitchY + (pitchY - boxH) / 2.0 : (ys[r - 1][2 * i] + ys[r - 1][2 * i + 1]) / 2.0);
            ys << col;
        }
        auto has = [&](int r, int i, int who) {
            if (who < 0 || r >= m_t.rounds().size())
                return false;
            const TournamentMatch &m = m_t.rounds().at(r).at(i);
            return m.a == who || m.b == who;
        };

        // Titres des colonnes.
        p.setFont(smallCaps());
        for (int r = 0; r < cols; ++r) {
            p.setPen(r == m_t.currentRound() && !m_t.isFinished() ? Theme::kAccent : Theme::kMuted);
            p.drawText(QRectF(colX(r), 8, pitchX, 20), Qt::AlignLeft | Qt::AlignVCenter, m_t.roundName(r));
        }

        // Liaisons, puis le chemin du joueur local en vert.
        QPainterPath links, path;
        for (int r = 0; r + 1 < cols; ++r) {
            const double x1 = colX(r) + boxW, x2 = colX(r + 1), mid = (x1 + x2) / 2.0;
            for (int i = 0; i + 1 < ys[r].size(); i += 2) {
                const double y1 = ys[r][i] + boxH / 2, y2 = ys[r][i + 1] + boxH / 2, yn = ys[r + 1][i / 2] + boxH / 2;
                links.moveTo(x1, y1);
                links.lineTo(mid, y1);
                links.lineTo(mid, y2);
                links.lineTo(x1, y2);
                links.moveTo(mid, yn);
                links.lineTo(x2, yn);
                for (int k : {i, i + 1}) {
                    if (has(r, k, m_local) && has(r + 1, i / 2, m_local)) {
                        path.moveTo(x1, ys[r][k] + boxH / 2);
                        path.lineTo(mid, ys[r][k] + boxH / 2);
                        path.lineTo(mid, yn);
                        path.lineTo(x2, yn);
                    }
                }
            }
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Theme::kBorderBright, 1.5));
        p.drawPath(links);
        p.setPen(QPen(Theme::kAccent, 3.0));
        p.drawPath(path);

        for (int r = 0; r < cols; ++r) {
            for (int i = 0; i < ys[r].size(); ++i)
                paintMatch(p, QRectF(colX(r), ys[r][i], boxW, boxH), r, i);
        }

        // Champion.
        const double size = std::min(100.0, boxH);
        const QRectF cup(colX(cols - 1) + boxW + 30, ys[cols - 1][0] + boxH / 2 - size / 2, size, size);
        QRadialGradient glow(cup.center(), size * 0.8);
        glow.setColorAt(0.0, withAlpha(Theme::kWarning, 60));
        glow.setColorAt(1.0, withAlpha(Theme::kWarning, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(cup.center(), size * 0.8, size * 0.8);
        const QPainterPath frame = Theme::chamfer(cup, 20, 0, 20, 0);
        p.fillPath(frame, QColor(0x0e, 0x0b, 0x30));
        p.setPen(QPen(Theme::kWarning, 2.0));
        p.drawPath(frame);
        Theme::paintIcon(p, QStringLiteral("trophy"), cup.adjusted(size * 0.26, size * 0.26, -size * 0.26, -size * 0.26), Theme::kWarning);
        p.setFont(display(14, 2.0));
        p.setPen(Theme::kWarning);
        const QRectF caption(cup.center().x() - 80, cup.bottom() + 12, 160, 22);
        p.drawText(caption, Qt::AlignCenter, TournamentPage::tr("Champion").toUpper());
        if (const int champion = m_t.champion(); champion >= 0) {
            p.setFont(display(16, 1.0));
            p.setPen(Theme::kText);
            p.drawText(caption.translated(0, 24), Qt::AlignCenter, m_t.participants().at(champion).name.toUpper());
        }
    }

private:
    int columns() const { return m_t.settings().format == TournamentFormat::Knockout ? m_t.roundCount() : 0; }
    int firstRound() const { return std::max(1, (1 << columns()) / 2); }

    void paintMatch(QPainter &p, const QRectF &box, int r, int i) const
    {
        const bool known = r < m_t.rounds().size();
        const TournamentMatch m = known ? m_t.rounds().at(r).at(i) : TournamentMatch{};
        const bool you = m_local >= 0 && known && (m.a == m_local || m.b == m_local);
        const auto live = r == m_t.currentRound() ? m_live.find(i) : m_live.end();
        const bool hasScore = (m.played && !m.isBye()) || live != m_live.end();
        const std::array<int, 2> score = live != m_live.end() && !m.played ? *live : m.score;

        const QPainterPath frame = Theme::chamfer(box, 14, 0, 14, 0);
        p.fillPath(frame, QColor(0x0e, 0x0b, 0x30));
        p.setPen(QPen(you ? Theme::kAccent : Theme::kBorder, you ? 2.0 : 1.2));
        p.drawPath(frame);
        p.setPen(QPen(Theme::kBorder, 1.0));
        p.drawLine(QPointF(box.left() + 14, box.top() + box.height() * 0.52),
                   QPointF(box.right() - 14, box.top() + box.height() * 0.52));
        const int who[2] = {m.a, m.b};
        for (int k = 0; k < 2; ++k) {
            const double cy = box.top() + box.height() * (k == 0 ? 0.3 : 0.74);
            const QRectF row(box.left() + 20, cy - 12, box.width() - 40, 24);
            if (who[k] < 0) {
                p.setFont(Theme::bodyFont(16, QFont::DemiBold));
                p.setPen(Theme::kDim);
                p.drawText(row, Qt::AlignLeft | Qt::AlignVCenter,
                           known && k == 1 && m.isBye() ? TournamentPage::tr("Exempt") : TournamentPage::tr("À déterminer"));
                continue;
            }
            const Participant &part = m_t.participants().at(who[k]);
            const bool win = m.played && score[k] > score[1 - k];
            if (!part.tops.isEmpty())
                TopPainter::paint(p, part.tops.first(), QPointF(box.left() + 30, cy), 16, k);
            p.setFont(display(box.width() < 200 ? 14 : 16, 1.0));
            p.setPen(win || !m.played ? Theme::kText : Theme::kMuted);
            const QRectF name = row.adjusted(32, 0, -26, 0);
            p.drawText(name, Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(part.name.toUpper(), Qt::ElideRight, int(name.width())));
            if (hasScore) {
                p.setFont(display(18));
                p.setPen(win ? Theme::kAccent : Theme::kDim);
                p.drawText(row, Qt::AlignRight | Qt::AlignVCenter, QString::number(score[k]));
            }
        }
        if (you && !m.played && r == m_t.currentRound()) {
            const QRectF tag(box.right() - 90, box.top() - 12, 90, 22);
            p.fillPath(Theme::skewed(tag, 6), Theme::kAccent);
            p.setFont(display(11, 1.5));
            p.setPen(QColor(0x03, 0x14, 0x0a));
            p.drawText(tag, Qt::AlignCenter, TournamentPage::tr("À jouer").toUpper());
        }
    }

    Tournament m_t;
    int m_local = -1;
    QHash<int, std::array<int, 2>> m_live;
};

// --- Votre match ----------------------------------------------------------------------

class MatchCard : public QWidget
{
public:
    struct Side
    {
        std::optional<Top> top;
        QString name;
        QString detail;
    };

    explicit MatchCard(QWidget *parent)
        : QWidget(parent)
    {
        setMinimumHeight(150);
    }

    void setMatch(const Side &left, const Side &right, const QString &info, const QString &message)
    {
        m_sides[0] = left;
        m_sides[1] = right;
        m_info = info;
        m_message = message;
        update();
    }

    QSize sizeHint() const override { return {480, 360}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double w = width(), h = height();
        if (!m_sides[0].top || !m_sides[1].top) {
            p.setFont(Theme::bodyFont(18, QFont::DemiBold));
            p.setPen(Theme::kMuted);
            p.drawText(rect().adjusted(10, 10, -10, -10), Qt::AlignCenter | Qt::TextWordWrap, m_message);
            return;
        }
        const double R = std::clamp((h - 170) / 2.3, 30.0, std::min(80.0, w * 0.14));
        const double cy = 20 + R * 1.25;
        const QColor names[2] = {Theme::kAccent, Theme::kDanger};
        for (int k = 0; k < 2; ++k) {
            const Top &top = *m_sides[k].top;
            const QPointF c(w * (k == 0 ? 0.23 : 0.77), cy);
            const QColor type = TopPainter::typeColor(top.type());
            QRadialGradient aura(c, R * 1.3);
            aura.setColorAt(0.6, withAlpha(type, 60));
            aura.setColorAt(1.0, withAlpha(type, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(aura);
            p.drawEllipse(c, R * 1.3, R * 1.3);
            const QRectF ring(c.x() - R * 1.14, c.y() - R * 1.14, R * 2.28, R * 2.28);
            p.setPen(QPen(type, R * 0.035, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            for (int i = 0; i < 3; ++i)
                p.drawArc(ring, (30 + k * 60 + i * 120) * 16, -80 * 16);
            TopPainter::paint(p, top, c, R, 0.4 + k * 1.6);
            p.setFont(display(22, 2.0));
            p.setPen(names[k]);
            const QRectF name(c.x() - w * 0.24, cy + R * 1.25 + 8, w * 0.48, 28);
            p.drawText(name, Qt::AlignCenter, p.fontMetrics().elidedText(m_sides[k].name.toUpper(), Qt::ElideRight, int(name.width())));
            p.setFont(Theme::bodyFont(17, QFont::DemiBold));
            p.setPen(Theme::kMuted);
            p.drawText(name.translated(0, 28), Qt::AlignCenter,
                       p.fontMetrics().elidedText(m_sides[k].detail, Qt::ElideRight, int(name.width())));
        }
        // « VS » à reflets magenta / cyan.
        const QRectF vs(w / 2 - 60, cy - 30, 120, 60);
        const QFont vsFont = display(46, 0.0, true);
        p.setFont(vsFont);
        p.setPen(withAlpha(Theme::kMagenta, 190));
        p.drawText(vs.translated(-5, 0), Qt::AlignCenter, QStringLiteral("VS"));
        p.setPen(withAlpha(Theme::kCyan, 190));
        p.drawText(vs.translated(5, 2), Qt::AlignCenter, QStringLiteral("VS"));
        p.setPen(Qt::white);
        p.drawText(vs, Qt::AlignCenter, QStringLiteral("VS"));

        double y = cy + R * 1.25 + 72;
        p.setFont(Theme::bodyFont(18, QFont::DemiBold));
        p.setPen(Theme::kText);
        p.drawText(QRectF(0, y, w, 26), Qt::AlignCenter, m_info);
        if (!m_message.isEmpty()) {
            p.setFont(Theme::bodyFont(16, QFont::DemiBold));
            p.setPen(Theme::kMuted);
            p.drawText(QRectF(0, y + 28, w, h - y - 28), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_message);
        }
    }

private:
    Side m_sides[2];
    QString m_info;
    QString m_message;
};

// --- Tours et résultats ---------------------------------------------------------------

class ResultList : public QWidget
{
public:
    struct Row
    {
        QString round;
        QString a;
        QString score;
        QString b;
        bool you = false;
    };

    explicit ResultList(QWidget *parent)
        : QWidget(parent)
    {
    }

    void setRows(const QList<Row> &rows)
    {
        m_rows = rows;
        setMinimumHeight(int(m_rows.size()) * 64);
        updateGeometry();
        update();
    }

    QSize sizeHint() const override { return {500, int(m_rows.size()) * 64}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double w = width() - 4, mid = w * 0.52, label = std::min(90.0, w * 0.17);
        for (int i = 0; i < m_rows.size(); ++i) {
            const Row &row = m_rows.at(i);
            const QRectF r(2, i * 64.0 + 2, w, 50);
            const QPainterPath path = Theme::skewed(r, 12);
            p.fillPath(path, row.you ? QColor(0x12, 0x30, 0x1f) : QColor(0x0e, 0x0b, 0x30));
            p.setPen(QPen(row.you ? Theme::kAccent : Theme::kBorder, 1.0));
            p.drawPath(path);
            p.setFont(smallCaps());
            p.setPen(Theme::kDim);
            p.drawText(r.adjusted(label * 0.22, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, row.round);
            p.setFont(display(17));
            p.setPen(Theme::kText);
            const QFontMetrics fm(p.font());
            const double nameWidth = mid - 40 - label;
            p.drawText(QRectF(label, r.top(), nameWidth, r.height()), Qt::AlignRight | Qt::AlignVCenter,
                       fm.elidedText(row.a.toUpper(), Qt::ElideRight, int(nameWidth)));
            p.drawText(QRectF(mid + 46, r.top(), w - mid - 70, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       fm.elidedText(row.b.toUpper(), Qt::ElideRight, int(w - mid - 70)));
            p.setFont(display(18));
            p.setPen(Theme::kCyan);
            p.drawText(QRectF(mid - 40, r.top(), 86, r.height()), Qt::AlignCenter, row.score);
        }
    }

private:
    QList<Row> m_rows;
};

// --- Classement de la ligue -----------------------------------------------------------

class StandingsList : public QWidget
{
public:
    struct Row
    {
        Standing standing;
        QString name;
        std::optional<Top> top;
        bool you = false;
    };

    explicit StandingsList(QWidget *parent)
        : QWidget(parent)
    {
    }

    void setRows(const QList<Row> &rows)
    {
        m_rows = rows;
        setMinimumHeight(kHeader + int(m_rows.size()) * kPitch);
        updateGeometry();
        update();
    }

    QSize sizeHint() const override { return {700, kHeader + int(m_rows.size()) * kPitch}; }
    QSize minimumSizeHint() const override { return {460, kHeader + int(m_rows.size()) * kPitch}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double w = width() - 4;
        // Colonnes chiffrées, alignées à droite : joués, victoires, défaites, points, différence.
        const QString heads[5] = {TournamentPage::tr("J"), TournamentPage::tr("V"), TournamentPage::tr("D"),
                                  TournamentPage::tr("Pts"), TournamentPage::tr("Diff.")};
        const double col = std::min(70.0, w * 0.09);
        auto colRect = [&](int i, double top, double h) { return QRectF(w - 24 - (5 - i) * col, top, col, h); };
        p.setFont(smallCaps());
        p.setPen(Theme::kMuted);
        p.drawText(QRectF(24, 0, 60, kHeader - 8), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("#"));
        p.drawText(QRectF(84, 0, 300, kHeader - 8), Qt::AlignLeft | Qt::AlignVCenter, TournamentPage::tr("Participant"));
        for (int i = 0; i < 5; ++i)
            p.drawText(colRect(i, 0, kHeader - 8), Qt::AlignRight | Qt::AlignVCenter, heads[i]);

        for (int i = 0; i < m_rows.size(); ++i) {
            const Row &row = m_rows.at(i);
            const Standing &st = row.standing;
            const QRectF r(2, kHeader + i * double(kPitch), w, kPitch - 10);
            const QPainterPath card = Theme::chamfer(r, 12, 0, 12, 0);
            p.fillPath(card, row.you ? QColor(0x12, 0x30, 0x1f) : QColor(0x0e, 0x0b, 0x30));
            p.setPen(QPen(row.you ? Theme::kAccent : i == 0 ? withAlpha(Theme::kWarning, 160) : Theme::kBorder,
                          row.you || i == 0 ? 1.6 : 1.0));
            p.drawPath(card);
            // Rang : or, argent et bronze pour le podium.
            const QColor podium[3] = {Theme::kWarning, QColor(0xc9, 0xd8, 0xff), QColor(0xff, 0x9a, 0x5c)};
            p.setFont(display(20));
            p.setPen(i < 3 ? podium[i] : Theme::kDim);
            p.drawText(QRectF(r.left() + 20, r.top(), 50, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(i + 1));
            if (row.top)
                TopPainter::paint(p, *row.top, QPointF(r.left() + 100, r.center().y()), 16, i);
            p.setFont(display(16, 1.0));
            p.setPen(Theme::kText);
            const QRectF name(r.left() + 126, r.top(), colRect(0, 0, 0).left() - r.left() - 136, r.height());
            p.drawText(name, Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(row.name.toUpper(), Qt::ElideRight, int(name.width())));
            const int diff = st.points - st.against;
            const QString values[5] = {QString::number(st.played), QString::number(st.won), QString::number(st.lost),
                                       QString::number(st.points),
                                       diff > 0 ? QStringLiteral("+%1").arg(diff) : QString::number(diff)};
            for (int c = 0; c < 5; ++c) {
                p.setFont(display(c == 3 ? 20 : 16));
                p.setPen(c == 3 ? Theme::kCyan : c == 4 ? (diff > 0 ? Theme::kAccent : diff < 0 ? Theme::kDanger : Theme::kMuted)
                                                        : Theme::kMuted);
                p.drawText(colRect(c, r.top(), r.height()), Qt::AlignRight | Qt::AlignVCenter, values[c]);
            }
        }
    }

private:
    static constexpr int kHeader = 34;
    static constexpr int kPitch = 60;
    QList<Row> m_rows;
};

} // namespace

// --- Page -----------------------------------------------------------------------------

TournamentPage::TournamentPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(22);

    auto *leave = new QPushButton(tr("Quitter"), this);
    leave->setProperty("variant", QStringLiteral("ghost"));
    leave->setProperty("glyph", QStringLiteral("back"));
    leave->setProperty("key", tr("Échap"));
    m_bar = new TopBar(leave, QString(), QString(), this);
    const std::pair<QColor, const char *> chips[3] = {
        {Theme::kCyan, "trophy"}, {Theme::kViolet, "user"}, {Theme::kAccent, "timer"}};
    for (int i = 0; i < 3; ++i) {
        auto *chip = new Chip(chips[i].first, QString::fromLatin1(chips[i].second), m_bar);
        m_bar->addOptional(chip);
        m_chips[i] = chip;
    }
    m_returnToLobby = new QPushButton(tr("Retour au lobby"), m_bar);
    m_returnToLobby->setProperty("variant", QStringLiteral("primary"));
    m_bar->row()->addSpacing(12);
    m_bar->row()->addWidget(m_returnToLobby);
    root->addWidget(m_bar);

    auto *body = new QHBoxLayout;
    body->setSpacing(28);
    root->addLayout(body, 1);

    m_boardBox = new QGroupBox(tr("Tableau"), this);
    m_boardBox->setProperty("accent", Theme::kCyan);
    auto *board = new QVBoxLayout(m_boardBox);
    board->setContentsMargins(18, 14, 18, 18);
    m_bracketScroll = new QScrollArea(m_boardBox);
    m_bracket = new Bracket(m_bracketScroll);
    board->addWidget(transparentScroll(m_bracketScroll, m_bracket));
    m_standingsScroll = new QScrollArea(m_boardBox);
    m_standings = new StandingsList(m_standingsScroll);
    board->addWidget(transparentScroll(m_standingsScroll, m_standings));
    body->addWidget(m_boardBox, 125); // Proportions de la maquette : 1250 / 560.

    auto *side = new QVBoxLayout;
    side->setSpacing(28);
    m_matchBox = new QGroupBox(tr("Votre match"), this);
    m_matchBox->setProperty("accent", Theme::kAccent);
    auto *matchLayout = new QVBoxLayout(m_matchBox);
    matchLayout->setContentsMargins(22, 8, 22, 22);
    m_matchCard = new MatchCard(m_matchBox);
    matchLayout->addWidget(m_matchCard, 1);
    m_ready = new QPushButton(tr("Prêt — jouer le match"), m_matchBox);
    m_ready->setProperty("variant", QStringLiteral("primary"));
    m_ready->setProperty("glyph", QStringLiteral("play"));
    m_ready->setMinimumHeight(62);
    matchLayout->addWidget(m_ready);
    side->addWidget(m_matchBox, 52);

    m_resultsBox = new QGroupBox(tr("Tours et résultats"), this);
    m_resultsBox->setProperty("accent", Theme::kViolet);
    auto *resultsLayout = new QVBoxLayout(m_resultsBox);
    resultsLayout->setContentsMargins(18, 14, 18, 18);
    m_resultsScroll = new QScrollArea(m_resultsBox);
    m_results = new ResultList(m_resultsScroll);
    resultsLayout->addWidget(transparentScroll(m_resultsScroll, m_results));
    side->addWidget(m_resultsBox, 36);
    auto *sideWidget = new QWidget(this);
    sideWidget->setLayout(side);
    sideWidget->setMinimumWidth(380);
    sideWidget->setMaximumWidth(560);
    side->setContentsMargins(0, 0, 0, 0);
    body->addWidget(sideWidget, 56);

    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this,
            &TournamentPage::leaveRequested);
    connect(leave, &QPushButton::clicked, this, &TournamentPage::leaveRequested);
    connect(m_returnToLobby, &QPushButton::clicked, this, &TournamentPage::returnToLobbyRequested);
    connect(m_ready, &QPushButton::clicked, this, [this] {
        if (!m_controller)
            return;
        const Tournament &t = m_controller->tournament();
        const QList<Top> tops = t.participants().value(m_controller->localParticipant()).tops;
        QList<int> order{0};
        if (t.settings().matchFormat == MatchFormat::Classic) {
            const auto chosen = chooseTopOrder(this, tops);
            if (!chosen || !m_controller)
                return;
            order = *chosen;
        }
        m_controller->submitOrder(order);
    });
}

void TournamentPage::setController(net::TournamentController *controller, bool host)
{
    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);
    m_controller = controller;
    m_host = host;
    if (m_controller)
        connect(m_controller, &net::TournamentController::changed, this, &TournamentPage::rebuild);
    rebuild();
}

QString TournamentPage::participantLabel(int participant) const
{
    if (!m_controller || participant < 0)
        return tr("exempt");
    const Participant &p = m_controller->tournament().participants().at(participant);
    if (participant == m_controller->localParticipant())
        return p.name == tr("Vous") ? p.name : tr("%1 (vous)").arg(p.name);
    return p.isBot() ? tr("%1 (%2)").arg(p.name, toString(p.level)) : p.name;
}

void TournamentPage::rebuild()
{
    if (!m_controller)
        return;
    const Tournament &t = m_controller->tournament();
    const TournamentSettings &settings = t.settings();
    const int local = m_controller->localParticipant();
    const int current = t.currentRound();
    const bool knockout = settings.format == TournamentFormat::Knockout;
    MatchSeries series;
    series.format = settings.matchFormat;

    // Barre du haut.
    m_bar->setTitle(t.isFinished() ? tr("Tournoi terminé") : tr("Tournoi · %1").arg(t.roundName(current)));
    static_cast<Chip *>(m_chips[0])->setText(toString(settings.format));
    static_cast<Chip *>(m_chips[1])->setText(tr("%1 participants").arg(t.participants().size()));
    static_cast<Chip *>(m_chips[2])->setText(settings.matchFormat == MatchFormat::Classic
                                                 ? tr("Classique · %1 points").arg(series.target())
                                                 : tr("Express · %1 manches").arg(series.target()));

    // Tableau (élimination directe) ou classement (ligue).
    m_bracketScroll->setVisible(knockout);
    m_standingsScroll->setVisible(!knockout);
    m_boardBox->setTitle(knockout ? tr("Tableau") : tr("Classement"));
    m_boardBox->setProperty("index", knockout ? tr("TOUR %1 / %2").arg(current + 1).arg(t.roundCount())
                                              : t.roundName(current).toUpper());
    m_boardBox->update();
    if (knockout) {
        static_cast<Bracket *>(m_bracket)->setTournament(t, local, m_controller);
    } else {
        QList<StandingsList::Row> rows;
        for (const Standing &s : t.standings()) {
            const Participant &p = t.participants().at(s.participant);
            rows.append({s, p.isBot() ? tr("%1 · %2").arg(p.name, toString(p.level)) : p.name,
                         p.tops.isEmpty() ? std::nullopt : std::optional<Top>(p.tops.first()),
                         s.participant == local});
        }
        static_cast<StandingsList *>(m_standings)->setRows(rows);
    }

    // Tours et résultats : matchs joués et matchs du tour courant.
    QList<ResultList::Row> rows;
    int played = 0;
    for (int r = 0; r <= current && r < t.rounds().size(); ++r) {
        const QList<TournamentMatch> &round = t.rounds().at(r);
        for (int i = 0; i < round.size(); ++i) {
            const TournamentMatch &m = round.at(i);
            if (m.isBye())
                continue;
            QString score = QStringLiteral("–");
            if (m.played) {
                score = QStringLiteral("%1 – %2").arg(m.score[0]).arg(m.score[1]);
                ++played;
            } else if (const auto *live = r == current ? m_controller->liveOf(i) : nullptr;
                       live && live->series.rounds > 0) {
                score = QStringLiteral("%1 – %2").arg(live->series.score[0]).arg(live->series.score[1]);
            }
            rows.append({shortRoundName(t, r), t.participants().at(m.a).name, score,
                         t.participants().at(m.b).name, local >= 0 && (m.a == local || m.b == local)});
        }
    }
    const int n = static_cast<int>(t.participants().size());
    static_cast<ResultList *>(m_results)->setRows(rows);
    m_resultsBox->setProperty("index", tr("%1 / %2").arg(played).arg(knockout ? n - 1 : n * (n - 1) / 2));
    m_resultsBox->update();
    // Une fois la liste redimensionnée : derniers résultats (tour courant) visibles.
    QTimer::singleShot(0, m_resultsScroll, [scroll = m_resultsScroll] {
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    });

    // Match du joueur local.
    const int match = m_controller->localMatch();
    const bool ready = m_controller->localReady();
    const bool classic = settings.matchFormat == MatchFormat::Classic;
    m_matchBox->setProperty("index", t.isFinished() ? QString() : t.roundName(current).toUpper());
    m_matchBox->update();
    m_ready->setVisible(match >= 0);
    m_ready->setEnabled(!ready);
    m_ready->setText(classic ? tr("Choisir l'ordre de mes toupies et jouer…")
                             : tr("Prêt — jouer le match"));
    MatchCard::Side left, right;
    QString message;
    if (match >= 0) {
        const TournamentMatch &m = t.rounds().at(current).at(match);
        const int opponent = m.a == local ? m.b : m.a;
        const Participant &me = t.participants().at(local), &other = t.participants().at(opponent);
        left = {me.tops.value(0), me.name, me.tops.value(0).name()};
        right = {other.tops.value(0), other.name,
                 other.isBot() ? tr("%1 · bot %2").arg(other.tops.value(0).name(), toString(other.level).toLower())
                               : other.tops.value(0).name()};
        if (ready)
            message = tr("Contre %1 — en attente de votre adversaire…").arg(participantLabel(opponent));
    } else if (t.isFinished()) {
        const int champion = t.champion();
        message = champion == local ? tr("Tournoi terminé : vous êtes champion·ne !")
                                    : tr("Tournoi terminé — champion·ne : %1").arg(participantLabel(champion));
    } else if (local < 0) {
        message = tr("Vous suivez le tournoi.");
    } else if (knockout && t.matchOf(local) < 0
               && std::none_of(t.rounds().at(current).begin(), t.rounds().at(current).end(),
                               [local](const TournamentMatch &m) { return m.a == local; })) {
        message = tr("Vous êtes éliminé·e. La suite du tournoi se joue sans vous.");
    } else {
        message = tr("Rien à jouer pour vous dans ce tour : en attente des autres matchs…");
    }
    const QString stadium = Stadium::find(settings.stadiumId).value_or(Stadium::classic()).name;
    const QString info = classic ? tr("Premier à %1 points · stadium %2").arg(series.target()).arg(stadium)
                                 : tr("Premier à %1 manches · stadium %2").arg(series.target()).arg(stadium);
    static_cast<MatchCard *>(m_matchCard)->setMatch(left, right, info, message);
    m_returnToLobby->setVisible(m_host && t.isFinished());
}

} // namespace toupie
