#!/usr/bin/env python3
"""Requirement -> test -> result matrix.

Reads the requirements (docs/requirements.md, "### REQ-nnn Title"), finds which tests
verify each one (a "/* Verifies: REQ-... */" comment right above a ZTEST, or a
"# Verifies: REQ-..." comment right above a Twister scenario), and takes the results from
Twister's twister.json. Prints the matrix as Markdown.

Exits 1 when a requirement has no test, a test cites an unknown requirement, a test did
not run on a platform, or a test failed.

    scripts/trace.py twister-out/twister.json > traceability.md
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REQ_HEADING = re.compile(r"^### (REQ-\d{3}) (.+)$", re.M)
REQ_ID = re.compile(r"REQ-\d{3}")
ZTEST = re.compile(r"/\* Verifies: ([^*]+)\*/\s*\nZTEST(?:_F)?\((\w+),\s*(\w+)\)")
SCENARIO = re.compile(r"^\s*# Verifies: (.+)\n\s*([\w.]+):\s*$", re.M)
ICON = {"passed": "pass", "failed": "FAIL", "error": "FAIL", "missing": "not run"}


def requirements():
    text = (ROOT / "docs" / "requirements.md").read_text()
    return dict(REQ_HEADING.findall(text))


def tests():
    """[(kind, name, file, [req ids])], kind 'ztest' (suite.test) or 'scenario'."""
    found = []
    for path in sorted(ROOT.glob("tests/**/*.c")):
        for reqs, suite, case in ZTEST.findall(path.read_text()):
            found.append(("ztest", f"{suite}.{case}", path, REQ_ID.findall(reqs)))
    for path in [ROOT / "sample.yaml", *sorted(ROOT.glob("tests/**/testcase.yaml"))]:
        for reqs, scenario in SCENARIO.findall(path.read_text()):
            found.append(("scenario", scenario, path, REQ_ID.findall(reqs)))
    return found


def results(twister_json):
    """{platform: {name: status}} for ztest cases (suite.test) and scenarios."""
    data = json.loads(Path(twister_json).read_text())
    out = {}
    for suite in data.get("testsuites", []):
        platform = suite.get("platform", "?")
        if suite.get("status") in ("filtered", "skipped"):
            continue
        per = out.setdefault(platform, {})
        scenario = suite.get("name", "").split("/")[-1]
        per[scenario] = suite.get("status", "missing")
        for case in suite.get("testcases", []):
            ident = case.get("identifier", "")
            status = case.get("status", "missing")
            # "<scenario>.<suite>.<test>", with or without the "test_" prefix.
            parts = ident.split(".")
            if len(parts) >= 2:
                suite_name, test = parts[-2], parts[-1]
                per[f"{suite_name}.{test}"] = status
                per[f"{suite_name}.test_{test}"] = status
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    reqs = requirements()
    found = tests()
    res = results(sys.argv[1])
    platforms = sorted(res)
    problems = []

    by_req = {r: [] for r in reqs}
    for kind, name, path, ids in found:
        for r in ids:
            if r not in reqs:
                problems.append(f"{path.relative_to(ROOT)}: {name} cites unknown {r}")
            else:
                by_req[r].append((kind, name, path))

    lines = ["# Traceability: requirement → test → result", ""]
    lines.append(f"Results from Twister on {', '.join(platforms) or 'no platform'}.")
    lines.append("")
    lines.append("| Requirement | Test | " + " | ".join(platforms) + " |")
    lines.append("|---|---|" + "---|" * len(platforms))
    for r, title in reqs.items():
        if not by_req[r]:
            problems.append(f"{r} has no test")
            lines.append(f"| **{r}** {title} | (none) |" + " |" * len(platforms))
            continue
        for i, (kind, name, path) in enumerate(by_req[r]):
            cells = []
            for p in platforms:
                status = res[p].get(name, "missing")
                cells.append(ICON.get(status, status))
                if status not in ("passed",):
                    problems.append(f"{r}: {name} on {p}: {status}")
            label = f"**{r}** {title}" if i == 0 else ""
            where = path.relative_to(ROOT).parent
            lines.append(f"| {label} | `{name}` ({kind}, {where}) | " + " | ".join(cells) + " |")

    tested = sum(1 for r in reqs if by_req[r])
    lines += ["", f"{tested}/{len(reqs)} requirements have tests; "
              f"{len({n for _, n, _, _ in found})} tests cite requirements."]
    if problems:
        lines += ["", "## Problems", ""] + [f"- {p}" for p in sorted(set(problems))]
    print("\n".join(lines))
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
