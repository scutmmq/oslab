#!/bin/sh
set -e
MOD=oslab_ringbuf
BUF=${1:-1024}
sudo rmmod "$MOD" 2>/dev/null || true
sudo insmod "${MOD}.ko" buf_size="${BUF}"
sudo chmod 666 "/dev/${MOD}"
echo "loaded ${MOD} (buf_size=${BUF}); /dev/${MOD} ready"
