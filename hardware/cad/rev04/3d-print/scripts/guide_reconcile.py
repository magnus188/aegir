"""Reconcile the guide's purchasing groups with actual native metadata.

Reads root-controlled native exports. Writes only guide-owned source data.
This checks identifiers, quantities and purchased-assembly membership, without
claiming a procurement SKU, print setting or physical fit has been qualified.
"""
from __future__ import annotations
import csv
import hashlib
import json
from pathlib import Path

RELEASE = Path(__file__).resolve().parents[1]
SOURCE = RELEASE / "docs" / "source"


def reconcile():
    native_path = RELEASE / "verification/native-part-map.json"
    assemblies_path = RELEASE / "verification/purchased-assemblies.json"
    native = json.loads(native_path.read_text())
    assembly_export = json.loads(assemblies_path.read_text())
    mapping_path = SOURCE / "part-map.json"
    mapping = json.loads(mapping_path.read_text())
    intended = {p["cad_part_id"]: p for p in mapping["parts"]}
    actual = {p["part_id"]: p for p in native["parts"]}
    if len(actual) != len(native["parts"]) or set(intended) != set(actual):
        raise ValueError("Native part IDs do not exactly match the guide's CAD-definition map.")
    for pid, item in intended.items():
        part = actual[pid]
        expected = (item["original_component_name"], item["procurement_id"], item["category"], item["cad_instance_quantity"])
        found = (part["component"], part["procurement_group"], part["category"], part["quantity"])
        if expected != found:
            raise ValueError(f"Native map mismatch for {pid}: {expected} versus {found}")
        item["native_description"] = part["description"]
        item["native_part_id"] = part["part_id"]
    groups = {a["part_id"]: a for a in assembly_export["assemblies"]}
    if set(groups) != {"TMX-A3-C01", "TMX-A3-C04", "TMX-A3-C05"}:
        raise ValueError("Expected the display, button and USB purchased assembly parents.")
    for pid, assembly in groups.items():
        expected_children = {p["cad_part_id"] for p in intended.values() if p["procurement_id"] == pid}
        if set(assembly["children"]) != expected_children:
            raise ValueError("Purchased assembly children differ: " + pid)
    child_instances = sum(p["quantity"] for p in actual.values())
    if child_instances + len(groups) != assembly_export["occurrences"]:
        raise ValueError("The placed occurrence count does not reconcile with the assembly parents.")
    with (SOURCE / "procurement-bom.csv").open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    by_proc = {r["part_id"]: r for r in rows}
    for pid in groups:
        if int(by_proc[pid]["quantity"]) != 1:
            raise ValueError("Purchased assembly must appear once in the shopping list: " + pid)
    for row in rows:
        pid = row["part_id"]
        if row["category"] == "printed":
            row["status"] = "ID and quantity match revised native metadata; print release and physical fit checks apply"
        if pid in {"TMX-A3-F04", "TMX-A3-F05"}:
            row["status"] = ("Generic insert reference. Exact SKU and coupon retention test required. "
                             "Current M2 holes around 3.2/3.3 mm and M3 holes around 4.3 mm are clearance references. "
                             "Revise full-part pilot holes to the qualified result before heat installation.")
    with (SOURCE / "procurement-bom.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    report = {
        "status": "native_identifiers_quantities_and_assembly_groups_match",
        "native_document": native["document"], "cad_definitions": len(actual),
        "purchased_parent_assemblies": len(groups), "placed_occurrences": assembly_export["occurrences"],
        "printed_parts": sum(p["category"] == "printed" for p in actual.values()),
        "screws": sum(p["quantity"] for p in actual.values() if p["part_id"] in {"TMX-A3-F01", "TMX-A3-F02", "TMX-A3-F03"}),
        "inserts": sum(p["quantity"] for p in actual.values() if p["part_id"] in {"TMX-A3-F04", "TMX-A3-F05"}),
        "native_map_sha256": hashlib.sha256(native_path.read_bytes()).hexdigest(),
        "assembly_export_sha256": hashlib.sha256(assemblies_path.read_bytes()).hexdigest(),
        "scope": "BOM identity and quantity reconciliation only. Supplier choice, print profiles and physical tests remain separate.",
    }
    mapping["status"] = "matched_native_metadata"
    mapping["native_document"] = native["document"]
    mapping["purchased_parent_assemblies"] = assembly_export["assemblies"]
    mapping_path.write_text(json.dumps(mapping, indent=2) + "\n")
    (SOURCE / "native-bom-reconciliation.json").write_text(json.dumps(report, indent=2) + "\n")
    release_path = SOURCE / "release.json"
    release = json.loads(release_path.read_text())
    release["final_native_bom_reconciled"] = True
    release["facts"]["print_material"] = "PLA fit project / PETG target project"
    release["facts"]["license_note"] = "Project-created CAD and print files: CERN-OHL-S-2.0; project-written scripts: GPL-3.0-only. See the repository license map."
    release["evidence"]["native_parts"] = str(native_path.relative_to(RELEASE))
    release["evidence"]["purchased_assemblies"] = str(assemblies_path.relative_to(RELEASE))
    release["evidence"]["bom_reconciliation"] = "docs/source/native-bom-reconciliation.json"
    release_path.write_text(json.dumps(release, indent=2) + "\n")
    return report


if __name__ == "__main__":
    print(json.dumps(reconcile(), indent=2))
