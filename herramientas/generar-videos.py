#!/usr/bin/env python3
"""Videos cortos de las guías a partir de las capturas de la autoprueba
(WORSHIPSTUDIO_SELFTEST_ONLY=guia): cada paso con lo que hay que tocar
iluminado, un cursor que va hasta ahí y hace clic, y transiciones suaves.

Uso: python3 herramientas/generar-videos.py <carpeta-de-capturas>
Deja los videos y sus portadas en sitio/video/guias/.
"""
import glob
import json
import math
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SALIDA = os.path.join(RAIZ, "sitio", "video", "guias")
W, H, FPS = 1600, 1034, 30
TINTA = (7, 7, 7)
CHAMPAN = (220, 197, 156)
FUENTE = "/System/Library/Fonts/SFNS.ttf"


def font(size):
    try:
        return ImageFont.truetype(FUENTE, size)
    except OSError:
        return ImageFont.load_default()


def fit(img):
    """La captura dentro de W x H sobre negro tinta."""
    img = img.convert("RGB")
    scale = min(W / img.width, H / img.height)
    resized = img.resize((round(img.width * scale), round(img.height * scale)), Image.LANCZOS)
    frame = Image.new("RGB", (W, H), TINTA)
    ox, oy = (W - resized.width) // 2, (H - resized.height) // 2
    frame.paste(resized, (ox, oy))
    return frame, scale, ox, oy


def card(title, subtitle):
    frame = Image.new("RGB", (W, H), TINTA)
    glow = Image.new("RGB", (W, H), TINTA)
    ImageDraw.Draw(glow).ellipse((W * 0.2, H * 0.15, W * 0.8, H * 0.95), fill=(70, 32, 18))
    frame = Image.blend(frame, glow.filter(ImageFilter.GaussianBlur(160)), 0.9)
    logo = Image.open(os.path.join(RAIZ, "herramientas", "logo", "logo-1024.png")).convert("RGBA").resize((150, 150), Image.LANCZOS)
    frame.paste(logo, ((W - 150) // 2, int(H * 0.26)), logo)
    d = ImageDraw.Draw(frame)
    for text, size, y, color in ((title, 64, 0.50, (244, 241, 236)), (subtitle, 30, 0.60, (156, 149, 139))):
        f = font(size)
        w = d.textlength(text, font=f)
        d.text(((W - w) / 2, H * y), text, font=f, fill=color)
    return frame


def cursor_image(pressed):
    """Flecha blanca con borde oscuro; más chica al hacer clic."""
    s = 2
    img = Image.new("RGBA", (40 * s, 52 * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pts = [(2, 2), (2, 40), (12, 31), (19, 47), (26, 44), (19, 29), (33, 29)]
    pts = [(x * s, y * s) for x, y in pts]
    d.polygon(pts, fill=(255, 255, 255, 255), outline=(20, 18, 16, 255))
    d.line(pts + [pts[0]], fill=(20, 18, 16, 255), width=2 * s)
    size = (int(28 * (0.85 if pressed else 1)), int(36 * (0.85 if pressed else 1)))
    return img.resize(size, Image.LANCZOS)


def ease(t):
    return t * t * (3 - 2 * t)


def steps_of(folder, prefix):
    steps = []
    for png in sorted(glob.glob(os.path.join(folder, prefix + "*.png"))):
        frame, scale, ox, oy = fit(Image.open(png))
        target = None
        meta = png[:-4] + ".json"
        if os.path.exists(meta):
            areas = json.load(open(meta)).get("areas", [])
            src = Image.open(png)
            for x, y, w, h in areas:
                if 0 <= x < src.width and 0 <= y < src.height:
                    target = (ox + (x + w / 2) * scale, oy + (y + h / 2) * scale)
                    break
        steps.append((frame, target))
    return steps


def render(steps, title, subtitle, out):
    os.makedirs(SALIDA, exist_ok=True)
    proc = subprocess.Popen(
        ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS),
         "-i", "-", "-c:v", "libx264", "-preset", "slow", "-crf", "26", "-pix_fmt", "yuv420p", "-movflags", "+faststart", out],
        stdin=subprocess.PIPE)

    def emit(img, seconds):
        data = img.tobytes()
        for _ in range(int(seconds * FPS)):
            proc.stdin.write(data)

    def fade(a, b, seconds=0.4):
        n = int(seconds * FPS)
        for i in range(n):
            proc.stdin.write(Image.blend(a, b, ease((i + 1) / n)).tobytes())

    intro = card(title, subtitle)
    emit(intro, 2.0)
    previous = intro
    pos = (W * 0.55, H * 0.6)
    def with_cursor(img, at):
        out = img.copy()
        c = cursor_image(False)
        out.paste(c, (int(at[0]), int(at[1])), c)
        return out

    first = True
    for frame, target in steps:
        shown = frame if first else with_cursor(frame, pos)
        first = False
        fade(previous, shown)
        emit(shown, 0.5)
        last = shown
        if target:
            start = pos
            moves = int(0.8 * FPS)
            for i in range(moves):
                t = ease((i + 1) / moves)
                p = (start[0] + (target[0] - start[0]) * t, start[1] + (target[1] - start[1]) * t)
                img = frame.copy()
                img.paste(cursor_image(False), (int(p[0]), int(p[1])), cursor_image(False))
                proc.stdin.write(img.tobytes())
            pos = target
            clicks = int(0.5 * FPS)
            for i in range(clicks):
                t = (i + 1) / clicks
                img = frame.copy().convert("RGBA")
                ring = Image.new("RGBA", img.size, (0, 0, 0, 0))
                r = 8 + 34 * t
                ImageDraw.Draw(ring).ellipse((pos[0] - r, pos[1] - r, pos[0] + r, pos[1] + r),
                                             outline=CHAMPAN + (int(255 * (1 - t)),), width=4)
                img = Image.alpha_composite(img, ring).convert("RGB")
                c = cursor_image(t < 0.45)
                img.paste(c, (int(pos[0]), int(pos[1])), c)
                proc.stdin.write(img.tobytes())
            last = frame.copy()
            c = cursor_image(False)
            last.paste(c, (int(pos[0]), int(pos[1])), c)
        emit(last, 2.6)
        previous = last
    outro = card("Descargalo gratis", "solsox.github.io/WorshipStudio")
    fade(previous, outro, 0.6)
    emit(outro, 2.5)
    proc.stdin.close()
    proc.wait()
    steps[min(4, len(steps) - 1)][0].save(out[:-4] + ".jpg", quality=84)


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else "."
    videos = [("video-guia", "Primeros pasos", "Escenas, cantos, Biblia y el proyector", "primeros-pasos.mp4"),
              ("video-sala", "Proyector y transmisión", "Cada pantalla con lo suyo", "proyector-y-transmision.mp4")]
    for prefix, title, subtitle, name in videos:
        steps = steps_of(folder, prefix)
        if steps:
            render(steps, title, subtitle, os.path.join(SALIDA, name))
            print(name, len(steps), "pasos")


if __name__ == "__main__":
    main()
