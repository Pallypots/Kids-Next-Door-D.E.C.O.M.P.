#!/usr/bin/env python3
"""Function and class inventory for an XBE, with explicit confidence labels.

Evidence sources (each function records which ones found it):
  entry       the decoded entry point                                                 (high)
  call        target of a direct rel32 CALL from already-discovered code              (high)
  vtable      slot of a vtable recovered through MSVC RTTI (COL -> type descriptor)   (high, and gives a class)
  ptr         aligned pointer in .rdata/.data into code, after padding, decodes sanely (medium)
  gap         16-aligned uncovered .text address after 0xCC/ret padding, decodes sanely (low)
  tailjmp     (annotation) target of a JMP from another function

Function bodies come from recursive descent (conditional branches, in-function jumps, MSVC jump tables of the
form `jmp [reg*4+table]`). Bodies that contain another function's start (shared tails, fall-through, or a mis-seed)
are flagged in `contains_other_starts`, never silently merged. Counts from this tool are an inventory, not progress.

  python -I tools/funcs.py <default.xbe> <out_dir>
writes functions.json, classes.json, coverage.json and a summary to stdout.
"""
import json
import os
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xbe as xbemod  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402
from capstone import x86 as cx  # noqa: E402

CODE_SECTIONS = (".text", "D3D", "D3DX", "XGRPH", "DSOUND", "XACTENG", "PSFD_I", "PSFD_B", "PSFD_P", "PSFD00",
                 "WMADEC", "XPP", "DOLBY")
DATA_SECTIONS = (".rdata", ".data")
CONFIDENCE = {"entry": "high", "call": "high", "vtable": "high", "ptr": "medium", "gap": "low", "tailjmp": "low"}
RANK = ["low", "medium", "high"]
PAD = (b"\xcc", b"\xc3", b"\x90")


class Analyzer:
    def __init__(self, x):
        self.x = x
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = True
        self.code = [s for s in x.sections if s.name in CODE_SECTIONS]
        self.text = next(s for s in self.code if s.name == ".text")
        self.funcs = {}
        self.evidence = defaultdict(set)
        self._cache = {}

    def code_sec(self, va):
        for s in self.code:
            if s.raw_contains(va):
                return s
        return None

    def decode_one(self, va):
        if va in self._cache:
            return self._cache[va]
        i = None
        s = self.code_sec(va)
        if s is not None:
            buf = self.x.read_va(va, min(16, s.va + s.rsize - va))
            for i in self.md.disasm(buf, va, 1):
                break
        self._cache[va] = i
        return i

    def jump_table(self, insn, fstart):
        op = insn.operands[0]
        if op.type != cx.X86_OP_MEM or op.mem.scale != 4 or op.mem.base != 0 or op.mem.index == 0:
            return []
        tbl = op.mem.disp & 0xFFFFFFFF
        sec = self.code_sec(fstart)
        out = []
        for k in range(1024):
            try:
                t = self.x.u32_va(tbl + 4 * k)
            except xbemod.XbeError:
                break
            if sec is None or not sec.raw_contains(t) or t < fstart:
                break
            out.append(t)
        return out

    def explore(self, start):
        todo, seen = [start], set()
        calls, tails, jtabs = set(), set(), []
        while todo:
            va = todo.pop()
            while va not in seen:
                i = self.decode_one(va)
                if i is None:
                    break
                seen.add(va)
                m = i.mnemonic
                if m == "call":
                    op = i.operands[0]
                    if op.type == cx.X86_OP_IMM:
                        calls.add(op.imm & 0xFFFFFFFF)
                elif m in ("ret", "retf", "int3", "hlt", "ud2"):
                    break
                elif m == "jmp":
                    op = i.operands[0]
                    if op.type == cx.X86_OP_IMM:
                        t = op.imm & 0xFFFFFFFF
                        if t in self.funcs and t != start:
                            tails.add(t)
                        else:
                            todo.append(t)
                    else:
                        tbl = self.jump_table(i, start)
                        if tbl:
                            jtabs.append({"at": "0x%08X" % va, "table": "0x%08X" % (op.mem.disp & 0xFFFFFFFF),
                                          "entries": len(tbl)})
                            todo.extend(tbl)
                    break
                elif m.startswith("j") or m.startswith("loop"):
                    op = i.operands[0]
                    if op.type == cx.X86_OP_IMM:
                        todo.append(op.imm & 0xFFFFFFFF)
                va += i.size
        return seen, calls, tails, jtabs

    def add(self, va, why):
        self.evidence[va].add(why)
        if va in self.funcs or self.code_sec(va) is None:
            return False
        self.funcs[va] = None
        return True

    def grow(self, seeds, why):
        work = [v for v in seeds if self.add(v, why)]
        n = len(work)
        while work:
            f = work.pop()
            insns, calls, tails, jtabs = self.explore(f)
            self.funcs[f] = {"insns": insns, "jtables": jtabs, "calls": sorted(calls), "tails": sorted(tails)}
            for c in calls:
                if self.add(c, "call"):
                    work.append(c)
            for t in tails:
                self.evidence[t].add("tailjmp")
        return n

    def covered(self):
        c = set()
        for info in self.funcs.values():
            for v in info["insns"]:
                c.update(range(v, v + self.decode_one(v).size))
        return c

    def plausible_start(self, va, n=6):
        p = va
        for _ in range(n):
            i = self.decode_one(p)
            if i is None or i.mnemonic in ("int3", "hlt") or i.bytes[:2] == b"\x00\x00":
                return False
            if i.mnemonic in ("ret", "jmp"):
                return True
            p += i.size
        return True

    def pointer_seeds(self):
        out = set()
        for s in self.x.sections:
            if s.name not in DATA_SECTIONS:
                continue
            raw = self.x.data[s.raw:s.raw + s.rsize]
            for o in range(0, len(raw) - 3, 4):
                v = struct.unpack_from("<I", raw, o)[0]
                if v in self.funcs or self.code_sec(v) is None:
                    continue
                sec = self.code_sec(v)
                prev = self.x.read_va(v - 1, 1) if v > sec.va else b"\xcc"
                if prev in PAD and self.plausible_start(v):
                    out.add(v)
        return sorted(out)

    def gap_seeds(self):
        cov = self.covered()
        out = []
        t = self.text
        for va in range(t.va + 16, t.va + t.rsize, 16):
            if va in cov or va in self.funcs:
                continue
            if self.x.read_va(va, 1) in (b"\xcc", b"\x00") or self.x.read_va(va - 1, 1) not in (b"\xcc", b"\xc3"):
                continue
            if self.plausible_start(va, 8):
                out.append(va)
        return out


def rtti(x):
    """MSVC RTTI: TypeDescriptor{pVFTable, spare, name}; COL{sig=0, offset, cdOffset, pTD, pCHD}; vftable[-1]=&COL."""
    data = x.data
    tds = {}
    for s in x.sections:
        if s.name not in DATA_SECTIONS:
            continue
        raw = data[s.raw:s.raw + s.rsize]
        p = raw.find(b".?A")
        while p >= 0:
            end = raw.find(b"\0", p)
            name = raw[p:end].decode("latin-1")
            if p >= 8 and name.endswith("@@"):
                tds[s.va + p - 8] = name
            p = raw.find(b".?A", p + 3)
    rd = next(s for s in x.sections if s.name == ".rdata")
    raw = data[rd.raw:rd.raw + rd.rsize]
    cols = {}
    for o in range(0, len(raw) - 20, 4):
        sig, off, cd, ptd, pchd = struct.unpack_from("<5I", raw, o)
        if sig == 0 and ptd in tds and off < 0x10000:
            cols[rd.va + o] = (tds[ptd], off)
    classes = defaultdict(list)
    for o in range(0, len(raw) - 8, 4):
        p = struct.unpack_from("<I", raw, o)[0]
        if p not in cols:
            continue
        vt = rd.va + o + 4
        slots = []
        for k in range(4096):
            try:
                v = x.u32_va(vt + 4 * k)
            except xbemod.XbeError:
                break
            sec = x.section_for(v)
            if v in cols or sec is None or sec.name not in CODE_SECTIONS:
                break
            slots.append(v)
        name, off = cols[p]
        classes[name].append({"vftable": "0x%08X" % vt, "col": "0x%08X" % p, "this_offset": off,
                              "slots": ["0x%08X" % v for v in slots]})
    return tds, classes


def demangle_td(name):
    """'.?AVFoo@Bar@@' -> ('class', 'Bar::Foo'). Readable form only; no templates/backrefs handled."""
    kind = {"V": "class", "U": "struct", "W": "enum"}.get(name[3:4], "?")
    parts = [p for p in name[4:-2].split("@") if p]
    return kind, "::".join(reversed(parts))


def main(argv):
    x = xbemod.load(argv[0])
    out = argv[1]
    os.makedirs(out, exist_ok=True)
    tds, classes = rtti(x)
    a = Analyzer(x)
    a.grow([x.entry], "entry")
    vt_names = defaultdict(list)
    for cname, vts in classes.items():
        for vt in vts:
            for k, sl in enumerate(vt["slots"]):
                vt_names[int(sl, 16)].append("%s[%d]@+%d" % (demangle_td(cname)[1], k, vt["this_offset"]))
    n_vt = a.grow(sorted(vt_names), "vtable")
    n_ptr = a.grow(a.pointer_seeds(), "ptr")
    rounds = []
    for _ in range(30):
        g = a.gap_seeds()
        if not g:
            break
        rounds.append(a.grow(g, "gap"))
    print("new seeds: vtable %d, pointer %d, gap rounds %s" % (n_vt, n_ptr, rounds))

    starts = set(a.funcs)
    funcs = []
    for f in sorted(a.funcs):
        info = a.funcs[f]
        ev = sorted(a.evidence[f])
        conf = max((CONFIDENCE[e] for e in ev), key=RANK.index)
        rec = {"va": "0x%08X" % f, "section": a.code_sec(f).name,
               "insn_bytes": sum(a.decode_one(v).size for v in info["insns"]),
               "min": "0x%08X" % min(info["insns"]), "max_end": "0x%08X" % max(v + a.decode_one(v).size for v in info["insns"]),
               "evidence": ev, "confidence": conf, "calls": ["0x%08X" % c for c in info["calls"]],
               "jtables": info["jtables"]}
        inner = sorted(v for v in info["insns"] if v in starts and v != f)
        if inner:
            rec["contains_other_starts"] = ["0x%08X" % v for v in inner[:8]]
        if f in vt_names:
            rec["vtable_slots"] = vt_names[f][:6]
        funcs.append(rec)

    reached = defaultdict(set)
    for info in a.funcs.values():
        for v in info["insns"]:
            reached[a.code_sec(v).name].update(range(v, v + a.decode_one(v).size))
    cov = {}
    for s in a.code:
        fs = [r for r in funcs if r["section"] == s.name]
        cov[s.name] = {"raw_size": s.rsize, "unique_code_bytes_reached": len(reached[s.name]),
                       "functions": len(fs), **{c: sum(1 for r in fs if r["confidence"] == c) for c in RANK}}
    cls = {}
    for td_va, n in sorted(tds.items()):
        k, nm = demangle_td(n)
        cls[n] = {"kind": k, "name": nm, "type_descriptor": "0x%08X" % td_va, "vtables": classes.get(n, [])}
    json.dump(funcs, open(os.path.join(out, "functions.json"), "w"), indent=0)
    json.dump(cls, open(os.path.join(out, "classes.json"), "w"), indent=1)
    json.dump(cov, open(os.path.join(out, "coverage.json"), "w"), indent=1)
    print("functions: %d  (%s)" % (len(funcs), ", ".join("%s %d" % (c, sum(1 for r in funcs if r["confidence"] == c))
                                                          for c in reversed(RANK))))
    print("flagged containing another start: %d" % sum(1 for r in funcs if "contains_other_starts" in r))
    print("RTTI type descriptors: %d, with vtables: %d, vtables: %d" % (
        len(cls), sum(1 for c in cls.values() if c["vtables"]), sum(len(c["vtables"]) for c in cls.values())))
    for n, c in cov.items():
        print("  %-8s raw=%8d reached=%8d (%5.1f%%) funcs=%5d  h/m/l=%d/%d/%d" % (
            n, c["raw_size"], c["unique_code_bytes_reached"], 100.0 * c["unique_code_bytes_reached"] / max(1, c["raw_size"]),
            c["functions"], c["high"], c["medium"], c["low"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
