#!/usr/bin/env bash
# Build every reproducer, run each once with WAYLAND_DEBUG, report the outcome.
set -u
cd "$(dirname "$0")"
mkdir -p traces

declare -A DEPS=(
  [01-webkit]="gtk+-3.0 webkit2gtk-4.1"
  [02-glarea]="gtk+-3.0 epoxy"
  [03-plain]="gtk+-3.0"
  [04-drawfromgl]="gtk+-3.0 epoxy"
  [05-earlygl]="gtk+-3.0 epoxy"
  [06-webkit-earlygl]="gtk+-3.0 webkit2gtk-4.1"
)

for t in 01-webkit 02-glarea 03-plain 04-drawfromgl 05-earlygl 06-webkit-earlygl; do
  gcc -g -O0 -o "repro/$t" "repro/$t.c" $(pkg-config --cflags --libs ${DEPS[$t]}) || exit 1
done

for t in 01-webkit 02-glarea 03-plain 04-drawfromgl 05-earlygl 06-webkit-earlygl; do
  WAYLAND_DEBUG=1 timeout 20 "./repro/$t" >/dev/null 2>"traces/$t.wl.log"
  rc=$?
  err=$(grep -m1 -o 'wl_display#1.error(.*' "traces/$t.wl.log")
  if [ $rc -eq 0 ]; then
    printf '%-22s survived\n' "$t"
  else
    printf '%-22s FAILED (exit %d) %s\n' "$t" "$rc" "$err"
  fi
done
