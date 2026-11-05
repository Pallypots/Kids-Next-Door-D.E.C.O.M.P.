#!/usr/bin/env python3
"""Build and run the differential oracle (tests/oracle/oracle_gm.cpp), and qualify it with deliberately broken source.

  python -I tools/oracle_run.py [--seeds 5] [--ops 3000] [--no-mutants]

Windows x86 only, local only: needs the owner's XBE. Writes build/oracle-report.json. Exit 0 only if every seed shows no
difference, every recorded function's entry was reached, and every source mutant is detected.

A mutant is one small edit to a copy of the recovered source. "Detected" means the oracle exited with a difference or
crashed; a mutant that survives means the oracle cannot see that behaviour, which fails this run.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build", "oracle")
MUTANT_BUILD = os.path.join(ROOT, "build", "oracle-mutant")
MUTANT_SRC = os.path.join(ROOT, "build", "oracle-mutant-src")
CONFIG = "RelWithDebInfo"
XBE = os.path.join(ROOT, "orig", "xiso", "default.xbe")

# (name, file, exact text to replace, replacement). The text must occur exactly once.
MUTANTS = [
    ("Append: prev link dropped", "GmLinkedList.cpp", "node->prev = last;", "node->prev = 0;"),
    ("Append: head never set", "GmLinkedList.cpp", "    if (!head) {\n        head = node;\n    }\n", ""),
    ("Node::Disconnect: tail not updated", "GmLinkedList.cpp", "list->tail = prev;", ""),
    ("Node::Disconnect: entry not freed", "GmLinkedList.cpp", "    m_active = 0;\n    Unlink();", "    Unlink();"),
    ("Node::Disconnect: plug link kept", "GmLinkedList.cpp", "    m_active = 0;\n    Unlink();", "    m_active = 0;"),
    ("Allocate: no wrap-around", "GmLinkedList.cpp", "if (index == kPoolSize) {\n            index = 0;\n        }",
     "if (index == kPoolSize) {\n            index = kPoolSize - 1;\n        }"),
    ("Allocate: cursor not stored", "GmLinkedList.cpp", "            s_cursor = index;\n            s_pool", "            s_pool"),
    # Not used: removing the `return` in GmPlug::Disconnect is an equivalent mutant. Disconnect -> Unlink zeroes
    # m_plugNext, so the loop stops by itself.
    ("Plug::Disconnect: matches the wrong socket", "GmLinkedList.cpp", "        if (c->m_socket == socket) {\n            c->Disconnect();",
     "        if (c->m_socket != socket) {\n            c->Disconnect();"),
    ("IsConnectedTo: wrong result", "GmLinkedList.cpp", "            return 1;", "            return 2;"),
    ("~GmLinkedList: context not cleared", "GmLinkedList.cpp", "    SetContext(0);\n", ""),
    ("Iterator ctor: does not start at head", "GmLinkedList.cpp", "        m_cur = list->head;\n    }\n}", "        m_cur = list->tail;\n    }\n}"),
    ("Iterator::Prev: moves to null", "GmLinkedList.cpp", "    if (prev) {\n        m_cur = prev;\n    }\n}", "    m_cur = prev;\n}"),
    ("Iterator::NextData: does not advance", "GmLinkedList.cpp", "    m_cur = node->next;\n    return data;", "    return data;"),
    ("Iterator::PrevData: goes forward", "GmLinkedList.cpp", "    m_cur = node->prev;", "    m_cur = node->next;"),
    ("Iterator::At: off by one", "GmLinkedList.cpp", "for (int i = 0; i < index; i++) {", "for (int i = 0; i <= index; i++) {"),
    ("Iterator::Count: counts one extra", "GmLinkedList.cpp", "    int count = 0;", "    int count = 1;"),
    ("RemoveCurrent: position not advanced", "GmLinkedList.cpp", "        node->Disconnect();\n        m_cur = following;", "        node->Disconnect();"),
    ("Connect: not added to plug", "GmConnection.cpp", "    plug->m_connections = this;\n", ""),
    ("Connect: previous first not linked", "GmConnection.cpp", "        first->m_plugPrev = this;", ""),
    ("Unlink: plug head not updated", "GmConnection.cpp", "        m_plug->m_connections = m_plugNext;", ""),
    ("Unlink: next link not repaired", "GmConnection.cpp", "        m_plugNext->m_plugPrev = m_plugPrev;", ""),
    ("Socket::Contains: inverted", "GmSocket.cpp", "return item->IsConnectedTo(this);", "return !item->IsConnectedTo(this);"),
    ("Socket::Remove: does nothing", "GmSocket.cpp", "    item->Disconnect(this);\n", ""),
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def configure_and_build(build_dir, extra=()):
    r = run(["cmake", "-S", ROOT, "-B", build_dir, "-A", "Win32"] + list(extra))
    if r.returncode:
        raise SystemExit("configure failed:\n" + r.stdout + r.stderr)
    r = run(["cmake", "--build", build_dir, "--config", CONFIG, "--target", "oracle_gm"])
    if r.returncode:
        return False, r.stdout + r.stderr
    return True, ""


def exe(build_dir):
    return os.path.join(build_dir, CONFIG, "oracle_gm.exe")


def write_variant(src, mutant):
    """Write every source file with a fresh timestamp. A copy that kept old timestamps would leave the previous mutant's
    object file in place, and the build system would silently reuse it."""
    shutil.rmtree(MUTANT_SRC, ignore_errors=True)
    os.makedirs(MUTANT_SRC)
    for leaf in sorted(os.listdir(src)):
        with open(os.path.join(src, leaf), encoding="utf-8") as f:
            text = f.read()
        if mutant and leaf == mutant[1]:
            if text.count(mutant[2]) != 1:
                raise SystemExit("mutant %r: text occurs %d times, expected 1" % (mutant[0], text.count(mutant[2])))
            text = text.replace(mutant[2], mutant[3])
        with open(os.path.join(MUTANT_SRC, leaf), "w", encoding="utf-8") as f:
            f.write(text)
    return True


def run_mutant():
    try:
        return run([exe(MUTANT_BUILD), XBE, "--ops", "500"], timeout=60).returncode
    except subprocess.TimeoutExpired:
        return "timeout"


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=5)
    ap.add_argument("--ops", type=int, default=3000)
    ap.add_argument("--no-mutants", action="store_true")
    a = ap.parse_args(argv)
    if not os.path.exists(XBE):
        print("reference XBE not found (owner input)")
        return 2
    with open(os.path.join(ROOT, "config", "functions.toml"), "rb") as f:
        funcs = tomllib.load(f)["function"]
    ok, log = configure_and_build(BUILD)
    if not ok:
        print(log)
        return 2

    entries = os.path.join(ROOT, "build", "oracle-entries.txt")
    with open(entries, "w") as f:
        f.write("\n".join("%X" % fn["va"] for fn in funcs) + "\n")
    report = {"xbe_sha256": hashlib.sha256(open(XBE, "rb").read()).hexdigest(), "ops_per_seed": a.ops, "seeds": [],
              "mutants": []}
    good = True
    unreached = set()
    for seed in range(1, a.seeds + 1):
        r = run([exe(BUILD), XBE, "--seed", str(seed), "--ops", str(a.ops), "--entries", entries], timeout=900)
        print(r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "no output, exit %d" % r.returncode)
        missing = re.findall(r"entry not reached: 0x([0-9A-F]+)", r.stdout)
        unreached.update(int(m, 16) for m in missing)
        report["seeds"].append({"seed": seed, "exit": r.returncode, "summary": r.stdout.strip().splitlines()[-1:],
                                "not_reached": missing})
        good &= r.returncode == 0
    report["entries_reached"] = len(funcs) - len(unreached)
    report["entries_total"] = len(funcs)
    report["entries_not_reached"] = ["0x%08X" % v for v in sorted(unreached)]
    print("entries reached in every seed: %d / %d %s" % (report["entries_reached"], len(funcs),
                                                         sorted(hex(v) for v in unreached) or ""))
    good &= not unreached

    r = run([exe(BUILD), XBE, "--ops", "500", "--mutate"], timeout=300)
    print("harness mutation (--mutate): exit %d" % r.returncode)
    report["harness_mutation_exit"] = r.returncode
    good &= r.returncode == 1

    survived = []
    if not a.no_mutants:
        src = os.path.join(ROOT, "src", "gm")
        if not write_variant(src, None):
            raise SystemExit("unreachable")
        ok, log = configure_and_build(MUTANT_BUILD, ["-DKND_GM_SRC=" + MUTANT_SRC.replace("\\", "/")])
        if not ok:
            raise SystemExit("positive control does not compile:\n" + log)
        code = run_mutant()
        print("  %-42s %s" % ("(unmodified copy: positive control)", "passes" if code == 0 else "FAILS: %s" % code))
        report["mutant_positive_control_exit"] = code
        good &= code == 0
        for name, file, old, new in MUTANTS:
            write_variant(src, (name, file, old, new))
            ok, log = configure_and_build(MUTANT_BUILD, ["-DKND_GM_SRC=" + MUTANT_SRC.replace("\\", "/")])
            if not ok:
                raise SystemExit("mutant %r does not compile:\n%s" % (name, log))
            code = run_mutant()
            verdict = "detected (difference)" if code == 1 else "survived" if code == 0 else "detected (crash/hang: %s)" % code
            print("  %-42s %s" % (name, verdict))
            report["mutants"].append({"name": name, "verdict": verdict})
            if code == 0:
                survived.append(name)
        print("mutants detected: %d / %d" % (len(MUTANTS) - len(survived), len(MUTANTS)))
        good &= not survived
    with open(os.path.join(ROOT, "build", "oracle-report.json"), "w") as f:
        json.dump(report, f, indent=1)
    return 0 if good else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
