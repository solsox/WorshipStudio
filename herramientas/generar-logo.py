#!/usr/bin/env python3
"""Logo de WorshipStudio: un ventanal en arco lleno de luz, con la cruz, sobre
una Biblia abierta y negro tinta. Parte de herramientas/logo/logo-1024.png
(fondo transparente) y genera los íconos de la web y de la app (macOS,
Windows, Linux) y la silueta de la bandeja, dibujada con Chrome sin ventana.

Uso: python3 herramientas/generar-logo.py
"""
import io
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageFilter

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(RAIZ, "app")
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

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
    grande = Image.open(os.path.join(RAIZ, "herramientas", "logo", "logo-1024.png")).convert("RGBA")
    # macOS: el cuadro ocupa 824 de 1024 y lleva una sombra suave, como los del sistema.
    grande_mac = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    sombra = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    cuadro = grande.resize((824, 824), Image.LANCZOS)
    negro = Image.new("RGBA", (824, 824), (0, 0, 0, 110))
    negro.putalpha(cuadro.getchannel("A").point(lambda a: a * 110 // 255))
    sombra.alpha_composite(negro, (100, 112))
    grande_mac.alpha_composite(sombra.filter(ImageFilter.GaussianBlur(14)))
    grande_mac.alpha_composite(cuadro, (100, 100))

    def tam(img, s):
        return img.resize((s, s), Image.LANCZOS)

    # Web
    tam(grande, 512).save(os.path.join(RAIZ, "sitio", "logo.png"))
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
    # El "scalable" de Linux lleva la imagen grande dentro de un SVG.
    import base64
    datos = io.BytesIO()
    tam(grande, 512).save(datos, "PNG")
    with open(os.path.join(linux, "obs-logo-scalable.svg"), "w") as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
                'viewBox="0 0 512 512"><image width="512" height="512" xlink:href="data:image/png;base64,'
                + base64.b64encode(datos.getvalue()).decode() + '"/></svg>\n')

    shutil.rmtree(tmp)
    print("Logo listo: web, macOS, Windows, Linux y bandeja.")


if __name__ == "__main__":
    main()
