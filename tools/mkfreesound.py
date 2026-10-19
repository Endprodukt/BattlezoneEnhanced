"""mkfreesound.py - the futuristic shot and explosions from Freesound (CC0).

    python tools/mkfreesound.py DIR [OUT] [--bzoneii DIR2]

DIR holds the downloaded files (assets/freesound; their Freesound names, e.g.
702000__xkeril__heavy-blaster-shot-05.wav); OUT is package/sounds by
default. Writes SHELL_LOUD.wav (the player's shot), SHELL_SOFT.wav (the
enemy's: the same, duller and softer), EXPLO_SOFT.wav (the enemy
destroyed) and EXPLO_LOUD.wav (the player's death), each layered
from several recordings: a bright attack that cuts through on the
cabinet's speaker and a low body under it. Mono, 16 bits, 44.1 kHz.

All the recordings are CC0 (no attribution needed); docs/SOUNDS.md lists
them. Needs numpy and soundfile (which also reads the one MP3).

The engines, from recordings of real tanks: ENGINE_IDLE.wav (standing,
the engine rumbling), ENGINE_REV.wav (driving: the engine up and the
tracks rattling; the game blends the two by how far the engine has
revved) and ENEMY_ENGINE.wav (the enemy tanks', another engine, heard at
their place). Each a loop: every layer cut to the same length and its
join faded, so the sum joins too.

Running into an obstacle: BOING.wav (the bump: a heavy metal crash) and
BLKSND.wav (stuck against it). The game starts BLKSND again every time the
board's own tune would end, 34 interrupts (0.138 s) while the tank pushes;
and cuts BOING after the board's 0.47 s when it does. So BOING has what
counts in its first half second, and BLKSND is 0.138 s of grinding with a
small clank at its start: over and over, a rattle of tracks slipping.

--bzoneii: the shot of Battle Zone II as it is on the owner's cabinet
(assets/bzoneii, copied from /persistent/games/bzoneii/sounds,
player_fire.wav) in place of the layered one, the enemy's made from it.
Not CC0: for the owner's own cabinet, not to be passed on.
"""
import os
import sys

import numpy as np
import soundfile as sf

RATE = 44100


def load(d, sound_id):
    """The recording whose name starts with the Freesound number (or is that name), as mono at RATE."""
    name = next(f for f in os.listdir(d) if f.startswith(f"{sound_id}__") or f == sound_id)
    x, sr = sf.read(os.path.join(d, name), always_2d=True)
    m = x.mean(axis=1)
    if sr != RATE:
        t = np.arange(int(len(m) * RATE / sr)) * sr / RATE
        m = np.interp(t, np.arange(len(m)), m)
    return m / (np.abs(m).max() + 1e-12)


def highpass(x, hz):
    """One pole: takes away the rumble no cabinet speaker plays and that only eats headroom."""
    a = 1.0 / (1.0 + 2 * np.pi * hz / RATE)
    y = np.zeros_like(x)
    prev_x = prev_y = 0.0
    for i, v in enumerate(x):
        prev_y = a * (prev_y + v - prev_x)
        prev_x = v
        y[i] = prev_y
    return y


def lowpass(x, hz):
    """Two poles: further away, duller."""
    a = np.exp(-2 * np.pi * hz / RATE)
    for _ in range(2):
        y = np.zeros_like(x)
        prev = 0.0
        for i, v in enumerate(x):
            prev = (1 - a) * v + a * prev
            y[i] = prev
        x = y
    return x


def seamless(x, seconds=0.03, power=False):
    """A loop whose end runs into its start: the last moments faded over the first.
    (player_engine_loop_low.wav jumps by a quarter of full scale where it joins.)
    power: for noise, which two parts do not cancel or double: as loud through the fade."""
    n = int(seconds * RATE)
    k = np.linspace(0.0, 1.0, n)
    a, b = (np.sin(k * np.pi / 2), np.cos(k * np.pi / 2)) if power else (k, 1.0 - k)
    y = x[:-n].copy()
    y[:n] = x[:n] * a + x[-n:] * b
    return y


def loop_of(x, start_s, seconds, fade_s=0.25):
    """seconds of x from start_s on as a loop (repeated first if x is shorter)."""
    need = int((seconds + fade_s) * RATE)
    x = x[int(start_s * RATE):]
    while len(x) < need:
        x = np.concatenate([x, x])
    return seamless(highpass(x[:need], 40.0), fade_s, power=True)


def steady(x, seconds=0.2, amount=0.8):
    """An engine runs evenly: swells and dips of the loop mostly taken out.
    (The envelope is taken round the loop, so its join stays smooth.)"""
    w = int(seconds * RATE)
    p = np.concatenate([x[-w:], x, x[:w]]) ** 2
    c = np.cumsum(np.concatenate([[0.0], p]))
    env = np.sqrt((c[2 * w:] - c[:-2 * w])[:len(x)] / (2 * w)) + 1e-9
    return x * (env.mean() / env) ** amount


def at_rms(x, db):
    """x brought to that average level (an engine is steady: its average says how loud it is)."""
    return x * (10 ** (db / 20.0) / (np.sqrt((x * x).mean()) + 1e-12))


def engines(d, out):
    """The player's two engine loops and the enemy's, each 8 s."""
    idle_hum = load(d, 182793)      # military vehicle humming: a tank standing, engine running
    mech = load(d, 186934)          # engine4_loop: a tank's engine and its machinery
    t26 = load(d, 329800)           # T-26 moving: a real tank, engine and tracks
    clank = load(d, 849062)         # tracks moving: the chain's rattle
    tracked = load(d, 121531)       # tracked vehicle moving
    big = load(d, 572294)           # tank engine: long, steadiest from 20 s to 36 s
    sand = load(d, 187676)          # a tank driving in the sand
    L = 8.0
    idle = at_rms(loop_of(idle_hum, 4.0, L), -22) + at_rms(loop_of(mech, 0.0, L), -24)
    write(os.path.join(out, "ENGINE_IDLE.wav"), at_rms(steady(idle), -22))     # (3 dB under 0.1.10: "etwas leiser")
    rev = (at_rms(loop_of(t26, 2.0, L), -20) + at_rms(loop_of(mech, 3.0, L), -25) +
           at_rms(loop_of(clank, 0.0, L), -26) + at_rms(loop_of(tracked, 0.5, L), -27))
    write(os.path.join(out, "ENGINE_REV.wav"), at_rms(steady(rev), -19))
    enemy = (at_rms(loop_of(big, 20.0, L), -20) + at_rms(loop_of(sand, 0.0, L), -23) +
             at_rms(loop_of(tracked, 1.0, L), -27))
    write(os.path.join(out, "ENEMY_ENGINE.wav"), at_rms(steady(enemy), -19))


def obstacles(d, out):
    """The bump into an obstacle and the grinding against it."""
    clank = load(d, 151624)         # clank car crash collision: the low crash
    crash = load(d, 67617)          # metal crash collision: the bright one
    step = load(d, 837389)          # heavy robot footsteps: one of them, the weight
    grind = load(d, 336001)         # metal grinding very slow
    scrape = load(d, 697758)        # metal resonant scrape impact: the low resonance
    bump = layer(0.8, [(clank, 0.0, 0.0, 1.0), (crash, 0.03, 0.0, 0.8), (step, 0.33, 0.0, 0.9)])
    write(os.path.join(out, "BOING.wav"), finish(bump, 1.8, 0.35))
    stuck_s = 34 / (1512000 / 6144)   # (BLKSND's time on the board: its restarts come at this pace)
    rub = layer(stuck_s, [(highpass(grind, 300.0), 6.0, 0.0, 0.7), (scrape, 0.6, 0.0, 0.6), (crash, 0.03, 0.0, 0.35)])
    n = int(0.006 * RATE)
    rub[:n] *= np.linspace(0.0, 1.0, n)
    rub[-n:] *= np.linspace(1.0, 0.0, n)
    write(os.path.join(out, "BLKSND.wav"), rub / (np.abs(rub).max() + 1e-12) * 0.5)


def layer(seconds, parts):
    """parts: (samples, from_s, at_s, gain): the recording from from_s on, placed at at_s."""
    out = np.zeros(int(seconds * RATE))
    for x, start, at, gain in parts:
        x = x[int(start * RATE):]
        i = int(at * RATE)
        n = min(len(x), len(out) - i)
        out[i:i + n] += x[:n] * gain
    return out


def finish(x, drive, fade_s, peak=0.89):
    """Soft saturation for loudness (drive), a fade at the end, the peak at about -1 dB."""
    x = highpass(x, 35.0)
    x = np.tanh(x * drive) / np.tanh(drive)
    n = int(fade_s * RATE)
    x[-n:] *= np.linspace(1.0, 0.0, n) ** 2
    x[:32] *= np.linspace(0.0, 1.0, 32)          # (no click at the start)
    return x / (np.abs(x).max() + 1e-12) * peak


def write(path, x):
    sf.write(path, (x * 32767).astype(np.int16), RATE, subtype="PCM_16")
    w = int(RATE * 0.05)
    env = np.sqrt(np.convolve(x * x, np.ones(w) / w, "same"))
    print(f"{os.path.basename(path):16s} {len(x) / RATE:4.2f} s, loudest 50 ms {20 * np.log10(env.max()):5.1f} dBFS")


def main():
    args = sys.argv[1:]
    bz2 = None
    if "--bzoneii" in args:
        i = args.index("--bzoneii")
        bz2 = args[i + 1]
        del args[i:i + 2]
    d = args[0]
    out = args[1] if len(args) > 1 else os.path.join(os.path.dirname(__file__), "..", "package", "sounds")
    blaster = load(d, 702000)       # heavy blaster shot: the bright crack
    cannon = load(d, 728519)        # sci-fi cannon shot: the body
    plasma = load(d, 567308)        # plasma cannon: the low thump
    space = load(d, 394128)         # space explosion: a short punch
    scifi = load(d, 490266)         # sci-fi explosion 2: the roll
    impact = load(d, 413224)        # energy impact: crackle on top
    retro = load(d, 814052)         # retro bit explosion: bit-crushed mids
    void = load(d, 873747)          # void energy blast: a low punch

    if bz2:
        # Battle Zone II's shot as it is, its tail shortened (it rings for 2.3 s of its 3.8)
        # (a little saturation: it peaks high but is soft on average, under the explosions)
        loud_shot = finish(load(bz2, "player_fire.wav")[:int(2.6 * RATE)], 1.6, 0.6)
    else:
        shot = layer(1.2, [(blaster, 0.0, 0.0, 1.0), (cannon, 0.05, 0.0, 0.8), (highpass(plasma, 60.0), 0.12, 0.0, 0.55)])
        loud_shot = finish(shot, 2.2, 0.35)
    write(os.path.join(out, "SHELL_LOUD.wav"), loud_shot)
    engines(d, out)
    obstacles(d, out)
    write(os.path.join(out, "SHELL_SOFT.wav"), lowpass(loud_shot, 2500.0) * 0.6)    # (duller and softer, as from afar)

    roll = highpass(scifi, 70.0)    # (its lowest octave the cabinet would only rattle with)
    soft = layer(2.6, [(space, 0.0, 0.0, 1.0), (roll, 0.0, 0.0, 0.9), (impact, 0.25, 0.0, 0.8), (retro, 0.0, 0.05, 0.5)])
    write(os.path.join(out, "EXPLO_SOFT.wav"), finish(soft, 2.0, 0.9))

    loud = layer(3.6, [(void, 0.0, 0.0, 0.9), (space, 0.0, 0.0, 0.8), (roll, 0.0, 0.0, 1.0), (retro, 0.0, 0.15, 0.7)])
    write(os.path.join(out, "EXPLO_LOUD.wav"), finish(loud, 2.0, 1.2))


if __name__ == "__main__":
    main()
