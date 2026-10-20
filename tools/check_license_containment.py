#!/usr/bin/env python3
"""Check that AGPL-3.0 material from the mmx4 decomp has not reached psxport.

  python3 tools/check_license_containment.py                     # scan the framework checkouts it finds
  python3 tools/check_license_containment.py --psxport <dir> ...  # scan these instead

Classes (A, B, C hard; A2, B2 advisory):
  A   AGPL/Affero licence notice, or a code/build file sourcing from the decomp
  A2  the title or decomp merely named
  B   whole-word match on a distinctive identifier from the decomp's C sources
  B2  a name found only in the decomp's symbol map
  C   exact match on a long comment or string literal from the decomp

Not detected: paraphrase, ideas, layouts, vendor/ (unless --include-vendor), ignored files, history.
Refuses (exit 2) rather than report clean when it cannot see the corpus, derives no signatures, scans
no files, or has no baseline.

Exit: 0 nothing found, 1 at least one hit, 2 the tool could not look.
"""
import argparse
import contextlib
import io
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

MINLEN = 8       # identifier signatures must be at least this long
PHRASELEN = 40   # comment/string signatures must be at least this long, after normalisation

# Class A, hard. Code/build files only for the source patterns: docs/workspace/*.md must name the decomp to state the rule.
A_HARD_ANY = [
    r"GNU Affero General Public License",
    r"under the terms of the GNU Affero",
    r"Affero General Public License",
]
A_HARD_CODE = [
    r"external/mmx4",
    r"sozud/mmx4",
    r"\bmmx4/(?:src|include|config|tools)\b",
    r'#\s*include\s*[<"][^">]*mmx4',
]
# Class A, advisory: the title or decomp named without sourcing from it.
A_SOFT = ["mmx4", "sozud", "SLUS_005.61", "SLUS_00561", "megamanx4", "Mega Man", "MegaMan", "AGPL"]

CODE_EXT = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inl", ".s", ".cmake", ".py",
            ".sh", ".ini", ".in", ".json", ".jsonl", ".yaml", ".yml"}


def is_code_or_build(path):
    return (os.path.splitext(path)[1].lower() in CODE_EXT
            or os.path.basename(path) == "CMakeLists.txt")

# Third-party subtrees of the decomp, not mined for signatures.
CORPUS_EXCLUDE_DIRS = {
    os.path.join("include", "psy-q-4.0"),   # verbatim Sony SDK headers, (C) Sony Corporation
    os.path.join("tools", "maspsx"),        # third-party submodules
    os.path.join("tools", "psximager"),
    os.path.join("tools", "asm-differ"),
    ".git",
}
SDK_DIR = os.path.join("include", "psy-q-4.0")

CORPUS_EXT = {".c", ".h", ".s", ".py", ".yaml", ".txt", ".inc"}
# Class B is mined only from files where a token is an identifier; the rest feed class C.
IDENT_SOURCE_EXT = {".c", ".h", ".s", ".inc"}
SCAN_EXT = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inl", ".s", ".py", ".sh",
            ".cmake", ".txt", ".md", ".yaml", ".yml", ".json", ".ini", ".in", ".jsonl", ""}

IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
WS_RE = re.compile(r"\s+")

# Reserved words and libc names are vocabulary, never evidence.
LANGUAGE_WORDS = {
    "unsigned", "continue", "register", "volatile", "restrict", "typedef", "sizeof", "default",
    "include", "defined", "ifndef", "endif", "return", "struct", "static", "extern", "inline",
    "memcpy", "memset", "strncpy", "strcmp", "sprintf", "printf", "malloc", "calloc", "realloc",
}


def die(msg):
    print("[containment] REFUSING: " + msg, file=sys.stderr)
    raise SystemExit(2)


def read_text(path):
    try:
        with open(path, "rb") as fh:
            raw = fh.read()
    except OSError:
        return None
    if b"\x00" in raw[:4096]:
        return None
    return raw.decode("utf-8", "replace")


def corpus_files(corpus):
    """(own_files, sdk_files) under the decomp checkout, or refuse if it is not there."""
    if not os.path.isdir(corpus):
        die(f"{corpus} does not exist — the mmx4 submodule is not checked out. This run compared "
            "NOTHING. `git submodule update --init external/mmx4`.")
    own, sdk = [], []
    for dirpath, dirs, files in os.walk(corpus):
        rel = os.path.relpath(dirpath, corpus)
        rel = "" if rel == "." else rel
        dirs[:] = [d for d in dirs
                   if os.path.join(rel, d) not in CORPUS_EXCLUDE_DIRS and d != ".git"]
        for f in files:
            p = os.path.join(dirpath, f)
            if os.path.splitext(f)[1].lower() in CORPUS_EXT or f.endswith(".H"):
                own.append(p)
    sdk_root = os.path.join(corpus, SDK_DIR)
    for dirpath, _dirs, files in os.walk(sdk_root):
        for f in files:
            sdk.append(os.path.join(dirpath, f))
    if not own:
        die(f"walked {corpus} and found ZERO source files to derive signatures from. A search with no "
            "signatures cannot find anything; that is not a pass.")
    return own, sdk


def strip_prose(text, hash_is_comment):
    """Remove comments and string/char literals, leaving code.

    `hash_is_comment` is false for C headers, where `#define` lines must survive.
    """
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    if hash_is_comment:
        text = re.sub(r"^\s*#[^\n]*", " ", text, flags=re.M)
    text = re.sub(r'"(?:[^"\\\n]|\\.)*"', " ", text)
    text = re.sub(r"'(?:[^'\\\n]|\\.)*'", " ", text)
    return text


def hash_comments(path):
    return os.path.splitext(path)[1].lower() in {".s", ".py", ".sh", ".inc"}


def code_idents(path, text):
    return {w for w in IDENT_RE.findall(strip_prose(text, hash_comments(path))) if len(w) >= MINLEN}


def is_symbol_map(path):
    return os.path.basename(path).startswith("symbols")


def is_ident_source(path):
    """Code files only; the symbol map is handled in build_signatures."""
    return (os.path.splitext(path)[1].lower() in IDENT_SOURCE_EXT
            or os.path.basename(path).endswith(".H"))


def phrases_from(text):
    """Comment bodies and string literals, whitespace-normalised, >= PHRASELEN chars."""
    out = set()
    for m in re.finditer(r"/\*(.*?)\*/", text, re.S):
        body = WS_RE.sub(" ", m.group(1)).strip(" *")
        if len(body) >= PHRASELEN:
            out.add(body)
    for line in text.splitlines():
        for marker in ("//", "#"):
            i = line.find(marker)
            if i >= 0:
                body = WS_RE.sub(" ", line[i + len(marker):]).strip()
                if len(body) >= PHRASELEN:
                    out.add(body)
                break
    for m in re.finditer(r'"((?:[^"\\\n]|\\.){%d,})"' % PHRASELEN, text):
        body = WS_RE.sub(" ", m.group(1)).strip()
        if len(body) >= PHRASELEN:
            out.add(body)
    return out


# Tracked so a bare clone, with no sibling port trees, still subtracts generic vocabulary.
# It stores the intersection actually removed; sibling trees are subtracted on top when present.
# psxport's own vocabulary is deliberately excluded: subtracting the scan target would hide a leak.
BASELINE_FILE = os.path.join("docs", "containment-baseline.txt")


def load_baseline_file(path):
    """-> (set_of_names, note) or (None, why) if the file is not there."""
    if not path or not os.path.isfile(path):
        return None, f"{path} not present"
    names, meta = set(), []
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            if line.startswith("#"):
                meta.append(line)
                continue
            names.add(line.split()[0])
    return names, f"{len(names)} names from {path}"


def walk_vocab(dirs):
    """Every identifier >= MINLEN in these trees, with the file count; prose included."""
    vocab, nfiles = set(), 0
    for b in dirs:
        for dirpath, subs, files in os.walk(b):
            subs[:] = [d for d in subs if d not in (".git", "build", "scratch",
                                                    "external", "__pycache__")]
            for f in files:
                if os.path.splitext(f)[1].lower() in SCAN_EXT:
                    t = read_text(os.path.join(dirpath, f))
                    if t is not None:
                        nfiles += 1
                        vocab |= {w for w in IDENT_RE.findall(t) if len(w) >= MINLEN}
    return vocab, nfiles


def build_signatures(corpus, baselines, verbose=True, baseline_file=None,
                     require_baseline=False):
    own, sdk = corpus_files(corpus)
    idents, mapnames, phrases = set(), set(), set()
    ident_srcs, map_srcs = 0, 0
    for p in own:
        t = read_text(p)
        if t is None:
            continue
        if is_ident_source(p):
            ident_srcs += 1
            idents |= code_idents(p, t)
        elif is_symbol_map(p):
            map_srcs += 1
            for line in t.splitlines():
                m = re.match(r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*=", line)
                if m and len(m.group(1)) >= MINLEN:
                    mapnames.add(m.group(1))
        phrases |= phrases_from(t)
    raw_idents, raw_map = len(idents), len(mapnames)

    sdk_vocab = set()
    for p in sdk:
        t = read_text(p)
        if t is not None:
            # Stripped like the mining side, so comment prose does not widen the subtraction.
            sdk_vocab |= code_idents(p, t)
    idents -= sdk_vocab
    idents -= LANGUAGE_WORDS
    mapnames -= sdk_vocab
    mapnames -= LANGUAGE_WORDS
    mapnames -= idents
    after_sdk = len(idents)

    file_names, file_note = load_baseline_file(baseline_file)
    base_vocab, base_files = walk_vocab(baselines)
    if require_baseline and file_names is None and not baselines:
        die("class B has NO baseline: the tracked exclusion list is missing "
            f"({file_note}) and no unrelated port tree was found next to this repo. Without it "
            "\"distinctive\" is undefined — generic vocabulary reads as a leak (measured: 27 false "
            "HARD hits in a bare clone). Subtracted NOTHING, so this run compared signatures that are "
            "not evidence. Restore the file, or regenerate it with "
            "`--write-baseline` next to the sibling port trees, or pass --no-baseline to audit anyway.")
    excluded = (file_names or set()) | base_vocab
    idents -= excluded
    mapnames -= excluded
    if verbose:
        print(f"[containment] corpus: {len(own)} decomp source files under {corpus}")
        print(f"[containment]   identifiers >= {MINLEN} chars from {ident_srcs} CODE files "
              f"(comments and string literals stripped): {raw_idents}")
        print(f"[containment]   minus Sony PSY-Q SDK vocabulary ({len(sdk_vocab)} names from "
              f"{len(sdk)} files in {SDK_DIR}): {after_sdk}")
        if file_names is not None:
            print(f"[containment]   minus the TRACKED generic-vocabulary exclusion list "
                  f"({file_note}) — portable, works in a bare clone")
        else:
            print(f"[containment]   NO TRACKED EXCLUSION LIST ({file_note}) — "
                  + ("relying on the sibling port trees instead, which a bare clone will NOT have."
                     if baselines else
                     "and no sibling port tree either: class B subtracted NOTHING and its hits are "
                     "NOT evidence (--no-baseline was passed, or the file was deleted)."))
        if baselines:
            print(f"[containment]   minus sibling port-tree vocabulary ({len(base_vocab)} names from "
                  f"{base_files} files in {len(baselines)} unrelated tree(s))")
        print(f"[containment]   class B signatures after every subtraction: {len(idents)}")
        print(f"[containment]   symbol-map-ONLY names from {map_srcs} map file(s) (class B2,"
              f"ADVISORY — a decomp symbol map also lists Sony BIOS/SPU API names): "
              f"{len(mapnames)} of {raw_map}")
        print(f"[containment]   phrase signatures >= {PHRASELEN} chars: {len(phrases)}")
    if not idents and not phrases:
        die("derived ZERO identifier and ZERO phrase signatures from the corpus. Nothing could have "
            "matched; reporting a clean tree here would be a lie.")
    return idents, mapnames, phrases, {"corpus_files": len(own), "sdk_names": len(sdk_vocab),
                                       "idents": len(idents), "mapnames": len(mapnames),
                                       "phrases": len(phrases), "baseline_files": base_files,
                                       "baseline_note": file_note if file_names is not None
                                       else "NO tracked exclusion list: " + file_note,
                                       "baseline_dirs": len(baselines),
                                       "raw_idents": raw_idents,
                                       "after_sdk": after_sdk}


def write_baseline(corpus, baselines, path):
    """Regenerate the tracked exclusion list: the signatures unrelated ports also use.

    Refuses without a sibling tree; an empty list would subtract nothing.
    """
    if not baselines:
        die("--write-baseline found no unrelated port tree to learn generic vocabulary FROM (looked "
            f"for {', '.join(SIBLING_TREES)} next to {os.path.abspath(os.path.join(ROOT, '..'))}). "
            "Writing an empty list would be a baseline in name only.")
    idents, mapnames, _phrases, _st = build_signatures(corpus, [], verbose=False)
    vocab, nfiles = walk_vocab(baselines)
    hit = sorted((idents | mapnames) & vocab)
    if not hit:
        die(f"the {len(idents | mapnames)} signatures and the {len(vocab)} names from "
            f"{nfiles} files in {len(baselines)} sibling tree(s) INTERSECT IN NOTHING. Either the "
            "sibling trees are empty or the mining changed shape; an empty list is not a baseline.")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("# containment-baseline.txt — generic vocabulary EXCLUDED from class B.\n"
                 "# Generated by `tools/check_license_containment.py --write-baseline`. Tracked on\n"
                 "# purpose: the subtraction used to come from sibling port trees OUTSIDE this repo,\n"
                 "# so a bare clone silently subtracted nothing and generic words read as leaks.\n"
                 "#\n"
                 "# Each line is a decomp signature (identifier >= %d chars, mined from the decomp's\n"
                 "# CODE with comments and strings stripped) that ALSO occurs in an unrelated port\n"
                 "# tree, i.e. it is vocabulary rather than evidence. A real leak of one of these\n"
                 "# names is INVISIBLE to class B — that is the price, and it is why the list is\n"
                 "# short enough to read.\n"
                 "#\n"
                 "# corpus:   %s\n"
                 "# learned from: %s (%d files)\n"
                 "# signatures before this subtraction: %d\n"
                 "# excluded by this file: %d\n"
                 % (MINLEN, os.path.relpath(corpus, ROOT),
                    ", ".join(os.path.relpath(b, os.path.join(ROOT, "..")) for b in baselines),
                    nfiles, len(idents | mapnames), len(hit)))
        for n in hit:
            fh.write(n + "\n")
    print(f"[containment] wrote {len(hit)} excluded name(s) to {path} "
          f"(from {len(idents | mapnames)} signatures against {len(vocab)} names in {nfiles} files)")
    return 0


def scan_files(tree, include_vendor):
    """Files psxport would ship: tracked + untracked-not-ignored, or a plain walk if not a git tree."""
    files, how = [], "git ls-files -co --exclude-standard"
    try:
        out = subprocess.run(["git", "-C", tree, "ls-files", "-co", "--exclude-standard"],
                             capture_output=True, text=True, timeout=120)
        # Drop empty strings so a git listing of nothing falls through to the walk.
        rels = [r for r in out.stdout.split("\n") if r] if out.returncode == 0 else []
    except (OSError, subprocess.SubprocessError):
        rels = []
    if rels:
        for rel in rels:
            if not rel:
                continue
            if not include_vendor and rel.startswith("vendor/"):
                continue
            if os.path.splitext(rel)[1].lower() in SCAN_EXT:
                files.append(os.path.join(tree, rel))
    else:
        how = ("filesystem walk (not a git checkout, or git listed NO files — an empty checkout, "
               "or one every file of which is git-ignored)")
        for dirpath, dirs, fs in os.walk(tree):
            dirs[:] = [d for d in dirs if d not in (".git", "scratch", "__pycache__")
                       and not d.startswith("build")
                       and (include_vendor or d != "vendor")]
            for f in fs:
                if os.path.splitext(f)[1].lower() in SCAN_EXT:
                    files.append(os.path.join(dirpath, f))
    return files, how


def _word_re(words):
    return re.compile(r"\b(" + "|".join(sorted(map(re.escape, words))) + r")\b") if words else None


def scan(tree, idents, mapnames, phrases, include_vendor, self_path):
    files, how = scan_files(tree, include_vendor)
    hits, scanned = [], 0
    ident_re, map_re = _word_re(idents), _word_re(mapnames)
    for p in files:
        if os.path.abspath(p) == self_path:
            continue                      # this tool quotes the markers it hunts for
        t = read_text(p)
        if t is None:
            continue
        scanned += 1
        lines = t.splitlines()
        code = is_code_or_build(p)
        for i, line in enumerate(lines, 1):
            for pat in A_HARD_ANY + (A_HARD_CODE if code else []):
                if re.search(pat, line, re.I):
                    hits.append(("A licence/source", p, i, pat, line.strip()[:160]))
            for mk in A_SOFT:
                if mk.lower() in line.lower():
                    hits.append(("A2 name-mention", p, i, mk, line.strip()[:160]))
            if ident_re:
                for m in ident_re.finditer(line):
                    hits.append(("B identifier", p, i, m.group(1), line.strip()[:160]))
            if map_re:
                for m in map_re.finditer(line):
                    hits.append(("B2 map-name", p, i, m.group(1), line.strip()[:160]))
        flat = WS_RE.sub(" ", t)
        for ph in phrases:
            if ph in flat:
                idx = flat.index(ph)
                hits.append(("C phrase", p, 1 + t[:idx].count("\n"), ph[:80], ""))
    return hits, scanned, how, len(files)


def default_psxport_trees():
    # A set but wrong $PSXPORT_DIR must refuse, not scan a different checkout.
    env = os.environ.get("PSXPORT_DIR")
    if env is not None and env != "" and not os.path.isdir(env):
        die(f"$PSXPORT_DIR={env} is not a directory — refusing to silently scan a different "
            "checkout. Unset it to fall back to external/psxport, or fix the path.")
    cands = [env,
             os.path.join(ROOT, "external", "psxport"),
             os.path.abspath(os.path.join(ROOT, "..", "psxport"))]
    seen, out = set(), []
    for c in cands:
        if c and os.path.isdir(c):
            r = os.path.realpath(c)
            if r not in seen:
                seen.add(r)
                out.append(c)
    return out


SIBLING_TREES = ("Tomba2Engine", "spyro", "spider1", "vagrant")


def default_baselines():
    ws = os.path.abspath(os.path.join(ROOT, ".."))
    out = []
    for name in SIBLING_TREES:
        g = os.path.join(ws, name, "game")
        if os.path.isdir(g):
            out.append(g)
    return out


def run(corpus, trees, baselines, include_vendor, strict=False, show_advisory=False,
        baseline_file=None, require_baseline=True):
    self_path = os.path.abspath(__file__)
    if not trees:
        die("found no psxport checkout to scan (looked at $PSXPORT_DIR, "
            f"{os.path.join(ROOT, 'external/psxport')}, and ../psxport). Scanned NOTHING.")
    idents, mapnames, phrases, stats = build_signatures(corpus, baselines,
                                                        baseline_file=baseline_file,
                                                        require_baseline=require_baseline)
    total_hits, total_scanned, total_listed = [], 0, 0
    for t in trees:
        hits, scanned, how, listed = scan(t, idents, mapnames, phrases, include_vendor, self_path)
        print(f"[containment] scanned {scanned} text files ({listed} listed via {how}) in {t}"
              f"{'' if include_vendor else ' — vendor/ EXCLUDED'}")
        total_hits += hits
        total_scanned += scanned
        total_listed += listed
    # The enumerator can return nothing (uninitialised submodule, all files ignored); refuse rather than pass over zero files.
    if total_scanned == 0:
        die(f"scanned ZERO text files across {len(trees)} checkout(s) "
            f"({', '.join(trees)}) — {total_listed} file(s) were even enumerated. The framework "
            "checkout(s) are empty, absent or entirely git-ignored, so NOTHING was compared. This is "
            "NOT a clean tree; a leak sitting in there would have looked exactly like this. "
            "`git submodule update --init external/psxport`, or point --psxport / $PSXPORT_DIR at a "
            "populated checkout.")
    print()
    hard = [h for h in total_hits if h[0][:2] not in ("A2", "B2")]
    advisory = [h for h in total_hits if h[0][:2] in ("A2", "B2")]
    if hard:
        print(f"[containment] FAIL — {len(hard)} hit(s) in the HARD classes (A licence/source, "
              "B code identifier, C copied phrase). Each needs a human decision: remove it from "
              "psxport, or move it into this game repo.")
        for cls, p, ln, what, ctx in hard[:200]:
            print(f"  {cls:16} {p}:{ln}  <{what}>  {ctx}")
        if len(hard) > 200:
            print(f"  ... and {len(hard) - 200} more")
    if advisory:
        per = {}
        for cls, p, _ln, _w, _c in advisory:
            per[(cls, p)] = per.get((cls, p), 0) + 1
        print(f"[containment] ADVISORY — {len(advisory)} hit(s) in {len(per)} file(s), EXPECTED to be "
              "false positives, summarised not listed"
              + (" (counted toward the exit code: --strict)" if strict
                 else " (not counted toward the exit code; --strict counts them, --show-advisory "
                      "lists them)") + ":")
        print("  A2 name-mention = the title/decomp NAMED without sourcing from it. psxport is the "
              "home of docs/workspace/*.md, which must name this game to STATE the containment rule.")
        print("  B2 map-name     = a name found ONLY in the decomp's symbol map, never in its C "
              "sources. That map also lists Sony BIOS/SPU/GPU API names psxport legitimately uses.")
        for (cls, p), n in sorted(per.items(), key=lambda kv: -kv[1]):
            print(f"  {cls:16} {n:4}x  {p}")
        if show_advisory:
            for cls, p, ln, what, ctx in advisory:
                print(f"    {cls:16} {p}:{ln}  <{what}>  {ctx}")
    if hard or (strict and advisory):
        return 1
    print(f"[containment] PASS — scanned {total_scanned} text files across {len(trees)} framework "
          f"checkout(s) against {len(A_HARD_ANY) + len(A_HARD_CODE)} hard licence/source patterns, "
          f"{stats['idents']} distinctive code identifiers and {stats['phrases']} phrases from "
          f"{stats['corpus_files']} decomp source files (plus {len(A_SOFT)} advisory name markers and "
          f"{stats['mapnames']} advisory symbol-map names): 0 hard matches, "
          f"{len(advisory)} advisory.")
    print(f"[containment] class B baseline: {stats['baseline_note']}"
          f" + {stats['baseline_dirs']} sibling port tree(s)")
    print("[containment] CANNOT SEE, and this result says nothing about any of it: paraphrased or "
          "re-typed logic, renamed identifiers, struct layouts / field offsets / address maps carried "
          "in a human's head, "
          + ("" if include_vendor else "anything under vendor/, ")
          + "binary and git-ignored files, and psxport's git HISTORY (working tree only).")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--corpus", default=os.path.join(ROOT, "external", "mmx4"),
                    help="the AGPL decomp checkout (default: external/mmx4)")
    ap.add_argument("--psxport", action="append", default=[],
                    help="framework checkout to scan; repeatable. Default: $PSXPORT_DIR, "
                         "external/psxport, ../psxport (whichever exist)")
    ap.add_argument("--baseline", action="append", default=None,
                    help="unrelated port tree whose vocabulary is subtracted from class B; "
                         "repeatable. Default: the sibling game trees' game/ dirs")
    ap.add_argument("--baseline-file", default=os.path.join(ROOT, BASELINE_FILE),
                    help=f"tracked list of generic names excluded from class B (default: "
                         f"{BASELINE_FILE}). This is what makes the check work in a bare clone")
    ap.add_argument("--no-baseline", action="store_true",
                    help="audit with NO generic-vocabulary subtraction at all. Over-reports by "
                         "construction (measured: 27 false HARD hits); use to see what the "
                         "exclusion list is hiding, never as a gate")
    ap.add_argument("--write-baseline", action="store_true",
                    help=f"regenerate {BASELINE_FILE} from the sibling port trees, then exit")
    ap.add_argument("--include-vendor", action="store_true",
                    help="also scan psxport/vendor/ (noisy: third-party PSX code)")
    ap.add_argument("--strict", action="store_true",
                    help="count the ADVISORY classes (A2 name-mention, B2 symbol-map name) toward "
                         "the exit code")
    ap.add_argument("--show-advisory", action="store_true",
                    help="list every advisory hit instead of summarising per file")
    a = ap.parse_args()
    baselines = a.baseline if a.baseline is not None else default_baselines()
    if a.write_baseline:
        raise SystemExit(write_baseline(a.corpus, baselines, a.baseline_file))
    trees = a.psxport or default_psxport_trees()
    if a.no_baseline:
        baselines, a.baseline_file = [], None
    raise SystemExit(run(a.corpus, trees, baselines, a.include_vendor, a.strict,
                         a.show_advisory, baseline_file=a.baseline_file,
                         require_baseline=not a.no_baseline))


if __name__ == "__main__":
    main()
