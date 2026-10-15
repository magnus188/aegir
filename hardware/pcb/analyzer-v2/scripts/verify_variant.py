#!/usr/bin/env python3
"""Independent, read-only circuit/layout preservation checks for PCB 2.0.

Uses a standard-library S-expression reader, not the builder's parser or KiCad
object mutation API. Fresh schematic netlists are exported by kicad-cli unless
explicit paths are provided. Writes evidence only below reference/verification.
Passing this audit is not fabrication release or a substitute for native DRC,
CAM/assembly review, mechanical checks or physical qualification.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from collections import Counter
from datetime import datetime, timezone

HERE = Path(__file__).resolve().parents[1]
ROOT = HERE.parents[2]
SOURCE = HERE.parent / "analyzer"
NAME = "Trimix_Analyzer"
PCB_SOURCE_SHA256 = "5382ab1d68a307343b9165bacb90025dee995f66c4689cadbe77df3740eb5f11"
VSYS_SEGMENTS = {
    "18845727-8852-493c-99fc-786af6a05a06",
    "1b8f78a3-4291-426a-99ac-a5700ffa3132",
    "a21ed52b-daff-45ba-9248-a2e63742ae14",
}
VSYS_POINTS = {
    (34.11651, 80.89651): (34.16651, 80.84651),
    (34.1007, 80.89651): (34.1507, 80.84651),
}
REMOVED_J101_STUB = "a38913a3-74e5-452c-a094-3e0a9727ca4d"
INTEGRATED = {"J901", "U901", "D901"}
MEMBERSHIP = INTEGRATED | {"J101"}
USB_METADATA = {
    "Value": "USB4105-GF-A",
    "MPN": "USB4105-GF-A",
    "Footprint": "USB4105:USB_C_Receptacle_GCT_USB4105_GF_A_GroundLandOffset025",
    "Datasheet": "https://gct.co/files/drawings/usb4105.pdf",
    "Board thickness": "1.6 mm; top mount, plated shell slots",
    "Interface": "GCT B4 / KiCad lands with 0.025 mm rearward GND land offset; DFM review required",
}
# Exact non-electrical annotation replacements, independently pinned to original
# text UUIDs. No text position/style, symbol, pin, label or wire edits are allowed.
USB_ANNOTATIONS = {
    "024ae10b-a413-4571-84d0-d253c9dfb851": (
        "01  SEALED USB-C RECEPTACLE", "01  USB-C RECEPTACLE | PCB 2.0"),
    "320c6c03-4158-4a69-90e6-604456f1c7fa": (
        "03  SIX-WIRE OUTPUT", "03  NET REFERENCES | J101/J902 NOT FITTED"),
    "41275033-a1ea-4904-8910-e7d14c44f22f": (
        "TRIMIX USB INPUT | GCT + ESD + SIX-WIRE HARNESS",
        "TRIMIX PCB 2.0 | USB-C + EXISTING ESD CIRCUIT"),
    "99ca8298-63d1-48e8-9658-e8abe05084e6": (
        "Dedicated 0.60 +/-0.10 mm board. Nominal 5 V input; USB-C attachment and BC1.2 detection are on the main PCB.",
        "Integrated 4-layer / 1.6 mm main PCB. Nominal 5 V input; USB-C attachment and BC1.2 detection are unchanged."),
    "b4854b23-7752-4c75-b291-c6f72d56445b": (
        "GCT Rev B cutout and fixed connector pose retained.\nNo copper-edge exception rule;0.10mm GCT edge margin needs closure.\nFactory stencil/reflow for U901; support the port internally.\nMeasure harness/solder height inside the 5.7 mm gap.\nNo 9/12/20 V request; tests required for both cable orientations,\nsource change, hot plug, overcurrent and assembled-port ESD.",
        "USB4105-GF-A top mount; no daughterboard or harness.\nGND lands offset 0.025 mm; supplier DFM approval pending.\nFactory stencil/reflow for U901 and connector shell slots.\nSupport the new PCB tab and socket inside the revised case.\nNo 9/12/20 V request; test both cable orientations, source\nchange, hot plug, temperature, overcurrent and port ESD."),
    "bc8991be-ebb9-4d3a-969b-c11f60549d6d": (
        "1 = USB_5V   2 = GND\n3 = CC1       4 = CC2\n5 = D+        6 = D-\nDo not swap CC/data wires or populate old Rd.",
        "Net references only: no harness is assembled in PCB 2.0.\n1 = USB_5V   2 = GND\n3 = CC1       4 = CC2       5 = D+       6 = D-\nNo extra Rd: existing TUSB320LAI supplies termination."),
}
USB_TITLE = {
    "title": ("Trimix USB input / source and ESD interface", "Trimix PCB 2.0 / integrated USB input and ESD"),
    "rev": ("A3-USB6-ESD", "2.0-layout-01"),
    "date": ("2026-09-07", "2026-10-15"),
}
CHECKS = []


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_sexpr(path):
    """Read exactly one complete KiCad expression, retaining order and scalars."""
    return parse_sexpr(Path(path).read_text())


def parse_sexpr(text):
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+', text)
    stack, root = [], None
    for token in tokens:
        if token == "(":
            item = []
            if stack:
                stack[-1].append(item)
            elif root is not None:
                raise ValueError("Multiple root expressions")
            else:
                root = item
            stack.append(item)
        elif token == ")":
            if not stack:
                raise ValueError("Unbalanced closing parenthesis")
            stack.pop()
        else:
            if not stack:
                raise ValueError("Token outside root expression")
            if token.startswith('"'):
                item = json.loads(token)
            elif re.fullmatch(r"[+-]?\d+", token):
                item = int(token)
            elif re.fullmatch(r"[+-]?(?:\d+\.\d*|\.\d+)(?:[eE][+-]?\d+)?", token):
                item = float(token)
            else:
                item = token
            stack[-1].append(item)
    if stack or root is None:
        raise ValueError("Incomplete expression")
    return root


def children(tree, key):
    return [x for x in tree if isinstance(x, list) and x and x[0] == key]


def child(tree, key):
    values = children(tree, key)
    if len(values) > 1:
        raise ValueError(f"Expected at most one {key}, got {len(values)}")
    return values[0] if values else None


def props(tree):
    return {p[1]: p for p in children(tree, "property")}


def uid(tree):
    return child(tree, "uuid")[1]


def diff(a, b, path="", limit=12):
    """Bounded structured differences, never just a silently accepted exception."""
    if a == b:
        return []
    if isinstance(a, list) and isinstance(b, list):
        out = []
        if len(a) != len(b):
            out.append({"path": path, "length_original": len(a), "length_variant": len(b)})
        for i, (x, y) in enumerate(zip(a, b)):
            out.extend(diff(x, y, f"{path}/{i}", limit-len(out)))
            if len(out) >= limit:
                break
        return out
    return [{"path": path, "expected": a, "actual": b}][:limit]


def check(name, passed, details=None):
    CHECKS.append({"check": name, "passed": bool(passed), "details": details})


def expected_symbol_metadata(tree, filename=None):
    """Apply only explicit assembly metadata and USB-sheet annotations."""
    result = copy.deepcopy(tree)
    changes = []
    if filename == "USB_Input.kicad_sch":
        title = child(result, "title_block")
        for field, (old, new) in USB_TITLE.items():
            current = child(title, field)
            if current[1] != old:
                raise ValueError(f"Original USB title field {field} no longer matches the reviewed source")
            current[1] = new
            changes.append({"annotation": "title_block", "field": field, "old": old, "new": new})
        comment = next(c for c in children(title, "comment") if c[1] == 2)
        old = "GCT edge margin unresolved; factory assembly and physical tests pending."
        new = "Same electrical nets; new connector/layout require DFM and prototype verification."
        if comment[2] != old:
            raise ValueError("Original USB title comment no longer matches reviewed source")
        comment[2] = new
        changes.append({"annotation": "title_block", "field": "comment 2", "old": old, "new": new})
        texts = {uid(t): t for t in children(result, "text")}
        for identity, (old, new) in USB_ANNOTATIONS.items():
            if texts[identity][1] != old:
                raise ValueError(f"Original USB note {identity} no longer matches reviewed source")
            texts[identity][1] = new
            changes.append({"annotation_uuid": identity, "field": "text content only", "old": old, "new": new})
    for symbol in children(result, "symbol"):
        fields = props(symbol)
        ref = fields.get("Reference", [None, None, None])[2]
        if ref not in MEMBERSHIP:
            continue
        for field in ("on_board", "in_bom"):
            target = "no" if ref == "J101" else "yes"
            old = child(symbol, field)[1]
            child(symbol, field)[1] = target
            changes.append({"reference": ref, "field": field, "old": old, "new": target})
        metadata = {"Assembly": "Integrated main PCB 2.0"} if ref in INTEGRATED else {}
        if ref == "J901":
            metadata.update(USB_METADATA)
        for key, value in metadata.items():
            old = fields[key][2]
            fields[key][2] = value
            changes.append({"reference": ref, "field": key, "old": old, "new": value})
    return result, changes


def xml_signature(element):
    return [element.tag, sorted(element.attrib.items()), (element.text or "").strip(),
            [xml_signature(c) for c in element]]


def expected_xml_component(original):
    result = copy.deepcopy(original)
    ref = result.attrib["ref"]
    if ref in MEMBERSHIP:
        for flag in ("exclude_from_board", "exclude_from_bom"):
            existing = result.find(f"./property[@name='{flag}']")
            if existing is not None:
                result.remove(existing)
            if ref == "J101":
                ET.SubElement(result, "property", {"name": flag})
    updates = {"Assembly": "Integrated main PCB 2.0"} if ref in INTEGRATED else {}
    if ref == "J901":
        updates.update(USB_METADATA)
        for name, value in (("value", USB_METADATA["Value"]),
                            ("footprint", USB_METADATA["Footprint"]),
                            ("datasheet", USB_METADATA["Datasheet"])):
            result.find(name).text = value
    for key, value in updates.items():
        field = result.find(f"./fields/field[@name='{key}']")
        if field is not None:
            field.text = value
        prop = result.find(f"./property[@name='{key}']")
        if prop is not None:
            prop.attrib["value"] = value
    return result


def component_signature(element):
    # XML exports may reorder property elements; order has no electrical meaning.
    return sorted([xml_signature(c) for c in element], key=lambda v: json.dumps(v, sort_keys=True))


def net_graph(xmlroot):
    return {n.attrib["name"]: sorted(tuple(sorted(p.attrib.items())) for p in n.findall("node"))
            for n in xmlroot.findall("./nets/net")}


def pad_signature(footprint):
    return {uid(p): p for p in children(footprint, "pad")}


def board_net(pad):
    n = child(pad, "net")
    return n[-1] if n else ""


def world_pad(footprint, pad):
    f = child(footprint, "at")[1:]
    a = child(pad, "at")[1:]
    theta = math.radians(-(f[2] if len(f) > 2 else 0))
    return (f[0]+a[0]*math.cos(theta)-a[1]*math.sin(theta),
            f[1]+a[0]*math.sin(theta)+a[1]*math.cos(theta))


def point_close(a, b):
    return math.dist(a, b) < 0.000003


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variant-board", type=Path, default=HERE/(NAME+".kicad_pcb"))
    parser.add_argument("--output-dir", type=Path, default=HERE/"reference/verification")
    parser.add_argument("--original-netlist", type=Path)
    parser.add_argument("--variant-netlist", type=Path)
    parser.add_argument("--kicad-cli", default=shutil.which("kicad-cli") or
                        "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")
    args = parser.parse_args()
    if bool(args.original_netlist) != bool(args.variant_netlist):
        parser.error("Supply both netlist paths, or neither for fresh exports")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    input_paths = set()

    manifest_path = HERE/"reference/original-source-hashes.json"
    manifest = json.loads(manifest_path.read_text())
    watched_inputs = ({ROOT/p for p in manifest} | set(HERE.glob("*.kicad_sch"))
                      | {args.variant_board, HERE/(NAME+".kicad_pro"), HERE/(NAME+".kicad_dru"),
                         manifest_path, Path(__file__).resolve()})
    start_hashes = {p: sha(p) for p in watched_inputs if p.exists()}
    mismatches = []
    for relative, expected in manifest.items():
        path = ROOT/relative
        actual = sha(path) if path.exists() else None
        input_paths.add(path)
        if actual != expected:
            mismatches.append({"path": relative, "expected": expected, "actual": actual})
    check("All canonical source files still match the frozen hash manifest", not mismatches,
          {"file_count": len(manifest), "mismatches": mismatches})
    check("Canonical PCB matches the independently reviewed revision", 
          sha(SOURCE/(NAME+".kicad_pcb")) == PCB_SOURCE_SHA256)

    schematic_changes, schematic_mismatches = [], []
    originals = list(SOURCE.glob("*.kicad_sch"))
    check("Derived schematic file set matches original", 
          {p.name for p in originals} == {p.name for p in HERE.glob("*.kicad_sch")})
    for source in originals:
        variant = HERE/source.name
        input_paths.add(variant)
        expected, changes = expected_symbol_metadata(read_sexpr(source), source.name)
        schematic_changes.extend({"file": source.name, **c} for c in changes)
        actual = read_sexpr(variant)
        if expected != actual:
            schematic_mismatches.append({"file": source.name, "differences": diff(expected, actual)})
    check("Schematics retain all topology, symbol/pin geometry and UUIDs; only exact listed metadata and USB annotations change",
          not schematic_mismatches, {"allowed_changes": schematic_changes, "mismatches": schematic_mismatches})

    netlist_files = [args.original_netlist, args.variant_netlist]
    export_results = []
    if not args.original_netlist:
        netlist_files = [args.output_dir/"original-netlist.xml", args.output_dir/"variant-netlist.xml"]
        for source, destination in zip([SOURCE/(NAME+".kicad_sch"), HERE/(NAME+".kicad_sch")], netlist_files):
            run = subprocess.run([args.kicad_cli, "sch", "export", "netlist", "--format", "kicadxml",
                                  "--output", str(destination), str(source)], capture_output=True, text=True)
            export_results.append({"source": str(source), "returncode": run.returncode,
                                   "stdout": run.stdout.strip(), "stderr": run.stderr.strip()})
            if run.returncode:
                raise RuntimeError(f"Netlist export failed: {export_results[-1]}")
    input_paths.update(netlist_files)
    oldxml, newxml = [ET.parse(p).getroot() for p in netlist_files]
    oldgraph, newgraph = net_graph(oldxml), net_graph(newxml)
    check("All schematic nets and reference/pin/function/type memberships are identical", oldgraph == newgraph,
          {"original_nets": len(oldgraph), "variant_nets": len(newgraph),
           "different_nets": sorted(n for n in oldgraph.keys() | newgraph.keys() if oldgraph.get(n) != newgraph.get(n))})
    oldcomps = {e.attrib["ref"]: e for e in oldxml.findall("./components/comp")}
    newcomps = {e.attrib["ref"]: e for e in newxml.findall("./components/comp")}
    differences = []
    for ref in oldcomps.keys() | newcomps.keys():
        if ref not in oldcomps or ref not in newcomps:
            differences.append(ref)
        elif component_signature(expected_xml_component(oldcomps[ref])) != component_signature(newcomps[ref]):
            differences.append(ref)
    check("Exported component metadata differs only by explicit USB assembly substitutions", not differences,
          {"original_components": len(oldcomps), "variant_components": len(newcomps), "unexpected_references": sorted(differences)})

    before = read_sexpr(SOURCE/(NAME+".kicad_pcb"))
    after = read_sexpr(args.variant_board)
    input_paths.add(args.variant_board)
    original = {props(f)["Reference"][2]: f for f in children(before, "footprint")}
    variant = {props(f)["Reference"][2]: f for f in children(after, "footprint")}
    check("Physical membership removes only J101 and adds only J901, U901, D901",
          set(variant) == (set(original)-{"J101"}) | INTEGRATED,
          {"removed": sorted(set(original)-set(variant)), "added": sorted(set(variant)-set(original))})
    fp_errors, pad_errors = [], []
    thermal_sizes = []
    for ref, f in original.items():
        if ref == "J101":
            continue
        if ref not in variant:
            fp_errors.append({"reference": ref, "error": "missing"})
            continue
        newf = variant[ref]
        for field in ("at", "layer", "path"):
            if child(f, field) != child(newf, field):
                fp_errors.append({"reference": ref, "field": field, "expected": child(f, field), "actual": child(newf, field)})
        if f[1] != newf[1]:
            fp_errors.append({"reference": ref, "field": "library footprint", "expected": f[1], "actual": newf[1]})
        if uid(f) != uid(newf):
            fp_errors.append({"reference": ref, "field": "UUID"})
        for field in ("Value", "MPN"):
            oldfield, newfield = props(f).get(field), props(newf).get(field)
            if (oldfield[2] if oldfield else None) != (newfield[2] if newfield else None):
                fp_errors.append({"reference": ref, "field": field})
        oldpads, newpads = pad_signature(f), pad_signature(newf)
        if set(oldpads) != set(newpads):
            pad_errors.append({"reference": ref, "error": "pad UUID set changed"})
        for padid, pad in oldpads.items():
            expected = copy.deepcopy(pad)
            if ref == "U201" and pad[1:3] == ["15", "thru_hole"]:
                oldsize = child(expected, "size")[1:]
                child(expected, "size")[1:] = [0.45, 0.45]
                thermal_sizes.append({"uuid": padid, "original": oldsize, "variant": child(newpads.get(padid, []), "size")})
            if newpads.get(padid) != expected:
                pad_errors.append({"reference": ref, "pad": pad[1], "uuid": padid,
                                   "differences": diff(expected, newpads.get(padid))})
    check("All retained footprint positions, orientations, layers, paths, identities and component values are unchanged", not fp_errors, fp_errors)
    check("All original pad geometry and pin-to-net assignments retained, except 15 enlarged U201 thermal lands",
          not pad_errors and len(thermal_sizes) == 15 and all(p["original"] == [.4, .4] for p in thermal_sizes),
          {"changed_thermal_lands": len(thermal_sizes), "mismatches": pad_errors})

    oldcopper = {uid(t): t for kind in ("segment", "via", "arc") for t in children(before, kind)}
    newcopper = {uid(t): t for kind in ("segment", "via", "arc") for t in children(after, kind)}
    copper_errors, changed = [], []
    for identity, item in oldcopper.items():
        if identity == REMOVED_J101_STUB:
            if identity in newcopper:
                copper_errors.append({"uuid": identity, "error": "Redundant J101 terminal stub should be removed"})
            else:
                changed.append(identity)
            continue
        expected = copy.deepcopy(item)
        if identity in VSYS_SEGMENTS:
            for end in ("start", "end"):
                point = child(expected, end)
                point[1:] = VSYS_POINTS.get(tuple(point[1:]), point[1:])
        if newcopper.get(identity) != expected:
            copper_errors.append({"uuid": identity, "differences": diff(expected, newcopper.get(identity))})
        if newcopper.get(identity) != item:
            changed.append(identity)
    check("Every original track/via is preserved except three exact VSYS adjustments and the redundant J101 stub",
          not copper_errors and set(changed) == VSYS_SEGMENTS | {REMOVED_J101_STUB},
          {"original_by_type": dict(Counter(t[0] for t in oldcopper.values())),
           "variant_by_type": dict(Counter(t[0] for t in newcopper.values())),
           "changed_original_uuids": changed, "mismatches": copper_errors})
    stub = oldcopper[REMOVED_J101_STUB]
    terminal = next(p for p in children(original["J101"], "pad") if p[1] == "6")
    centre = world_pad(original["J101"], terminal)
    size = child(terminal, "size")[1:]
    radius = size[0]/2
    halfwidth = child(stub, "width")[1]/2
    endpoint_distances = [math.dist(centre, child(stub, endpoint)[1:]) + halfwidth for endpoint in ("start", "end")]
    # A circle is convex, so both endpoint disks being inside it is sufficient
    # for the full round-ended straight track to be within the old pad copper.
    contained = (terminal[3] == "circle" and size[0] == size[1]
                 and board_net(stub) == board_net(terminal) == "USB_D_M"
                 and child(stub, "layer") == ["layer", "B.Cu"]
                 and all(d <= radius+1e-9 for d in endpoint_distances))
    check("Removed J101.6 terminal stub lay wholly inside the original same-net pad outer boundary", contained,
          {"removed_uuid": REMOVED_J101_STUB, "pad_radius_mm": radius,
           "endpoint_distance_plus_track_radius_mm": endpoint_distances})

    thermal = []
    for ref, pin, count in (("U101", "25", 9), ("U201", "15", 15)):
        old = [p for p in children(original[ref], "pad") if p[1:3] == [pin, "thru_hole"]]
        new = [p for p in children(variant[ref], "pad") if p[1:3] == [pin, "thru_hole"]]
        records = lambda pads: sorted((uid(p), child(p, "at"), child(p, "drill"), child(p, "layers"), board_net(p)) for p in pads)
        identical = records(old) == records(new) and len(old) == len(new) == count
        check(f"{ref}: all {count} thermal bores retain diameter, location, layers and ground connection", identical,
              {"original_count": len(old), "variant_count": len(new)})
        thermal.extend({"reference": ref, "pad": p[1], "uuid": uid(p),
                        "pcb_xy_mm": world_pad(variant[ref], p), "drill": child(p, "drill")[1:],
                        "net": board_net(p)} for p in new)
    check("Exactly 24 original thermal bores remain at 0.20 mm drill", len(thermal) == 24 and all(p["drill"] == [.2] for p in thermal), thermal)
    stack_old, stack_new = child(child(before, "setup"), "stackup"), child(child(after, "setup"), "stackup")
    copper_layers = [l for l in child(after, "layers")[1:] if l[1].endswith(".Cu")]
    check("Four-layer stackup, material/thickness details and layer table remain unchanged",
          stack_old == stack_new and child(before, "layers") == child(after, "layers") and len(copper_layers) == 4,
          {"copper_layer_names": [l[1] for l in copper_layers], "stackup_differences": diff(stack_old, stack_new)})
    check("Board thickness remains unchanged", child(before, "general") == child(after, "general"),
          {"original": child(before, "general"), "variant": child(after, "general")})
    original_settings = json.loads((SOURCE/(NAME+".kicad_pro")).read_text())
    variant_settings = json.loads((HERE/(NAME+".kicad_pro")).read_text())
    check("Global board design and netclass settings remain unchanged",
          all(original_settings.get(k) == variant_settings.get(k) for k in ("board", "net_settings")))
    oldrules = (SOURCE/(NAME+".kicad_dru")).read_text()
    newrules = (HERE/(NAME+".kicad_dru")).read_text()
    rule_suffix = newrules[len(oldrules):] if newrules.startswith(oldrules) else ""
    parsed_suffix = parse_sexpr("(" + re.sub(r"(?m)#.*$", "", rule_suffix) + ")")
    expected_rule = parse_sexpr('''((rule "USB4105 locating holes within adapted footprint"
      (condition "A.Type == 'Pad' && B.Type == 'Pad' && A.memberOfFootprint('J901') && B.memberOfFootprint('J901')")
      (constraint hole_clearance (min 0.20mm))))''')
    check("All original custom rules are retained; sole added exception is J901-to-J901 pad hole clearance 0.20 mm",
          newrules.startswith(oldrules) and parsed_suffix == expected_rule,
          {"original_rules_byte_prefix_unchanged": newrules.startswith(oldrules), "added_rules": parsed_suffix})
    input_paths.update([HERE/(NAME+".kicad_pro"), HERE/(NAME+".kicad_dru")])
    edges = [e for e in children(after, "gr_line") if child(e, "layer") == ["layer", "Edge.Cuts"]]
    endpoints = [tuple(child(e, end)[1:]) for e in edges for end in ("start", "end")]
    dimensions = [max(p[i] for p in endpoints)-min(p[i] for p in endpoints) for i in (0, 1)]
    graph = {p: set() for p in endpoints}
    for edge in edges:
        start, end = (tuple(child(edge, key)[1:]) for key in ("start", "end"))
        graph[start].add(end)
        graph[end].add(start)
    seen, pending = set(), [endpoints[0]] if endpoints else []
    while pending:
        point = pending.pop()
        if point not in seen:
            seen.add(point)
            pending.extend(graph[point]-seen)
    connected = (bool(endpoints) and len(seen) == len(graph)
                 and all(n == 2 for n in Counter(endpoints).values()))
    check("Nominal outline is 45.1 x 99 mm with a closed line loop", 
          all(abs(a-b) < 1e-6 for a, b in zip(dimensions, (45.1, 99))) and connected,
          {"dimensions_mm": dimensions, "edge_segments": len(edges), "one_closed_loop": connected})

    expected_usb = {n: "GND" for n in ("A1", "A12", "B1", "B12", "SH")}
    expected_usb.update({n: "USB_5V" for n in ("A4", "A9", "B4", "B9")})
    expected_usb.update({"A5": "USB_CC1", "B5": "USB_CC2", "A6": "USB_D_P", "B6": "USB_D_P",
                         "A7": "USB_D_M", "B7": "USB_D_M", "A8": "unconnected-(J901-SBU1-PadA8)",
                         "B8": "unconnected-(J901-SBU2-PadB8)"})
    expected_esd = dict(zip(map(str, range(1, 11)),
                           ("USB_CC1", "USB_CC2", "GND", "USB_D_P", "USB_D_M", "USB_D_M", "USB_D_P", "GND", "USB_CC2", "USB_CC1")))
    for ref, expected in (("J901", expected_usb), ("U901", expected_esd), ("D901", {"1": "USB_5V", "2": "GND"})):
        pads = [p for p in children(variant[ref], "pad") if p[1]]
        unexpected = [{"pad": p[1], "expected": expected.get(p[1]), "actual": board_net(p)}
                      for p in pads if expected.get(p[1]) != board_net(p)]
        missing = sorted(set(expected)-{p[1] for p in pads})
        check(f"{ref}: connector/protection pin mapping is exact", not unexpected and not missing,
              {"pad_mapping": [{"pad": p[1], "net": board_net(p)} for p in pads], "mismatches": unexpected, "missing": missing})
    esdpads = {p[1]: p for p in children(variant["U901"], "pad") if p[1]}
    bridges = []
    for first, second, net in (("1", "10", "USB_CC1"), ("2", "9", "USB_CC2"),
                               ("4", "7", "USB_D_P"), ("5", "6", "USB_D_M")):
        a, b = world_pad(variant["U901"], esdpads[first]), world_pad(variant["U901"], esdpads[second])
        matches = [uid(t) for t in children(after, "segment")
                   if board_net(t) == net and child(t, "layer") == ["layer", "F.Cu"]
                   and child(t, "width")[1] >= .15
                   and ((point_close(child(t, "start")[1:], a) and point_close(child(t, "end")[1:], b))
                        or (point_close(child(t, "start")[1:], b) and point_close(child(t, "end")[1:], a)))]
        bridges.append({"pins": [first, second], "net": net, "explicit_direct_copper_segment_uuids": matches})
    check("U901's four NC-pad pairs have real direct PCB copper bridges, not merely matching net names",
          all(b["explicit_direct_copper_segment_uuids"] for b in bridges), bridges)

    changed_during_audit = [str(p) for p, initial in start_hashes.items() if not p.exists() or sha(p) != initial]
    check("Source, variant and verifier inputs remained stable throughout this audit", not changed_during_audit, changed_during_audit)
    input_paths.update([manifest_path, Path(__file__).resolve()])

    report = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "purpose": "Independent preservation and USB integration audit; not a production release",
        "input_sha256": {str(p.relative_to(ROOT) if p.is_relative_to(ROOT) else p): sha(p) for p in sorted(input_paths)},
        "fresh_netlist_exports": export_results,
        "netlist_freshness": "Freshly exported from current files" if export_results else "Caller-supplied XML; source freshness not independently established",
        "checks": CHECKS, "passed": sum(c["passed"] for c in CHECKS), "failed": sum(not c["passed"] for c in CHECKS),
        "limitations": ["Native filled-zone DRC and connectivity are separate mandatory checks.",
                        "No manufactured copper, solder quality, thermal/ESD/transient or functional performance is measured.",
                        "Fill/cap process acceptance and mechanical/assembly validation remain outstanding.",
                        "Explicit U901 bridge test intentionally requires a direct F.Cu segment; a differently routed valid bridge needs review of this test.",
                        "This script does not qualify new copper routing, other than its listed mapping/bridge and preservation checks."]}
    destination = args.output_dir/"independent-verification.json"
    destination.write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n")
    print(f"{report['passed']} passed, {report['failed']} failed: {destination}")
    for c in CHECKS:
        if not c["passed"]:
            print("FAIL:", c["check"])
    return 1 if report["failed"] else 0


if __name__ == "__main__":
    sys.exit(main())
