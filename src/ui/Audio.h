#pragma once

#include "session/MatchSession.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>

#include <array>
#include <vector>

class QAudioSink;
class QMediaPlayer;

namespace toupie {

class EffectMixer;

/// Effets sonores (fichiers « :/audio/<nom>.wav », générés par tools/make_sounds.py).
enum class Sfx {
    Click, Countdown, Go, Hit, Wall, Dash, DuelStart, DuelPress, DuelWin, DuelLose, Ejection,
    Victory, Defeat,
    Special ///< Son propre au coup spécial `SoundCue::special`.
};

/// Un son à jouer, à un volume relatif (0..1).
struct SoundCue
{
    SoundCue(Sfx sfx = Sfx::Click, double volume = 1.0, QString special = {})
        : sfx(sfx), volume(volume), special(std::move(special)) {}

    Sfx sfx;
    double volume;
    QString special; ///< Identifiant du coup spécial (Sfx::Special).
};

/// Musiques (menus, combats) et effets sonores. Chaque canal s'active et se règle
/// séparément ; les réglages sont enregistrés.
class Audio : public QObject
{
public:
    enum Channel { MenuMusic, BattleMusic, Effects };
    static constexpr int kChannels = 3;
    enum class Music { None, Menu, Battle };

    static Audio &instance();
    /// Nom du fichier d'un son (sans dossier ni extension).
    static QString fileName(const SoundCue &cue);
    /// Échantillons d'un effet (WAV PCM 16 bits mono 44,1 kHz) ; vide si absent ou autre format.
    static QByteArray loadPcm(const QString &path);

    bool isEnabled(Channel channel) const { return m_enabled[channel]; }
    int volume(Channel channel) const { return m_volume[channel]; } ///< 0..100.
    void setEnabled(Channel channel, bool enabled);
    void setVolume(Channel channel, int percent);

    /// Musique en boucle (ne la relance pas si c'est déjà elle).
    void setMusic(Music music);
    void play(const SoundCue &cue);

protected:
    /// Installé sur l'application : son de clic des boutons.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit Audio(QObject *parent);
    void save(Channel channel) const;
    void applyMusic();
    float gain(Channel channel) const;

    std::array<bool, kChannels> m_enabled{};
    std::array<int, kChannels> m_volume{};
    Music m_music = Music::None;
    std::array<QMediaPlayer *, 2> m_players{}; ///< Menus, combats (créés au besoin).
    QAudioSink *m_effectsSink = nullptr;       ///< Flux des effets (créé au premier effet).
    EffectMixer *m_mixer = nullptr;
    QHash<QString, QByteArray> m_samples;      ///< Échantillons des effets déjà chargés.
    QHash<QString, QElapsedTimer> m_lastPlayed;
};

/// Déduit les sons d'un combat de l'évolution de son état, image par image
/// (combat local ou reçu du réseau).
class BattleSounds
{
public:
    void reset();
    QList<SoundCue> update(const MatchState &state, const QList<ImpactEvent> &impacts, int local);

private:
    struct TopState
    {
        bool dashing = false;
        bool special = false;
        Spinner::Status status = Spinner::Status::Waiting;
    };
    std::vector<TopState> m_tops; ///< Vide : prochain état = point de départ, sans son.
    MatchPhase m_phase = MatchPhase::Aiming;
    int m_count = 0;              ///< Chiffre affiché du compte à rebours (0 : aucun).
    bool m_duel = false;
    int m_duelMarks = 0;          ///< Appuis réussis du joueur local dans le duel.
    double m_duelZone = 0.0;
};

} // namespace toupie
