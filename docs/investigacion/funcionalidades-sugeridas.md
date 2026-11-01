# Funcionalidades sugeridas para cultos y eventos

Revisión de lo que ofrecen los programas de presentación para iglesias más usados (ProPresenter, EasyWorship,
FreeShow, Holyrics, OpenLP) y de lo que suele hacer falta en un culto, una conferencia o un evento. Cada punto
indica su requerimiento en [requerimientos-funcionales.md](../requerimientos/requerimientos-funcionales.md).

## Lo que ya cubre WorshipStudio y casi nadie más tiene

- Todo es OBS: transmisión, grabación, cámaras, filtros y escenas en la misma aplicación que proyecta las letras.
  Los demás programas necesitan OBS o vMix aparte para transmitir.
- Himnario Adventista con audio sincronizado.
- PDF y libros legibles en pantalla (modo libro).

## Imprescindibles que faltan (P1)

1. **Cantos propios en texto** (RF-05): la mayoría de las iglesias canta música que no está en el himnario. Pegar
   la letra, dividirla sola en diapositivas y saltar al coro con una tecla es la función más usada en todos los
   programas de la competencia.
2. **Orden del culto / planificador** (RF-06): preparar el servicio antes y pasarlo con un solo botón. Reduce
   errores del operador, que en muchas iglesias es un voluntario distinto cada semana.
3. **Cuenta regresiva** (RF-08.1): "El culto comienza en 5:00" antes de empezar y en la transmisión.
4. **Rótulos de nombre** (RF-08.3): nombre del predicador o del cantante en el tercio inferior.
5. **Paginación automática de pasajes largos** (RF-03.5).

## Muy útiles (P2)

- **Monitor de escenario** (RF-07): el que canta o predica ve la diapositiva actual, la siguiente, el reloj y
  notas. ProPresenter y FreeShow lo tienen y es de lo más pedido por los equipos de alabanza.
- **Control desde el celular** (RF-10.1): el predicador pasa sus diapositivas o el operador maneja todo desde la
  tablet. OBS ya trae websocket; falta una página web simple servida por la app.
- **Bucle de anuncios** antes del culto (RF-08.2).
- **Mensaje urgente** sobre lo que esté al aire (RF-08.4): niños perdidos, autos mal estacionados.
- **Biblia en dos versiones** y **letras bilingües** (RF-03.6, RF-11.2) para iglesias con visitantes de otro idioma.
- **Importar cantos** de otros programas (RF-05.5): baja la barrera para que una iglesia se cambie.
- **Registro de uso de cantos** para informes de licencias (RF-05.6).
- **Editor de tiempos del himnario** (RF-02.8) para himnos sin tiempos y para el himnario antiguo.

## Deseables (P3)

- Subtítulos automáticos de la predicación, útiles para personas sordas y para la transmisión (RF-11.1).
- Acordes y transposición para músicos (RF-05.7).
- Macros (RF-10.3), NDI (RF-10.4), MIDI para luces.
- Integración con Planning Center (RF-06.5).
- Código QR para ofrendas o inscripciones (RF-08.5).
- Música de fondo con fundido (RF-09.3).

## Eventos fuera del culto

| Evento | Qué necesita | Cubierto por |
|--------|--------------|--------------|
| Conferencia / seminario | PDF y PowerPoint del orador, clicker, temporizador para el orador, rótulos de nombre | RF-04 (hecho), RF-07, RF-08 |
| Boda / funeral | Bucle de fotos con música, letras, video de recuerdos | RF-04.3 (hecho), RF-08.2, RF-09 |
| Campamento / retiro | Cantos, anuncios, cuenta regresiva entre actividades | RF-05, RF-08 |
| Clase / escuela sabática | PDF del material, Biblia, pizarra | RF-04 (hecho), RF-03 (hecho) |
| Concierto | Letras, videos de fondo, salida a transmisión | RF-05, RF-09, OBS |

## Orden recomendado de desarrollo

1. Cantos en texto (RF-05.1–05.3), con temas compartidos con la Biblia.
2. Planificador de servicio (RF-06.1–06.3).
3. Cuenta regresiva y rótulos de nombre (RF-08.1, 08.3).
4. Control remoto web (RF-10.1).
5. Monitor de escenario (RF-07).

## Fuentes consultadas

- [Capterra: ProPresenter vs FreeShow](https://www.capterra.com/compare/170172-251606/ProPresenter-vs-FreeShow)
- [faith.tools: FreeShow](https://faith.tools/app/175-freeshow)
- [FitGap: FreeShow](https://us.fitgap.com/products/037948/freeshow)
- [Capterra: EasyWorship vs ProPresenter](https://www.capterra.com/compare/101489-170172/EasyWorship-vs-ProPresenter)
- Análisis de Holyrics e Himnario Adventista: [analisis-importacion.md](analisis-importacion.md)
