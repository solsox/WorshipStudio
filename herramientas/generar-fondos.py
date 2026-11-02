#!/usr/bin/env python3
# Generates the original backgrounds in app/frontend/data/worship/fondos.
# Usage: python3 generar-fondos.py <carpeta> <nombre>, then convert the PPM to
# JPEG 1920x1080 (sips on macOS, ImageMagick elsewhere). Released under GPL like the app.
import math, random, sys, os
W, H = 960, 540
out = sys.argv[1]

def lerp(a, b, t): return a + (b - a) * t
def mix(c1, c2, t): return [lerp(c1[i], c2[i], t) for i in range(3)]
def hexc(h): return [int(h[i:i+2], 16) for i in (1, 3, 5)]

def gradient(stops, angle=90):
    """Vertical-ish gradient with stops [(pos, color)]."""
    img = [[0.0] * (W * 3) for _ in range(H)]
    ca, sa = math.cos(math.radians(angle)), math.sin(math.radians(angle))
    for y in range(H):
        row = img[y]
        for x in range(W):
            t = ((x / W - .5) * ca + (y / H - .5) * sa) + .5
            t = min(1, max(0, t))
            for i in range(len(stops) - 1):
                if stops[i][0] <= t <= stops[i + 1][0]:
                    k = (t - stops[i][0]) / max(1e-6, stops[i + 1][0] - stops[i][0])
                    c = mix(stops[i][1], stops[i + 1][1], k)
                    break
            row[3*x:3*x+3] = c
    return img

def glow(img, cx, cy, r, color, strength=1.0):
    x0, x1 = max(0, int(cx - r)), min(W, int(cx + r))
    y0, y1 = max(0, int(cy - r)), min(H, int(cy + r))
    for y in range(y0, y1):
        row = img[y]
        for x in range(x0, x1):
            d = math.hypot(x - cx, y - cy) / r
            if d < 1:
                a = (1 - d) ** 2 * strength
                for i in range(3):
                    row[3*x+i] = row[3*x+i] + (color[i] - row[3*x+i]) * min(1, a) if a < 1 else color[i]

def disc(img, cx, cy, r, color, alpha, soft=0.25):
    x0, x1 = max(0, int(cx - r - 2)), min(W, int(cx + r + 2))
    y0, y1 = max(0, int(cy - r - 2)), min(H, int(cy + r + 2))
    for y in range(y0, y1):
        row = img[y]
        for x in range(x0, x1):
            d = math.hypot(x - cx, y - cy) / r
            if d < 1:
                edge = 1 if d < 1 - soft else (1 - d) / soft
                a = alpha * edge * (0.85 + 0.15 * d)  # brighter rim like real bokeh
                for i in range(3):
                    row[3*x+i] = row[3*x+i] * (1 - a) + color[i] * a

def rays(img, ox, oy, color, n, width, alpha, rnd):
    angles = [rnd.uniform(0.15, 1.25) for _ in range(n)]
    for y in range(H):
        row = img[y]
        for x in range(W):
            ang = math.atan2(y - oy, x - ox)
            dist = math.hypot(x - ox, y - oy)
            a = 0
            for t in angles:
                dd = abs(ang - t)
                if dd < width:
                    a += (1 - dd / width) ** 2
            if a:
                a = min(1, a * alpha * max(0, 1 - dist / (W * 1.3)))
                for i in range(3):
                    row[3*x+i] = row[3*x+i] * (1 - a) + color[i] * a

def vignette(img, k=0.45):
    for y in range(H):
        row = img[y]
        for x in range(W):
            d = math.hypot((x / W - .5) * 1.2, (y / H - .5)) * 1.4
            f = 1 - k * max(0, d - .35)
            for i in range(3):
                row[3*x+i] *= f

def stars(img, rnd, n, color):
    for _ in range(n):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H * 0.75)
        r = rnd.choice([0.8, 0.9, 1.0, 1.2, 1.6, 2.2])
        disc(img, x, y, r, color, rnd.uniform(.35, .95), soft=.9)

def wave_band(img, color, base, amp, freq, phase, thick, alpha):
    for y in range(H):
        row = img[y]
        for x in range(W):
            c = base + amp * math.sin(x / W * freq + phase) + amp * .4 * math.sin(x / W * freq * 2.3 + phase * 1.7)
            d = abs(y - c * H) / (thick * H)
            if d < 1:
                a = alpha * (1 - d) ** 2
                for i in range(3):
                    row[3*x+i] = row[3*x+i] * (1 - a) + color[i] * a

def save(img, name):
    path = os.path.join(out, name + '.ppm')
    with open(path, 'wb') as f:
        f.write(b'P6 %d %d 255\n' % (W, H))
        rnd = random.Random(7)
        data = bytearray()
        for row in img:
            for v in row:
                # light dithering avoids banding in gradients
                data.append(int(min(255, max(0, v + rnd.uniform(-1.2, 1.2)))))
        f.write(data)

which = sys.argv[2]
rnd = random.Random(hash(which) & 0xffff)

if which == 'amanecer':
    img = gradient([(0, hexc('#1b1f4b')), (.45, hexc('#7a3e6e')), (.75, hexc('#e57a4f')), (1, hexc('#f6c46b'))])
    glow(img, W * .5, H * 1.02, W * .55, hexc('#ffe2a8'), 1.4)
    vignette(img, .35)
elif which == 'bokeh-dorado':
    img = gradient([(0, hexc('#120d0a')), (1, hexc('#2b1c10'))], 70)
    for _ in range(70):
        disc(img, rnd.uniform(0, W), rnd.uniform(0, H), rnd.uniform(12, 70),
             mix(hexc('#ffcf7a'), hexc('#ff9a3c'), rnd.random()), rnd.uniform(.08, .38))
    vignette(img, .5)
elif which == 'bokeh-azul':
    img = gradient([(0, hexc('#050b1e')), (1, hexc('#0c2148'))], 110)
    for _ in range(65):
        disc(img, rnd.uniform(0, W), rnd.uniform(0, H), rnd.uniform(10, 65),
             mix(hexc('#7fc8ff'), hexc('#b49bff'), rnd.random()), rnd.uniform(.07, .33))
    vignette(img, .5)
elif which == 'rayos-de-luz':
    img = gradient([(0, hexc('#0b2238')), (1, hexc('#0f4a63'))], 60)
    rays(img, -W * .05, -H * .15, hexc('#d9f2ff'), 9, .035, .45, rnd)
    glow(img, 0, 0, W * .45, hexc('#eaf8ff'), .9)
    vignette(img, .4)
elif which == 'noche-estrellada':
    img = gradient([(0, hexc('#02040f')), (.7, hexc('#0a1636')), (1, hexc('#1b2a5a'))])
    stars(img, rnd, 420, hexc('#ffffff'))
    glow(img, W * .5, H * 1.1, W * .7, hexc('#2d4b8c'), .7)
elif which == 'aurora':
    img = gradient([(0, hexc('#030b14')), (1, hexc('#071c26'))])
    wave_band(img, hexc('#3ee6b0'), .38, .08, 5, .4, .22, .55)
    wave_band(img, hexc('#5aa9ff'), .30, .06, 4, 2.0, .16, .35)
    wave_band(img, hexc('#b47cff'), .22, .05, 6, 3.1, .10, .25)
    stars(img, rnd, 160, hexc('#ffffff'))
    vignette(img, .4)
elif which == 'ondas-violeta':
    img = gradient([(0, hexc('#1a0b2e')), (1, hexc('#3b1260'))], 120)
    for k in range(6):
        wave_band(img, hexc('#c58bff'), .3 + k * .09, .05, 3 + k * .4, k * 1.3, .05, .22)
    vignette(img, .45)
elif which == 'bosque-niebla':
    img = gradient([(0, hexc('#c9d6cf')), (.55, hexc('#6f8f86')), (1, hexc('#1f3530'))])
    for layer, (base, col) in enumerate([(.55, '#5a7a70'), (.65, '#3f5d55'), (.78, '#28423c'), (.9, '#16271f')]):
        c = hexc(col)
        for x in range(W):
            top = base + .04 * math.sin(x / W * (6 + layer) + layer) + .02 * math.sin(x / W * 23 + layer * 3)
            # little pine tips
            top -= .035 * max(0, math.sin(x / W * (90 + layer * 17))) ** 6
            for y in range(int(top * H), H):
                img[y][3*x:3*x+3] = c
        for y in range(H):
            row = img[y]
            fog = max(0, 1 - abs(y / H - base) / .12) * .35
            for x in range(W):
                for i in range(3):
                    row[3*x+i] = row[3*x+i] * (1 - fog) + 215 * fog
elif which == 'gracia':
    img = gradient([(0, hexc('#f7efe4')), (1, hexc('#e8d6c0'))], 100)
    glow(img, W * .75, H * .2, W * .6, hexc('#fffaf2'), 1.2)
    for _ in range(25):
        disc(img, rnd.uniform(0, W), rnd.uniform(0, H), rnd.uniform(20, 90), hexc('#ffffff'), rnd.uniform(.08, .25))
elif which == 'brasas':
    img = gradient([(0, hexc('#0c0303')), (1, hexc('#3a0d06'))])
    glow(img, W * .5, H * 1.05, W * .6, hexc('#ff5a1f'), .8)
    for _ in range(140):
        disc(img, rnd.uniform(0, W), rnd.uniform(H * .2, H), rnd.uniform(1.5, 6),
             mix(hexc('#ffb347'), hexc('#ff4e1a'), rnd.random()), rnd.uniform(.4, .95), soft=.8)
    vignette(img, .5)
save(img, which)
