#!/usr/bin/env bash
# Arc language test suite.
#   tests/lang/run/*.ar     programs whose stdout must equal the matching .out file
#                           (each is also run with ARC_GC_STRESS=1, which collects
#                           garbage on every allocation to catch rooting bugs)
#   tests/lang/errors/*.ar  must fail to compile; every `// ERROR: text` line must
#                           appear in the diagnostics
#   tests/lang/panics/*.ar  must exit with code 101 (or `// EXIT: n`); every
#                           `// PANIC: text` line must appear on stderr
#   tests/lang/unit/        `arc test` runs; failing_test.ar must fail
set -u
cd "$(dirname "$0")/.."
ARC=${ARC:-./arc}
export NO_COLOR=1
pass=0
fail=0
failures=()

ok() { pass=$((pass + 1)); }
bad() { fail=$((fail + 1)); failures+=("$1"); echo "FAIL: $1"; [ -n "${2:-}" ] && echo "$2" | head -20; }

for f in tests/lang/run/*.ar; do
    expected="${f%.ar}.out"
    actual=$($ARC run -q "$f" 2>&1)
    if [ "$actual" == "$(cat "$expected")" ]; then ok; else
        bad "$f" "$(diff <(echo "$actual") "$expected")"
    fi
    stress=$(ARC_GC_STRESS=1 $ARC run -q "$f" 2>&1)
    if [ "$stress" == "$(cat "$expected")" ]; then ok; else
        bad "$f (GC stress)" "$(diff <(echo "$stress") "$expected")"
    fi
done

for f in tests/lang/errors/*.ar; do
    out=$($ARC check "$f" 2>&1)
    code=$?
    if [ $code -eq 0 ]; then bad "$f" "expected a compile error, but it compiled"; continue; fi
    missing=""
    while IFS= read -r want; do
        [[ "$out" == *"$want"* ]] || missing+="  missing: $want"$'\n'
    done < <(sed -n 's|^// ERROR: ||p' "$f")
    if [ -z "$missing" ]; then ok; else bad "$f" "$missing$out"; fi
done

for f in tests/lang/panics/*.ar; do
    out=$($ARC run -q "$f" 2>&1)
    code=$?
    want_code=$(sed -n 's|^// EXIT: ||p' "$f")
    want_code=${want_code:-101}
    missing=""
    [ "$code" -eq "$want_code" ] || missing+="  exit code $code, expected $want_code"$'\n'
    while IFS= read -r want; do
        [[ "$out" == *"$want"* ]] || missing+="  missing: $want"$'\n'
    done < <(sed -n 's|^// PANIC: ||p' "$f")
    if [ -z "$missing" ]; then ok; else bad "$f" "$missing$out"; fi
done

out=$($ARC test tests/lang/unit/math_test.ar 2>&1)
if [ $? -eq 0 ] && [[ "$out" == *"3 passed; 0 failed"* ]]; then ok; else bad "arc test math_test.ar" "$out"; fi
out=$($ARC test tests/lang/unit/failing_test.ar 2>&1)
if [ $? -ne 0 ] && [[ "$out" == *"1 passed; 1 failed"* ]] && [[ "$out" == *"test_fails"* ]]; then ok; else
    bad "arc test failing_test.ar (must fail)" "$out"
fi

# imports: compile a two-file program
tmp=$(mktemp -d)
printf 'fn triple(x: int) -> int { return x * 3 }\n' > "$tmp/util.ar"
printf 'import "util"\nfn main() { println(triple(14)) }\n' > "$tmp/main.ar"
out=$($ARC run -q "$tmp/main.ar" 2>&1)
if [ "$out" == "42" ]; then ok; else bad "imports" "$out"; fi
rm -rf "$tmp"

# example programs must compile and run cleanly
for f in examples/complex_logic.ar examples/server.ar examples/wordfreq.ar examples/inventory.ar examples/calculator.ar; do
    out=$($ARC run -q "$f" 2>&1)
    if [ $? -eq 0 ]; then ok; else bad "$f" "$out"; fi
done

echo
echo "language tests: $pass passed, $fail failed"
[ $fail -eq 0 ]
