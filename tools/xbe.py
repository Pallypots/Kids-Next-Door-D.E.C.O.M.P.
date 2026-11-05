#!/usr/bin/env python3
"""Read-only, bounds-checked XBE (Xbox executable) parser and inventory.

Field layout: https://xboxdevwiki.net/Xbe . Every pointer stored in the header is a virtual address; header
pointers are resolved inside the raw header image (mapped at base address), everything else through the section
table. Raw (file-backed) and zero-filled (virtual size > raw size) ranges are reported separately.

  python -I tools/xbe.py info <default.xbe>            human-readable summary
  python -I tools/xbe.py json <default.xbe> [out.json]  machine-readable inventory
"""
import hashlib
import json
import struct
import sys

XOR_EP = {"retail": 0xA8FC57AB, "debug": 0x94859D4B, "chihiro": 0x40B5C16E}
XOR_KT = {"retail": 0x5B6D40B6, "debug": 0xEFB1F152, "chihiro": 0x2290059D}

SEC_FLAGS = {0x1: "WRITABLE", 0x2: "PRELOAD", 0x4: "EXECUTABLE", 0x8: "INSERTED_FILE",
             0x10: "HEAD_PAGE_RO", 0x20: "TAIL_PAGE_RO"}
INIT_FLAGS = {0x1: "MOUNT_UTILITY_DRIVE", 0x2: "FORMAT_UTILITY_DRIVE", 0x4: "LIMIT_64MB",
              0x8: "DONT_SETUP_HARDDISK"}


class XbeError(Exception):
    pass


def _u32(d, o):
    if o < 0 or o + 4 > len(d):
        raise XbeError("u32 read at 0x%X outside %d bytes" % (o, len(d)))
    return struct.unpack_from("<I", d, o)[0]


class Section:
    def __init__(self, idx, d, off, xbe):
        if off + 0x38 > len(d):
            raise XbeError("section header %d truncated" % idx)
        (self.flags, self.va, self.vsize, self.raw, self.rsize, self.name_addr, self.name_refcount,
         self.head_ref_addr, self.tail_ref_addr) = struct.unpack_from("<9I", d, off)
        self.digest = d[off + 0x24:off + 0x38]
        self.index = idx
        self.name = xbe.header_cstring(self.name_addr)
        if self.raw + self.rsize > len(d):
            raise XbeError("section %s raw range 0x%X+0x%X beyond file" % (self.name, self.raw, self.rsize))
        if self.rsize > self.vsize:
            raise XbeError("section %s raw size exceeds virtual size" % self.name)

    def flag_names(self):
        return [n for b, n in SEC_FLAGS.items() if self.flags & b] + (
            ["UNKNOWN_0x%X" % (self.flags & ~0x3F)] if self.flags & ~0x3F else [])

    def contains(self, va):
        return self.va <= va < self.va + self.vsize

    def raw_contains(self, va):
        return self.va <= va < self.va + self.rsize


class Xbe:
    def __init__(self, data):
        self.data = d = bytes(data)
        if len(d) < 0x178 or d[:4] != b"XBEH":
            raise XbeError("not an XBE (magic/size)")
        self.signature = d[4:0x104]
        (self.base, self.headers_size, self.image_size, self.image_header_size, self.timedate, self.cert_addr,
         self.num_sections, self.sections_addr, self.init_flags, self.ep_enc, self.tls_addr, self.stack_commit,
         self.heap_reserve, self.heap_commit, self.pe_base, self.pe_image_size, self.pe_checksum, self.pe_timedate,
         self.debug_path_addr, self.debug_file_addr, self.debug_ufile_addr, self.kthunk_enc, self.nonkernel_import_addr,
         self.num_libs, self.libs_addr, self.kernel_lib_addr, self.xapi_lib_addr, self.logo_addr,
         self.logo_size) = struct.unpack_from("<29I", d, 0x104)
        if self.headers_size > len(d) or self.image_header_size > self.headers_size:
            raise XbeError("header sizes inconsistent with file length")
        if not 0 < self.num_sections < 256:
            raise XbeError("implausible section count %d" % self.num_sections)
        self.sections = [Section(i, d, self.hdr_off(self.sections_addr, 0x38 * self.num_sections) + 0x38 * i, self)
                         for i in range(self.num_sections)]
        self._decode_keys()
        self.cert = self._parse_cert()
        self.libraries = self._parse_libs()

    # --- address helpers -------------------------------------------------------------------------------------
    def hdr_off(self, va, n=1):
        off = va - self.base
        if off < 0 or off + n > self.headers_size:
            raise XbeError("header VA 0x%X+%d outside headers" % (va, n))
        return off

    def header_cstring(self, va, limit=256):
        if va == 0:
            return ""
        off = self.hdr_off(va)
        end = self.data.find(b"\0", off, min(off + limit, self.headers_size))
        if end < 0:
            raise XbeError("unterminated header string at 0x%X" % va)
        return self.data[off:end].decode("latin-1")

    def header_wstring(self, va, limit=512):
        if va == 0:
            return ""
        off = self.hdr_off(va)
        out = []
        while off + 2 <= self.headers_size and len(out) < limit:
            c = struct.unpack_from("<H", self.data, off)[0]
            if c == 0:
                return "".join(out)
            out.append(chr(c))
            off += 2
        raise XbeError("unterminated header wide string at 0x%X" % va)

    def section_for(self, va):
        for s in self.sections:
            if s.contains(va):
                return s
        return None

    def va_to_file(self, va, n=1):
        """File offset of a raw-backed VA range; raises for header-external or zero-fill addresses."""
        if self.base <= va and va + n <= self.base + self.headers_size:
            return va - self.base
        for s in self.sections:
            if s.va <= va and va + n <= s.va + s.rsize:
                return s.raw + (va - s.va)
        raise XbeError("VA 0x%X+%d is not file-backed" % (va, n))

    def read_va(self, va, n):
        o = self.va_to_file(va, n)
        return self.data[o:o + n]

    def u32_va(self, va):
        return struct.unpack("<I", self.read_va(va, 4))[0]

    # --- decoded fields ----------------------------------------------------------------------------------------
    def _decode_keys(self):
        cands = []
        for kind in XOR_EP:
            ep = self.ep_enc ^ XOR_EP[kind]
            kt = self.kthunk_enc ^ XOR_KT[kind]
            s_ep, s_kt = self.section_for(ep), self.section_for(kt)
            if s_ep is not None and s_ep.flags & 0x4 and s_kt is not None:
                cands.append((kind, ep, kt))
        if len(cands) != 1:
            raise XbeError("entry point/kernel thunk key ambiguous or invalid: %r" % cands)
        self.variant, self.entry, self.kthunk = cands[0]

    def kernel_imports(self):
        out = []
        va = self.kthunk
        for i in range(400):
            v = self.u32_va(va + 4 * i)
            if v == 0:
                return out
            if not v & 0x80000000:
                raise XbeError("kernel thunk %d not an ordinal import: 0x%08X" % (i, v))
            out.append({"slot_va": va + 4 * i, "ordinal": v & 0x7FFFFFFF})
        raise XbeError("kernel thunk table not terminated")

    def _parse_cert(self):
        o = self.hdr_off(self.cert_addr, 0x1D0)
        d = self.data
        size, timedate, title_id = struct.unpack_from("<III", d, o)
        name = d[o + 0xC:o + 0xC + 80].decode("utf-16-le").split("\0")[0]
        alt_ids = list(struct.unpack_from("<16I", d, o + 0x5C))
        media, region, ratings, disk, version = struct.unpack_from("<5I", d, o + 0x9C)
        return {"size": size, "timedate": timedate, "title_id": "%08X" % title_id, "title_name": name,
                "publisher_code": d[o + 0xF:o + 0xF].decode() if False else self._pub(title_id),
                "alt_title_ids": ["%08X" % a for a in alt_ids if a], "allowed_media": "0x%08X" % media,
                "game_region": "0x%08X" % region, "game_ratings": ratings, "disk_number": disk, "version": version}

    @staticmethod
    def _pub(tid):
        hi = (tid >> 24) & 0xFF, (tid >> 16) & 0xFF
        return "".join(chr(c) if 0x20 < c < 0x7F else "?" for c in hi) + "-%d" % (tid & 0xFFFF)

    def _parse_libs(self):
        if not self.num_libs:
            return []
        o = self.hdr_off(self.libs_addr, 16 * self.num_libs)
        out = []
        for i in range(self.num_libs):
            name = self.data[o + 16 * i:o + 16 * i + 8].split(b"\0")[0].decode("latin-1")
            maj, mnr, build, fl = struct.unpack_from("<4H", self.data, o + 16 * i + 8)
            out.append({"name": name, "version": "%d.%d.%d" % (maj, mnr, build), "qfe": fl & 0x1FFF,
                        "approved": (fl >> 13) & 3, "debug": bool(fl & 0x8000)})
        return out

    def section_digest_ok(self, s):
        """XBE section digest: SHA-1 over (raw size as u32 LE) + raw bytes (xboxdevwiki); reported, not assumed."""
        h = hashlib.sha1(struct.pack("<I", s.rsize) + self.data[s.raw:s.raw + s.rsize]).digest()
        return h == s.digest

    def inventory(self):
        secs = []
        for s in self.sections:
            secs.append({"index": s.index, "name": s.name, "flags": s.flag_names(), "va": "0x%08X" % s.va,
                         "vsize": s.vsize, "raw": "0x%X" % s.raw, "rsize": s.rsize,
                         "raw_range": ["0x%08X" % s.va, "0x%08X" % (s.va + s.rsize)],
                         "zero_fill_range": ["0x%08X" % (s.va + s.rsize), "0x%08X" % (s.va + s.vsize)]
                         if s.vsize > s.rsize else None,
                         "sha256_raw": hashlib.sha256(self.data[s.raw:s.raw + s.rsize]).hexdigest(),
                         "digest_matches": self.section_digest_ok(s)})
        tls = None
        if self.tls_addr:
            st, en, idx, cb, zf, ch = struct.unpack("<6I", self.read_va(self.tls_addr, 24))
            tls = {"addr": "0x%08X" % self.tls_addr, "raw_start": "0x%08X" % st, "raw_end": "0x%08X" % en,
                   "index_addr": "0x%08X" % idx, "callbacks_addr": "0x%08X" % cb, "zero_fill": zf,
                   "characteristics": "0x%X" % ch}
        return {
            "file_sha256": hashlib.sha256(self.data).hexdigest(), "file_size": len(self.data),
            "variant_by_xor_key": self.variant, "base": "0x%08X" % self.base,
            "headers_size": self.headers_size, "image_size": self.image_size,
            "image_header_size": self.image_header_size, "timedate": self.timedate,
            "init_flags": [n for b, n in INIT_FLAGS.items() if self.init_flags & b], "init_flags_raw": self.init_flags,
            "entry_point": "0x%08X" % self.entry, "kernel_thunk": "0x%08X" % self.kthunk,
            "tls": tls, "stack_commit": self.stack_commit, "heap_reserve": self.heap_reserve,
            "heap_commit": self.heap_commit, "pe_base": "0x%08X" % self.pe_base, "pe_image_size": self.pe_image_size,
            "pe_checksum": "0x%08X" % self.pe_checksum, "pe_timedate": self.pe_timedate,
            "debug_path": self.header_cstring(self.debug_path_addr),
            "debug_file": self.header_cstring(self.debug_file_addr),
            "debug_unicode_file": self.header_wstring(self.debug_ufile_addr),
            "nonkernel_import_addr": self.nonkernel_import_addr, "certificate": self.cert,
            "libraries": self.libraries, "sections": secs,
            "kernel_imports": [{"slot_va": "0x%08X" % k["slot_va"], "ordinal": k["ordinal"]}
                               for k in self.kernel_imports()],
        }


def load(path):
    with open(path, "rb") as f:
        return Xbe(f.read())


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    x = load(argv[1])
    inv = x.inventory()
    if argv[0] == "json":
        s = json.dumps(inv, indent=1)
        if len(argv) > 2:
            open(argv[2], "w").write(s)
        else:
            print(s)
        return 0
    c = inv["certificate"]
    print("title     %s  id=%s (%s)  version=%d region=%s media=%s disk=%d" % (
        c["title_name"], c["title_id"], c["publisher_code"], c["version"], c["game_region"], c["allowed_media"],
        c["disk_number"]))
    print("variant   %s   base=%s entry=%s kthunk=%s timedate=%d" % (
        inv["variant_by_xor_key"], inv["base"], inv["entry_point"], inv["kernel_thunk"], inv["timedate"]))
    print("debug     %s | %s" % (inv["debug_path"], inv["debug_file"]))
    print("init      %s  stack=0x%X heap=0x%X/0x%X" % (inv["init_flags"], inv["stack_commit"], inv["heap_reserve"],
                                                     inv["heap_commit"]))
    print("tls       %s" % inv["tls"])
    print("libraries:")
    for l in inv["libraries"]:
        print("  %-8s %-12s qfe=%d approved=%d%s" % (l["name"], l["version"], l["qfe"], l["approved"],
                                                    " DEBUG" if l["debug"] else ""))
    print("sections:")
    for s in inv["sections"]:
        print("  %2d %-10s va=%s vsize=0x%-7X raw=%-8s rsize=0x%-7X digest=%s %s" % (
            s["index"], s["name"], s["va"], s["vsize"], s["raw"], s["rsize"], "ok" if s["digest_matches"] else "BAD",
            ",".join(s["flags"])))
    print("kernel imports: %d" % len(inv["kernel_imports"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
