#!/usr/bin/env bash
# MISRA C:2012 check of the portable modules (src/lib) with the cppcheck addon.
# The addon is run as a script on cppcheck's dump files: `cppcheck --addon=misra` can
# fail silently when the addon cannot start, and report nothing. Exits 1 on any finding.
set -euo pipefail
cd "$(dirname "$0")/.."

addon="$(cppcheck --version >/dev/null && dirname "$(command -v cppcheck)")/../share/cppcheck/addons/misra.py"
[ -f "$addon" ] || addon="$(find /usr /opt -name misra.py -path '*addons*' 2>/dev/null | head -1)"
[ -f "$addon" ] || { echo "misra.py not found"; exit 2; }

out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
cp src/lib/*.c src/lib/*.h "$out"/
for f in "$out"/*.c; do
  cppcheck --dump --quiet --std=c11 -I"$out" "$f"
done

# Self-test: a file with known violations must be reported, or the check is not running.
printf 'int probe(int x)\n{\n\tif (x) goto out;\n\treturn 1;\nout:\n\treturn x;\n}\n' > "$out/probe.c"
cppcheck --dump --quiet "$out/probe.c"
# (The addon exits 1 when it finds violations: capture first, then filter.)
probe="$(python3 "$addon" "$out/probe.c.dump" 2>&1 || true)"
grep -q "misra-c2012-15.1" <<<"$probe" || { echo "MISRA addon is not reporting"; exit 2; }
rm "$out"/probe.c*

report="$(python3 "$addon" "$out"/*.c.dump 2>&1 || true)"
report="${report//$out\//src/lib/}"
findings="$(grep -E '^\[src/lib/' <<<"$report" || true)"
if [ -n "$findings" ]; then
  echo "$findings"
  echo "MISRA C:2012: $(wc -l <<<"$findings" | tr -d ' ') finding(s) in src/lib"
  exit 1
fi
echo "MISRA C:2012 (cppcheck $(cppcheck --version | cut -d' ' -f2) addon): no findings in src/lib"
