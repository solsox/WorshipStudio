# Seguimiento de la evolución del producto

Auditoría del 26 de noviembre de 2026 (versión 0.9.0) frente al documento maestro de evolución y su anexo de
diseño. Estados: EXISTENTE Y FUNCIONAL · EXISTENTE, REQUIERE MEJORA · PARCIALMENTE IMPLEMENTADA · NO IMPLEMENTADA ·
EN DESARROLLO · IMPLEMENTADA Y VERIFICADA · BLOQUEADA POR DEPENDENCIAS.

## Arquitectura actual (lo que se reutiliza)

- **Framework:** fork de OBS Studio en C++/Qt 6. Paneles = `OBSDock` en `app/frontend/docks/`.
- **Salida:** todo se proyecta con escenas `WS ...` normales de OBS (`WorshipOutput`); las escenas "… en vivo"
  permiten usar cada panel como fuente dentro de cualquier escena.
- **Estado en vivo compartido:** `WorshipLive` (qué está en pantalla, siguiente/anterior, celular, monitor).
- **Almacenamiento:** archivos JSON por canto (`SongLibrary`), `.wsservice` para el orden del culto, Biblias JSON,
  configuración en `user.ini` (`WorshipOutput::ConfigString`).
- **Dibujo de diapositivas:** `WorshipRender` (letras, Biblia, cuenta regresiva, rótulos) a imágenes PNG.
- **Temas:** `data/themes/WorshipStudio_Noche.ovt` y `_Dia.ovt`; el personalizado se genera en
  `OBSApp_Themes.cpp` a partir de ellos.
- **Pruebas:** autoprueba integrada (`WORSHIPSTUDIO_SELFTEST`, `docks/WorshipSelfTest.cpp`) con capturas reales.
- **Web:** `sitio/` publicado en la rama `gh-pages`.

## Estado por módulo

| # | Módulo | Estado inicial | Qué hay | Qué falta |
|---|---|---|---|---|
| 1 | Auditoría y estabilización | EN DESARROLLO | Arrastrar a Fuentes, flechas, cámara en Mac y compilación de Linux corregidos en 0.9 | Compilación de Windows y Linux en verde |
| 2 | Worship Planner (Orden del culto) | IMPLEMENTADA Y VERIFICADA (fase 1, 0.9.1) — antes: PARCIALMENTE IMPLEMENTADA | `ServiceDock`: nuevo, abrir, guardar `.wsservice`, arrastrar para ordenar, + Agregar (himno, canto, pasaje, presentación, cuenta regresiva, rótulo, anuncio, escena), ir al elemento, siguiente, celular | Duración por elemento, tiempo total y restante, responsables, notas internas, duplicar, plantillas, guardado automático, tipos video/prédica/oración/personalizado |
| 3 | Worship AI | NO IMPLEMENTADA | — | Todo. Opcional, clave guardada en el llavero del sistema, nunca envía a pantalla |
| 4 | Sermon Studio | NO IMPLEMENTADA | Presentaciones (PDF/PowerPoint) y Biblia reutilizables | Editor de bosquejos, exportar a presentación |
| 5 | Worship Charts (acordes) | PARCIALMENTE IMPLEMENTADA | Importa ChordPro y OpenSong pero descarta los acordes | Guardar acordes, tonalidad, transponer, cifrado latino, PDF de partituras |
| 6 | Modo ensayo | NO IMPLEMENTADA | Monitor de escenario reutilizable | Todo |
| 7 | Producción avanzada | PARCIALMENTE IMPLEMENTADA | Todo OBS (escenas, filtros, websocket → Stream Deck), rótulos animados, mensaje urgente, cuenta regresiva, cronómetro, fondos animados, proyector y transmisión por separado, fuentes "… en vivo" y "Gráficos en vivo" | NDI (plugin externo), MIDI, títulos de transmisión |
| 8 | Biblioteca multimedia | PARCIALMENTE IMPLEMENTADA | Mis fondos, fondos animados incluidos, búsqueda de fondos en internet (Pexels) | Colecciones, favoritos, plantillas, filtros por formato |
| 9 | Equipos | NO IMPLEMENTADA | — | Todo (local primero) |
| 10 | Subtítulos y traducción | NO IMPLEMENTADA | — | Todo; requiere motor de voz local |
| 11 | Remote | EXISTENTE Y FUNCIONAL | Control desde el celular y la tablet con PIN y QR (`WorshipRemote`) | Elegir elementos del orden del culto desde el celular |
| 12 | Church Connect | NO IMPLEMENTADA | El servidor del celular se puede ampliar | Futuro |
| 13 | Marketplace | NO IMPLEMENTADA | — | Futuro |
| 14 | API y plugins | EXISTENTE (de OBS) | Plugins y websocket de OBS | Documentar para WorshipStudio |

### Diseño

| Punto | Estado | Notas |
|---|---|---|
| Tokens de color, radios y transparencias | IMPLEMENTADA Y VERIFICADA (0.9.1) | Capa "vidrio" en Noche y Día, resplandor del color principal (`--ws_glow`), radios 12/18 px; capturas de la autoprueba de apariencia |
| Desenfoque real del fondo | BLOQUEADA | Qt no difumina lo que hay detrás de un panel; en macOS se podría con `NSVisualEffectView`, pero las vistas de video de OBS son ventanas nativas: riesgo de parpadeo y consumo |
| Secciones plegables en En vivo | IMPLEMENTADA Y VERIFICADA (0.9.1) | Encabezado propio (`FoldHeader`): flecha fina animada, divisoria, etiqueta verde "En pantalla"; recuerdan su estado |
| Paneles que no arrancan flotando | IMPLEMENTADA (0.9.1) | `WorshipLayout::restore` vuelve a acomodar la columna si un panel de WorshipStudio quedó suelto |
| Espacios de trabajo | PARCIALMENTE IMPLEMENTADA | OBS guarda distribuciones de paneles; falta elegir "Sencillo / Producción / Predicación / Ensayo" |

### Web

| Punto | Estado |
|---|---|
| Español neutro (tú) | IMPLEMENTADA (0.9.1) |
| Comunidad, Roadmap, Desarrolladores | NO IMPLEMENTADA |
| Demostraciones con capturas reales | PARCIALMENTE (las guías ya usan capturas de la autoprueba) |

## Riesgos

- Cambiar los textos fuente de la app (`tr()`) rompe las traducciones a otros idiomas, que usan el español como
  clave: hay que actualizarlas a la vez.
- El modelo de canto (`Song`) y `.wsservice` se guardan en JSON: los campos nuevos deben ser opcionales para que
  los archivos anteriores sigan abriéndose.
- La IA y cualquier servicio en la nube deben ser opcionales y no bloquear el modo en vivo.

## Fase 1 — Worship Planner (hecha, 0.9.1)

- **Archivos:** `docks/ServiceDock.{hpp,cpp}`, accesos de selección en `SongsDock.hpp`, `BibleDock.hpp`,
  `PresentationCatalogDock.hpp`, `PresentationDock.{hpp,cpp}`, prueba en `WorshipSelfTest.cpp`.
- **Hecho:** duración, responsable y notas por elemento (Detalles… y clic derecho); tiempo total, restante y hora
  estimada de fin; duplicar elemento y reunión; guardar como plantilla y nueva desde plantilla (incluye "Culto de
  domingo"); tipos Video, Prédica (muestra el rótulo del predicador), Oración y Otro momento; arrastrar cantos,
  versículos, himnos y presentaciones desde sus paneles; guardado automático (ya existía).
- **Compatibilidad:** los campos nuevos son opcionales en `.wsservice`; un archivo anterior abre con la duración
  habitual de cada tipo.
- **Pruebas:** autoprueba "envivo" con 4 comprobaciones nuevas del planificador, 0 fallos. La autoprueba ya no
  escribe en el orden del culto del usuario.
- **Pendiente:** miniaturas por elemento, abrir Sermon Studio desde una Prédica (fase 4), que el Video se elija
  desde una biblioteca (fase 8).

## Próximo paso

**Fase 2: Worship AI** en Herramientas → Asistente IA, con acceso rápido desde el Orden del culto.

1. Ajustes del asistente: proveedor y clave guardada en el llavero del sistema (Keychain en Mac, Credential
   Manager en Windows, Secret Service en Linux); sin clave, el menú explica cómo activarlo y nada más cambia.
2. Ventana del asistente: el usuario escribe un tema ("una reunión sobre el perdón") y recibe pasajes, cantos de
   su propia biblioteca, una estructura de reunión y textos; cada sugerencia se revisa y se acepta una por una.
3. "Agregar al orden del culto" crea elementos con `ServiceDock::insertItem`; nunca proyecta nada.
4. Los pasajes sugeridos se validan con la Biblia local antes de ofrecerlos.
5. Autoprueba sin red: respuestas simuladas del proveedor.
