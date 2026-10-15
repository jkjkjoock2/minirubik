#!/bin/sh
# worst.sh - run ida/rubik.s on Ripes (RV32_ISS) for every state in
# ida/d11.txt, several at a time, and report the worst instruction count.
# Usage, from the repository root:  sh ida/worst.sh
# Writes "state exit_code moves iret" lines to ida/d11_iret.txt.

RIPES=${RIPES:-/Applications/Ripes-v2.2.6-106-g5b8a616-mac-universal2.app/Contents/MacOS/Ripes}
JOBS=${JOBS:-8}
OUT=${OUT:-ida/d11_iret.txt}
export RIPES

start=$(date +%s)
xargs -P "$JOBS" -n 1 sh -c '
    s=$1
    d=$(mktemp -d)
    sed -e "/# RENDER_BEGIN/,/# RENDER_END/d" -e "s/\"21345671111111\"/\"$s\"/" ida/rubik.s |
        cat - ida/tables.s > "$d/t.s"
    "$RIPES" --mode cli -t asm --src "$d/t.s" --proc RV32_ISS --iret > "$d/out" 2>&1
    code=$(sed -n "s/^Program exited with code: //p" "$d/out")
    moves=$(head -1 "$d/out" | wc -w | tr -d " ")
    iret=$(tail -1 "$d/out")
    echo "$s $code $moves $iret"
    rm -rf "$d"
' _ < ida/d11.txt > "$OUT"
end=$(date +%s)

awk -v secs=$((end - start)) '
    { n++; sum += $4
      if ($2 != 0 || $3 != 11) bad++
      if ($4 > max) { max = $4; worst = $1 } }
    END {
      printf "%d states, %d wrong (exit != 0 or moves != 11)\n", n, bad
      printf "max iret %d (state %s), average %.0f\n", max, worst, sum / n
      printf "budget 50000000: %s, %d s wall-clock\n", (max < 50000000 ? "PASS" : "FAIL"), secs
    }' "$OUT"
