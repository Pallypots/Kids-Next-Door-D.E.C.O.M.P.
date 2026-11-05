#!/usr/bin/env python3
"""Relocation-aware comparison of MSVC COFF (i386) object functions against the reference XBE.

This is the project's Xbox checker. It replaces nothing in the supplied matching-decomp skill (whose dk.py is
MIPS/ELF only); it is written for this project and qualified by tests/test_xcheck.py.

Rules (docs/verification.md):
  * Candidate = the function's COMDAT section (compile with /Gy) or the bytes from its symbol to the next symbol.
  * Its length must equal the original function size recorded in config (catches removed / truncated code).
  * Every byte not covered by a relocation must be identical.
  * Every relocation must resolve by NAME through the symbol map; an unknown symbol is UNRESOLVED (fail), never
    masked. DIR32: original u32 == map[sym] + addend. REL32: field_va + 4 + s32(original) == map[sym] + addend.
  * Other relocation types fail as UNSUPPORTED until a regression test exists for them.

Result codes: MATCH, MISMATCH, SIZE, UNRESOLVED, UNSUPPORTED, MISSING.
"""
import struct

IMAGE_FILE_MACHINE_I386 = 0x14C
REL_I386 = {0x0006: "DIR32", 0x0014: "REL32", 0x0007: "DIR32NB", 0x000B: "SECREL", 0x000A: "SECTION"}
IMAGE_SCN_LNK_COMDAT = 0x1000
IMAGE_SCN_CNT_CODE = 0x20


class CoffError(Exception):
    pass


class Coff:
    def __init__(self, data):
        self.data = d = bytes(data)
        if len(d) < 20:
            raise CoffError("too small for a COFF header")
        (self.machine, nsec, _, self.symptr, self.nsym, opt, _) = struct.unpack_from("<HHIIIHH", d, 0)
        if self.machine != IMAGE_FILE_MACHINE_I386:
            raise CoffError("not an i386 COFF object (machine 0x%X)" % self.machine)
        strtab_off = self.symptr + 18 * self.nsym
        if strtab_off + 4 > len(d):
            raise CoffError("symbol/string table outside file")
        strsize = struct.unpack_from("<I", d, strtab_off)[0]
        self.strtab = d[strtab_off:strtab_off + strsize]
        self.sections = []
        for i in range(nsec):
            o = 20 + opt + 40 * i
            if o + 40 > len(d):
                raise CoffError("section header %d truncated" % i)
            name = d[o:o + 8].rstrip(b"\0").decode("latin-1")
            vsize, va, size, ptr, prel, _, nrel, _, ch = struct.unpack_from("<IIIIIIHHI", d, o + 8)
            if name.startswith("/"):
                name = self._str(int(name[1:]))
            if ptr + size > len(d) or prel + 10 * nrel > len(d):
                raise CoffError("section %s data/relocations outside file" % name)
            relocs = [struct.unpack_from("<IIH", d, prel + 10 * k) for k in range(nrel)]
            self.sections.append({"name": name, "raw": d[ptr:ptr + size] if ptr else b"\0" * size,
                                  "relocs": relocs, "flags": ch, "index": i + 1})
        self.symbols = []
        k = 0
        while k < self.nsym:
            o = self.symptr + 18 * k
            raw = d[o:o + 8]
            name = self._str(struct.unpack_from("<I", raw, 4)[0]) if raw[:4] == b"\0\0\0\0" else \
                raw.rstrip(b"\0").decode("latin-1")
            value, secno, typ, cls, naux = struct.unpack_from("<IhHBB", d, o + 8)
            self.symbols.append({"name": name, "value": value, "section": secno, "type": typ, "class": cls,
                                 "index": k})
            for _ in range(naux):
                self.symbols.append(None)
            k += 1 + naux

    def _str(self, off):
        end = self.strtab.find(b"\0", off)
        if off < 4 or end < 0:
            raise CoffError("bad string table offset %d" % off)
        return self.strtab[off:end].decode("latin-1")

    def function(self, name):
        """(bytes, relocs[(offset, type, symname)]) for a defined function symbol, or None if absent."""
        sym = next((s for s in self.symbols if s and s["name"] == name and s["section"] > 0), None)
        if sym is None:
            return None
        sec = self.sections[sym["section"] - 1]
        if not sec["flags"] & IMAGE_SCN_CNT_CODE:
            raise CoffError("%s is not in a code section" % name)
        start = sym["value"]
        ends = sorted(s["value"] for s in self.symbols if s and s["section"] == sym["section"] and
                      s["value"] > start and s["class"] in (2, 3) and s["type"] == 0x20)
        end = ends[0] if ends else len(sec["raw"])
        relocs = []
        for off, symidx, typ in sec["relocs"]:
            if start <= off < end:
                s = self.symbols[symidx]
                relocs.append((off - start, typ, s["name"] if s else "?aux"))
        return sec["raw"][start:end], relocs

    def defined_functions(self):
        return [s["name"] for s in self.symbols if s and s["section"] > 0 and s["type"] == 0x20 and s["class"] == 2]


def compare(cand, relocs, orig, va, symmap, expected_size):
    """Pure comparison core (unit-tested with synthetic inputs). Returns (code, detail)."""
    if len(cand) != expected_size:
        return "SIZE", "candidate %d bytes, original function %d bytes" % (len(cand), expected_size)
    if len(orig) < len(cand):
        return "SIZE", "original range shorter than candidate"
    covered = set()
    for off, typ, name in relocs:
        kind = REL_I386.get(typ)
        if kind not in ("DIR32", "REL32"):
            return "UNSUPPORTED", "relocation type 0x%X (%s) at +0x%X -> %s" % (typ, kind, off, name)
        if off + 4 > len(cand):
            return "MISMATCH", "relocation at +0x%X overruns candidate" % off
        if name not in symmap:
            return "UNRESOLVED", "symbol %s (reloc %s at +0x%X) is not in the symbol map" % (name, kind, off)
        addend = struct.unpack_from("<i", cand, off)[0]
        want = (symmap[name] + addend) & 0xFFFFFFFF
        field = struct.unpack_from("<I", orig, off)[0]
        got = field if kind == "DIR32" else (va + off + 4 + field) & 0xFFFFFFFF
        if got != want:
            return "MISMATCH", "%s at +0x%X: original refers to 0x%08X, candidate %s+%d = 0x%08X" % (
                kind, off, got, name, addend, want)
        covered.update(range(off, off + 4))
    for k in range(len(cand)):
        if k not in covered and cand[k] != orig[k]:
            return "MISMATCH", "byte +0x%X: original %02X, candidate %02X" % (k, orig[k], cand[k])
    return "MATCH", "%d bytes, %d relocations resolved by symbol" % (len(cand), len(relocs))


def check_function(coff, symbol, xbe, va, size, symmap):
    f = coff.function(symbol)
    if f is None:
        return "MISSING", "symbol %s not defined in object (defined: %s)" % (symbol, ", ".join(coff.defined_functions()[:12]))
    cand, relocs = f
    try:
        orig = xbe.read_va(va, max(size, len(cand)))
    except Exception as e:  # noqa: BLE001 - report, do not hide
        return "SIZE", "cannot read original range: %s" % e
    return compare(cand, relocs, orig, va, symmap, size)


def load(path):
    with open(path, "rb") as f:
        return Coff(f.read())
