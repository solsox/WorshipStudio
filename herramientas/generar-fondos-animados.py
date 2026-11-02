#!/usr/bin/env python3
"""Original animated backgrounds for WorshipStudio (seamless 12 s loops).

Each one is a math formula drawn by ffmpeg's geq filter at low resolution,
then blurred and scaled to 1280x720: soft moving lights, rotating rays and a
breathing glow. Usage: python3 herramientas/generar-fondos-animados.py <carpeta> [nombre...]
Released under GPL like the app.
"""
import os, subprocess, sys

W, H, FPS, LOOP = 480, 270, 30, 12
P = f'2*PI*T/{LOOP}'  # one full turn per loop: frame 0 == frame LOOP*FPS


def blob(cx, cy, r):
    return f'exp(-(pow(X-({cx}),2)+pow(Y-({cy}),2))/{2 * r * r})'


def lights(base, colors):
    """Five soft stage lights drifting on Lissajous paths over a dark base."""
    paths = [
        (f'{W/2}+{W*0.34}*sin({P})', f'{H/2}+{H*0.30}*cos({P})', W * 0.17),
        (f'{W/2}+{W*0.38}*cos(2*{P}+1)', f'{H/2}+{H*0.32}*sin({P}+2)', W * 0.20),
        (f'{W/2}+{W*0.30}*sin({P}+4)', f'{H/2}+{H*0.36}*sin(2*{P})', W * 0.15),
        (f'{W/2}+{W*0.42}*sin(3*{P}+2)', f'{H/2}+{H*0.26}*cos(2*{P}+3)', W * 0.12),
        (f'{W/2}+{W*0.26}*cos({P}+5)', f'{H/2}+{H*0.38}*cos(3*{P})', W * 0.14),
    ]
    palette = colors + colors[:2]
    vignette = f'(1-0.55*pow(hypot((X-{W/2})/{W/2},(Y-{H/2})/{H/2})/1.42,2))'
    chans = []
    for c in range(3):
        terms = [f'{palette[i][c] * 0.62:.0f}*{blob(*paths[i])}' for i in range(5)]
        chans.append(f'({base[c]}+' + '+'.join(terms) + f')*{vignette}')
    return chans


def rays(base, color):
    """Light rays turning slowly around a point above the screen."""
    angle = f'atan2(Y-({-H*0.25}),X-{W/2})'
    beam = f'pow(max(0,cos(10*{angle}+{P})),14)'
    fall = f'exp(-hypot(X-{W/2},Y+{H*0.25})/{H*1.1})'
    glow = f'exp(-hypot(X-{W/2},Y+{H*0.1})/{H*0.5})'
    return [f'{base[c]}+{color[c]}*({beam}*{fall}*1.1+{glow}*0.5)' for c in range(3)]


def breathe(base, color):
    """A warm glow that grows and fades like breathing."""
    radius = f'({H*0.55}+{H*0.12}*sin({P}))'
    glow = f'exp(-pow(hypot(X-{W/2},Y-{H*0.62})/{radius},2))'
    return [f'{base[c]}+{color[c]}*{glow}' for c in range(3)]


BACKGROUNDS = {
    'aurora-viva': lights((6, 12, 20), [(30, 200, 170), (50, 110, 230), (140, 80, 220)]),
    'brasas-suaves': lights((14, 8, 8), [(230, 90, 40), (240, 170, 80), (120, 40, 110)]),
    'azul-profundo': lights((4, 8, 22), [(40, 90, 200), (20, 160, 200), (90, 60, 180)]),
    'rosa-y-oro': lights((18, 6, 16), [(200, 60, 140), (230, 140, 90), (120, 50, 170)]),
    'amanecer-vivo': lights((20, 14, 30), [(240, 130, 70), (250, 200, 120), (120, 60, 140)]),
    'esperanza-verde': lights((4, 14, 10), [(30, 170, 110), (120, 200, 120), (20, 110, 140)]),
    'rayos-de-gloria': rays((8, 22, 36), (210, 235, 255)),
    'luz-que-respira': breathe((12, 8, 6), (240, 150, 80)),
}


def render(name, chans, out_dir):
    out = os.path.join(out_dir, name + '.mp4')
    # clip: geq wraps values over 255 instead of saturating them
    expr = ':'.join(f"{k}='clip({v},0,255)'" for k, v in zip(('r', 'g', 'b'), chans))
    graph = (f'nullsrc=s={W}x{H}:r={FPS}:d={LOOP},format=rgb24,geq={expr},'
             f'scale=1280:720:flags=bicubic,gblur=sigma=3,format=yuv420p')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'lavfi', '-i', graph, '-c:v', 'libx264',
                    '-preset', 'slow', '-crf', '24', '-movflags', '+faststart', out], check=True)
    print(name, os.path.getsize(out) // 1024, 'KB')


def main():
    out_dir = sys.argv[1]
    names = sys.argv[2:] or list(BACKGROUNDS)
    os.makedirs(out_dir, exist_ok=True)
    for name in names:
        render(name, BACKGROUNDS[name], out_dir)


main()
