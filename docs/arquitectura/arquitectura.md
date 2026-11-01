# Arquitectura del fork

## Visión

La aplicación será un fork completo de OBS Studio con módulos nativos para contenido de culto y presentación.
La prioridad técnica es no romper ninguna capacidad de OBS:

- Escenas
- Fuentes
- Studio Mode
- Streaming
- Grabación
- Audio Mixer
- Filtros
- Plugins
- Salidas múltiples
- Configuración avanzada

## Capas funcionales

### 1. Núcleo OBS

Base tecnológica heredada de OBS Studio.
Debe permanecer como el motor principal de escenas, fuentes, audio y salidas.

### 2. Extensiones nativas de presentación

Módulos nuevos integrados en la app:

- Himnario
- Biblia
- PDF
- Browser limpio
- Biblioteca multimedia
- Planificador de servicio
- Macros

### 3. Control y automatización

Funciones para:

- Cambio de escenas
- Lanzamiento de elementos del servicio
- Navegación de contenido
- Ejecución de macros
- Control remoto desde móvil y tablet

## Reglas de identidad

- Nombre de aplicación independiente.
- Bundle Identifier propio.
- Carpeta de configuración propia.
- Iconos propios.
- Repositorio Git independiente.

## Separación de configuración

La app debe guardar datos en una ruta distinta a OBS Studio para evitar conflictos.

Ejemplo conceptual:

- `~/Library/Application Support/WorshipStudio`
- `~/Library/Caches/WorshipStudio`
- `~/Library/Preferences/com.worshipstudio.app.plist`

## Compatibilidad

La compatibilidad con OBS se conserva mientras el fork use:

- La misma lógica de escenas y fuentes
- Los mismos modelos de audio y video
- El mismo sistema de plugins donde aplique

Las herramientas nuevas deben integrarse como paneles y flujos nativos, no como una app aparte.

