#!/usr/bin/env bash
# Cuántas veces se descargó WorshipStudio, por versión y por sistema.
# Uso: herramientas/estadisticas.sh   (necesita gh con sesión iniciada)
# La app no tiene telemetría: las descargas de GitHub son el número real que tenemos.
set -euo pipefail
REPO="${1:-solsox/WorshipStudio}"

gh api "repos/$REPO/releases" --paginate --jq '
  .[] | [.tag_name, .published_at[0:10],
         ([.assets[] | select(.name | test("macOS")) | .download_count] | add // 0),
         ([.assets[] | select(.name | test("Windows")) | .download_count] | add // 0),
         ([.assets[] | select(.name | test("Linux")) | .download_count] | add // 0)] | @tsv' |
awk -F'\t' 'BEGIN { printf "%-10s %-11s %8s %8s %8s %8s\n", "Versión", "Fecha", "Mac", "Windows", "Linux", "Total" }
  { t = $3 + $4 + $5; m += $3; w += $4; l += $5
    printf "%-10s %-11s %8d %8d %8d %8d\n", $1, $2, $3, $4, $5, t }
  END { printf "%-22s %8d %8d %8d %8d\n", "TOTAL", m, w, l, m + w + l }'

echo
echo "Visitas al repositorio (últimos 14 días):"
gh api "repos/$REPO/traffic/views" --jq '"  \(.count) visitas, \(.uniques) personas distintas"' 2>/dev/null || echo "  (requiere permiso de administrador del repositorio)"
gh api "repos/$REPO" --jq '"Estrellas: \(.stargazers_count) · Forks: \(.forks_count)"'
