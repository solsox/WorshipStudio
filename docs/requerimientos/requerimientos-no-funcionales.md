# Requerimientos no funcionales

| ID | Requerimiento |
|----|---------------|
| RNF-01 | **Plataformas**: macOS 12+ (Apple Silicon e Intel), Windows 10/11 de 64 bits y Linux (Ubuntu 24.04 o equivalente). |
| RNF-02 | **Sin conexión**: todo funciona sin internet durante el culto. Nada depende de servicios en la nube. |
| RNF-03 | **Fiabilidad en vivo**: un error en un panel nunca debe tirar la salida de OBS. Si algo falla se muestra un mensaje en el panel y la escena anterior sigue al aire. |
| RNF-04 | **Respuesta**: cambio de diapositiva en menos de 100 ms (la siguiente se prepara por adelantado); la interfaz no se congela con PDFs de más de 1000 páginas (miniaturas por lotes). |
| RNF-05 | **Calidad**: las diapositivas se dibujan a la resolución del lienzo de OBS (1080p, 4K) para que el texto se vea nítido. |
| RNF-06 | **Sin dependencias pesadas**: los PDF se dibujan con el motor de cada sistema (CoreGraphics en macOS, Windows.Data.Pdf en Windows, poppler-utils en Linux). Los programas de oficina solo se usan para convertir PowerPoint/Keynote/Impress. |
| RNF-07 | **Licencia**: GPL-2.0 o posterior (heredada de OBS Studio). Todo el código nuevo lleva el encabezado GPL. |
| RNF-08 | **Contenido con derechos**: el repositorio solo incluye contenido libre (Biblias de dominio público o con licencia libre, con su atribución). Himnarios, cantos y Biblias con derechos se cargan desde carpetas del usuario y nunca se suben al repositorio. |
| RNF-09 | **Configuración separada** de OBS Studio para poder instalar ambos. |
| RNF-10 | **Idioma**: interfaz en español; textos preparados para traducción. |
| RNF-11 | **Compatibilidad de datos**: los formatos propios (servicios, cantos, Biblias importadas) son JSON documentado, para poder migrar o editar a mano. |
| RNF-12 | **Pruebas**: cada módulo tiene pasos en la autoprueba con capturas reales de la salida. |
