#!/bin/sh
# test.sh - run ida/rubik.s on Ripes (RV32_ISS) for each test vector and
# check the exit code and the number of moves printed.
# Usage, from the repository root:  sh ida/test.sh

RIPES=${RIPES:-/Applications/Ripes-v2.2.6-106-g5b8a616-mac-universal2.app/Contents/MacOS/Ripes}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' 0 1 2 15
pass=0
fail=0

# run STATE: build rubik.s with STATE inlined, run it on Ripes.
# Sets $code (exit code), $moves (moves printed), $iret (instructions retired).
run() {
    sed -e '/# RENDER_BEGIN/,/# RENDER_END/d' -e "s/\"21345671111111\"/\"$1\"/" ida/rubik.s |
        cat - ida/tables.s > "$TMP/t.s"
    "$RIPES" --mode cli -t asm --src "$TMP/t.s" --proc RV32_ISS --iret > "$TMP/out" 2>&1
    code=$(sed -n 's/^Program exited with code: //p' "$TMP/out")
    iret=$(tail -1 "$TMP/out")
    if [ "$code" = 0 ]; then
        moves=$(head -1 "$TMP/out" | wc -w | tr -d ' ')
    else
        moves=-
    fi
}

# check NAME STATE CODE MOVES: expect exit code CODE and MOVES moves
# (MOVES is "-" when no solution is printed, as for invalid input).
check() {
    run "$2"
    if [ "$code" = "$3" ] && [ "$moves" = "$4" ]; then
        pass=$((pass + 1))
        printf 'PASS  %-16s %s  code %s  moves %s  iret %s\n' "$1" "$2" "$code" "$moves" "$iret"
    else
        fail=$((fail + 1))
        printf 'FAIL  %-16s %s  code %s (want %s)  moves %s (want %s)\n' \
            "$1" "$2" "$code" "$3" "$moves" "$4"
    fi
}

# ---------------------------------------------------------------
# ② TODO: test vectors.  One line per test:
#    check NAME STATE CODE MOVES
# ---------------------------------------------------------------
# Own test cases (required by the spec)
check solved         12345671111111  0 0
check short_RBi      25371462312233  0 2
check distance11     21345671111111  0 11

# Vectors from tests/solutions.txt
check dist8          62345713133111  0 8
check dist9          24513763133333  0 9
check dist10         25416373331111  0 10

# Invalid input, from INVALID_STATES in the Makefile: exit 2
check short_input    1234567111111   2 -    # (1) length
check long_input     123456711111111 2 -    # (1) length
check cubie_low      02345671111111  2 -    # (2) permutation range, low
check cubie_high     82345671111111  2 -    # (2) permutation range, high
check orient_low     12345671111110  2 -    # (3) orientation range, low
check orient_high    12345671111114  2 -    # (3) orientation range, high
check non_digit      1234567111111a  2 -    # (3) orientation range ('a' - '1' = 48)
check duplicate      11345671111111  2 -    # (2) seen mask
check parity         12345671111112  2 -    # (3) sum mod 3

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
