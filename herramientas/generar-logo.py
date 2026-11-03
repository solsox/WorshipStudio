#!/usr/bin/env python3
"""Logo de WorshipStudio: un ventanal en arco lleno de luz, con la cruz en el
espacio negativo, sobre negro tinta. Genera el SVG de la web y todos los
íconos de la app (macOS, Windows, Linux, bandeja) con Chrome sin ventana.

Uso: python3 herramientas/generar-logo.py
"""
import io
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(RAIZ, "app")
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

DEFS = """
  <defs>
    <linearGradient id="fondo" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#2B2622"/><stop offset="1" stop-color="#0D0C0B"/>
    </linearGradient>
    <linearGradient id="borde" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#FFFFFF" stop-opacity=".16"/>
      <stop offset=".5" stop-color="#FFFFFF" stop-opacity=".03"/>
      <stop offset="1" stop-color="#FFFFFF" stop-opacity=".06"/>
    </linearGradient>
    <linearGradient id="luz" x1="0" y1="1" x2="0" y2="0">
      <stop offset="0" stop-color="#FFE3B0"/><stop offset=".28" stop-color="#FFA764"/>
      <stop offset=".62" stop-color="#D9532E"/><stop offset="1" stop-color="#4A2140"/>
    </linearGradient>
    <radialGradient id="sol" cx=".5" cy="1" r=".8">
      <stop offset="0" stop-color="#FFF4DC" stop-opacity=".95"/>
      <stop offset=".55" stop-color="#FFC98A" stop-opacity=".25"/>
      <stop offset="1" stop-color="#FFC98A" stop-opacity="0"/>
    </radialGradient>
    <radialGradient id="halo" cx=".5" cy=".6" r=".5">
      <stop offset="0" stop-color="#F0703A" stop-opacity=".40"/>
      <stop offset="1" stop-color="#F0703A" stop-opacity="0"/>
    </radialGradient>
    <clipPath id="arco"><path d="M318 830V478a194 194 0 0 1 388 0v352z"/></clipPath>
    <clipPath id="cuadro"><rect width="1024" height="1024" rx="230"/></clipPath>
  </defs>"""

CUERPO = """
  <g clip-path="url(#cuadro)">
    <rect width="1024" height="1024" fill="url(#fondo)"/>
    <ellipse cx="512" cy="610" rx="420" ry="330" fill="url(#halo)"/>
  </g>
  <rect x="3" y="3" width="1018" height="1018" rx="227" fill="none" stroke="url(#borde)" stroke-width="6"/>
  <path d="M276 850V478a236 236 0 0 1 472 0v372" fill="none" stroke="#E6DED0" stroke-opacity=".24" stroke-width="10" stroke-linecap="round"/>
  <g clip-path="url(#arco)">
    <rect x="300" y="260" width="424" height="590" fill="url(#luz)"/>
    <rect x="300" y="260" width="424" height="590" fill="url(#sol)"/>
    <rect x="501" y="260" width="22" height="590" fill="#14110F"/>
    <rect x="300" y="476" width="424" height="22" fill="#14110F"/>
  </g>
  <rect x="236" y="828" width="552" height="10" rx="5" fill="#E6DED0" fill-opacity=".24"/>"""


def logo(extra=""):
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" role="img" '
            'aria-label="WorshipStudio">' + DEFS + CUERPO + extra + "\n</svg>\n")


def logo_mac():
    """macOS: el cuadro ocupa 824 de 1024 y lleva sombra, como los íconos del sistema."""
    interior = DEFS + CUERPO
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024">'
            '<defs><filter id="sombra" x="-20%" y="-20%" width="140%" height="140%">'
            '<feDropShadow dx="0" dy="10" stdDeviation="14" flood-opacity=".35"/></filter></defs>'
            '<g filter="url(#sombra)"><svg x="100" y="100" width="824" height="824" viewBox="0 0 1024 1024">'
            + interior + "</svg></g></svg>\n")


def mono(paused=False):
    """Bandeja de macOS: silueta negra (el sistema la tiñe)."""
    if paused:
        return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 18 18" width="18" height="18">'
                '<path fill="none" stroke="#000" stroke-width="1.4" d="M4.2 15.8V8a4.8 4.8 0 0 1 9.6 0v7.8z"/>'
                '<path fill="#000" d="M7 7.5h1.4v5.5H7zm2.6 0H11v5.5H9.6z"/></svg>\n')
    # Ventanal con contorno y cruz: lleno parecería una lápida a este tamaño.
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 18 18" width="18" height="18">'
            '<path fill="none" stroke="#000" stroke-width="1.4" d="M4.2 15.8V8a4.8 4.8 0 0 1 9.6 0v7.8z"/>'
            '<path fill="#000" d="M8.4 3.6h1.2v12.2H8.4zM4.2 8.1h9.6v1.2H4.2z"/></svg>\n')


def render(svg, size, tmp):
    """PNG transparente del SVG al tamaño pedido."""
    svg_path = os.path.join(tmp, "a.svg")
    html_path = os.path.join(tmp, "a.html")
    png_path = os.path.join(tmp, "a.png")
    with open(svg_path, "w") as f:
        f.write(svg)
    with open(html_path, "w") as f:
        f.write('<html><body style="margin:0;background:transparent">'
                f'<img src="a.svg" width="{size}" height="{size}" style="display:block"></body></html>')
    subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                    "--default-background-color=00000000", f"--window-size={max(size, 500)},{max(size, 500)}",
                    "--force-device-scale-factor=1", f"--screenshot={png_path}", "file://" + html_path],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return Image.open(png_path).convert("RGBA").crop((0, 0, size, size))


def main():
    if not os.path.exists(CHROME):
        sys.exit("Hace falta Google Chrome para dibujar los íconos.")
    tmp = tempfile.mkdtemp()
    plano = logo()
    mac = logo_mac()
    grande = render(plano, 1024, tmp)
    grande_mac = render(mac, 1024, tmp)

    def tam(img, s):
        return img.resize((s, s), Image.LANCZOS)

    # Web
    with open(os.path.join(RAIZ, "sitio", "logo.svg"), "w") as f:
        f.write(plano)
    tam(grande, 180).save(os.path.join(RAIZ, "sitio", "img", "icono-180.png"))
    tam(grande, 32).save(os.path.join(RAIZ, "sitio", "img", "icono-32.png"))

    # Ventana y bandeja
    imagenes = os.path.join(APP, "frontend", "forms", "images")
    tam(grande, 256).save(os.path.join(imagenes, "obs.png"))
    pausa = tam(grande, 256).copy()
    pausa.putalpha(pausa.getchannel("A").point(lambda a: a * 55 // 100))
    pausa.save(os.path.join(imagenes, "obs_paused.png"))
    for nombre, svg in (("obs_macos", mono()), ("obs_paused_macos", mono(True))):
        with open(os.path.join(imagenes, nombre + ".svg"), "w") as f:
            f.write(svg)
        render(svg.replace('width="18" height="18"', 'width="36" height="36"'), 36, tmp).save(
            os.path.join(imagenes, nombre + ".png"))

    # macOS: Assets.xcassets y AppIcon.icns
    iconset_dir = os.path.join(APP, "frontend", "cmake", "macos", "Assets.xcassets", "AppIcon.appiconset")
    iconset = os.path.join(tmp, "AppIcon.iconset")
    os.makedirs(iconset)
    for base in (16, 32, 128, 256, 512):
        for escala in (1, 2):
            nombre = f"icon_{base}x{base}{'@2x' if escala == 2 else ''}.png"
            img = tam(grande_mac, base * escala)
            img.save(os.path.join(iconset, nombre))
            if os.path.exists(os.path.join(iconset_dir, nombre)):
                img.save(os.path.join(iconset_dir, nombre))
    subprocess.run(["iconutil", "-c", "icns", iconset, "-o",
                    os.path.join(APP, "cmake", "macos", "resources", "AppIcon.icns")], check=True)

    # Windows
    for ico in (os.path.join(APP, "frontend", "cmake", "windows", "obs-studio.ico"),
                os.path.join(APP, "cmake", "bundle", "windows", "obs-studio.ico")):
        tam(grande, 256).save(ico, sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])

    # Linux
    linux = os.path.join(APP, "frontend", "cmake", "linux", "icons")
    for s in (128, 256, 512):
        tam(grande, s).save(os.path.join(linux, f"obs-logo-{s}.png"))
    with open(os.path.join(linux, "obs-logo-scalable.svg"), "w") as f:
        f.write(plano)

    shutil.rmtree(tmp)
    print("Logo listo: web, macOS, Windows, Linux y bandeja.")


if __name__ == "__main__":
    main()
