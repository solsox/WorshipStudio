# Dirección de diseño

Referente: agencias como [Clay](https://clay.global), que diseñan "experiencias de consumo para usuarios
profesionales": producto complejo por dentro, simple por fuera, sostenido por un sistema de diseño.

## Principios

1. **Lo del culto primero.** Himnario, Cantos, Biblia y Presentaciones están siempre a la vista y tienen una sola
   acción principal (botón índigo "Proyectar"). Lo técnico de OBS (mezclador, transiciones, fuentes) queda
   disponible pero no compite.
2. **Un voluntario nuevo lo entiende en 5 minutos.** Textos en español claro, sin jerga ("Ocultar", "Sin texto",
   "Modo libro"), teclas que se recuerdan (C coro, P puente, B ocultar).
3. **Lo que ves es lo que sale.** Las vistas previas usan el mismo dibujo que la pantalla.
4. **Calma en la cabina.** Tema oscuro de poco brillo, un solo color de acento, bordes discretos, nada parpadea.
5. **Profundidad para el técnico.** Todo sigue siendo OBS: escenas `WS ...` editables, filtros, atajos globales,
   websocket.

## Sistema de diseño (tokens)

| Token | Noche (predeterminado) | Día |
|-------|------------------------|-----|
| Acento | Índigo `#636BE6` (hover `#8189F5`) | Índigo `#4E55C7` |
| Fondo ventana | `#16181F` | claro (Yami Light) |
| Superficies | `#1D2029` / `#252836` / `#2E3241` | claro |
| Texto secundario | `#9AA0B4` | gris |
| Radio | 8 px (6 px chico, 12 px grande) | igual |

Pantalla proyectada:

- Acento dorado `#E8C46A` para referencias bíblicas.
- Tipografías incluidas (OFL): Montserrat (predeterminada), Inter, Poppins, Raleway, Nunito, Work Sans, Lato,
  Josefin Sans, Oswald, Bebas Neue, Playfair Display, Lora, Merriweather, Source Serif 4, EB Garamond,
  Cormorant Garamond, DM Serif Display.
- 18 fondos generados: degradados, mesh, aurora boreal, rayos de luz, noche estrellada, bokeh.

Temas: `app/frontend/data/themes/WorshipStudio_Noche.ovt` y `WorshipStudio_Dia.ovt`.

## Próximos pasos de interfaz

1. **Modo Culto / Modo Técnico** (RF-13.1): un botón en la barra superior. El Modo Culto muestra solo la vista
   previa, el programa y los paneles de contenido en columna. El Modo Técnico muestra todo OBS.
2. **Pantalla de bienvenida** (RF-13.2): elegir pantalla del proyector, carpeta del himnario y tema en tres pasos.
3. **Iconos propios** para los paneles y la barra (hoy se usan textos).
4. **Panel "En vivo"** fijo que muestre qué está al aire y la siguiente diapositiva, venga del panel que venga.
5. **Icono y logo de WorshipStudio** (hoy se hereda el de OBS en algunos lugares).
