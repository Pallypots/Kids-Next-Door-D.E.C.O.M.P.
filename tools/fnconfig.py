"""What is recorded about recovered functions, and where the owner's private files are.

Functions, vftables and symbols live in config/functions.toml and in one file per subsystem under config/functions.d/. Subsystems recovered in
parallel keep their records apart, so that their branches do not collide.

The owner's inputs (orig/, analysis-local/) are ignored by git, so a git worktree has none. private_root() finds the main checkout's copy, read
only by convention. KND_PRIVATE_ROOT overrides it.
"""
import glob
import os
import tomllib


def load(root):
    with open(os.path.join(root, "config", "functions.toml"), "rb") as f:
        cfg = tomllib.load(f)
    for path in sorted(glob.glob(os.path.join(root, "config", "functions.d", "*.toml"))):
        with open(path, "rb") as f:
            more = tomllib.load(f)
        cfg.setdefault("function", []).extend(more.get("function", []))
        cfg.setdefault("vtable", []).extend(more.get("vtable", []))
        for name, va in more.get("symbols", {}).items():
            if cfg.setdefault("symbols", {}).setdefault(name, va) != va:
                raise SystemExit("%s: symbol %s recorded at two addresses" % (path, name))
    seen = set()
    for fn in cfg["function"]:
        if fn["va"] in seen:
            raise SystemExit("function 0x%08X recorded twice" % fn["va"])
        seen.add(fn["va"])
    return cfg


def private_root(root):
    env = os.environ.get("KND_PRIVATE_ROOT")
    if env:
        return env
    if os.path.isdir(os.path.join(root, "orig")):
        return root
    # A worktree's .git is a file: "gitdir: <main>/.git/worktrees/<name>".
    git = os.path.join(root, ".git")
    if os.path.isfile(git):
        with open(git, encoding="utf-8") as f:
            text = f.read().strip()
        if text.startswith("gitdir:") and "/.git/" in text.replace("\\", "/"):
            return text[len("gitdir:"):].strip().replace("\\", "/").split("/.git/")[0]
    return root
