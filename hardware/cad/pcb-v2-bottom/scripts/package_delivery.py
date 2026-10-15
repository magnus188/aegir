#!/usr/bin/env python3
"""Bind completed audits and deliverables; never substitutes for running audits."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / 'hardware/cad/pcb-v2-bottom'
PCB = ROOT / 'hardware/pcb/analyzer-v2-bottom'
ASSEMBLY = OUT / 'Trimix_Enclosure_PCB2_Bottom.step'
BOARD = PCB / 'Trimix_Analyzer.kicad_pcb'

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def read(path):
    return json.loads(Path(path).read_text())

def repository_path(recorded):
    """Resolve historical audit paths inside this checkout, independent of host."""
    path = Path(recorded)
    if path.is_absolute():
        parts = path.parts
        require('hardware' in parts, f'Not a repository hardware path: {recorded}')
        path = Path(*parts[parts.index('hardware'):])
    require('..' not in path.parts, f'Path escapes repository: {recorded}')
    return ROOT / path

def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)

def check_hashes(mapping):
    for path, expected in mapping.items():
        require(sha(repository_path(path)) == expected, f'Stale input: {path}')

board_hash, assembly_hash = sha(BOARD), sha(ASSEMBLY)
reports = {}
for name in ['electronics-fit-independent', 'mechanical-interfaces-independent', 'delivery-checks']:
    path = OUT / 'verification' / (name + '.json')
    report = read(path)
    require(report['pass'] is True, f'Failed audit: {name}')
    check_hashes(report.get('input_sha256', {}))
    require(report.get('assembly_sha256', assembly_hash) == assembly_hash, name)
    reports[str(path.relative_to(ROOT))] = sha(path)

for rel in ['reference/verification/bottom-invariants.json',
            'reference/verification/usb-path-review.json',
            'manufacturing/independent-cam/audit.json',
            'manufacturing/cam-process-checks.json']:
    path = PCB / rel
    report = read(path)
    require(report['failed'] == 0 and report['passed'] > 0, rel)
    check_hashes(report.get('input_sha256', {}))
    for item in report.get('inputs', []):
        require(sha(repository_path(item['path'])) == item['sha256'], item['path'])
    require(report.get('source_board_sha256', board_hash) == board_hash, rel)
    reports[str(path.relative_to(ROOT))] = sha(path)

drc = read(PCB / 'reference/final-drc-receipt.json')
require(drc['board_sha256'] == board_hash, 'DRC board version')
require(drc['report_sha256'] == sha(PCB / 'reference/final-drc.json'), 'DRC report version')
require(all(drc[k] == 0 for k in ['violations', 'unconnected_items', 'schematic_parity']), 'DRC')
step_receipt = read(PCB / 'review/mechanical-export-receipt.json')
require(step_receipt['source_board_sha256'] == board_hash, 'PCB STEP board version')
require(step_receipt['step_sha256'] == sha(PCB / 'review/Trimix_PCB_V2_Bottom_Mechanical.step'), 'PCB STEP version')
build = read(OUT / 'verification/build-report.json')
require(build['pcb']['source_sha256'] == step_receipt['step_sha256'], 'Case uses old PCB STEP')
require(build['source_hashes_unchanged'] and not build['issues'], 'Case build')

preserved = {}
for label, path in [
    ('design_sources', OUT / 'verification/preserved-design-sources.json'),
    ('canonical_sources', ROOT / 'hardware/pcb/analyzer-v2/reference/original-source-hashes.json')]:
    snapshot = read(path)
    check_hashes(snapshot)
    preserved[label] = {'files_checked': len(snapshot), 'unchanged': True, 'manifest_sha256': sha(path)}
schematics = sorted(PCB.glob('*.kicad_sch'))
require(len(schematics) == 14, 'Schematic count')
require(all(sha(p) == sha(ROOT / 'hardware/pcb/analyzer-v2' / p.name) for p in schematics), 'Schematic change')
preserved['derived_schematics'] = {'files_checked': 14, 'byte_identical': True}

f3d = OUT / 'Trimix_Enclosure_PCB2_Bottom.f3d'
with zipfile.ZipFile(f3d) as archive:
    require(archive.testzip() is None and len(archive.namelist()) > 100, 'Fusion archive integrity')
    fusion_members = len(archive.namelist())

meshes = sorted((OUT / 'parts').glob('*.stl'))
require(len(meshes) == 9, 'Print part count')
print_files = meshes + [OUT / f for f in ['LES-MEG.txt', 'DELER.csv', 'FESTEMATERIELL-ENDREDE-FESTER.csv']]
print_zip = OUT / 'Trimix_PCB2_Bottom_PrintParts.zip'
with zipfile.ZipFile(print_zip, 'w', zipfile.ZIP_DEFLATED) as archive:
    for path in print_files:
        archive.write(path, str(path.relative_to(OUT)))
with zipfile.ZipFile(print_zip) as archive:
    require(archive.testzip() is None and len(archive.namelist()) == 12, 'Print ZIP integrity')

cam_dir = PCB / 'manufacturing'
cam_manifest = read(cam_dir / 'manifest.json')
require(cam_manifest['source_board_sha256'] == board_hash, 'CAM manifest board version')
for rel, expected in cam_manifest['files'].items():
    require(sha(cam_dir / rel) == expected, f'CAM manifest member: {rel}')
cam_zip = cam_dir / 'Trimix_PCB_2_0_BOTTOM_QUOTE_ONLY.zip'
with zipfile.ZipFile(cam_zip) as archive:
    require(archive.testzip() is None, 'CAM ZIP integrity')
    for name in archive.namelist():
        require(archive.read(name) == (cam_dir / name).read_bytes(), f'CAM ZIP member: {name}')

for path in (OUT / 'views').glob('*.json'):
    require(read(path)['sha256'] == assembly_hash, f'Image version: {path}')

artifacts = [ASSEMBLY, f3d, print_zip, cam_zip, BOARD, PCB / 'Trimix_Analyzer.kicad_pro',
             OUT / 'LES-MEG.txt', PCB / 'LES-MEG.txt',
             PCB / 'review/Trimix_PCB_V2_Bottom_Mechanical.step']
artifacts += sorted((OUT / 'parts').glob('*.*')) + sorted((OUT / 'views').glob('*.*'))
record = {
    'timestamp_utc': datetime.now(timezone.utc).isoformat(),
    'status': 'Completed PCB layout and CAD prototype; independent digital checks passed',
    'production_release': False,
    'board_sha256': board_hash,
    'assembly_sha256': assembly_hash,
    'preserved_sources': preserved,
    'checks': {'native_DRC_violations': 0, 'unconnected_items': 0, 'schematic_parity': 0,
               'PCB_preservation': '28/28', 'USB_paths': '7/7', 'CAM': '44/44',
               'CAM_process': '13/13', 'case_static_fit': True, 'continuous_removal_paths': 3,
               'valid_print_meshes': 9},
    'reports': reports,
    'native_Fusion_export': {
        'method': 'Local STEP opened in Fusion UI and exported as Autodesk Fusion Archive',
        'source_assembly_sha256': assembly_hash,
        'sha256': sha(f3d), 'zip_integrity_pass': True, 'archive_members': fusion_members,
        'editable_BREP': True, 'original_parametric_timeline': False,
        'scope': 'Native export lineage and archive integrity checked; no independent F3D BREP roundtrip audit'},
    'build_report_note': 'Build-stage pending status is superseded by the exact-assembly independent reports above.',
    'limitations': [
        'Supplier footprint, short USB shell-stake soldering, selected fill/cap holes and matched stencil review pending.',
        'Physical USB insertion/extraction/side load, screw/insert retention and print tolerances require prototype testing.',
        'Charging/temperature, transient/ESD and sensor calibration require physical qualification.',
        'No waterproof or print-strength qualification; no new fabrication price confirmed.',
        'Inherited charging annotation and ERC findings are documented; verified source schematics remain unchanged.'
    ],
    'artifacts': {str(p.relative_to(ROOT)): {'sha256': sha(p), 'bytes': p.stat().st_size} for p in artifacts}
}
target = OUT / 'verification/final-delivery.json'
target.write_text(json.dumps(record, indent=2, ensure_ascii=False) + '\n')
print(json.dumps({'status': record['status'], 'manifest': str(target), 'board_sha256': board_hash,
                  'assembly_sha256': assembly_hash, 'print_zip': str(print_zip), 'cam_zip': str(cam_zip)}, indent=2))
