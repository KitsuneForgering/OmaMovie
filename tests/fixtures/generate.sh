#!/usr/bin/env bash
# Gera mídia de teste pequena e reproduzível em tests/fixtures/generated/ (não versionada).
# Uso: make fixtures   (ou tests/fixtures/generate.sh [diretório])
#
# Cobre os casos do CLAUDE.md §6 e §25: CFR e VFR, taxas NTSC, 8 e 10 bits, H.264/HEVC/AV1,
# AAC/Opus, rotação, imagem estática e arquivo truncado. Codecs ausentes no FFmpeg local são
# pulados com aviso. Nenhuma mídia com copyright: tudo vem dos geradores lavfi.

set -euo pipefail

out="${1:-$(dirname "$0")/generated}"
mkdir -p "$out"

if ! command -v ffmpeg >/dev/null; then
    echo "ffmpeg não encontrado" >&2
    exit 1
fi

export SVT_LOG=1 # SVT-AV1: só erros
ff() { ffmpeg -hide_banner -loglevel error -y "$@"; }
# Lida uma vez: com pipefail, `ffmpeg | grep -q` falharia por SIGPIPE quando o grep sai cedo.
encoders="$(ffmpeg -hide_banner -encoders 2>/dev/null)"
has_encoder() { grep -qE "^ [A-Z.]{6} $1 " <<<"$encoders"; }

# 1 s de vídeo de teste + tom de 440 Hz.
video_src() { echo "testsrc2=size=320x180:rate=$1:duration=${2:-1}"; }
audio_src() { echo "sine=frequency=440:sample_rate=${1:-48000}:duration=${2:-1}"; }

made=()
skipped=()
note() { made+=("$1"); }
skip() { skipped+=("$1 ($2)"); }

# H.264 CFR, 30 fps, AAC 48 kHz — caso base.
if has_encoder libx264; then
    ff -f lavfi -i "$(video_src 30)" -f lavfi -i "$(audio_src 48000)" \
        -c:v libx264 -preset veryfast -pix_fmt yuv420p -c:a aac -shortest "$out/h264_30fps_aac.mp4"
    note h264_30fps_aac.mp4

    # Taxas NTSC (frações exatas).
    ff -f lavfi -i "$(video_src 30000/1001)" -c:v libx264 -preset veryfast -pix_fmt yuv420p \
        "$out/h264_29.97fps.mp4"
    note h264_29.97fps.mp4
    ff -f lavfi -i "$(video_src 24000/1001)" -c:v libx264 -preset veryfast -pix_fmt yuv420p \
        "$out/h264_23.976fps.mp4"
    note h264_23.976fps.mp4

    # VFR: alterna intervalos de 1/30 e 1/15 s entre quadros.
    ff -f lavfi -i "$(video_src 30 2)" \
        -vf "setpts='if(eq(mod(N,2),0),N,N+0.5)/30/TB'" -fps_mode passthrough \
        -c:v libx264 -preset veryfast -pix_fmt yuv420p "$out/h264_vfr.mkv"
    note h264_vfr.mkv

    # Rotação via display matrix (vídeo de celular em pé).
    ff -display_rotation 90 -i "$out/h264_30fps_aac.mp4" -c copy "$out/h264_rotated90.mp4"
    note h264_rotated90.mp4

    # Arquivo truncado (entrada inválida para os testes de robustez).
    head -c 4096 "$out/h264_30fps_aac.mp4" >"$out/truncated.mp4"
    note truncated.mp4
else
    skip "H.264" "libx264 ausente"
fi

# HEVC 10 bits (P010 no decode por hardware).
if has_encoder libx265; then
    ff -f lavfi -i "$(video_src 30)" -c:v libx265 -preset ultrafast -pix_fmt yuv420p10le \
        -x265-params log-level=error "$out/hevc_10bit.mp4"
    note hevc_10bit.mp4
else
    skip "HEVC 10 bits" "libx265 ausente"
fi

# AV1 (formato das gravações de tela do Omarchy em GPUs recentes).
if has_encoder libsvtav1; then
    ff -f lavfi -i "$(video_src 30)" -f lavfi -i "$(audio_src 48000)" \
        -c:v libsvtav1 -preset 12 -pix_fmt yuv420p -c:a libopus -shortest "$out/av1_opus.mkv"
    note av1_opus.mkv
elif has_encoder libaom-av1; then
    ff -f lavfi -i "$(video_src 30)" -c:v libaom-av1 -cpu-used 8 -pix_fmt yuv420p "$out/av1.mkv"
    note av1.mkv
else
    skip "AV1" "libsvtav1/libaom ausentes"
fi

# Áudio isolado em 44.1 kHz (conversão de taxa de amostragem).
ff -f lavfi -i "$(audio_src 44100 2)" -c:a pcm_s16le "$out/tone_44100.wav"
note tone_44100.wav

# Imagem estática.
ff -f lavfi -i "testsrc2=size=640x360:rate=1:duration=1" -frames:v 1 "$out/still.png"
note still.png

echo "Fixtures em $out:"
printf '  %s\n' "${made[@]}"
if ((${#skipped[@]})); then
    echo "Puladas:"
    printf '  %s\n' "${skipped[@]}"
fi
