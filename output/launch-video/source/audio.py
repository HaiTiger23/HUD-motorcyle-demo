"""Original score + synthesized UI sounds + ElevenLabs voice, mixed to one WAV.

Music: 120 BPM, A minor (Am F C G), bars start at t = 1, 3, 5 ...  Drop on "HUD" (t = 5.0),
breakdown in the rain scene, second drop at 31.0, riser into the logo hit at 41.0.
"""
import json, sys, subprocess, wave
import numpy as np
from scipy import signal

SR = 48000
d = json.load(open("cues.json"))
END = d["END"]; LS = d["LS"]; CUES = d["cues"]
N = int((END + 0.5) * SR)
rng = np.random.default_rng(42)
tt = lambda dur: np.arange(int(dur * SR)) / SR

def lp(x, fc, order=2):
    b, a = signal.butter(order, min(fc, SR / 2 - 100) / (SR / 2), "low"); return signal.lfilter(b, a, x)
def hp(x, fc, order=2):
    b, a = signal.butter(order, fc / (SR / 2), "high"); return signal.lfilter(b, a, x)
def bp(x, lo, hi, order=2):
    b, a = signal.butter(order, [lo / (SR / 2), hi / (SR / 2)], "band"); return signal.lfilter(b, a, x)
def place(buf, snd, t, peak_align=False, g=1.0):
    off = int(np.argmax(np.abs(snd))) if peak_align else 0
    i = int(t * SR) - off
    if i < 0: snd = snd[-i:]; i = 0
    n = min(len(snd), len(buf) - i)
    if n > 0: buf[i:i + n] += g * snd[:n]
def midi(m): return 440.0 * 2 ** ((m - 69) / 12)
def reverb(x, sec=1.6, wet=0.25):
    ir = rng.standard_normal(int(sec * SR)) * np.exp(-np.arange(int(sec * SR)) / (SR * sec / 5.5))
    ir = lp(ir, 6000); ir /= np.sqrt(np.sum(ir ** 2))
    y = signal.fftconvolve(x, ir)[: len(x)]
    return x * (1 - wet) + y * wet * 0.9

# ───────────────────────── music ─────────────────────────
BPM, BEAT, BAR0 = 120, 0.5, 1.0
CH = [[57, 60, 64], [53, 57, 60], [48, 52, 55], [55, 59, 62]]      # Am F C G (midi)
ROOT = [45, 41, 36, 43]
def section(t):
    if t < 1.0: return "air"
    if t < 5.0: return "intro"
    if t < 25.0: return "main"
    if t < 31.0: return "break"
    if t < 35.0: return "main"
    if t < 39.0: return "low"
    if t < 41.0: return "build"
    if t < 45.0: return "main"
    return "end"

kick_b, clap_b, hat_b, bass_b, pad_b, arp_b, fx_b = (np.zeros(N) for _ in range(7))

def kick():
    x = tt(0.42); f = 45 + 95 * np.exp(-x * 28); ph = 2 * np.pi * np.cumsum(f) / SR
    s = np.sin(ph) * np.exp(-x * 7.5); s[:200] += rng.standard_normal(200) * 0.25 * np.linspace(1, 0, 200)
    return np.tanh(s * 1.6)
def clap():
    x = tt(0.25); n = rng.standard_normal(len(x)); env = np.zeros(len(x))
    for k, o in enumerate([0, 0.011, 0.022]): env += (x >= o) * np.exp(-np.clip(x - o, 0, None) * (60 if k < 2 else 16))
    return bp(n, 900, 4200) * env * 0.9
def hat(open_=False):
    x = tt(0.25 if open_ else 0.06); n = hp(rng.standard_normal(len(x)), 7500)
    return n * np.exp(-x * (14 if open_ else 70)) * 0.45
def saw(f, dur):
    x = tt(dur); return 2 * ((x * f) % 1) - 1

K, CL, HO, HC = kick(), clap(), hat(True), hat(False)
nbars = int((END - BAR0) / 2) + 2
for bi in range(-1, nbars):
    b0 = BAR0 + bi * 2.0
    ch, root = CH[bi % 4], ROOT[bi % 4]
    for beat in range(4):
        tb = b0 + beat * BEAT
        if tb < 0 or tb >= END: continue
        sec = section(tb)
        if sec in ("main", "low"): place(kick_b, K, tb, g=1.0 if sec == "main" else 0.8)
        if sec == "build" and beat == 0: place(kick_b, K, tb, g=0.8)
        if sec == "main" and beat in (1, 3): place(clap_b, CL, tb)
        if sec == "main": place(hat_b, HO, tb + BEAT / 2, g=0.55)
        for s16 in range(4):
            t16 = tb + s16 * BEAT / 4
            if sec in ("main", "intro", "build") and s16 != 2:
                place(hat_b, HC, t16, g=(0.35 if sec == "main" else 0.22 * min(1, (t16 - 1) / 4 + 0.2)))
        # bass: offbeat 8ths
        if sec in ("main", "low", "build"):
            for e in (0.25,):
                x = tt(0.22); f = midi(root - 12 if sec != "low" else root - 12)
                s = np.tanh(1.6 * np.sin(2 * np.pi * f * x) + 0.3 * np.sin(4 * np.pi * f * x)) * np.minimum(1, x * 200) * np.exp(-x * 6)
                place(bass_b, s, tb + e, g=0.7)
    # pad: whole bar
    if b0 + 2 > 0 and b0 < END:
        sec = section(max(b0, 0))
        x = tt(2.1); p = np.zeros(len(x))
        for m in ch:
            for det in (-0.12, 0, 0.11):
                p += saw(midi(m) * 2 ** (det / 12), 2.1)
        env = np.minimum(1, x / 0.08) * np.minimum(1, (2.1 - x) / 0.12)
        fc = {"air": 500, "intro": 900, "main": 2600, "break": 1300, "low": 1100, "build": 1800, "end": 2200}[sec]
        place(pad_b, lp(p * env, fc, 2) * 0.09, b0)
        # arp: 16ths over chord tones, two octaves
        if sec in ("intro", "main", "break", "build", "low"):
            notes = [ch[0] + 12, ch[1] + 12, ch[2] + 12, ch[0] + 24, ch[2] + 12, ch[1] + 12, ch[0] + 24, ch[1] + 24]
            for k in range(16):
                ts = b0 + k * BEAT / 2
                if ts < 1.0 or ts >= END: continue
                f = midi(notes[k % 8]); x = tt(0.3)
                s = (np.sin(2 * np.pi * f * x) + 0.35 * np.sin(4 * np.pi * f * x) + 0.12 * saw(f, 0.3)) * np.exp(-x * 16)
                gg = {"intro": 0.10, "main": 0.13, "break": 0.15, "build": 0.13, "low": 0.09}[sec]
                place(arp_b, s, ts, g=gg)

# risers + impacts
def riser(dur):
    x = tt(dur); n = rng.standard_normal(len(x))
    out = np.zeros(len(x)); seg = int(0.05 * SR)
    for i in range(0, len(x), seg):
        f = 400 + 7000 * (i / len(x)) ** 2
        out[i:i + seg] = bp(n[i:i + seg], f * 0.7, min(f * 1.4, 20000))
    sw = np.sin(2 * np.pi * np.cumsum(180 + 900 * (x / dur) ** 2) / SR) * 0.25
    return (out * 1.4 + sw) * (x / dur) ** 2.2
def impact():
    x = tt(2.6); boom = np.sin(2 * np.pi * np.cumsum(38 + 60 * np.exp(-x * 9)) / SR) * np.exp(-x * 2.6)
    crash = hp(rng.standard_normal(len(x)), 3500) * np.exp(-x * 2.2) * 0.35
    return np.tanh(boom * 1.4) * 0.9 + crash
R1, IMP = riser(3.0), impact()
for t_end in (5.0, 31.0, 41.0):
    place(fx_b, R1 * 0.38, t_end - 3.0)
    place(fx_b, IMP * 0.75, t_end)
# snare roll into the first drop
for k in range(16):
    ts = 4.0 + 1.0 * (1 - (1 - k / 16) ** 1.6)
    place(clap_b, CL * (0.25 + 0.5 * k / 16), ts)
# final chord
x = tt(4.0); fin = sum(saw(midi(m) * 2 ** (det / 12), 4.0) for m in (45, 57, 60, 64, 69) for det in (-0.1, 0.1))
place(pad_b, lp(fin * np.exp(-x * 0.9), 1800) * 0.07, 45.0)
place(fx_b, IMP * 0.5, 45.0)

# sidechain on kick hits
sc = np.ones(N)
for c in range(N // SR * 2 + 4):
    tb = BAR0 + c * BEAT - 2 * BEAT
    if tb < 0 or tb >= END or section(tb) not in ("main", "low", "build"): continue
    i = int(tb * SR); n = min(int(0.35 * SR), N - i)
    sc[i:i + n] = np.minimum(sc[i:i + n], 1 - 0.55 * np.exp(-np.arange(n) / (0.09 * SR)))
music = kick_b * 0.55 + clap_b * 0.3 + hat_b * 0.35 + bass_b * 0.42 * sc + reverb(pad_b * sc + arp_b * sc, 1.8, 0.3) + fx_b
music = reverb(music, 0.6, 0.06)
fade_in = np.minimum(1, np.arange(N) / (0.6 * SR)); music *= fade_in
tail = np.clip((END + 0.4 - np.arange(N) / SR) / 1.4, 0, 1); music *= tail

# ───────────────────────── SFX ─────────────────────────
def env_bell(x, dur): return np.sin(np.pi * np.clip(x / dur, 0, 1)) ** 2
def sfx(kind, r):
    if kind == "click":
        a = hp(rng.standard_normal(int(0.012 * SR)), 2500) * np.exp(-tt(0.012) * 400)
        b = np.zeros(int(0.08 * SR)); b[: len(a)] += a; b[int(0.06 * SR):int(0.06 * SR) + len(a)] += a * 0.5
        return b * 0.9 + np.sin(2 * np.pi * 1800 * tt(0.08)) * np.exp(-tt(0.08) * 90) * 0.25
    if kind == "tick":
        x = tt(0.05); return np.sin(2 * np.pi * (2100 + 300 * r) * x) * np.exp(-x * 120) * 0.5
    if kind == "key":
        x = tt(0.03); return bp(rng.standard_normal(len(x)), 1500, 6000) * np.exp(-x * 220) * 0.6
    if kind in ("whoosh", "swish"):
        dur = 0.5 if kind == "whoosh" else 0.28; x = tt(dur)
        n = rng.standard_normal(len(x)); lo = bp(n, 300, 2500); hi = bp(n, 2500, 9000)
        mix = lo * (1 - x / dur) + hi * (x / dur) * 0.6
        return mix * env_bell(x, dur) * (0.55 if kind == "whoosh" else 0.4)
    if kind == "pop":
        x = tt(0.12); f = 520 + 520 * np.minimum(1, x / 0.04)
        return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-x * 30) * 0.55
    if kind == "blip":
        x = tt(0.22); f = np.where(x < 0.09, 880, 1320)
        return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-(x % 0.09) * 30) * 0.4
    if kind == "buzz":
        x = tt(0.32); s = np.sign(np.sin(2 * np.pi * 165 * x)) * 0.5 + np.sin(2 * np.pi * 330 * x) * 0.3
        return lp(s, 900) * (0.6 + 0.4 * np.sin(2 * np.pi * 28 * x)) * env_bell(x, 0.32) * 0.5
    if kind == "note":
        x = tt(0.45); s = np.sin(2 * np.pi * midi(88) * x) * np.exp(-x * 9) + np.sin(2 * np.pi * midi(93) * np.clip(x - 0.09, 0, None)) * np.exp(-np.clip(x - 0.09, 0, None) * 9) * (x > 0.09)
        return s * 0.18
    if kind == "chime":
        x = tt(1.2); s = 0
        for k, (m, o) in enumerate([(81, 0), (88, 0.11), (93, 0.22)]):
            xo = np.clip(x - o, 0, None); s = s + (x > o) * (np.sin(2 * np.pi * midi(m) * xo) + 0.3 * np.sin(2 * np.pi * midi(m) * 2.76 * xo)) * np.exp(-xo * 5)
        return s * 0.22
    if kind in ("hit", "slam"):
        x = tt(1.4); boom = np.sin(2 * np.pi * np.cumsum(48 + 90 * np.exp(-x * 14)) / SR) * np.exp(-x * 4)
        click = hp(rng.standard_normal(len(x)), 2000) * np.exp(-x * 40) * 0.4
        shim = sum(np.sin(2 * np.pi * midi(m) * x) for m in (93, 100, 105)) * np.exp(-x * 3) * 0.05
        return (np.tanh(boom * 1.5) * 0.7 + click + shim) * (1 if kind == "hit" else 0.7)
    if kind == "thump":
        x = tt(0.3); return np.sin(2 * np.pi * np.cumsum(70 + 80 * np.exp(-x * 30)) / SR) * np.exp(-x * 14) * 0.6
    if kind == "link":
        x = tt(0.3); f = np.where(x < 0.1, midi(76), midi(83))
        return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-(x % 0.1) * 25) * 0.35
    if kind == "snap":
        x = tt(0.35); f = 900 * np.exp(-x * 7)
        return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-x * 10) * 0.35 + bp(rng.standard_normal(len(x)), 1000, 5000) * np.exp(-x * 30) * 0.2
    if kind == "lock":
        x = tt(0.25); a = bp(rng.standard_normal(len(x)), 1500, 7000) * np.exp(-x * 90)
        b = np.sin(2 * np.pi * np.cumsum(140 + 200 * np.exp(-x * 40)) / SR) * np.exp(-x * 22)
        c = np.zeros(len(x)); o = int(0.045 * SR); c[o:] = a[: len(x) - o] * 0.6
        return a * 0.7 + b * 0.6 + c
    if kind == "pin":
        x = tt(0.4); f = 1100 * np.exp(-x * 9) + 300
        s = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-x * 9) * 0.4; th = sfx("thump", r); s[:len(th)] += th * 0.6; return s
    if kind == "drop":
        x = tt(0.05); f = 1400 + 1800 * r
        return np.sin(2 * np.pi * np.cumsum(f * (1 + x * 8)) / SR) * np.exp(-x * 110) * 0.35
    if kind == "shimmer":
        x = tt(1.0); s = 0
        for k, m in enumerate((81, 84, 88, 91, 93, 96)):
            xo = np.clip(x - k * 0.045, 0, None); s = s + (x > k * 0.045) * np.sin(2 * np.pi * midi(m) * xo) * np.exp(-xo * 6)
        return s * 0.1
    return np.zeros(10)

sfx_b = np.zeros(N)
for c in CUES:
    if c["type"] == "riserEnd": continue
    s = sfx(c["type"], rng.random())
    place(sfx_b, s, c["t"], peak_align=c["type"] in ("click", "tick", "key", "whoosh", "swish", "pop", "hit", "slam", "thump", "lock", "pin", "drop"), g=c["g"])
sfx_b = reverb(sfx_b, 0.8, 0.12)

# ───────────────────────── voice ─────────────────────────
def readwav(fn):
    with wave.open(fn) as w:
        a = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float64) / 32768
        return a if w.getnchannels() == 1 else a.reshape(-1, w.getnchannels()).mean(1)
tag = sys.argv[1] if len(sys.argv) > 1 else "liam"
vo = np.zeros(N)
for i, t0 in enumerate(LS):
    v = readwav(f"audio/{tag}_{i + 1:02d}.wav")
    v = hp(v, 80); place(vo, v, t0)
# gentle compression on the voice
venv = np.sqrt(np.maximum(lp(vo ** 2, 12), 0)); gain = np.where(venv > 0.12, (0.12 / np.maximum(venv, 1e-6)) ** 0.4, 1)
vo = vo * gain
vo /= np.max(np.abs(vo)) + 1e-9

# duck music under voice
act = (np.abs(lp(np.abs(vo), 8)) > 0.02).astype(float)
duck = 1 - 0.65 * np.convolve(act, np.ones(int(0.18 * SR)) / int(0.18 * SR), "same")
mix = music * 0.5 * duck + sfx_b * 0.55 + vo * 0.95
mix = mix / (np.max(np.abs(mix)) + 1e-9) * 0.89
st = np.stack([mix, mix], 1)
with wave.open("mix.wav", "w") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes((st * 32767).astype(np.int16).tobytes())
for name, buf in (("music_only.wav", music), ("sfx_only.wav", sfx_b)):
    with wave.open(name, "w") as w:
        b = buf / (np.max(np.abs(buf)) + 1e-9) * 0.9
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((b * 32767).astype(np.int16).tobytes())
print("mix.wav", N / SR, "s")
# balance report
def rms(x): return 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-9)
sp = act > 0.5
print("voice rms in speech", round(rms(vo[sp] * 0.95), 1), "| music under voice", round(rms((music * 0.5 * duck)[sp]), 1),
      "| music between lines", round(rms((music * 0.5 * duck)[~sp]), 1), "| sfx", round(rms(sfx_b * 0.55), 1))
