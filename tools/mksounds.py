"""mksounds.py - the sound pack that comes with the program.

Makes OUT_DIR/*.wav (default _scratch/synth): fuller versions of the board's noise sounds
(shots, explosions), its engine and the missile's buzz, to play in place of
them when `sound_replace` is on (docs/SOUNDS.md). All of it is computed
here: filtered noise, a few sine waves, nothing recorded. The POKEY's
tunes are left as the board plays them.

No longer what comes with the program: package/sounds has the shots and
explosions made by tools/mkfreesound.py from recordings (0.1.9). This one
stays for the engine and the missile, which nobody has heard yet.

    python tools/mksounds.py [OUT_DIR]
"""
import math
import os
import random
import struct
import sys
import wave

RATE = 22050


def write(path, samples, peak=0.85):
    top = max(1e-9, max(abs(v) for v in samples))
    data = b"".join(struct.pack("<h", int(v / top * peak * 32767)) for v in samples)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)


def lowpass(samples, cutoff):
    """One pole; cutoff a number or a function of the time in seconds."""
    out, y = [], 0.0
    for i, v in enumerate(samples):
        fc = cutoff(i / RATE) if callable(cutoff) else cutoff
        k = 1.0 - math.exp(-2.0 * math.pi * fc / RATE)
        y += (v - y) * k
        out.append(y)
    return out


def noise(n, seed):
    rnd = random.Random(seed)
    return [rnd.uniform(-1.0, 1.0) for _ in range(n)]


def shot(seconds, thump_hz, crack, seed):
    """A gun: a thump that falls in pitch under a short crack of noise."""
    n = int(seconds * RATE)
    hiss = lowpass(noise(n, seed), lambda t: 5000.0 * math.exp(-t * 14.0) + 300.0)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        phase += 2.0 * math.pi * thump_hz * (0.45 + 0.55 * math.exp(-t * 9.0)) / RATE
        out.append(math.sin(phase) * math.exp(-t * 7.0) + crack * hiss[i] * math.exp(-t * 16.0))
    return out


def explosion(seconds, rumble_hz, seed):
    """Noise whose brightness and loudness die away over a low rumble."""
    n = int(seconds * RATE)
    body = lowpass(noise(n, seed), lambda t: 3200.0 * math.exp(-t * 3.5) + 110.0)
    body = lowpass(body, 4000.0)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        phase += 2.0 * math.pi * rumble_hz * (0.6 + 0.4 * math.exp(-t * 2.0)) / RATE
        attack = min(1.0, t / 0.004)
        out.append(attack * (2.2 * body[i] * math.exp(-t * 3.2 / seconds * 1.6) +
                             0.7 * math.sin(phase) * math.exp(-t * 2.4 / seconds * 1.6)))
    tail = int(0.05 * RATE)
    for i in range(tail):
        out[n - 1 - i] *= i / tail
    return out


def engine(hz, seed):
    """One second that repeats without a seam: the firing strokes of a
    diesel as a row of harmonics, and a little rattle."""
    n = RATE
    rnd = random.Random(seed)
    parts = [(k, 1.0 / k ** 1.15 * rnd.uniform(0.6, 1.0), rnd.uniform(0.0, 6.283)) for k in range(1, 18)]
    rattle = noise(n, seed + 1)
    rattle = lowpass(rattle + rattle, 900.0)[n:]        # (run in, so the loop's start is its end)
    out = []
    for i in range(n):
        t = i / RATE
        v = sum(a * math.sin(2.0 * math.pi * hz * k * t + p) for k, a, p in parts)
        stroke = 0.6 + 0.4 * math.sin(2.0 * math.pi * hz * t) ** 2
        out.append(v * stroke + 0.25 * rattle[i] * stroke)
    return out


def buzz(hz, wobble_hz):
    """Half a second that repeats: the missile's saw-toothed whine."""
    n = RATE // 2
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        f = hz * (1.0 + 0.04 * math.sin(2.0 * math.pi * wobble_hz * t))
        phase += f / RATE
        saw = 2.0 * (phase % 1.0) - 1.0
        out.append(0.7 * saw + 0.3 * math.sin(2.0 * math.pi * 2.0 * hz * t))
    return lowpass(out + out, 2600.0)[n:]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join("_scratch", "synth")
    os.makedirs(out, exist_ok=True)
    sounds = {
        "SHELL_LOUD": (shot(0.55, 95.0, 1.3, 1), 0.9),
        "SHELL_SOFT": (shot(0.40, 130.0, 0.9, 2), 0.55),
        "EXPLO_LOUD": (explosion(1.8, 48.0, 3), 0.95),
        "EXPLO_SOFT": (explosion(1.0, 70.0, 4), 0.7),
        "ENGINE_IDLE": (engine(28.0, 5), 0.22),
        "ENGINE_REV": (engine(44.0, 6), 0.3),
        "MISSILE": (buzz(120.0, 8.0), 0.35),
    }
    for name, (samples, peak) in sounds.items():
        path = os.path.join(out, name + ".wav")
        write(path, samples, peak)
        print("%-12s %.2f s  %s" % (name, len(samples) / RATE, path))


if __name__ == "__main__":
    main()
