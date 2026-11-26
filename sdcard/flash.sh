#!/bin/bash
# Writes a book to the raw SD card.
#
# Usage: flash.sh <device> [chapter files...]
#   Each file is one chapter, in the order given (zero-pad numbers so globs
#   sort right: ch01.txt, ch02.txt, ...). Without files, data.txt is used.
#   Chapters are joined with \r. A \r inside a file also starts a chapter.
#
# Card layout (all numbers uint32 little endian):
#   block 0..H-1  header
#                   bytes 0-3   "EBK1"
#                   bytes 4-7   header size H in 512-byte blocks
#                   bytes 8-11  book length in bytes
#                   bytes 12-15 chapter count N
#                   bytes 16..  N chapter start offsets, relative to the book
#                               (first is 0, the others follow each \r)
#   block H..     book text, then one NUL byte (end of book)
#
# The text is transliterated from UTF-8 to ASCII (curly quotes -> ' ",
# dashes -> -, accents dropped). \r is kept: it marks a chapter change.
set -euo pipefail

[ -z "${1:-}" ] && {
	echo "usage: $0 <device, e.g. sda> [chapter files...]"
	exit 1
}
device=$1
# Without this, a missing card makes dd create a plain file at /dev/$device
[ -b "/dev/$device" ] || { echo "/dev/$device: not a block device (card inserted?)"; exit 1; }
shift

files=("$@")
[ ${#files[@]} -eq 0 ] && files=(data.txt)
for f in "${files[@]}"; do
	[ -f "$f" ] || { echo "$f: not found"; exit 1; }
done

book=$(mktemp)
trap 'rm -f "$book"' EXIT

# One chapter per file, separated by \r
{
	first=1
	for f in "${files[@]}"; do
		[ $first -eq 1 ] || printf '\r'
		first=0
		iconv -f UTF-8 -t ASCII//TRANSLIT "$f"
	done
} > "$book"
len=$(stat -c %s "$book")

# Chapter starts: 0, then the byte after every \r (unless that's the end)
chapters=(0)
while IFS=: read -r pos _; do
	start=$((pos + 1))
	if [ "$start" -lt "$len" ]; then
		chapters+=("$start")
	fi
done < <(grep -boa $'\r' "$book" || true)

count=${#chapters[@]}
header_bytes=$((16 + 4 * count))
header_blocks=$(((header_bytes + 511) / 512))

le32() {
	local h
	h=$(printf '%08x' "$1")
	printf "\x${h:6:2}\x${h:4:2}\x${h:2:2}\x${h:0:2}"
}

echo "Book: $len bytes, $count chapters, header $header_blocks block(s)"

{
	printf 'EBK1'
	le32 "$header_blocks"
	le32 "$len"
	le32 "$count"
	for c in "${chapters[@]}"; do
		le32 "$c"
	done
	head -c $((header_blocks * 512 - header_bytes)) /dev/zero

	cat "$book"
	printf '\0'
} | dd of="/dev/$device" bs=1M iflag=fullblock conv=fdatasync status=progress

sync
