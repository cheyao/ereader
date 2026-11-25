#!/bin/bash
# Writes data.txt to the raw SD card, starting at block 0.
# - UTF-8 is transliterated to ASCII (curly quotes -> ' ", dashes -> -,
#   accents dropped), so the reader doesn't show '?' for them
# - \r is kept: it marks a chapter change
# - A NUL byte is appended: it marks the end of the book
set -euo pipefail

[ -z "${1:-}" ] && { echo "usage: $0 <device, e.g. sda>"; exit 1; }

book=$(mktemp)
trap 'rm -f "$book"' EXIT

iconv -f UTF-8 -t ASCII//TRANSLIT data.txt > "$book"

{
	cat "$book"
	printf '\0'
} | dd of="/dev/$1" bs=1M iflag=fullblock conv=fdatasync status=progress

sync
