#!/usr/bin/env python3
"""Effets sonores de Top Arena, synthétisés (WAV mono 16 bits, 44,1 kHz) dans src/audio/.

Bibliothèque standard uniquement : sons originaux et sans licence. Après avoir modifié
un réglage :   python3 tools/make_sounds.py
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 44100
OUT = Path(__file__).resolve().parent.parent / "src" / "audio"
rng = random.Random(1234)  # Graine fixe : les fichiers ne changent que si le code change.

# Notes (Hz)
E4, F4, FS4, G4 = 329.63, 349.23, 369.99, 392.00
C5, E5, G5 = 523.25, 659.26, 783.99
C6, E6, G6 = 1046.50, 1318.51, 1567.98


def frames(seconds):
    return int(seconds * RATE)


def const(value):
    return value if callable(value) else (lambda t: value)


# --- Sources ---------------------------------------------------------------------------

def tone(freq, seconds, shape="sine", vibrato=(0.0, 0.0)):
    """Oscillateur à bande limitée (somme d'harmoniques) ; freq : Hz ou fonction de t."""
    f = const(freq)
    rate, depth = vibrato
    out, phase = [], 0.0
    for i in range(frames(seconds)):
        t = i / RATE
        phase += 2 * math.pi * f(t) * (1 + depth * math.sin(2 * math.pi * rate * t)) / RATE
        if shape == "sine":
            s = math.sin(phase)
        elif shape == "square":
            s = 0.9 * sum(math.sin(k * phase) / k for k in (1, 3, 5, 7))
        elif shape == "saw":
            s = 0.6 * sum(math.sin(k * phase) / k for k in range(1, 9))
        else:  # triangle
            s = 0.8 * sum((-1) ** ((k - 1) // 2) * math.sin(k * phase) / (k * k) for k in (1, 3, 5, 7))
        out.append(s)
    return out


def noise(seconds):
    return [rng.uniform(-1.0, 1.0) for _ in range(frames(seconds))]


def fm(seconds, carrier, ratio, index):
    """Modulation de fréquence (timbre « distordu ») ; carrier et index : fonctions de t."""
    out, pc, pm = [], 0.0, 0.0
    for i in range(frames(seconds)):
        t = i / RATE
        fc = carrier(t)
        pc += 2 * math.pi * fc / RATE
        pm += 2 * math.pi * fc * ratio / RATE
        out.append(math.sin(pc + index(t) * math.sin(pm)))
    return out


# --- Traitements -----------------------------------------------------------------------

def env(sig, attack, decay):
    """Attaque linéaire puis décroissance exponentielle (constante de temps `decay`)."""
    out = []
    for i, s in enumerate(sig):
        t = i / RATE
        out.append(s * (t / attack if t < attack else math.exp(-(t - attack) / decay)))
    return out


def shape(sig, curve):
    """Multiplie par une courbe d'amplitude fonction de t."""
    return [s * curve(i / RATE) for i, s in enumerate(sig)]


def lowpass(sig, cutoff, poles=1):
    """Filtre passe-bas à un pôle (répété `poles` fois) ; cutoff : Hz ou fonction de t."""
    fc = const(cutoff)
    for _ in range(poles):
        out, y = [], 0.0
        for i, x in enumerate(sig):
            y += (1 - math.exp(-2 * math.pi * fc(i / RATE) / RATE)) * (x - y)
            out.append(y)
        sig = out
    return sig


def highpass(sig, cutoff):
    return [x - y for x, y in zip(sig, lowpass(sig, cutoff))]


def mix(*parts):
    """Somme de (signal, gain, décalage en s) ou de signaux seuls."""
    parts = [p if isinstance(p, tuple) else (p, 1.0, 0.0) for p in parts]
    parts = [(p + (0.0,))[:3] for p in parts]
    length = max(frames(off) + len(sig) for sig, _, off in parts)
    out = [0.0] * length
    for sig, gain, off in parts:
        start = frames(off)
        for i, s in enumerate(sig):
            out[start + i] += gain * s
    return out


def concat(*sigs):
    return [s for sig in sigs for s in sig]


def echo(sig, delay, feedback, repeats):
    return mix(*[(sig, feedback ** k, delay * k) for k in range(repeats + 1)])


def write(name, sig, peak):
    """Normalise au niveau crête `peak` (relatif entre les sons), adoucit la fin, écrit."""
    fade = frames(0.005)
    sig = [s * min(1.0, (len(sig) - i) / fade) for i, s in enumerate(sig)]
    top = max(abs(s) for s in sig)
    assert top > 0 and 0 < peak <= 1, name  # Pas de son muet ni saturé.
    data = struct.pack(f"<{len(sig)}h", *(round(s / top * peak * 32767) for s in sig))
    with wave.open(str(OUT / f"{name}.wav"), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(data)
    print(f"{name}.wav  {len(sig) / RATE:.2f} s")


# --- Briques ---------------------------------------------------------------------------

def clang(partials, seconds, transient=1.0):
    """Choc métallique : partiels inharmoniques (Hz, gain, décroissance) + bruit d'attaque."""
    parts = [(env(tone(f, seconds), 0.0008, d), a) for f, a, d in partials]
    parts.append((env(highpass(noise(0.02), 1500), 0.0005, 0.010), transient))
    return mix(*parts)


def thud(seconds=0.22, low=60, high=160):
    body = env(tone(lambda t: low + (high - low) * math.exp(-t * 18), seconds), 0.002, 0.06)
    return mix(body, (env(lowpass(noise(seconds), 600, 2), 0.001, 0.03), 0.8))


def notes(seq, shape_name="square", decay=0.08, gap=0.0):
    """Suite de notes (fréquence, durée) enchaînées."""
    out = []
    for f, d in seq:
        out += env(tone(f, d, shape_name), 0.004, decay) + [0.0] * frames(gap)
    return out


def brass(f, seconds, vibrato=(0.0, 0.0)):
    return env(lowpass(tone(f, seconds, "saw", vibrato), 3500), 0.02, seconds * 0.8)


# --- Sons du jeu -----------------------------------------------------------------------

def build():
    # Interface et combat
    write("click", mix(env(tone(lambda t: 2200 - 18000 * t, 0.03), 0.001, 0.008),
                       (env(highpass(noise(0.01), 3000), 0.0005, 0.002), 0.5)), 0.55)
    write("countdown", mix(env(tone(880, 0.18, "square"), 0.003, 0.06),
                           (env(tone(1760, 0.18), 0.003, 0.05), 0.3)), 0.75)
    write("go", mix(env(tone(lambda t: 880 * 2 ** min(t / 0.12, 1.0), 0.5, "square"), 0.005, 0.18),
                    (env(tone(3520, 0.5), 0.005, 0.12), 0.25),
                    (env(highpass(noise(0.1), 2000), 0.002, 0.03), 0.4)), 0.9)
    write("hit", clang([(620, 1.0, 0.09), (1340, 0.7, 0.07), (2110, 0.5, 0.05),
                        (2970, 0.35, 0.04), (4180, 0.25, 0.03)], 0.35, 1.2), 0.98)
    write("wall", thud(), 0.9)
    write("dash", shape(lowpass(noise(0.32), lambda t: 400 + 4600 * math.sin(math.pi * t / 0.32) ** 2, 2),
                        lambda t: math.sin(math.pi * t / 0.32) ** 1.5), 0.8)
    write("duel_start", mix(clang([(440, 1.0, 0.14), (980, 0.7, 0.1), (1530, 0.5, 0.08), (2210, 0.35, 0.06)],
                                  0.5),
                            (env(lowpass(tone(110, 0.5, "saw", (18, 0.03)), 1500), 0.01, 0.25), 0.7)), 0.98)
    write("duel_press", mix(env(tone(1320, 0.07), 0.001, 0.02), (env(tone(2640, 0.07), 0.001, 0.015), 0.3)),
          0.65)
    write("duel_win", notes([(C6, 0.07), (E6, 0.07), (G6, 0.2)], decay=0.07), 0.8)
    write("duel_lose", concat(env(tone(G4, 0.12, "saw"), 0.004, 0.08),
                              env(tone(lambda t: 277 - 40 * t, 0.3, "saw"), 0.004, 0.15)), 0.8)
    write("ejection", mix(env(tone(lambda t: 1400 * math.exp(-t * 3.2) + 120, 0.75, "triangle"), 0.01, 0.4),
                          (thud(0.25, 45, 120), 1.0, 0.62)), 0.92)
    fanfare = concat(brass(G5, 0.12), brass(C6, 0.12), brass(E6, 0.12),
                     mix(brass(G6, 0.75, (5.5, 0.008)), (brass(C5, 0.75), 0.5), (brass(E5, 0.75), 0.4),
                         (brass(G5, 0.75), 0.4)))
    write("victory", echo(fanfare, 0.11, 0.3, 2), 0.92)
    lament = concat(*[env(tone(f, 0.26, "triangle"), 0.01, 0.2) for f in (G4, FS4, F4)],
                    env(tone(E4, 0.9, "triangle", (5.0, 0.01)), 0.02, 0.5))
    write("defeat", lowpass(lament, 2500), 0.8)

    # Coups spéciaux : un son par coup (identifiant du SpecialMove)
    write("special_onde", echo(mix(env(tone(lambda t: 40 + 80 * math.exp(-t * 8), 0.9), 0.003, 0.35),
                                   (env(lowpass(noise(0.9), lambda t: 150 + 2500 * math.exp(-t * 6), 2),
                                        0.002, 0.25), 0.9),
                                   (env(tone(220, 0.6), 0.02, 0.2), 0.25)), 0.09, 0.35, 3), 0.98)
    write("special_lance", mix(env(highpass(noise(0.45), 2500), 0.005, 0.12),
                               (env(tone(lambda t: 800 + 1600 * min(t / 0.15, 1.0), 0.3, "saw"), 0.003, 0.08), 0.4),
                               (clang([(2400, 1.0, 0.35), (3600, 0.7, 0.25), (5100, 0.5, 0.2), (6800, 0.3, 0.15)],
                                      0.6, 0.3), 0.8, 0.04)), 0.95)
    crystal = [(shape(env(tone(f, 0.75), 0.005, 0.25), lambda t: 0.75 + 0.25 * math.sin(2 * math.pi * 11 * t)),
                0.5, 0.05 * k) for k, f in enumerate((2637, 3136, 3951, 5274))]
    write("special_givre", mix((shape(highpass(noise(0.9), 4000), lambda t: min(t / 0.15, 1.0) * math.exp(-t / 0.4)),
                                0.6), *crystal), 0.9)
    write("special_vortex", mix(env(lowpass(tone(lambda t: 500 - 330 * t, 0.9, "saw", (7, 0.15)), 1800), 0.05, 0.5),
                                (shape(lowpass(noise(0.9), 1200, 2),
                                       lambda t: (0.5 + 0.5 * math.sin(2 * math.pi * 7 * t)) * math.exp(-t / 0.6)),
                                 0.8)), 0.95)
    write("special_distorsion", env(fm(0.8, lambda t: 300 + 200 * math.sin(2 * math.pi * 1.5 * t), 0.5,
                                       lambda t: 6 * t / 0.8), 0.02, 0.4), 0.9)
    write("special_siphon", mix(shape(lowpass(noise(0.7), lambda t: 300 + 4000 * (t / 0.7) ** 2), lambda t: (t / 0.7) ** 1.5),
                                (shape(tone(lambda t: 200 * 4.5 ** (t / 0.7), 0.7), lambda t: 0.6 * (t / 0.7) ** 2), 1.0),
                                (env(tone(1200, 0.05), 0.001, 0.01), 0.8, 0.7)), 0.9)
    rumble = shape(lowpass(noise(1.2), 110, 3),
                   lambda t: (0.6 + 0.4 * math.sin(2 * math.pi * 9 * t) * math.sin(2 * math.pi * 13 * t))
                   * math.exp(-t / 0.7))
    write("special_seisme", mix((rumble, 8.0), (env(tone(45, 1.2), 0.01, 0.5), 0.6),
                                (thud(0.3, 40, 110), 1.0), (thud(0.3, 40, 100), 0.7, 0.35)), 0.98)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    build()
