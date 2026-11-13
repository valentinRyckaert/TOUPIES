#include "ui/Audio.h"

#include <QAbstractButton>
#include <QApplication>
#include <QAudio>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QAudioSink>
#include <QEvent>
#include <QFile>
#include <QMediaPlayer>
#include <QMutex>
#include <QSettings>
#include <QUrl>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <vector>

namespace toupie {

/// Mélange les effets en cours (mono) en un seul flux stéréo continu. QSoundEffect est
/// évité : ses flux sont des « sons système », que le bureau peut couper (KDE par exemple).
class EffectMixer : public QIODevice
{
public:
    explicit EffectMixer(QObject *parent)
        : QIODevice(parent)
    {
        open(QIODevice::ReadOnly);
    }

    void start(const QByteArray &pcm, float gain)
    {
        const QMutexLocker lock(&m_mutex);
        if (m_voices.size() < 16)
            m_voices.push_back({pcm, 0, gain});
    }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 4096 + QIODevice::bytesAvailable(); }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 count = maxSize / 4; // Trames stéréo 16 bits.
        std::vector<float> mix(static_cast<std::size_t>(count), 0.0f);
        {
            const QMutexLocker lock(&m_mutex); // La sortie audio peut lire depuis un autre fil.
            for (Voice &voice : m_voices) {
                const auto *samples = reinterpret_cast<const qint16 *>(voice.pcm.constData());
                const qint64 n = std::min(count, voice.pcm.size() / 2 - voice.position);
                for (qint64 i = 0; i < n; ++i)
                    mix[i] += samples[voice.position + i] * voice.gain;
                voice.position += n;
            }
            m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(),
                                          [](const Voice &v) { return v.position * 2 >= v.pcm.size(); }),
                           m_voices.end());
        }
        auto *out = reinterpret_cast<qint16 *>(data);
        for (qint64 i = 0; i < count; ++i) // Silence entre les effets : le flux ne s'arrête pas.
            out[2 * i] = out[2 * i + 1] = static_cast<qint16>(std::clamp(mix[i], -32768.0f, 32767.0f));
        return count * 4;
    }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    struct Voice
    {
        QByteArray pcm;
        qint64 position; // En échantillons.
        float gain;
    };
    QMutex m_mutex;
    std::vector<Voice> m_voices;
};

namespace {
constexpr int kEffectRate = 44100;

QString settingsKey(Audio::Channel channel, const char *what)
{
    static const char *const names[] = {"menuMusic", "battleMusic", "effects"};
    return QStringLiteral("audio/%1/%2").arg(QLatin1String(names[channel]), QLatin1String(what));
}
} // namespace

Audio &Audio::instance()
{
    static Audio *audio = new Audio(qApp); // Détruit avec l'application.
    return *audio;
}

Audio::Audio(QObject *parent)
    : QObject(parent)
{
    const QSettings settings;
    for (int c = 0; c < kChannels; ++c) {
        const auto channel = static_cast<Channel>(c);
        m_enabled[c] = settings.value(settingsKey(channel, "enabled"), true).toBool();
        const int fallback = channel == Effects ? 100 : 70; // Effets brefs : à fond par défaut.
        m_volume[c] = std::clamp(settings.value(settingsKey(channel, "volume"), fallback).toInt(), 0, 100);
    }
}

QByteArray Audio::loadPcm(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray wav = file.readAll();
    bool supported = false;
    // RIFF : en-tête de 12 octets, puis des blocs « identifiant (4), taille (4), données ».
    for (qint64 at = 12; at + 8 <= wav.size();) {
        const char *chunk = wav.constData() + at;
        const quint32 size = qFromLittleEndian<quint32>(chunk + 4);
        if (std::equal(chunk, chunk + 4, "fmt ") && size >= 16 && at + 8 + 16 <= wav.size()) {
            const char *f = chunk + 8;
            supported = qFromLittleEndian<quint16>(f) == 1 && qFromLittleEndian<quint16>(f + 2) == 1
                        && qFromLittleEndian<quint32>(f + 4) == kEffectRate
                        && qFromLittleEndian<quint16>(f + 14) == 16;
        } else if (std::equal(chunk, chunk + 4, "data")) {
            return supported ? wav.mid(at + 8, size) : QByteArray();
        }
        at += 8 + qint64(size) + (size & 1);
    }
    return {};
}

QString Audio::fileName(const SoundCue &cue)
{
    static const char *const names[] = {"click", "countdown", "go", "hit", "wall", "dash",
                                        "duel_start", "duel_press", "duel_win", "duel_lose",
                                        "ejection", "victory", "defeat"};
    if (cue.sfx == Sfx::Special)
        return QStringLiteral("special_") + cue.special;
    return QLatin1String(names[static_cast<int>(cue.sfx)]);
}

void Audio::setEnabled(Channel channel, bool enabled)
{
    m_enabled[channel] = enabled;
    save(channel);
    applyMusic();
}

void Audio::setVolume(Channel channel, int percent)
{
    m_volume[channel] = std::clamp(percent, 0, 100);
    save(channel);
    applyMusic();
}

void Audio::save(Channel channel) const
{
    QSettings settings;
    settings.setValue(settingsKey(channel, "enabled"), m_enabled[channel]);
    settings.setValue(settingsKey(channel, "volume"), m_volume[channel]);
}

float Audio::gain(Channel channel) const
{
    // Curseur perçu comme linéaire par l'oreille.
    return static_cast<float>(QAudio::convertVolume(m_volume[channel] / 100.0,
                                                    QAudio::LogarithmicVolumeScale,
                                                    QAudio::LinearVolumeScale));
}

void Audio::setMusic(Music music)
{
    if (music == m_music)
        return;
    m_music = music;
    applyMusic();
}

void Audio::applyMusic()
{
    for (int i = 0; i < 2; ++i) {
        const Channel channel = i == 0 ? MenuMusic : BattleMusic;
        const bool wanted = m_music == (i == 0 ? Music::Menu : Music::Battle) && m_enabled[channel]
                            && m_volume[channel] > 0;
        QMediaPlayer *&player = m_players[i];
        if (!wanted) {
            if (player)
                player->stop();
            continue;
        }
        if (!player) {
            player = new QMediaPlayer(this);
            player->setAudioOutput(new QAudioOutput(player));
            player->setSource(QUrl(i == 0 ? QStringLiteral("qrc:/audio/menu.wav")
                                          : QStringLiteral("qrc:/audio/battle.wav")));
            player->setLoops(QMediaPlayer::Infinite);
        }
        player->audioOutput()->setVolume(gain(channel));
        if (player->playbackState() != QMediaPlayer::PlayingState)
            player->play(); // Reprend au début après un stop().
    }
}

void Audio::play(const SoundCue &cue)
{
    if (!m_enabled[Effects] || m_volume[Effects] == 0)
        return;
    const QString name = fileName(cue);
    QElapsedTimer &last = m_lastPlayed[name];
    if (last.isValid() && last.elapsed() < 50)
        return; // Rafale (chocs répétés contre le rebord…) : un seul son.
    last.start();
    QByteArray &pcm = m_samples[name];
    if (pcm.isEmpty())
        pcm = loadPcm(QStringLiteral(":/audio/%1.wav").arg(name));
    if (pcm.isEmpty())
        return;
    if (!m_effectsSink) {
        QAudioFormat format;
        format.setSampleRate(kEffectRate);
        format.setChannelCount(2); // Mono étalé par PipeWire = −3 dB : on double la voie.
        format.setSampleFormat(QAudioFormat::Int16);
        // Sortie créée avant le mélangeur : détruite avant lui (enfants détruits dans l'ordre).
        m_effectsSink = new QAudioSink(format, this);
        m_effectsSink->setBufferSize(kEffectRate * 4 * 60 / 1000); // ≈ 60 ms : latence courte.
        m_mixer = new EffectMixer(this);
        m_effectsSink->start(m_mixer);
    }
    m_mixer->start(pcm, gain(Effects) * static_cast<float>(std::clamp(cue.volume, 0.0, 1.0)));
}

bool Audio::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *button = qobject_cast<QAbstractButton *>(watched);
        if (button && button->isEnabled())
            play({Sfx::Click});
    }
    return false;
}

// ---------------------------------------------------------------------------

void BattleSounds::reset()
{
    m_tops.clear();
}

QList<SoundCue> BattleSounds::update(const MatchState &state, const QList<ImpactEvent> &impacts,
                                     int local)
{
    const int count = state.phase == MatchPhase::Countdown
                          ? std::max(1, static_cast<int>(std::ceil(state.countdown)))
                          : 0;
    const int duelSide = state.duel.sideOf(local);
    QList<SoundCue> cues;
    if (m_tops.size() == state.spinners.size()) {
        if (count > 0 && count != m_count)
            cues.append(Sfx::Countdown);
        if (state.phase != m_phase && state.phase == MatchPhase::Fighting)
            cues.append(Sfx::Go);
        if (state.phase != m_phase && state.phase == MatchPhase::Finished)
            cues.append(state.winner == local ? Sfx::Victory : Sfx::Defeat);

        for (const ImpactEvent &impact : impacts) {
            if (impact.duelLoser != ImpactEvent::kNoDuel) {
                const bool involved = impact.first == local || impact.second == local;
                cues.append(!involved || impact.duelLoser < 0 ? Sfx::Hit
                            : impact.duelLoser == local       ? Sfx::DuelLose
                                                              : Sfx::DuelWin);
            } else if (impact.first < 0) {
                if (impact.intensity > 0.05)
                    cues.append({Sfx::Wall, impact.intensity});
            } else {
                cues.append({Sfx::Hit, impact.special ? 1.0 : 0.35 + 0.65 * impact.intensity});
            }
        }

        for (std::size_t i = 0; i < m_tops.size(); ++i) {
            const Spinner &s = state.spinners[i];
            if (s.isDashing() && !m_tops[i].dashing)
                cues.append({Sfx::Dash, static_cast<int>(i) == local ? 1.0 : 0.7});
            if (s.specialUsed && !m_tops[i].special)
                cues.append({Sfx::Special, 1.0, s.top.special()});
            if (s.status == Spinner::Status::RingOut && m_tops[i].status != Spinner::Status::RingOut)
                cues.append(Sfx::Ejection);
        }

        if (state.duel.active && !m_duel) {
            cues.append(Sfx::DuelStart);
        } else if (duelSide >= 0 && m_duel) {
            const DuelSide &side = state.duel.sides[duelSide];
            if (side.presses + side.streak > m_duelMarks || side.zone < m_duelZone - 1e-9)
                cues.append(Sfx::DuelPress);
        }
    }

    m_tops.resize(state.spinners.size());
    for (std::size_t i = 0; i < m_tops.size(); ++i) {
        const Spinner &s = state.spinners[i];
        m_tops[i] = {s.isDashing(), s.specialUsed, s.status};
    }
    m_phase = state.phase;
    m_count = count;
    m_duel = state.duel.active;
    if (duelSide >= 0) {
        const DuelSide &side = state.duel.sides[duelSide];
        m_duelMarks = side.presses + side.streak;
        m_duelZone = side.zone;
    }
    return cues;
}

} // namespace toupie
