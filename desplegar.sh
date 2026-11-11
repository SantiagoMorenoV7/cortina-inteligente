#!/usr/bin/env bash
# Despliega la cortina inteligente desde Linux o macOS.
#   ./desplegar.sh                 instala PlatformIO si falta, compila, sube y abre el monitor
#   ./desplegar.sh --puerto /dev/ttyUSB0
#   ./desplegar.sh --solo-compilar
#   ./desplegar.sh --sin-monitor
set -euo pipefail
cd "$(dirname "$0")"

PUERTO=""; SOLO=0; SINMON=0
while [ $# -gt 0 ]; do
  case "$1" in
    --puerto) PUERTO="$2"; shift 2 ;;
    --solo-compilar) SOLO=1; shift ;;
    --sin-monitor) SINMON=1; shift ;;
    *) echo "Opción desconocida: $1"; exit 1 ;;
  esac
done

paso() { printf '\n\033[36m==> %s\033[0m\n' "$1"; }

paso "Preparando PlatformIO"
if [ -x "$HOME/.platformio/penv/bin/pio" ]; then
  PIO="$HOME/.platformio/penv/bin/pio"
elif [ -x "$HOME/.cache/cortina-inteligente/venv/bin/pio" ]; then
  PIO="$HOME/.cache/cortina-inteligente/venv/bin/pio"
else
  command -v python3 >/dev/null || { echo "Instala Python 3 y vuelve a intentarlo."; exit 1; }
  python3 -m venv "$HOME/.cache/cortina-inteligente/venv"
  "$HOME/.cache/cortina-inteligente/venv/bin/pip" install -q "platformio>=6.1,<7"
  PIO="$HOME/.cache/cortina-inteligente/venv/bin/pio"
fi

paso "Compilando el firmware"
"$PIO" run
[ "$SOLO" = 1 ] && { echo "Listo (solo compilar)."; exit 0; }

paso "Subiendo al ESP32 (si se queda en 'Connecting....', mantén presionado BOOT)"
if [ -n "$PUERTO" ]; then "$PIO" run -t upload --upload-port "$PUERTO"; else "$PIO" run -t upload; fi

printf '\n\033[32mCortina desplegada. Conéctate al WiFi "Cortina-XXXX" (clave cortina123) y abre http://192.168.4.1\033[0m\n'
[ "$SINMON" = 1 ] && exit 0

paso "Monitor serie (Ctrl+C para salir)"
if [ -n "$PUERTO" ]; then "$PIO" device monitor -p "$PUERTO" -b 115200; else "$PIO" device monitor -b 115200; fi
