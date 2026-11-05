# Project KND

Source decompilation of **Codename: Kids Next Door – Operation: V.I.D.E.O.G.A.M.E.** (original Xbox, reference
build `knd-xbox-usa-5454008a-v1`), with a native port to grow from verified, recovered subsystems.
This is not a static recompilation or an emulator wrapper. See [CLAUDE.md](CLAUDE.md) for the charter.

**No game data is included.** You need your own disc image; see [docs/provenance.md](docs/provenance.md).

* Status: [docs/STATE.md](docs/STATE.md) · measured progress: [docs/PROGRESS.md](docs/PROGRESS.md)
* How claims are verified: [docs/verification.md](docs/verification.md) · compiler: [docs/toolchain.md](docs/toolchain.md)

```
python -I tools/xdvdfs.py extract "<your disc>.iso" default.xbe orig/xiso/default.xbe
python -I tools/xbe.py info orig/xiso/default.xbe
python -I tools/funcs.py orig/xiso/default.xbe analysis-local/inv
python -I tools/jam.py check "<your disc>.iso"
python -I tools/verify.py
python -I tools/oracle_run.py
python -I -m unittest discover -s tests -v
```

Requires Python 3.11+ with `capstone` and, for verification, MSVC (x86) as configured in `config/toolchain.toml`.
`tools/oracle_run.py` also needs CMake and builds a 32-bit Windows analysis program; it never ships.
