# WorshipStudio

Programa libre y gratuito para proyectar y transmitir cultos, conferencias y eventos.
Une en una sola aplicación lo que hoy hacen por separado Holyrics/ProPresenter (letras, Biblia, presentaciones)
y OBS Studio (cámaras, escenas, transmisión y grabación).

Funciona en **macOS, Windows y Linux**. Licencia **GPL-2.0** (fork de [OBS Studio](https://obsproject.com)).

## Qué hace

**Himnario**
- Himnario nuevo (2009) y antiguo (1962): búsqueda por número o título.
- Modos Cantado, Instrumental y Solo letra, con avance automático sincronizado con el audio.
- Acepta la carpeta del programa Himnario Adventista o carpetas simples `NNN - Titulo/` con imágenes y MP3.

**Cantos y coros**
- Biblioteca de cantos propios con buscador por título o letra.
- Editor: pegás la letra, cada párrafo es una diapositiva, `[Coro]` nombra secciones, vista previa exacta.
- Fondo por canto o por diapositiva (degradados, color, imagen, video en bucle), tercio inferior sobre la cámara.
- En vivo: C coro, P puente, 1-9 estrofa, L sin texto, B ocultar, atajos globales.

**Biblia**
- Seis versiones libres incluidas (RV1909, RV1865, BES, PDT, VBL, KJV) e importación de otras (p. ej. RV1960
  si la iglesia tiene permiso).
- Búsqueda por referencia (`Juan 3:16`, `jn 3 16`, `1 Cor 13:4-7`) o por frase.
- Cita grande con la versión debajo, 17 tipografías incluidas, 18 fondos (aurora boreal, rayos de luz,
  estrellas, bokeh...), imagen o video de fondo, pantalla completa o tercio inferior.

**Presentaciones**
- PDF completo página por página, sin exportar imágenes a mano.
- PowerPoint (`ppt`, `pptx`, `pps`, `ppsx`), Keynote e Impress: se convierten solos con LibreOffice, PowerPoint o
  Keynote (lo que esté instalado).
- Imágenes o carpetas de imágenes como presentación. Arrastrar y soltar.
- **Modo libro**: recorta márgenes y divide páginas verticales en dos mitades para leer libros en pantalla.
- Clickers, flechas, Re Pág/Av Pág, tecla B para ocultar y atajos globales.

**En vivo**
- **EN ESPERA / EN PANTALLA**: lo que elegís espera en la vista previa y pasa a la pantalla con un botón.
- **Personalizar apariencia**: modo claro u oscuro y color principal.
- **Orden del culto**: cuenta regresiva, himnos, cantos, pasajes, presentaciones, rótulos, anuncios y escenas,
  preparado antes y pasado con un botón. Se guarda como archivo `.wsservice`.
- **Control desde el celular** por la red local (página web con PIN y código QR, sin instalar nada).
- **Monitor de escenario**: diapositiva actual, siguiente, reloj, cronómetro y mensajes al escenario.
- **Cuenta regresiva**, **rótulos de nombre** y **mensajes urgentes** sobre lo que esté al aire.
- Importa cantos de OpenLP (OpenLyrics), OpenSong y ChordPro.

**Todo OBS Studio**: escenas, cámaras, filtros, transiciones, Studio Mode, grabación, transmisión, cámara
virtual, plugins y websocket. Lo que proyectan los paneles son escenas `WS ...` normales y editables.

## Estructura del repositorio

```
app/              Código de la aplicación (fork de OBS Studio)
  frontend/docks/ Paneles de WorshipStudio (Himnario, Cantos, Biblia, Presentaciones, Orden, En vivo)
  frontend/data/worship/  Biblias y tipografías libres incluidas
  tools/worship/  Herramientas (conversión de Biblias)
docs/             Documentación: requerimientos, arquitectura, roadmap, cómo compilar
sitio/            Página web pública (GitHub Pages)
.github/          Compilación automática para macOS, Windows y Linux
contenido-local/  (no se sube) himnarios y material con derechos de autor de cada iglesia
```

## Compilar

Ver [docs/desarrollo/compilar.md](docs/desarrollo/compilar.md). Resumen para macOS:

```sh
cd app
cmake --preset macos -B ~/WorshipStudio-build -DCMAKE_OSX_ARCHITECTURES=arm64 -DOBS_CODESIGN_IDENTITY=- \
  -DENABLE_BROWSER=ON -DENABLE_WHATSNEW=OFF -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
cmake --build ~/WorshipStudio-build --config RelWithDebInfo
```

## Contenido con derechos de autor

El repositorio solo incluye contenido libre. Los himnarios y las Biblias con derechos (por ejemplo
Reina-Valera 1960) no se distribuyen: cada iglesia los carga desde sus propias carpetas
(`contenido-local/`, `Documentos/WorshipStudio/` o el botón `Carpeta...` / `Importar...`).

## Documentación

- [Requerimientos funcionales](docs/requerimientos/requerimientos-funcionales.md)
- [Requerimientos no funcionales](docs/requerimientos/requerimientos-no-funcionales.md)
- [Arquitectura](docs/arquitectura/arquitectura.md)
- [Roadmap](docs/producto/roadmap.md)

## Contribuir

Ver [CONTRIBUTING.md](CONTRIBUTING.md).
