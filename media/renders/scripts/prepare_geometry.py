"""Prepare source-faithful CAD meshes and a populated KiCad GLB for Blender.

Run with a Python environment containing CadQuery 2.8, OCP and NumPy.
Example: build/pcb-v2-case-work/geometry-venv/bin/python media/renders/scripts/prepare_geometry.py
Hardware sources are read only. Mesh vertices are in enclosure-world millimeters.
"""
from pathlib import Path
from collections import Counter
import argparse
import csv
import hashlib
import json
import re
import struct
import subprocess

import cadquery as cq
import numpy as np
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TDocStd import TDocStd_Document
from OCP.TCollection import TCollection_ExtendedString
from OCP.TDF import TDF_Label, TDF_LabelSequence
from OCP.TDataStd import TDataStd_Name
from OCP.XCAFDoc import XCAFDoc_DocumentTool
from OCP.TopLoc import TopLoc_Location
from OCP.IFSelect import IFSelect_RetDone
from OCP.Interface import Interface_Static
from OCP.Bnd import Bnd_Box
from OCP.BRepBndLib import BRepBndLib

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'media/renders'
ASSEMBLY = ROOT / 'hardware/cad/pcb-v2-bottom/Trimix_Enclosure_PCB2_Bottom.step'
PCB = ROOT / 'hardware/pcb/analyzer-v2-bottom/Trimix_Analyzer.kicad_pcb'
POPULATION = PCB.parent / 'manufacturing/placement-populated.csv'
HEIGHT_CONTRACT = ROOT / 'hardware/system-review/electrical/component-height-contract.json'
KICAD = Path('/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli')
MISSING = {'L101', 'L201', 'L701', 'U110', 'U111', 'U115'}
TRANSLATION = [30.5, 122.58491, 24.0529]
DESCRIPTIONS = {
    'TMX-A3-P01': 'Main enclosure, current flush two-color logo',
    'TMX-A3-P02': 'Rear cover', 'TMX-A3-P03': 'PCB carrier',
    'TMX-A3-P04': 'Lower display retainer', 'TMX-A3-P05': 'Upper display retainer',
    'TMX-A3-P07': 'Sampling manifold', 'TMX-A3-P08': 'Sampling chamber lid',
    'TMX-A3-P11': 'AO2 threaded adapter', 'TMX-V2-P12': 'USB support',
    'TMX-A3-C01-V01': 'Guition factory display casing, measured envelope',
    'TMX-A3-C01-V02': 'Guition front glass, drawing reference',
    'TMX-A3-C02': 'FMA protected dual-cell holder, owner-measured envelope',
    'TMX-A3-C03-V01': '18650 battery cell A', 'TMX-A3-C03-V02': '18650 battery cell B',
    'TMX-A3-C10-V01': 'Battery RCY disconnect reference',
    'TMX-A3-C10-V02': 'Battery RCY disconnect mating reference',
    'TMX-A3-C11-V01': 'Gas inlet fitting reference',
    'TMX-A3-C11-V02': 'Gas exhaust fitting reference',
    'TMX-A3-C04-V01': 'Momentary illuminated button body',
    'TMX-A3-C04-V02': 'Momentary illuminated button cap',
    'TMX-A3-C04-V03': 'Momentary illuminated button retaining nut',
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def bounds(shape):
    box = Bnd_Box()
    BRepBndLib.AddOptimal_s(shape.wrapped, box, False, False)
    v = list(box.Get())
    return {'min': v[:3], 'max': v[3:], 'size': [v[i+3]-v[i] for i in range(3)]}


def label_name(label):
    name = TDataStd_Name()
    return name.Get().ToExtString() if label.FindAttribute(TDataStd_Name.GetID_s(), name) else ''


def load_step(path):
    """Read world-positioned named leaves without relying on ignored build helpers."""
    reader = STEPCAFControl_Reader()
    reader.SetNameMode(True)
    reader.SetColorMode(True)
    Interface_Static.SetCVal_s('xstep.cascade.unit', 'MM')
    if reader.ReadFile(str(path)) != IFSelect_RetDone:
        raise RuntimeError('Cannot read source STEP')
    doc = TDocStd_Document(TCollection_ExtendedString('render_source'))
    if not reader.Transfer(doc):
        raise RuntimeError('Cannot transfer source STEP')
    shape_tool = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
    roots = TDF_LabelSequence()
    shape_tool.GetFreeShapes(roots)
    leaves = []

    def visit(label, parent, names):
        name = label_name(label)
        definition = label
        world = parent
        if shape_tool.IsReference_s(label):
            definition = TDF_Label()
            shape_tool.GetReferredShape_s(label, definition)
            world = parent.Multiplied(shape_tool.GetLocation_s(label))
        name = name or label_name(definition)
        path_names = names + [name]
        if shape_tool.IsAssembly_s(definition):
            if not shape_tool.IsReference_s(label):
                world = parent.Multiplied(shape_tool.GetLocation_s(definition))
            children = TDF_LabelSequence()
            shape_tool.GetComponents_s(definition, children, False)
            for j in range(1, children.Length()+1):
                visit(children.Value(j), world, path_names)
        else:
            raw = shape_tool.GetShape_s(definition)
            if not raw.IsNull():
                leaves.append({'name': name, 'path': ' / '.join(path_names),
                               'shape': cq.Shape.cast(raw.Moved(world))})
    for i in range(1, roots.Length()+1):
        visit(roots.Value(i), TopLoc_Location(), [])
    return leaves


def part_id(name):
    return re.sub(r'^\d+_', '', name).split(':')[0]


def exclusion(pid):
    if pid.startswith('PCB2-'):
        return 'Mechanical PCB replaced by current KiCad GLB and six dimensional visual reconstructions'
    if pid in {'TMX-A3-C06', 'TMX-A3-C07', 'TMX-A3-C08', 'TMX-A3-C09', 'TMX-A3-C12'}:
        return 'Sensor or connector proxy replaced by appearance model'
    if pid in {'TMX-A3-C01-V03', 'TMX-A3-C01-V04'}:
        return 'Display plane or illustrative PCB replaced by photographed and firmware-derived details'
    if pid.startswith('TMX-A3-S'):
        return 'Engineering seal or feedthrough allowance, not a manufactured visible component'
    return None


def group(pid):
    if '-F' in pid:
        return 'fasteners'
    if pid == 'TMX-A3-P02':
        return 'cover'
    if pid in {'TMX-A3-P03', 'TMX-V2-P12'}:
        return 'frame'
    if pid in {'TMX-A3-P04', 'TMX-A3-P05'} or pid.startswith('TMX-A3-C01'):
        return 'display'
    if pid.startswith(('TMX-A3-C02', 'TMX-A3-C03', 'TMX-A3-C10')):
        return 'battery'
    if pid in {'TMX-A3-P07', 'TMX-A3-P08', 'TMX-A3-P11'}:
        return 'manifold'
    return 'shell'


def sexpr(text):
    stack = []
    root = None
    for token in re.findall(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()]+', text):
        if token == '(':
            stack.append([])
        elif token == ')':
            item = stack.pop()
            if stack:
                stack[-1].append(item)
            else:
                root = item
        else:
            stack[-1].append(json.loads(token) if token.startswith('"') else token)
    return root


def board_inventory():
    rows = []
    for fp in sexpr(PCB.read_text()):
        if not isinstance(fp, list) or fp[0] != 'footprint':
            continue
        fields = {x[1]: x[2] for x in fp if isinstance(x, list) and x[0] == 'property'}
        attr = next((x[1:] for x in fp if isinstance(x, list) and x[0] == 'attr'), [])
        at = next(x[1:] for x in fp if isinstance(x, list) and x[0] == 'at')
        side = next(x[1] for x in fp if isinstance(x, list) and x[0] == 'layer')
        models = [x[1] for x in fp if isinstance(x, list) and x[0] == 'model']
        ref = fields['Reference']
        rows.append({'reference': ref, 'value': fields.get('Value'), 'MPN': fields.get('MPN'),
                     'datasheet': fields.get('Datasheet'), 'footprint': fp[1], 'side': side,
                     'native_xy_mm': [float(at[0]), float(at[1])],
                     'rotation_deg': float(at[2]) if len(at) > 2 else 0,
                     'cad_xy_mm': [float(at[0])+TRANSLATION[0], TRANSLATION[1]-float(at[1])],
                     'dnp': 'dnp' in attr, 'models': models,
                     'visual_reconstruction_required': ref in MISSING})
    return rows


def glb_inventory(path):
    blob = path.read_bytes()
    magic, version, length = struct.unpack_from('<III', blob)
    if magic != 0x46546C67 or version != 2 or length != len(blob):
        raise RuntimeError('Invalid GLB')
    chunk_size = struct.unpack_from('<I', blob, 12)[0]
    data = json.loads(blob[20:20+chunk_size])
    refs = sorted(n['name'] for n in data['nodes'] if re.fullmatch(r'[A-Z]+\d+', n.get('name', '')))
    return {'component_references': refs, 'mesh_count': len(data['meshes']),
            'node_count': len(data['nodes']), 'material_count': len(data.get('materials', [])),
            'mesh_names': [m.get('name') for m in data['meshes']]}, data


def verify_glb_bounds(data, expected_parts):
    """Traverse GLB TRS and accessor extrema, compare known CAD PCB landmarks."""
    def local_matrix(node):
        if 'matrix' in node:
            return np.array(node['matrix']).reshape(4, 4).T
        m = np.eye(4)
        x, y, z, w = node.get('rotation', [0, 0, 0, 1])
        m[:3, :3] = [[1-2*y*y-2*z*z, 2*x*y-2*z*w, 2*x*z+2*y*w],
                      [2*x*y+2*z*w, 1-2*x*x-2*z*z, 2*y*z-2*x*w],
                      [2*x*z-2*y*w, 2*y*z+2*x*w, 1-2*x*x-2*y*y]]
        m[:3, :3] = m[:3, :3] @ np.diag(node.get('scale', [1, 1, 1]))
        m[:3, 3] = node.get('translation', [0, 0, 0])
        return m
    actual = {}
    def visit(idx, parent, ref=None):
        node = data['nodes'][idx]
        matrix = parent @ local_matrix(node)
        if re.fullmatch(r'[A-Z]+\d+', node.get('name', '')):
            ref = node['name']
        if 'mesh' in node:
            mesh = data['meshes'][node['mesh']]
            if mesh.get('name', '').endswith('_PCB'):
                ref = 'substrate'
            if ref in {'substrate', 'J901', 'C403', 'J301'}:
                points = []
                for primitive in mesh['primitives']:
                    accessor = data['accessors'][primitive['attributes']['POSITION']]
                    for x in [accessor['min'][0], accessor['max'][0]]:
                        for y in [accessor['min'][1], accessor['max'][1]]:
                            for z in [accessor['min'][2], accessor['max'][2]]:
                                p = matrix @ np.array([x, y, z, 1])
                                points.append([p[0]*1000+TRANSLATION[0], -p[2]*1000+TRANSLATION[1], p[1]*1000+TRANSLATION[2]])
                actual.setdefault(ref, []).extend(points)
        for child in node.get('children', []):
            visit(child, matrix, ref)
    for idx in data['scenes'][data.get('scene', 0)]['nodes']:
        visit(idx, np.eye(4))
    checks = []
    for ref, points in actual.items():
        points = np.array(points)
        got = [points.min(0).tolist(), points.max(0).tolist()]
        source = expected_parts['PCB2-'+ref]
        want = [source['min'], source['max']]
        delta = float(np.max(np.abs(np.array(want)-np.array(got))))
        checks.append({'reference': ref, 'GLB_in_CAD_bounds_mm': got,
                       'source_CAD_bounds_mm': want, 'max_delta_mm': delta, 'pass': delta < 0.02})
    if len(checks) != 4 or not all(x['pass'] for x in checks):
        raise RuntimeError('GLB registration failed: '+json.dumps(checks))
    return checks


def verify_cached_pcb_export(current_hashes, report_path=None, glb_path=None):
    """Require recorded provenance before reusing an existing PCB export."""
    report_path = report_path or OUT/'verification/geometry.json'
    glb_path = glb_path or OUT/'assets/pcb.glb'
    prefix = '--skip-pcb-export cannot reuse the cached PCB: '
    if not report_path.is_file():
        raise RuntimeError(prefix+'previous geometry verification report is missing; run without --skip-pcb-export')
    try:
        previous = json.loads(report_path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(prefix+'previous geometry verification report is unreadable') from error
    if not isinstance(previous, dict) or previous.get('source_hashes_after') != current_hashes:
        raise RuntimeError(prefix+'current input hashes do not match previous source_hashes_after; run a fresh export')
    output_hashes = previous.get('output_sha256')
    expected = output_hashes.get('pcb.glb') if isinstance(output_hashes, dict) else None
    if not isinstance(expected, str) or not expected:
        raise RuntimeError(prefix+'previous pcb.glb SHA256 is missing; run a fresh export')
    if not glb_path.is_file():
        raise RuntimeError(prefix+'pcb.glb is missing; run a fresh export')
    if sha(glb_path) != expected:
        raise RuntimeError(prefix+'pcb.glb SHA256 differs from the recorded export; run a fresh export')
    return expected


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--skip-pcb-export', action='store_true')
    ap.add_argument('--kicad', default=str(KICAD), help='Path to KiCad CLI executable')
    args = ap.parse_args()
    (OUT/'assets').mkdir(parents=True, exist_ok=True)
    (OUT/'verification').mkdir(parents=True, exist_ok=True)
    inputs = [ASSEMBLY, PCB, PCB.with_suffix('.kicad_pro'), POPULATION, HEIGHT_CONTRACT]
    hashes = {str(p.relative_to(ROOT)): sha(p) for p in inputs}
    if args.skip_pcb_export:
        verify_cached_pcb_export(hashes)
    source = load_step(ASSEMBLY)
    parts, excluded = [], []
    pcb_bounds = {}
    for i, leaf in enumerate(source):
        pid = part_id(leaf['name'])
        b = bounds(leaf['shape'])
        if pid.startswith('PCB2-'):
            pcb_bounds[pid] = b
        why = exclusion(pid)
        if why:
            excluded.append({'name': leaf['name'], 'part_id': pid, 'reason': why, 'bounds': b})
            continue
        solids = leaf['shape'].Solids()
        for j, solid in enumerate(solids):
            b = bounds(solid)
            render_group = group(pid)
            is_logo = pid == 'TMX-A3-P01' and solid.Volume() < 1000 and b['max'][2] < 0.49
            if is_logo:
                render_group = 'logo'
            verts, faces = solid.tessellate(0.04, 0.08)
            record = {'name': f'{pid}_{i:03d}_{j:02d}', 'part_id': pid, 'render_group': render_group,
                      'description': DESCRIPTIONS.get(pid, pid), 'source_name': leaf['name'],
                      'source_path': leaf['path'], 'source_leaf_index': i, 'solid_index': j,
                      'solid_count_in_source_leaf': len(solids), 'bounds': b,
                      'volume_mm3': solid.Volume(), 'is_valid': solid.isValid(),
                      'vertices': [[round(v, 7) for v in vertex.toTuple()] for vertex in verts],
                      'faces': [list(f) for f in faces]}
            parts.append(record)
    if sum(p['render_group'] == 'logo' for p in parts) != 6:
        raise RuntimeError('Expected six flush logo solids')
    if not all(p['is_valid'] for p in parts):
        raise RuntimeError('Invalid source solid')
    command = [args.kicad, 'pcb', 'export', 'glb', '--force', '--drill-origin', '--subst-models',
               '--no-dnp', '--include-pads', '--include-tracks', '--include-zones',
               '--include-silkscreen', '--include-soldermask', '--cut-vias-in-body',
               '--output', str(OUT/'assets/pcb.glb'), str(PCB)]
    export_log = 'Existing export reused after current input hashes and GLB SHA256 matched the previous verification report.'
    if not args.skip_pcb_export:
        run = subprocess.run(command, capture_output=True, text=True, check=True)
        export_log = run.stdout + run.stderr
    inventory, glb = glb_inventory(OUT/'assets/pcb.glb')
    component_rows = board_inventory()
    populated = {r['Ref'] for r in csv.DictReader(POPULATION.open())}
    exported = set(inventory['component_references'])
    missing = populated-exported
    extra = exported-populated
    dnp = {r['reference'] for r in component_rows if r['dnp']}
    if missing != MISSING or extra or exported & dnp:
        raise RuntimeError(f'Unexpected population discrepancy: missing={missing}; extra={extra}; dnp={exported&dnp}')
    alignment = verify_glb_bounds(glb, pcb_bounds)
    after = {str(p.relative_to(ROOT)): sha(p) for p in inputs}
    if hashes != after:
        raise RuntimeError('Hardware source changed during preparation')
    if args.skip_pcb_export:
        verify_cached_pcb_export(after)
    transform = {'GLB_units': 'meters', 'GLB_axes': 'X=native board X; Y=height; Z=native board Y',
                 'GLB_raw_to_CAD_mm': [[1000,0,0,30.5],[0,0,-1000,122.58491],[0,1000,0,24.0529],[0,0,0,1]],
                 'Blender_import_to_CAD_mm': [[1000,0,0,30.5],[0,1000,0,122.58491],[0,0,1000,24.0529],[0,0,0,1]],
                 'Blender_instruction': 'Blender glTF importer already converts Y-up to Z-up. Parent all imported roots to an Empty with scale (1000,1000,1000) and location (30.5,122.58491,24.0529), then parent that registration Empty to the scene CAD-mm root.'}
    payload = {'units': 'mm', 'coordinate_system': 'native enclosure CAD world coordinates',
               'linear_tessellation_tolerance_mm': 0.04, 'angular_tessellation_tolerance_radians': 0.08,
               'sources': hashes, 'parts': parts, 'pcb_transform': transform}
    geometry_path = OUT/'assets/geometry.json'
    geometry_path.write_text(json.dumps(payload, separators=(',', ':'))+'\n')
    report = {'source_hashes_before': hashes, 'source_hashes_after': after, 'hardware_unchanged': hashes == after,
              'source_leaf_count': len(source), 'render_solid_count': len(parts),
              'render_group_counts': dict(Counter(p['render_group'] for p in parts)),
              'triangle_count': sum(len(p['faces']) for p in parts), 'excluded': excluded,
              'all_included_solids_valid': True, 'pcb_export_command': command, 'pcb_export_log': export_log,
              'pcb': inventory, 'pcb_transform': transform,
              'blender_import_to_cad_mm': transform['Blender_import_to_CAD_mm'],
              'registration_checks': alignment,
              'population': {'populated_count': len(populated), 'exported_component_count': len(exported),
                             'missing_populated_visuals': sorted(missing), 'unexpected_exported': sorted(extra),
                             'dnp_references': sorted(dnp), 'dnp_exported': sorted(exported&dnp),
                             'missing_visuals_details': [r for r in component_rows if r['reference'] in missing]},
              'output_sha256': {'geometry.json': sha(geometry_path), 'pcb.glb': sha(OUT/'assets/pcb.glb')}}
    reconstruction_script = OUT/'scripts/pcb_details.py'
    if reconstruction_script.exists():
        report['population']['visual_reconstruction'] = {
            'script': str(reconstruction_script.relative_to(ROOT)), 'sha256': sha(reconstruction_script),
            'component_references': sorted(MISSING),
            'source_contract': str(HEIGHT_CONTRACT.relative_to(ROOT)),
            'source_contract_sha256': sha(HEIGHT_CONTRACT),
            'limitation': 'Dimension-based appearance reconstructions, not vendor CAD. Top inductance typography illustrative. Body dimensions exclude assembly clearance allowances. Placement uses current KiCad coordinates, not historical contract poses.'}
        report['population']['total_component_visuals_with_reconstructions'] = len(exported)+len(MISSING)
    (OUT/'verification/geometry.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:report[k] for k in ('render_solid_count','render_group_counts','triangle_count','hardware_unchanged','population','registration_checks')}, indent=2))


if __name__ == '__main__':
    main()
