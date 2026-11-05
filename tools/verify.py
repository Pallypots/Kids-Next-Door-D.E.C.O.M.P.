#!/usr/bin/env python3
"""Compile recovered sources and check every recorded function against the reference XBE.

  python -I tools/verify.py [--compiler msvc14-host] [--xbe orig/xiso/default.xbe] [--only VA ...]

Writes build/verify-report.json (inputs fingerprinted) and prints one line per function. Exit status 0 only if
every function checked is MATCH. Run from the repository root.
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xbe as xbemod  # noqa: E402
import xcheck  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def compile_src(compiler, src, out_obj, extra_flags=(), src_text=None):
    """Compile `src` (or `src_text`, written next to out_obj) with the configured compiler; return (ok, log)."""
    env = dict(os.environ)
    env["PATH"] = compiler["bin"].replace("/", os.sep) + os.pathsep + env.get("PATH", "")
    env["INCLUDE"] = ""
    os.makedirs(os.path.dirname(out_obj), exist_ok=True)
    if src_text is not None:
        src = out_obj[:-4] + ".cpp"
        with open(src, "w") as f:
            f.write(src_text)
    cmd = [os.path.join(compiler["bin"], "cl.exe")] + compiler["flags"] + list(extra_flags) + ["/Fo" + out_obj, src]
    p = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True)
    return p.returncode == 0 and os.path.exists(out_obj), (p.stdout + p.stderr).strip()


def compiler_version(compiler):
    env = dict(os.environ)
    env["PATH"] = compiler["bin"].replace("/", os.sep) + os.pathsep + env.get("PATH", "")
    p = subprocess.run([os.path.join(compiler["bin"], "cl.exe")], env=env, capture_output=True, text=True)
    return (p.stderr or p.stdout).splitlines()[0].strip()


def load_config():
    with open(os.path.join(ROOT, "config", "functions.toml"), "rb") as f:
        fcfg = tomllib.load(f)
    with open(os.path.join(ROOT, "config", "toolchain.toml"), "rb") as f:
        tcfg = tomllib.load(f)
    return fcfg, tcfg


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--compiler", default="msvc14-host")
    ap.add_argument("--xbe", default=os.path.join(ROOT, "orig", "xiso", "default.xbe"))
    ap.add_argument("--only", nargs="*", default=None)
    ap.add_argument("--report", default=os.path.join(ROOT, "build", "verify-report.json"))
    args = ap.parse_args(argv)
    fcfg, tcfg = load_config()
    comp = tcfg["compiler"][args.compiler]
    if not os.path.exists(args.xbe):
        print("reference XBE not found: %s (owner input; see docs/provenance.md)" % args.xbe)
        return 2
    x = xbemod.load(args.xbe)
    symmap = {k: v for k, v in fcfg.get("symbols", {}).items()}
    funcs = fcfg["function"]
    for f in funcs:
        symmap.setdefault(f["symbol"], f["va"])  # recovered functions may call each other by symbol
    if args.only:
        want = {int(v, 16) for v in args.only}
        funcs = [f for f in funcs if f["va"] in want]
    objs, logs, results = {}, {}, []
    for src in sorted({f["source"] for f in funcs}):
        obj = os.path.join(ROOT, "build", "obj", args.compiler, src.replace("/", "_") + ".obj")
        ok, log = compile_src(comp, src, obj)
        logs[src] = log
        objs[src] = xcheck.load(obj) if ok else None
        if not ok:
            print("COMPILE FAILED %s\n%s" % (src, log))
    for f in funcs:
        coff = objs[f["source"]]
        if coff is None:
            code, detail = "COMPILE_ERROR", logs[f["source"]][-300:]
        else:
            code, detail = xcheck.check_function(coff, f["symbol"], x, f["va"], f["size"], symmap)
        results.append({"va": "0x%08X" % f["va"], "size": f["size"], "symbol": f["symbol"], "source": f["source"],
                        "subsystem": f.get("subsystem"), "result": code, "detail": detail})
        print("%-11s 0x%08X %4d  %-48s %s" % (code, f["va"], f["size"], f["symbol"][:48],
                                              "" if code == "MATCH" else detail))
    report = {"reference_build": fcfg["reference_build"], "xbe_sha256": sha256(args.xbe),
              "compiler": args.compiler, "compiler_version": compiler_version(comp), "flags": comp["flags"],
              "checker_sha256": sha256(os.path.join(ROOT, "tools", "xcheck.py")),
              "results": results,
              "summary": {c: sum(1 for r in results if r["result"] == c) for c in sorted({r["result"] for r in results})}}
    os.makedirs(os.path.dirname(args.report), exist_ok=True)
    with open(args.report, "w") as fo:
        json.dump(report, fo, indent=1)
    print("summary: %s  (compiler: %s)" % (report["summary"], report["compiler_version"]))
    return 0 if all(r["result"] == "MATCH" for r in results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
