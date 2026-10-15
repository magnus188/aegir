#!/usr/bin/env python3
"""Verify the current editable hardware and exports without old version folders.

This verifies the frozen delivery, not a fresh electrical or mechanical audit.
Historical comparison tools and their source revisions remain in Git history.
"""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parent.parent
HW = ROOT / 'hardware'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def require(condition, message):
    if not condition:
        raise SystemExit(message)

def main():
    manifest = json.loads((HW / 'current-hardware-manifest.json').read_text())
    for name, expected in manifest['files'].items():
        path = ROOT / name
        require(path.is_file() and sha(path) == expected, 'Missing or changed: ' + name)
    pcb = HW / 'pcb/analyzer-v2-bottom'
    case = HW / 'cad/pcb-v2-bottom'
    receipt = json.loads((pcb / 'reference/final-drc-receipt.json').read_text())
    require(receipt['board_sha256'] == sha(pcb / 'Trimix_Analyzer.kicad_pcb'), 'DRC board mismatch')
    require(receipt['report_sha256'] == sha(pcb / 'reference/final-drc.json'), 'DRC report mismatch')
    require(all(receipt[k] == 0 for k in ['violations','unconnected_items','schematic_parity']), 'DRC failed')
    for name in ['electronics-fit-independent','mechanical-interfaces-independent','delivery-checks']:
        require(json.loads((case / 'verification' / (name + '.json')).read_text())['pass'], 'CAD report failed: ' + name)
    for name in ['independent-cam/audit.json','cam-process-checks.json','usb-path-review.json']:
        require(json.loads((pcb / 'manufacturing' / name).read_text())['failed'] == 0, 'PCB report failed: ' + name)
    plate = json.loads((case / 'verification/print-bed.json').read_text())
    require(plate['project_sha256'] == sha(ROOT / plate['project']), 'Print project mismatch')
    require(plate['mesh_checks_pass'] and plate['printed_part_count'] == 9, 'Print mesh checks failed')
    for name in manifest['files']:
        path = ROOT / name
        if path.suffix.lower() in {'.zip','.f3d','.3mf'}:
            with zipfile.ZipFile(path) as archive:
                require(archive.testzip() is None, 'Archive integrity failed: ' + name)
    print(json.dumps({'status':'PASS','files_checked':len(manifest['files']),
        'recorded_native_DRC':[0,0,0], 'requires_old_version_folders':False,
        'scope':'Frozen artifact integrity and recorded audit outcomes; no new physical qualification'},indent=2))

if __name__ == '__main__':
    main()
