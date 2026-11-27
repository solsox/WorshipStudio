# WorshipStudio — guía para Claude Code

Fork de OBS Studio (C++/Qt 6) para proyectar y transmitir cultos. Código en `app/`, web en `sitio/`
(publicada en la rama `gh-pages`), documentación en `docs/`.

## Dónde seguir

**Plan maestro:** el estado de cada módulo y el próximo paso están en
[`docs/producto/seguimiento.md`](docs/producto/seguimiento.md). Leerlo antes de empezar.

- Fase 1 (Worship Planner sobre el panel Orden del culto): hecha y publicada.
- **Siguiente: Fase 2, Asistente IA** (Herramientas → Asistente IA). Opcional; la clave del proveedor va en el
  llavero del sistema, nunca en texto plano; nunca envía nada a pantalla ni cambia el orden del culto sin que el
  usuario lo acepte.
- Después: Sermon Studio, acordes (Worship Charts), modo ensayo, producción avanzada, biblioteca multimedia,
  equipos, subtítulos (orden del documento maestro, sección 6).
- El cambio de nombre de la app quedó para después (candidatos: Lumbre, Candela, Emaús; Selah está tomado).

## Cómo trabaja el usuario

- **Por fases:** terminar una, mostrarla y preguntar antes de seguir con la próxima.
- **Mostrar sin commit:** compilar y abrir una copia de prueba aparte, sin tocar la instalada:
  `open -n -a ~/WorshipStudio-build/frontend/RelWithDebInfo/WorshipStudio.app --args --multi --portable`
  Hacer commit, push o publicar una versión solo cuando lo pida.
- **Español neutro con "tú"** en la web y en los textos nuevos (no voseo). Los textos viejos de la app todavía
  tienen voseo; cambiarlos exige actualizar las traducciones, que usan el español como clave.
- Responder en español.

## Compilar y probar

```sh
cmake --build ~/WorshipStudio-build --config RelWithDebInfo
# Autoprueba en una copia aparte (portátil) desde la raíz del repo, así encuentra contenido-local/:
WORSHIPSTUDIO_SELFTEST=/ruta/salida [WORSHIPSTUDIO_SELFTEST_ONLY=envivo,interfaz,guia,himnario,...] \
  ~/WorshipStudio-build/frontend/RelWithDebInfo/WorshipStudio.app/Contents/MacOS/WorshipStudio --multi --portable
```

- Sin `--multi` la autoprueba se traba si el usuario tiene la app abierta.
- `final: ultima diapositiva` del himno 1 falla a veces por tiempos; repetir solo `himnario` lo confirma.
- Reinstalar de cero: `herramientas/instalar-mac-desde-cero.sh` (borra la configuración; la app debe estar cerrada).
- Publicar: push a `main`, `gh release create vX.Y.Z` (GitHub compila y sube Mac, Windows y Linux) y la web con
  `git commit-tree HEAD:sitio -p origin/gh-pages` empujado a `gh-pages`. En zsh usar `"${P}:refs/heads/gh-pages"`.
- Linux compila con Qt 6.4 (Ubuntu 24.04): cuidado con APIs de Qt 6.5 o posteriores.

## Piezas clave

- `docks/WorshipOutput`: escenas `WS ...`, escenas "… en vivo" para usar paneles como fuente, `ShowScene`.
- `docks/WorshipLive`: qué está en pantalla, siguiente/anterior, flechas globales.
- `docks/WorshipDrag`: arrastrar desde los paneles a Fuentes y al Orden del culto.
- `docks/ServiceDock`: Orden del culto / Worship Planner (`.wsservice`, campos nuevos siempre opcionales).
- `docks/LiveToolsDock`: En vivo (secciones plegables, cuenta regresiva, rótulos, monitor, celular).
- Temas: `data/themes/WorshipStudio_Noche.ovt` y `_Dia.ovt` (capa de vidrio al final); el personalizado se arma en
  `OBSApp_Themes.cpp`.
