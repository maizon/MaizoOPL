#!/usr/bin/env python3
"""Fail-closed release assessment for an exact MaizoOPL revision.

CI success and physical-console reports are separate evidence.
Reports must be supplied by actual testers, never synthesized by CI.
This tool does not patch games, execute hardware tests or certify them.
"""
import argparse
import json
import re
import subprocess
from pathlib import Path

WORKFLOWS = (
    "Maizo HID tests", "CI-compile", "CI-format-check",
    "MAIZO In-Game PRO experimental build (stack guarded)",
)
VARIANTS = ("oz", "oz-no-once", "os-no-once")
# Each entry is a distinct acceptance test, not an estimated percentage.
REQUIRED = {
    "opening": ("mk-armageddon-midfight", "justice-league-midgame", "repeat-open-close"),
    "resume": ("mk-armageddon-next-fight", "load-new-area", "save-load-game"),
    "commercial-gs": ("path3-commercial", "path2-commercial", "path1-commercial",
                      "aa1-visible", "bilinear-visible", "dither-visible", "scanlines-visible"),
    "cheats": ("smb-original-zip", "master-hook", "raw-constant", "raw-conditional",
               "value-choice", "individual-toggle", "complex-group"),
    "persist-live": ("save-without-exit", "reboot-profile-retained"),
    "boot-profile": ("gsm-boot-geometry", "native-boot-geometry"),
}
def assess(sha, runs, artifacts, report):
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("full commit SHA required")
    statuses = {}
    for name in WORKFLOWS:
        found = [r for r in runs if r.get("head_sha")==sha and r.get("name")==name]
        latest=max(found,key=lambda r:r.get("id",0)) if found else {}
        statuses[name]=latest.get("conclusion") if latest.get("status")=="completed" else None
    valid={a.get("name") for a in artifacts if not a.get("expired") and
           a.get("workflow_run",{}).get("head_sha")==sha}
    build_ok=all("MAIZO-IGR-EXPERIMENTAL-ELF-"+v in valid for v in VARIANTS)
    rows=report.get("tests",[]) if report.get("commit")==sha else []
    results={}
    for area,ids in REQUIRED.items():
        missing=[]
        for key in ids:
            matches=[r for r in rows if r.get("id")==key]
            # A failing or ambiguous report cannot be hidden by a pass.
            if not matches or not all(r.get("result")=="pass" and
                isinstance(r.get("console"),str) and r["console"].strip() and
                isinstance(r.get("evidence"),str) and r["evidence"].strip()
                for r in matches):
                missing.append(key)
        results[area]=missing
    return {"commit":sha,"ci":statuses,"artifacts_ok":build_ok,
            "missing_physical_evidence":results,
            "release_ready":all(v=="success" for v in statuses.values())
                and build_ok and not any(results.values())}

def gh(path):
    return json.loads(subprocess.check_output(["gh","api",path],text=True))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("sha")
    p.add_argument("--repo",default="maizon/MaizoOPL")
    p.add_argument("--report",type=Path)
    args=p.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}",args.sha):
        p.error("full commit SHA required")
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+",args.repo):
        p.error("invalid repository")
    runs=gh(f"repos/{args.repo}/actions/runs?head_sha={args.sha}&per_page=100")["workflow_runs"]
    exp=[r for r in runs if r["name"]==WORKFLOWS[-1] and r["head_sha"]==args.sha]
    arts=[]
    if exp:
        run=max(exp,key=lambda r:r["id"])
        arts=gh(f"repos/{args.repo}/actions/runs/{run['id']}/artifacts?per_page=100")["artifacts"]
    report=json.loads(args.report.read_text()) if args.report else {}
    result=assess(args.sha,runs,arts,report)
    print(json.dumps(result,indent=2,ensure_ascii=False))
    return 0 if result["release_ready"] else 1

if __name__=="__main__":
    raise SystemExit(main())
