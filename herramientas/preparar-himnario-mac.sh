#!/bin/sh
# Deja el himnario listo para WorshipStudio en ~/Documents/WorshipStudio, que es
# una de las carpetas que la app revisa sola. Como vive fuera del programa,
# sobrevive a las actualizaciones y a reinstalar de cero.
#
# Uso:
#   herramientas/preparar-himnario-mac.sh [carpeta-del-himnario] [--copiar]
#
# Sin argumentos busca la carpeta en contenido-local/ del repositorio.
# Por defecto crea un enlace (no ocupa espacio); con --copiar hace una copia
# propia, que es lo que conviene si vas a borrar o mover contenido-local/.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
DESTINO="$HOME/Documents/WorshipStudio"

ORIGEN=""
COPIAR="no"
for arg in "$@"; do
  case "$arg" in
    --copiar) COPIAR="si" ;;
    *) ORIGEN="$arg" ;;
  esac
done

# Sin carpeta indicada: la primera de contenido-local/ que parezca un himnario.
if [ -z "$ORIGEN" ]; then
  for candidata in "$REPO/contenido-local/"*[Hh]imn*; do
    [ -d "$candidata" ] || continue
    ORIGEN="$candidata"
    break
  done
fi

if [ -z "$ORIGEN" ] || [ ! -d "$ORIGEN" ]; then
  echo "No encontré la carpeta del himnario." >&2
  echo "Pásala como argumento: herramientas/preparar-himnario-mac.sh \"/ruta/Himnario Adventista\"" >&2
  exit 1
fi

ORIGEN="$(cd "$ORIGEN" && pwd)"

# El número de himnos vive en Letra/ (programa original) o en la carpeta misma.
LETRA="$ORIGEN/Letra"
[ -d "$LETRA" ] || LETRA="$ORIGEN"
HIMNOS="$(find "$LETRA" -maxdepth 1 -type d -name '[0-9][0-9][0-9]*' | wc -l | tr -d ' ')"
if [ "$HIMNOS" -lt 3 ]; then
  echo "Esa carpeta no parece un himnario: no tiene carpetas \"NNN - Titulo\"." >&2
  exit 1
fi

# Un himnario viejo (1962) se reconoce por el nombre y va con su propio nombre,
# para que la app lo cargue en la edición correcta.
case "$(basename "$ORIGEN" | tr '[:upper:]' '[:lower:]')" in
  *antig*|*1962*) NOMBRE="Himnario Adventista 1962" ;;
  *) NOMBRE="Himnario Adventista 2009" ;;
esac

mkdir -p "$DESTINO"
rm -rf "$DESTINO/$NOMBRE"

if [ "$COPIAR" = "si" ]; then
  echo "Copiando $HIMNOS himnos a $DESTINO/$NOMBRE (puede tardar)..."
  ditto "$ORIGEN" "$DESTINO/$NOMBRE"
else
  ln -s "$ORIGEN" "$DESTINO/$NOMBRE"
fi

echo "Listo: $NOMBRE con $HIMNOS himnos en $DESTINO."
echo "Abre WorshipStudio y míralo en el panel Himnario; no hay que importar nada."
if [ "$COPIAR" != "si" ]; then
  echo "Es un enlace a $ORIGEN: si mueves o borras esa carpeta, vuelve a correr esto con --copiar."
fi
