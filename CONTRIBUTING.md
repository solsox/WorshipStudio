# Contribuir a WorshipStudio

1. Abrí un *issue* contando qué querés mejorar o qué falla (con capturas si es visual).
2. Hacé un fork, creá una rama (`git checkout -b mejora-cantos`) y compilá siguiendo
   [docs/desarrollo/compilar.md](docs/desarrollo/compilar.md).
3. El código nuevo de WorshipStudio va en `app/frontend/docks/` y sigue el estilo de OBS
   (`app/CODESTYLE.md`, `clang-format`).
4. Corré la autoprueba (`WORSHIPSTUDIO_SELFTEST`) y revisá las capturas.
5. Abrí un *pull request* explicando el cambio.

Reglas de contenido:

- No subas himnarios, cantos, Biblias ni imágenes con derechos de autor. Solo contenido de dominio público o con
  licencia libre, indicando la licencia y la fuente.
- Todo el código se publica bajo GPL-2.0 o posterior.
