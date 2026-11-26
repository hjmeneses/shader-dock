#!/bin/sh
# Actualiza la lista de shaders del Shader Dock sin reinstalar el plugin (macOS y Linux).
# Updates the Shader Dock's list of shaders without reinstalling the plugin (macOS and Linux).
#
#   curl -fsSL https://raw.githubusercontent.com/hjmeneses/shader-dock/main/tools/update-manifest.sh | sh
set -eu
url='https://raw.githubusercontent.com/hjmeneses/shader-dock/main/data/shaders.json'
case "$(uname -s)" in
  Darwin) dir="$HOME/Library/Application Support/obs-studio/plugin_config/shader-dock" ;;
  *)      dir="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugin_config/shader-dock" ;;
esac

tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
curl -fsSL "$url" -o "$tmp"
# Una lista JSON con entradas que tienen "repo" / A JSON list whose entries have "repo"
head -c 1 "$tmp" | grep -q '\[' && grep -q '"repo"' "$tmp" || {
  echo 'La lista descargada no es válida / The downloaded list is not valid' >&2; exit 1; }

mkdir -p "$dir"
mv "$tmp" "$dir/shaders.json"
trap - EXIT
echo "Shader Dock: $(grep -c '"repo"' "$dir/shaders.json") shaders"
echo 'Reinicia OBS para ver los cambios / Restart OBS to see the changes'
