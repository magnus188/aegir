#!/usr/bin/env python3
"""Build-only, exact-preimage ESP-Hosted patch. Never edits managed_components."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def digest(data):
    return hashlib.sha256(data).hexdigest()


def apply_exact(original, hunks):
    """Apply unified hunks with exact line numbers and contents; no fuzz/offsets."""
    old = original.splitlines(keepends=True)
    out = []
    cursor = 0
    pos = 0
    while pos < len(hunks):
        match = re.fullmatch(r"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@.*\n", hunks[pos])
        if not match:
            raise ValueError("invalid patch hunk header")
        start, count, new_start, new_count = (int(x) if x is not None else 1 for x in match.groups())
        start = start - 1 if count else start
        if start < cursor or start > len(old):
            raise ValueError("overlapping/out-of-range hunk")
        out.extend(old[cursor:start])
        cursor = start
        if len(out) != (new_start - 1 if new_count else new_start):
            raise ValueError("new hunk line mismatch")
        pos += 1
        removed = added = 0
        while pos < len(hunks) and not hunks[pos].startswith("@@ "):
            row = hunks[pos]
            if row[:1] not in (" ", "+", "-"):
                raise ValueError("unsupported patch line")
            if row[0] in " -":
                if cursor >= len(old) or old[cursor] != row[1:]:
                    raise ValueError("patch context mismatch")
                cursor += 1
                removed += 1
            if row[0] in " +":
                out.append(row[1:])
                added += 1
            pos += 1
        if (removed, added) != (count, new_count):
            raise ValueError("patch hunk count mismatch")
    out.extend(old[cursor:])
    return "".join(out)


def prepare(source, output, manifest_path):
    source, output = source.resolve(), output.resolve()
    if output == source or source in output.parents or output in source.parents:
        raise ValueError("output must be separate from the managed source tree")
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    if (manifest["component"], manifest["version"], manifest["idf_version"]) != (
        "espressif/esp_hosted", "2.12.9", "5.5.4"
    ):
        raise ValueError("unreviewed component/IDF version")
    component = (source / "idf_component.yml").read_text()
    if not re.search(r"^version: 2\.12\.9$", component, re.M):
        raise ValueError("managed component version mismatch")
    patch = (manifest_path.parent / manifest["patch"]).read_bytes()
    if digest(patch) != manifest["patch_sha256"]:
        raise ValueError("patch hash mismatch")
    lines = patch.decode().splitlines(keepends=True)
    sections = {}
    pos = 0
    while pos < len(lines):
        if not lines[pos].startswith("--- a/"):
            raise ValueError("invalid old file header")
        name = lines[pos][6:].rstrip("\n")
        if pos + 1 >= len(lines) or lines[pos + 1] != "+++ b/" + name + "\n":
            raise ValueError("patch paths differ")
        pos += 2
        start = pos
        while pos < len(lines) and not lines[pos].startswith("--- a/"):
            pos += 1
        if name in sections:
            raise ValueError("duplicate patched path")
        sections[name] = lines[start:pos]
    records = manifest["files"]
    if len(records) != 4 or {r["path"] for r in records} != set(sections):
        raise ValueError("unexpected patched file inventory")
    generated = []
    for row in records:
        relative = Path(row["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("unsafe patch path")
        before = (source / relative).read_bytes()
        if digest(before) != row["upstream_sha256"]:
            raise ValueError("managed source drift: " + str(relative))
        after = apply_exact(before.decode(), sections[row["path"]]).encode()
        if digest(after) != row["patched_sha256"]:
            raise ValueError("patched source hash mismatch: " + str(relative))
        generated.append((relative, before, after))
    # Validate every input before writing any build output, then recheck source.
    for relative, before, _ in generated:
        if (source / relative).read_bytes() != before:
            raise ValueError("concurrent managed source change")
    for relative, _, after in generated:
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists() or destination.read_bytes() != after:
            destination.write_bytes(after)
    receipt = dict(manifest, status="EXACT_PATCH_APPLIED_TO_BUILD_COPY",
                   manifest_sha256=digest(manifest_bytes), source=str(source), output=str(output))
    (output / "patch-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).resolve().parents[1] /
                        "patches/esp_hosted/manifest.json")
    args = parser.parse_args()
    try:
        result = prepare(args.source, args.output, args.manifest)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f"ESP-Hosted patch rejected: {exc}\n")
    print(f"Checked ESP-Hosted {result['version']}: {len(result['files'])} build-only source replacements")


if __name__ == "__main__":
    main()
