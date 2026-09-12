"""Scan files for identifiable information before they are committed.

Usage:
    python tools/scan_identifiable.py <file-or-directory> [...]

The search terms are built at run time from this machine's user name, computer name, and
home directory, plus any extra terms in the ISE_SCAN_TERMS environment variable (separated
by semicolons), so the tool itself carries nothing identifiable. Short terms only match as
whole words, so a user name that is part of an ordinary word is not reported.

Every file is scanned as raw bytes in UTF-8 and UTF-16LE. PDFs are also scanned object by
object after decompression, including the document information and XMP metadata. ZIP-based
files (IDML) are scanned entry by entry. Matches are printed with the term masked. The exit
status is 1 when anything matched.
"""
import getpass
import os
import re
import socket
import sys
import zipfile

try:
    import pikepdf
except ImportError:
    pikepdf = None


def build_terms():
    terms = {}
    user = getpass.getuser()
    if user:
        terms["user name"] = user
    host = socket.gethostname()
    if host:
        terms["computer name"] = host
    home = os.path.expanduser("~")
    if home and home != "~":
        terms["home directory"] = home
        terms["home directory (forward slashes)"] = home.replace("\\", "/")
    for i, extra in enumerate(t for t in os.environ.get("ISE_SCAN_TERMS", "").split(";") if t.strip()):
        terms["extra term %d" % (i + 1)] = extra.strip()
    return terms


def compile_patterns(terms):
    pats = []
    for label, term in terms.items():
        for enc in ("utf-8", "utf-16-le"):
            raw = term.encode(enc)
            body = re.escape(raw)
            if enc == "utf-8" and re.fullmatch(r"[A-Za-z0-9_.-]+", term):
                body = rb"(?<![A-Za-z0-9])" + body + rb"(?![A-Za-z0-9])"
            pats.append((label, enc, re.compile(body, re.IGNORECASE)))
    return pats


def scan_bytes(data, where, pats, findings):
    for label, enc, pat in pats:
        for m in pat.finditer(data):
            lo, hi = max(0, m.start() - 24), min(len(data), m.end() + 24)
            context = (data[lo:m.start()] + b"<" + label.upper().encode() + b">" + data[m.end():hi]).decode("latin-1", "replace")
            findings.append((where, label, enc, " ".join(context.split())[:120]))


def scan_pdf(path, pats, findings):
    if pikepdf is None:
        return
    try:
        pdf = pikepdf.open(path)
    except Exception as exc:  # not a readable PDF: the raw scan already ran
        findings.append((path, "unreadable PDF", "-", str(exc)[:80]))
        return
    for key, value in pdf.docinfo.items():
        scan_bytes(str(value).encode("utf-8"), "%s docinfo %s" % (path, key), pats, findings)
    for obj in pdf.objects:
        if isinstance(obj, pikepdf.Stream):
            try:
                data = obj.read_bytes()
            except Exception:
                continue
            scan_bytes(data, "%s stream %s" % (path, obj.objgen), pats, findings)


def scan_file(path, pats, findings):
    with open(path, "rb") as f:
        data = f.read()
    scan_bytes(data, path, pats, findings)
    lower = path.lower()
    if lower.endswith(".pdf"):
        scan_pdf(path, pats, findings)
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            for name in z.namelist():
                scan_bytes(z.read(name), "%s entry %s" % (path, name), pats, findings)


def main(paths):
    pats = compile_patterns(build_terms())
    findings, count = [], 0
    for root in paths:
        if os.path.isdir(root):
            for dirpath, dirnames, filenames in os.walk(root):
                dirnames[:] = [d for d in dirnames if d != ".git"]
                for fn in filenames:
                    scan_file(os.path.join(dirpath, fn), pats, findings)
                    count += 1
        else:
            scan_file(root, pats, findings)
            count += 1
    for where, label, enc, context in findings:
        print("MATCH [%s, %s] %s :: %s" % (label, enc, where, context))
    print("scanned %d files, %d matches" % (count, len(findings)))
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] or ["."]))
