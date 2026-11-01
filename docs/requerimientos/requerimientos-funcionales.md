# Requerimientos funcionales

Estado de cada requerimiento:

- **Hecho**: implementado y cubierto por la autoprueba o probado a mano.
- **Parcial**: funciona, con limitaciones anotadas.
- **Pendiente**: aún no existe.

Prioridad: **P1** imprescindible para un culto, **P2** muy útil, **P3** deseable.

---

## RF-01 Núcleo OBS

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-01.1 | Conservar toda la funcionalidad de OBS Studio: escenas, fuentes, filtros, transiciones, Studio Mode, mezclador de audio, grabación, transmisión, cámara virtual, plugins y websocket. | P1 | Hecho |
| RF-01.2 | Todo lo que proyectan los paneles de WorshipStudio son escenas y fuentes normales de OBS (`WS ...`), editables por el usuario. | P1 | Hecho |
| RF-01.3 | En Studio Mode el contenido va a Vista previa y pasa al aire con la transición elegida. | P1 | Hecho |
| RF-01.4 | Configuración separada de OBS (instalación paralela sin conflictos). | P1 | Hecho |

## RF-02 Himnario

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-02.1 | Buscar himnos por número o título (sin importar tildes ni mayúsculas). | P1 | Hecho |
| RF-02.2 | Elegir edición: Himnario nuevo (2009) o antiguo (1962); cada una recuerda su carpeta. | P1 | Hecho |
| RF-02.3 | Modos Cantado, Instrumental y Solo letra. | P1 | Hecho |
| RF-02.4 | Avance automático de diapositivas sincronizado con el audio. | P1 | Hecho |
| RF-02.5 | Clic en miniatura o Anterior/Siguiente mueve letra y audio juntos. | P1 | Hecho |
| RF-02.6 | Encuadre de diapositivas 4:3 en 16:9: fondo difuminado, estirar o barras negras. | P2 | Hecho |
| RF-02.7 | Aceptar carpetas simples `NNN - Titulo/` con imágenes, MP3 y `tiempos.txt` opcional. | P2 | Hecho |
| RF-02.8 | Editor de tiempos: grabar los cambios de diapositiva mientras suena el audio y guardarlos en `tiempos.txt`. | P2 | Pendiente |
| RF-02.9 | Himnos y cantos propios en texto (no solo imágenes), con estrofas y coro. | P1 | Hecho (panel Cantos, RF-05) |

## RF-03 Biblia

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-03.1 | Buscar por referencia (`Juan 3:16`, `jn 3 16`, `1 Cor 13:4-7`, `Sal 23`) o por frase. | P1 | Hecho |
| RF-03.2 | Versiones libres incluidas (RV1909, RV1865, BES, PDT, VBL, KJV) e importación de otras en JSON. | P1 | Hecho |
| RF-03.3 | Cambiar de versión conservando el pasaje (también en vivo). | P2 | Hecho |
| RF-03.4 | Diseño configurable: tipografía, fondos (degradados, color, imagen, video), pantalla completa o tercio inferior. | P1 | Hecho |
| RF-03.8 | La referencia (p. ej. "Juan 3:16") se muestra grande, y la versión debajo en letra chica y espaciada. | P1 | Hecho |
| RF-03.9 | 17 tipografías libres incluidas en la app, iguales en macOS, Windows y Linux. | P1 | Hecho |
| RF-03.10 | 18 fondos estéticos generados (mesh, aurora boreal, rayos de luz, estrellas, bokeh). | P1 | Hecho |
| RF-03.11 | Reina-Valera 1960 y otras versiones con derechos, mediante importación por parte de cada iglesia. | P1 | Parcial (importación hecha; no se puede incluir sin permiso, ver [contenido-con-derechos](../producto/contenido-con-derechos.md)) |
| RF-03.5 | Dividir pasajes largos en varias diapositivas automáticamente. | P1 | Parcial (se reduce la letra; falta paginar) |
| RF-03.6 | Mostrar dos versiones a la vez (p. ej. español e inglés) para cultos bilingües. | P2 | Pendiente |
| RF-03.7 | Historial y favoritos de pasajes. | P3 | Pendiente |

## RF-04 Presentaciones (PDF, PowerPoint, Keynote, imágenes)

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-04.1 | Abrir un PDF completo y proyectarlo página por página, sin exportar imágenes a mano. | P1 | Hecho |
| RF-04.2 | Abrir PowerPoint (`ppt`, `pptx`, `pps`, `ppsx`, `pptm`), Keynote (`key`) e Impress (`odp`). Se convierten a PDF con el programa instalado (LibreOffice, PowerPoint o Keynote) y la conversión queda en caché. | P1 | Hecho (ver limitaciones) |
| RF-04.3 | Abrir varias imágenes o una carpeta de imágenes como una presentación, en orden natural (1, 2, 10). | P2 | Hecho |
| RF-04.4 | Arrastrar archivos o carpetas al panel. | P2 | Hecho |
| RF-04.5 | Miniaturas de todas las diapositivas; clic para proyectar esa diapositiva. | P1 | Hecho |
| RF-04.6 | Anterior / Siguiente / Ocultar (pantalla sin diapositiva sin perder el lugar). | P1 | Hecho |
| RF-04.7 | Teclado y presentadores inalámbricos (clickers): flechas, Re Pág/Av Pág, espacio, B o punto para ocultar, Inicio/Fin. | P1 | Hecho |
| RF-04.8 | Atajos globales configurables en Ajustes → Atajos ("Presentacion: diapositiva siguiente/anterior", "ocultar / mostrar"); funcionan aunque OBS no tenga el foco. | P1 | Hecho |
| RF-04.9 | Encuadre igual que el Himnario: fondo difuminado, estirar o barras negras. | P1 | Hecho |
| RF-04.10 | **Modo libro**: recorta márgenes blancos y divide las páginas verticales (libros, A4) en mitad superior e inferior con solapamiento, para que el texto se lea grande. | P1 | Hecho |
| RF-04.11 | Lista de presentaciones recientes (15), con opción de quitar. | P2 | Hecho |
| RF-04.12 | La siguiente diapositiva se prepara por adelantado para que el cambio sea instantáneo. | P1 | Hecho |
| RF-04.13 | Animaciones, transiciones internas y videos incrustados de PowerPoint. | P3 | Pendiente (se proyecta la imagen final de cada diapositiva) |
| RF-04.14 | Notas del orador visibles en el monitor de escenario. | P2 | Pendiente (ver RF-07) |
| RF-04.15 | Recargar automáticamente si el archivo cambia en disco. | P3 | Parcial (la caché detecta cambios al volver a abrir) |

Limitaciones conocidas de RF-04.2:

- En macOS, Keynote está aislado (sandbox) y a veces no responde a archivos que no abrió el usuario; por eso va
  último y con tiempo límite de 90 s. LibreOffice es el conversor recomendado (gratis, silencioso).
- En Windows se usa PowerPoint mediante COM si está instalado, si no LibreOffice.
- Keynote (`.key`) solo se puede abrir en macOS con Keynote instalado.

## RF-05 Cantos y coros propios

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-05.1 | Biblioteca de cantos con título, autor, letra por secciones (Estrofa 1, Coro, Puente...), búsqueda por título o letra. | P1 | Hecho |
| RF-05.2 | Editor: pegar la letra, se divide en diapositivas por líneas en blanco, `[Coro]` nombra secciones, vista previa exacta. | P1 | Hecho |
| RF-05.2b | Fondo por canto (degradados, color, imagen, video en bucle) y fondo propio por diapositiva. | P1 | Hecho |
| RF-05.2c | "Sin texto" (solo fondo) y "Ocultar", tercio inferior sobre la cámara, título en la primera diapositiva. | P1 | Hecho |
| RF-05.3 | Teclas para saltar a una sección en vivo (C coro, P puente, E estrofa, I intro, F final, 1-9 estrofa N) y atajos globales. | P1 | Hecho |
| RF-05.4 | Temas de diseño reutilizables (tipografía, fondo, posición) compartidos con la Biblia. | P2 | Parcial (mismos fondos y tipografías; falta guardar temas con nombre) |
| RF-05.5 | Importar cantos de OpenLyrics/OpenLP, OpenSong, ProPresenter, EasyWorship, Holyrics y texto plano. | P2 | Hecho (OpenLyrics/OpenLP, OpenSong, ChordPro, texto, JSON propio; faltan ProPresenter y Holyrics) |
| RF-05.6 | Registro de uso para informes de derechos (CCLI u otras licencias), con número de licencia por canto. | P2 | Pendiente |
| RF-05.7 | Letras con acordes para los músicos, con transposición de tono. | P3 | Pendiente |

## RF-06 Planificador de servicio (orden del culto)

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-06.1 | Lista ordenada del culto con elementos de cualquier panel: himno, pasaje, presentación, canto, video, imagen, cuenta regresiva, escena de OBS. | P1 | Hecho |
| RF-06.2 | Pasar al siguiente elemento con un botón o atajo; el elemento en vivo se resalta. | P1 | Hecho |
| RF-06.3 | Guardar y abrir servicios como archivo (`.wsservice`) para prepararlos en otra computadora. | P1 | Hecho (.wsservice, autoguardado) |
| RF-06.4 | Duración estimada por elemento y hora prevista de fin. | P3 | Pendiente |
| RF-06.5 | Importar el plan desde Planning Center Services. | P3 | Pendiente |

## RF-07 Monitor de escenario (pantalla de confianza)

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-07.1 | Salida separada para el escenario con diapositiva actual, siguiente, reloj y temporizador. | P2 | Hecho (escena WS Escenario, proyector en cualquier pantalla) |
| RF-07.2 | Notas del predicador y mensajes del operador ("2 minutos", "Alargar"). | P2 | Hecho (mensajes del operador y cronometro) |
| RF-07.3 | Diseños de monitor de escenario configurables. | P3 | Pendiente |

## RF-08 Temporizadores, anuncios y superposiciones

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-08.1 | Cuenta regresiva hasta una hora ("El culto comienza en 04:59") o por duración. | P1 | Hecho |
| RF-08.2 | Bucle de anuncios antes del culto (imágenes/videos con duración por elemento). | P2 | Pendiente |
| RF-08.3 | Rótulos de nombre (tercio inferior) para predicador, cantantes, entrevistados. | P1 | Hecho |
| RF-08.4 | Mensaje urgente superpuesto (p. ej. "Padres del niño X, acercarse a la recepción"). | P2 | Hecho |
| RF-08.5 | Código QR para ofrendas, inscripciones o enlaces. | P3 | Pendiente |

## RF-09 Multimedia

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-09.1 | Biblioteca multimedia unificada: imágenes, videos y audios con miniaturas y búsqueda. | P2 | Pendiente |
| RF-09.2 | Reproducción de video con controles (pausa, posición, bucle, volumen) desde el panel. | P1 | Pendiente (hoy con fuentes de OBS) |
| RF-09.3 | Lista de música de fondo para antes/después del culto con fundido. | P3 | Pendiente |
| RF-09.4 | Fondos en movimiento para letras y versículos. | P2 | Parcial (Biblia admite video) |

## RF-10 Control remoto y automatización

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-10.1 | Control desde el celular o tablet en la red local (siguiente/anterior, elegir himno o pasaje) sin instalar nada (página web). | P2 | Hecho (pagina web con PIN y codigo QR, orden del culto incluido) |
| RF-10.2 | Compatibilidad con Stream Deck, MIDI y presentadores. | P2 | Parcial (atajos de OBS; Stream Deck vía websocket) |
| RF-10.3 | Macros: una acción ejecuta varias (cambiar escena + mostrar rótulo + iniciar grabación). | P3 | Pendiente |
| RF-10.4 | Salida NDI para otras computadoras o mezcladores. | P3 | Pendiente (plugin de terceros disponible) |

## RF-11 Accesibilidad e idiomas

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-11.1 | Subtítulos en vivo de la predicación (voz a texto local). | P3 | Pendiente |
| RF-11.2 | Letras en dos idiomas en la misma diapositiva. | P2 | Pendiente |
| RF-11.3 | Interfaz traducible; español por defecto si el sistema lo tiene. | P2 | Hecho (75 idiomas heredados de OBS) |

## RF-12 Pruebas

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-12.1 | Autoprueba que use la salida real de OBS y guarde capturas y `reporte.txt`. | P1 | Hecho |
| RF-12.2 | Autoprueba de presentaciones (`WORSHIPSTUDIO_SELFTEST_PRESENTATIONS`, `WORSHIPSTUDIO_SELFTEST_ONLY=presentaciones`). | P1 | Hecho |
| RF-12.3 | Compilación automática en macOS, Windows y Linux en cada cambio (GitHub Actions). | P1 | Parcial (flujo escrito, falta ejecutarlo en GitHub) |

## RF-13 Interfaz y experiencia

| ID | Requerimiento | Prioridad | Estado |
|----|---------------|-----------|--------|
| RF-13.1 | Modo Culto (solo contenido y vista previa) y Modo Técnico (todo OBS), con un botón. | P1 | Hecho |
| RF-13.2 | Guía de primera vez (5 pasos con diagramas, conecta el proyector y activa EN ESPERA). | P1 | Hecho |
| RF-13.6 | Un solo Control en vivo (EN PANTALLA / EN ESPERA, Pasar a pantalla, Anterior, Siguiente, Ocultar, proyector) y atajos globales. | P1 | Hecho |
| RF-13.7 | En Modo Estudio lo nuevo espera en la vista previa sin tocar la pantalla (escenas A/B por panel); el himno en espera no suena. | P1 | Hecho |
| RF-13.3 | Marca propia: nada visible dice "OBS" salvo el crédito en Acerca de. | P1 | Parcial (textos y título hechos; faltan icono y logo) |
| RF-13.4 | Temas propios WorshipStudio Noche (predeterminado) y Día. | P1 | Hecho |
| RF-13.5 | Actualizador propio (el heredado de OBS está desactivado). | P2 | Pendiente |
