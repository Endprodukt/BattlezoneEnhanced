"""soundcheck.py - the port's sound model against MAME's, sound by sound.

    python tools/soundcheck.py WRITES.txt MAME.wav MODEL.wav

WRITES.txt: what the game wrote to its sound hardware in a MAME run
(tools/mame/bzsound.lua); MAME.wav: MAME's sound of that run (-wavwrite);
MODEL.wav: the same writes played on the port's model (bzsound --replay).
Both WAVs are then the same game, sample for sample in time.

The writes say when which sound is on: the latch's bits (engine, shell,
explosion, loud or soft), the POKEY's channels and their volume. For each
kind of sound the stretches in which it sounds alone (or with the engine
only, which is always on in a game) are cut from both WAVs and compared:
how loud (dB, relative to the engine idling, so that the two programs'
overall levels do not matter), where the sound's strongest frequency is,
and its brightness (spectral centroid). Needs numpy.
"""
import sys
import wave

import numpy as np

RATE = 48000


def load(path):
    w = wave.open(path)
    n = w.getnframes()
    ch = w.getnchannels()
    d = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float64)
    if ch == 2:
        d = d.reshape(-1, 2)[:, 0]
    assert w.getframerate() == RATE, path
    return d / 32768.0


def states(path, end):
    """For every 1 ms: the latch, and the four POKEY channels' AUDC."""
    steps = int(end * 1000) + 1
    latch = np.zeros(steps, dtype=np.int32)
    audc = np.zeros((4, steps), dtype=np.int32)
    tune_start = []
    cur_latch, cur_c = 0, [0, 0, 0, 0]
    last = 0
    with open(path) as f:
        for line in f:
            t, a, v = line.replace(',', '.').split()
            t, a, v = float(t), int(a, 16), int(v, 16)
            i = min(int(t * 1000), steps - 1)
            latch[last:i] = cur_latch
            for c in range(4):
                audc[c, last:i] = cur_c[c]
            last = i
            if a == 0x1840:
                cur_latch = v
            elif 0x1820 <= a < 0x1828 and a & 1:
                ch = (a - 0x1820) >> 1
                if (cur_c[ch] & 15) == 0 and (v & 15) and ch < 2:
                    tune_start.append(t)
                cur_c[ch] = v
    latch[last:] = cur_latch
    for c in range(4):
        audc[c, last:] = cur_c[c]
    return latch, audc, tune_start


def runs(mask, min_ms):
    """Stretches (start ms, end ms) where mask holds for at least min_ms."""
    out, start = [], None
    for i, m in enumerate(mask):
        if m and start is None:
            start = i
        if not m and start is not None:
            if i - start >= min_ms:
                out.append((start, i))
            start = None
    if start is not None and len(mask) - start >= min_ms:
        out.append((start, len(mask)))
    return out


def measure(x):
    x = x - x.mean()
    rms = np.sqrt(np.mean(x * x)) + 1e-12
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    freqs = np.fft.rfftfreq(len(x), 1.0 / RATE)
    keep = freqs > 30
    peak = freqs[keep][np.argmax(spec[keep])]
    centroid = np.sum(freqs[keep] * spec[keep]) / (np.sum(spec[keep]) + 1e-12)
    return 20 * np.log10(rms), peak, centroid


def main():
    writes, mame_path, model_path = sys.argv[1:4]
    mame, model = load(mame_path), load(model_path)
    n = min(len(mame), len(model))
    mame, model = mame[:n], model[:n]
    latch, audc, tune_start = states(writes, n / RATE)

    # (the two must line up: the envelopes' best lag)
    env = lambda x: np.sqrt(np.convolve(x * x, np.ones(480) / 480, mode='same'))[::48]
    em, eo = env(mame), env(model)
    lags = range(-50, 51)
    best = max(lags, key=lambda L: np.corrcoef(np.roll(em, L)[100:-100], eo[100:-100])[0, 1])
    print("alignment: best lag %d ms (0 expected), correlation of the loudness over time %.3f"
          % (best, np.corrcoef(em[100:-100], eo[100:-100])[0, 1]))

    on = lambda bit: (latch & bit) != 0
    enable = on(0x20)
    vol = audc & 15
    pokey12 = (vol[0] + vol[1]) > 0
    pokey34 = (vol[2] + vol[3]) > 0
    shell, explo = on(0x04), on(0x01)
    # (what is still sounding: a burst rings for a while after its switch opens)
    def after(mask, ms):
        m = mask.copy()
        idx = np.nonzero(mask)[0]
        for i in idx:
            m[i:i + ms] = True
        return m[:len(mask)]
    shell_r, explo_r = after(shell, 400), after(explo, 900)
    quiet_pokey = ~after(pokey12 | pokey34, 50)

    def shot_window(mask):
        # (a shot's switch is closed for 16 ms: the 250 ms from its closing)
        m = np.zeros(len(mask), dtype=bool)
        rising = np.nonzero(mask[1:] & ~mask[:-1])[0] + 1
        for i in rising:
            m[i:i + 250] = True
        return m

    kinds = {
        "engine idling": enable & on(0x80) & ~on(0x10) & ~shell_r & ~explo_r & quiet_pokey,
        "engine revved": enable & on(0x80) & on(0x10) & ~shell_r & ~explo_r & quiet_pokey,
        "shot, loud (player)": enable & shot_window(shell & on(0x08)) & ~explo_r,
        "shot, soft (enemy)": enable & shot_window(shell & ~on(0x08)) & ~explo_r,
        "explosion, loud": enable & explo & on(0x02) & ~shell_r & quiet_pokey,
        "explosion, soft": enable & explo & ~on(0x02) & ~shell_r & quiet_pokey,
        "POKEY tunes (ch. 1, 2)": enable & pokey12 & ~pokey34 & ~shell_r & ~explo_r,
        "missile buzz (ch. 3, 4)": enable & pokey34 & ~pokey12 & ~shell_r & ~explo_r,
    }
    results = {}
    for name, mask in kinds.items():
        segs = runs(mask, 40)
        if not segs:
            results[name] = None
            continue
        a = np.concatenate([mame[s * 48:e * 48] for s, e in segs])
        b = np.concatenate([model[s * 48:e * 48] for s, e in segs])
        # the spectrum of a stretch that is long enough, the level of all of them
        long_seg = max(segs, key=lambda se: se[1] - se[0])
        la, _, _ = measure(a)
        lb, _, _ = measure(b)
        _, pa, ca = measure(mame[long_seg[0] * 48:long_seg[1] * 48])
        _, pb, cb = measure(model[long_seg[0] * 48:long_seg[1] * 48])
        results[name] = (len(segs), sum(e - s for s, e in segs), la, lb, pa, pb, ca, cb)

    ref = results["engine idling"]
    print("\n%-26s %5s %7s | %8s %8s %6s | %8s %8s | %8s %8s" %
          ("sound", "runs", "ms", "MAME dB", "model dB", "diff", "peak Hz", "peak Hz", "bright", "bright"))
    print("%-26s %5s %7s | %8s %8s %6s | %8s %8s | %8s %8s" % ("", "", "", "(engine 0)", "", "", "MAME", "model", "MAME", "model"))
    for name, r in results.items():
        if r is None:
            print("%-26s (not in this run)" % name)
            continue
        cnt, ms, la, lb, pa, pb, ca, cb = r
        ra, rb = la - ref[2], lb - ref[3]
        print("%-26s %5d %7d | %8.1f %8.1f %+6.1f | %8.0f %8.0f | %8.0f %8.0f" %
              (name, cnt, ms, ra, rb, rb - ra, pa, pb, ca, cb))
    print("\nlevels relative to the engine idling in each program; diff: the model's minus MAME's")
    # the tunes and the buzz by bands: where the model is too loud or too quiet
    for name in ("POKEY tunes (ch. 1, 2)", "missile buzz (ch. 3, 4)"):
        segs = runs(kinds[name], 40)
        if not segs:
            continue
        a = np.concatenate([mame[s * 48:e * 48] for s, e in segs])
        b = np.concatenate([model[s * 48:e * 48] for s, e in segs])
        fa = np.abs(np.fft.rfft(a)) ** 2
        fb = np.abs(np.fft.rfft(b)) ** 2
        fr = np.fft.rfftfreq(len(a), 1.0 / RATE)
        line = []
        for lo, hi in ((50, 300), (300, 700), (700, 1500), (1500, 3000), (3000, 6000), (6000, 12000), (12000, 24000)):
            k = (fr >= lo) & (fr < hi)
            da = 10 * np.log10(fa[k].sum() + 1e-12) - ref[2] * 1.0
            db = 10 * np.log10(fb[k].sum() + 1e-12) - ref[3] * 1.0
            line.append("%d-%d Hz %+.1f" % (lo, hi, (db - da)))
        print("%s, the model against MAME by band (dB): %s" % (name, ", ".join(line)))
    print("overall: MAME %.1f dB, model %.1f dB (whole run, RMS)" % (measure(mame)[0], measure(model)[0]))


if __name__ == "__main__":
    main()
