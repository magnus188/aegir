#!/usr/bin/env python3
"""Export the immutable saved PCB 2.0 for quotation and engineering review.

No board/schematic mutation, no zone refill, no supplier upload or order.
Run independent verify_variant.py and native filled-zone DRC before exporting.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import re
import shutil
import subprocess
import zipfile
from collections import Counter, defaultdict
from datetime import datetime, timezone
import verify_variant as v

HERE = Path(__file__).resolve().parents[1]
ROOT = HERE.parents[2]
NAME = 'Trimix_Analyzer'
BOARD = HERE/(NAME+'.kicad_pcb')
OUT = HERE/'manufacturing'
CAM = OUT/'cam'


def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def csvwrite(name, rows, fields):
    with (OUT/name).open('w', newline='') as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def order_ref(value):
    match = re.fullmatch(r'([^0-9]+)([0-9]+)', value)
    return (match[1], int(match[2])) if match else (value, 0)


def finalize_package():
    """Refresh hashes/ZIP after audits without regenerating or changing CAM."""
    manifest_path = OUT/'manifest.json'
    receipt = json.loads(manifest_path.read_text())
    assert digest(BOARD) == receipt['source_board_sha256'], 'Board changed since CAM export'
    audits = {}
    for name, path in [('export_geometry', OUT/'independent-cam/audit.json'),
                       ('selected_holes_and_population', OUT/'cam-process-checks.json')]:
        if path.exists():
            audit = json.loads(path.read_text())
            assert audit['failed'] == 0, f'{name} contains unresolved findings'
            audits[name] = {'passed': audit['passed'], 'failed': audit['failed'], 'sha256': digest(path)}
    receipt['independent_CAM_audits'] = audits
    receipt['files'] = {str(p.relative_to(OUT)): digest(p) for p in sorted(OUT.rglob('*'))
                        if p.is_file() and p.name not in ('manifest.json', 'zip-receipt.json') and p.suffix != '.zip'}
    manifest_path.write_text(json.dumps(receipt, indent=2)+'\n')
    package = OUT/'Trimix_PCB_2_0_QUOTE_ONLY.zip'
    include = [p for p in CAM.iterdir() if p.suffix in
               ('.gtl','.g1','.g2','.gbl','.gts','.gbs','.gtp','.gbp','.gto','.gbo','.gm1','.gbrjob','.drl')]
    include += [OUT/n for n in ('selected-fill-cap-holes.csv', 'FABRICATION_AND_ASSEMBLY_REVIEW_NOTES.txt',
                               'QUOTE_ONLY_NOT_FOR_PRODUCTION.txt', 'bom-populated-by-reference.csv',
                               'bom-populated-grouped.csv', 'placement-populated.csv', 'dnp-list.csv',
                               'pcb-features.csv', 'manifest.json', 'native-drc.json', 'source-preservation-audit.json')]
    include += [p for p in (OUT/'cam-process-checks.json', OUT/'independent-cam/audit.json') if p.exists()]
    with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(include):
            archive.write(path, str(path.relative_to(OUT)))
    zip_receipt = {'path': package.name, 'sha256': digest(package),
        'source_board_sha256': receipt['source_board_sha256'], 'file_count': len(include),
        'purpose': 'Quotation/review only, no production release'}
    (OUT/'zip-receipt.json').write_text(json.dumps(zip_receipt, indent=2)+'\n')
    return zip_receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kicad-cli', default=shutil.which('kicad-cli') or
                        '/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli')
    parser.add_argument('--finalize-only', action='store_true', help='Update manifest/ZIP after audits; do not regenerate CAM')
    args = parser.parse_args()
    if args.finalize_only:
        print(json.dumps(finalize_package(), indent=2))
        return
    CAM.mkdir(parents=True, exist_ok=True)
    before = digest(BOARD)
    raw = v.read_sexpr(BOARD)
    setup = v.child(raw, 'setup')
    origin = (v.child(setup, 'aux_axis_origin') or [None, 0, 0])[1:]
    drc = json.loads((HERE/'reference/final-drc.json').read_text())
    assert not any(drc[k] for k in ('violations', 'unconnected_items', 'schematic_parity')), 'Final native DRC is not clear'
    audit = json.loads((HERE/'reference/verification/independent-verification.json').read_text())
    assert audit['failed'] == 0
    assert audit['input_sha256'][str(BOARD.relative_to(ROOT))] == before, 'Invariant report is stale'

    commands = [
        ['pcb', 'export', 'gerbers', '--layers',
         'F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts',
         '--use-drill-file-origin', '-o', str(CAM), str(BOARD)],
        ['pcb', 'export', 'drill', '--drill-origin', 'plot', '--format', 'excellon',
         '--excellon-units', 'mm', '--excellon-zeros-format', 'decimal',
         '--excellon-oval-format', 'route', '--excellon-separate-th', '--generate-map',
         '--map-format', 'svg', '--generate-report', '--report-path', str(CAM/'drill-report.txt'),
         '-o', str(CAM), str(BOARD)],
        ['pcb', 'export', 'pos', '--format', 'csv', '--units', 'mm', '--use-drill-file-origin',
         '--side', 'both', '-o', str(CAM/'placement-all.csv'), str(BOARD)],
    ]
    results = []
    with (OUT/'export.log').open('w') as log:
        for command in commands:
            result = subprocess.run([args.kicad_cli]+command, capture_output=True, text=True)
            results.append({'command': [args.kicad_cli]+command, 'exit_code': result.returncode,
                            'stdout': result.stdout, 'stderr': result.stderr})
            log.write(json.dumps(results[-1], indent=2)+'\n')
            if result.returncode:
                raise RuntimeError(f'KiCad export failed: {command}')
    (OUT/'export-commands.json').write_text(json.dumps(results, indent=2)+'\n')

    positions = list(csv.DictReader((CAM/'placement-all.csv').open()))
    pos_by = {p['Ref']: p for p in positions}
    assert len(positions) == len(pos_by), 'Duplicate placement references'
    footprints = {v.props(f)['Reference'][2]: f for f in v.children(raw, 'footprint')}
    rows = []
    for ref in sorted(footprints, key=order_ref):
        f = footprints[ref]
        p = {key: item[2] for key, item in v.props(f).items()}
        attr = (v.child(f, 'attr') or [None])[1:]
        if 'dnp' in attr:
            category = 'DNP'
        elif ref.startswith(('TP', 'H')) or ref == 'J102' or 'exclude_from_bom' in attr:
            category = 'PCB feature / external termination'
        else:
            category = 'Populate'
        at = v.child(f, 'at')[1:]
        notes = [p[k] for k in ('Assembly', 'Assembly_hold', 'Package_Status', 'Harness_review') if p.get(k)]
        if ref == 'J104':
            notes.append('Fit header only; no charging-arm shunt. Keep OPEN pending commissioning.')
        if ref == 'J102':
            notes.append('Bare protected-pack solder termination, not a discrete purchased connector. Pack/wiring specification is separate.')
        if category == 'DNP':
            notes.append('Do not populate; assembler must omit corresponding paste from the final stencil.')
        if category == 'Populate':
            assert p.get('MPN'), f'Missing exact populated MPN: {ref}'
            assert ref in pos_by, f'Missing populated placement: {ref}'
        rows.append({'Reference': ref, 'Value': p['Value'], 'Manufacturer': p.get('Manufacturer', ''),
                     'MPN': p.get('MPN', ''), 'Quantity': 1, 'Population_quantity': int(category == 'Populate'),
                     'Category': category, 'Footprint': f[1].split(':')[-1],
                     'Side': 'top' if v.child(f, 'layer')[1] == 'F.Cu' else 'bottom',
                     'PCB_X_mm': at[0], 'PCB_Y_mm': at[1],
                     'Rotation_deg': at[2] if len(at)>2 else 0,
                     'Assembly_note': '; '.join(notes),
                     'Native_exclude_from_BOM': 'exclude_from_bom' in attr,
                     'Native_exclude_from_CPL': 'exclude_from_pos_files' in attr})
    fields = list(rows[0])
    csvwrite('assembly-reference-map.csv', rows, fields)
    populated = [r for r in rows if r['Population_quantity']]
    dnp = [r for r in rows if r['Category'] == 'DNP']
    features = [r for r in rows if r['Category'].startswith('PCB feature')]
    csvwrite('bom-populated-by-reference.csv', populated, fields)
    csvwrite('dnp-list.csv', dnp, fields)
    csvwrite('pcb-features.csv', features, fields)
    populated_refs = {r['Reference'] for r in populated}
    matched_pos = [r for r in positions if r['Ref'] in populated_refs]
    assert {r['Ref'] for r in matched_pos} == populated_refs
    csvwrite('placement-populated.csv', matched_pos, list(positions[0]))
    groups = defaultdict(list)
    for row in populated:
        groups[(row['Manufacturer'], row['MPN'], row['Footprint'])].append(row)
    grouped = []
    for key, group in sorted(groups.items()):
        grouped.append({'References': ', '.join(r['Reference'] for r in group),
                        'Manufacturer': key[0], 'MPN': key[1], 'Footprint': key[2],
                        'Quantity_per_board': len(group), 'Quantity_for_2_assembled_boards': 2*len(group),
                        'Values': ' | '.join(sorted({r['Value'] for r in group}))})
    csvwrite('bom-populated-grouped.csv', grouped, list(grouped[0]))

    holes = []
    for ref, pin, expected in (('U101', '25', 9), ('U201', '15', 15)):
        selected = [p for p in v.children(footprints[ref], 'pad') if p[1:3] == [pin, 'thru_hole']]
        assert len(selected) == expected
        for pad in selected:
            x, y = v.world_pad(footprints[ref], pad)
            holes.append({'Class': 'Thermal pad bore', 'Reference': ref, 'Pad': pin,
                          'UUID': v.uid(pad), 'Net': v.board_net(pad), 'PCB_X_mm': x, 'PCB_Y_mm': y,
                          'CAM_X_mm': x-origin[0], 'CAM_Y_mm': origin[1]-y,
                          'Drill_mm': v.child(pad, 'drill')[1], 'Copper_diameter_mm': v.child(pad, 'size')[1],
                          'Required_process': 'Epoxy fill, planarize, copper cap BOTH faces; preserve mask/paste'})
    expected_signal = {'9f64b717-53dd-40b1-b93f-1870e4071216',
                       '4a85dc30-3037-4222-be94-396b63231ef1',
                       'a555d98d-877b-4f68-a4b3-9825501d23c9'}
    selected_signal = []
    for via in v.children(raw, 'via'):
        if v.child(via, 'filling') == ['filling', 'yes'] or v.child(via, 'capping') == ['capping', 'yes']:
            selected_signal.append(v.uid(via))
            assert v.uid(via) in expected_signal, 'Unlisted filled/capped via'
            assert v.child(via, 'filling') == ['filling', 'yes'] and v.child(via, 'capping') == ['capping', 'yes']
            x, y = v.child(via, 'at')[1:]
            holes.append({'Class': 'Retained flagged signal via (now outside solder pad)', 'Reference': '', 'Pad': '',
                          'UUID': v.uid(via), 'Net': v.board_net(via), 'PCB_X_mm': x, 'PCB_Y_mm': y,
                          'CAM_X_mm': x-origin[0], 'CAM_Y_mm': origin[1]-y,
                          'Drill_mm': v.child(via, 'drill')[1], 'Copper_diameter_mm': v.child(via, 'size')[1],
                          'Required_process': 'Epoxy fill, planarize, copper cap BOTH faces; preserve tenting'})
    assert set(selected_signal) == expected_signal and len(holes) == 27
    holes.sort(key=lambda h: (h['Class'], h['Reference'], h['PCB_X_mm'], h['PCB_Y_mm']))
    csvwrite('selected-fill-cap-holes.csv', holes, list(holes[0]))

    note = f'''PCB 2.0 — QUOTATION AND ENGINEERING REVIEW ONLY
NOT RELEASED FOR FABRICATION, STENCIL PURCHASE, ASSEMBLY OR USE.

Source board SHA-256: {before}
Five bare prototype boards requested for pricing. Two complete assembled PCBs
may be priced separately using the full populated BOM; no customer-supplied
PCB components are specified. The board fits both main and USB-C circuits.

FABRICATION
4 copper layers, 45.1 x 99.0 mm nominal bounding box, 1.6 mm finished thickness.
Preserve the saved four-layer stack and copper weights: 1 oz outer / 0.5 oz inner.
Quote JLC04161H-3313, FR4 TG155, green mask, ENIG 1 uin, standard full electrical
test and normal lead time; quote optional Kelvin test separately. Factory must
confirm the quoted stack corresponds to native design and review actual files.
The smallest actual via drill is 0.20 mm, with at least 0.45 mm via copper
diameter. Do not enlarge drills to 0.30 mm based on a calculator category.
No blind/buried vias, edge plating or controlled-impedance service is requested.

SELECTED FILL/CAP — DO NOT APPLY TO ALL HOLES OF A DIAMETER
Exactly the 27 coordinate/UUID-selected holes in selected-fill-cap-holes.csv:
9 U101 thermal bores + 15 U201 thermal bores + 3 retained flagged signal vias.
Epoxy resin fill, planarization and copper cap on BOTH faces are required for
these selected holes. Ordinary soldermask plugging/tenting is not a substitute.
All other routed vias follow the normal tenting shown in the supplied masks.
Do not fill connector pins, USB shell slots, mounting holes or ordinary vias by
matching drill diameter alone. Thermal bores are encoded as plated footprint
pads; the generic via-covering dropdown alone does not identify them.
Factory review must confirm selected-hole classification, actual drill tools,
cap flatness, ENIG and compatibility with the assembler's stencil/reflow process.

COORDINATES
Native PCB coordinates: X right, Y down, millimetres.
Saved drill/place origin in native coordinates: ({origin[0]}, {origin[1]}) mm.
Gerbers, Excellon and position files all use this same saved origin:
CAM X = native X - {origin[0]}; CAM Y = {origin[1]} - native Y.
CAM/POS Y is up. Bottom positions use the same top-view coordinate system,
not a mirrored bottom X convention. Explicit top/bottom fields identify sides.
Excellon output is decimal millimetres; KiCad quantizes positions to 0.001 mm.
CSV hole coordinates retain full native precision for selected-hole identification.
The board outline is the centreline of Edge.Cuts, not its plotted stroke width.

ASSEMBLY
bom-populated-grouped.csv and placement-populated.csv describe the SAME full
populated set ({len(populated)} components per board). All are supplier-sourced;
no old manual/factory population split applies. Grouped quantity-for-two columns
exclude supplier setup attrition, which must be quoted separately.
The native placement-all.csv is a diagnostic audit export including DNP parts.
Do not use it as a population instruction. dnp-list.csv names {len(dnp)} omitted parts.
J104: FIT HEADER ONLY, NO SHUNT. Charging arm stays OPEN pending commissioning.
pcb-features.csv covers test pads, mounting holes and J102 protected-pack solder
termination; these are not discrete components to buy. Pack, NTC, display,
sensors, mating harnesses and enclosure are separate system-level items.
J101 and J902 are not fitted on this integrated board.
J901 is GCT USB4105-GF-A. Its four ground lands have a documented 0.025 mm
rearward offset; supplier DFM approval of this footprint remains pending.
The GF-A shell stakes are nominally 0.95 mm long on a 1.6 mm board and do not
protrude below it. The assembler must approve component-side/reflow soldering
of the plated shell slots, solder fill/wetting and mechanical joint inspection;
do not assume an ordinary protruding-pin wave/hand-solder process is suitable.
The scoped J901 hole-clearance rule is 0.20 mm; other rules remain unchanged.
U901 pins 1-10, 2-9, 4-7 and 5-6 have explicit PCB copper links: the IC itself
does not provide these internal pass-through connections.

PASTE/STENCIL
F.Paste/B.Paste are exact native full-board diagnostic exports. They can contain
apertures for DNP positions. The assembler must omit all dnp-list.csv apertures
and approve a matched full-population stencil, thickness, registration, solder
volume, double-sided reflow and thermal-pad void acceptance. These native paste
files are NOT approved standalone stencil-order files.

REMAINING QUALIFICATION
Native DRC and source-preservation checks are evidence of layout consistency,
not factory approval or measured product safety. Manufacturing review, new USB
mechanical support, solder inspection, startup/charging/thermal/current,
hot-plug/transient/ESD, sensor accuracy and calibration qualification remain.
Existing three inherited schematic ERC findings are not fixed by integration.
No order, payment or release is authorized by this package.
'''
    (OUT/'FABRICATION_AND_ASSEMBLY_REVIEW_NOTES.txt').write_text(note)
    (OUT/'QUOTE_ONLY_NOT_FOR_PRODUCTION.txt').write_text(
        'ENGINEERING REVIEW CANDIDATE. QUOTATION ONLY. NOT APPROVED FOR FABRICATION OR ASSEMBLY.\n'
        'Read FABRICATION_AND_ASSEMBLY_REVIEW_NOTES.txt, the selected fill/cap CSV and the DNP list.\n')
    shutil.copy2(HERE/'reference/final-drc.json', OUT/'native-drc.json')
    shutil.copy2(HERE/'reference/verification/independent-verification.json', OUT/'source-preservation-audit.json')
    assert digest(BOARD) == before, 'Board changed during export'
    receipt = {'timestamp_utc': datetime.now(timezone.utc).isoformat(), 'source_board_sha256': before,
               'source_project_sha256': digest(HERE/(NAME+'.kicad_pro')),
               'source_custom_rules_sha256': digest(HERE/(NAME+'.kicad_dru')),
               'source_schematic_sha256': {p.name: digest(p) for p in sorted(HERE.glob('*.kicad_sch'))},
               'coordinate_origin_native_mm': origin, 'population_counts': dict(Counter(r['Category'] for r in rows)),
               'populated_unique_line_items': len(grouped), 'selected_fill_cap_count': len(holes),
               'source_board_unchanged_after_export': True, 'production_release': False,
               'files': {str(p.relative_to(OUT)): digest(p) for p in sorted(OUT.rglob('*'))
                         if p.is_file() and p.name not in ('manifest.json', 'zip-receipt.json') and p.suffix != '.zip'}}
    (OUT/'manifest.json').write_text(json.dumps(receipt, indent=2)+'\n')
    package_receipt = finalize_package()
    print(json.dumps({'source_board_sha256': before, 'populated_components_per_board': len(populated),
                      'DNP_components': len(dnp), 'PCB_features': len(features), 'fill_cap_holes': len(holes),
                      'zip': package_receipt}, indent=2))


if __name__ == '__main__':
    main()
