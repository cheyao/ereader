#!/bin/bash
[ -z "$1" ] && { echo "usage: $0 <device, e.g. sda>"; exit 1; }
dd if=data.txt of="/dev/$1" bs=1M conv=fdatasync status=progress
sync
