#!/usr/bin/env python3
"""Independent selected-hole and profile checks of actual exported CAM bytes.

Run with the isolated Gerbonara 1.6.3 / Shapely 2.1.2 environment. Uses the
existing independent CAM geometry reader, never the KiCad mutation API.
This supplements independent-cam/audit.json; neither report is factory approval.
"""
from pathlib import Path
import csv
import hashlib
import json
import math
import sys
import warnings
from collections import Counter

ROOT = next(p for p in Path(__file__).resolve().parents if (p/'hardware/pcb/analyzer-v2').is_dir())
VARIANT = ROOT/'hardware/pcb/analyzer-v2-bottom'
OUT = VARIANT/'manufacturing'
sys.path.insert(0, str(ROOT/'hardware/system-review/electrical/main-final-independent'))
import sexpdata
from gerbonara import GerberFile, ExcellonFile
from cam_geometry import Native, geometry, sub, subs, Point, unary_union, LineString, Polygon, polys


def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    board = VARIANT/'Trimix_Analyzer.kicad_pcb'
    raw = sexpdata.loads(board.read_text())
    native = Native(raw)
    checks = []
    def check(name, condition, detail=None):
        checks.append({'check': name, 'passed': bool(condition), 'details': detail})
    cam = OUT/'cam'
    extensions = {'F.Cu': '.gtl', 'In1.Cu': '.g1', 'In2.Cu': '.g2', 'B.Cu': '.gbl'}
    input_paths = [board, ROOT/'hardware/pcb/analyzer-v2/Trimix_Analyzer.kicad_pcb', ROOT/'hardware/pcb/analyzer/Trimix_Analyzer.kicad_pcb', OUT/'selected-fill-cap-holes.csv', OUT/'bom-populated-by-reference.csv',
                   OUT/'bom-populated-grouped.csv', OUT/'placement-populated.csv', OUT/'dnp-list.csv']
    input_paths += [p for p in cam.iterdir() if p.suffix in (*extensions.values(), '.gm1', '.drl')]
    hashes = {str(p.relative_to(ROOT)): digest(p) for p in sorted(input_paths)}
    copper_files = {}
    with warnings.catch_warnings(record=True) as observed_warnings:
        warnings.simplefilter('always')
        for layer, suffix in extensions.items():
            paths = list(cam.glob('*'+suffix))
            assert len(paths) == 1
            copper_files[layer] = GerberFile.open(paths[0])
        edge = GerberFile.open(next(cam.glob('*.gm1')))
        pth = ExcellonFile.open(next(p for p in cam.glob('*-PTH.drl')))
        npth = ExcellonFile.open(next(p for p in cam.glob('*-NPTH.drl')))
    check('Exactly four copper Gerbers match the native layer sequence',
          native.layers == list(extensions) and len(copper_files) == 4, native.layers)
    # Read Edge.Cuts centreline coordinates, never its stroke-expanded bounds.
    assert all(type(o).__name__ == 'Line' for o in edge.objects), 'Unexpected non-linear outline'
    outline_points = [(o.x1, o.y1) for o in edge.objects] + [(o.x2, o.y2) for o in edge.objects]
    bbox = [min(x for x, y in outline_points), min(y for x, y in outline_points),
            max(x for x, y in outline_points), max(y for x, y in outline_points)]
    dimensions = [bbox[2]-bbox[0], bbox[3]-bbox[1]]
    check('Exported outline centreline measures46.38491 x99.8 mm',
          all(abs(a-b) < 0.000002 for a, b in zip(dimensions, (46.38491, 99.8))),
          {'bounds_CAM_mm': bbox, 'dimensions_mm': dimensions})

    selected = list(csv.DictReader((OUT/'selected-fill-cap-holes.csv').open()))
    expected = []
    for pad in native.pads:
        if (pad['ref'], pad['pin']) not in (('U101', '25'), ('U201', '15')) or str(pad['raw'][2]) != 'thru_hole':
            continue
        expected.append({'uuid': sub(pad['raw'], 'uuid')[0], 'reference': pad['ref'], 'pad': pad['pin'],
                         'x': pad['x'], 'y': pad['y'], 'drill': sub(pad['raw'], 'drill')[0],
                         'copper': sub(pad['raw'], 'size')[0], 'net': pad['net'], 'thermal': True})
    flagged = []
    for via in subs(raw, 'via'):
        if sub(via, 'filling') == [sexpdata.Symbol('yes')] or sub(via, 'capping') == [sexpdata.Symbol('yes')]:
            x, y = native.xy(sub(via, 'at'))
            flagged.append(sub(via, 'uuid')[0])
            expected.append({'uuid': sub(via, 'uuid')[0], 'reference': '', 'pad': '',
                             'x': x, 'y': y, 'drill': sub(via, 'drill')[0],
                             'copper': sub(via, 'size')[0], 'net': sub(via, 'net')[0], 'thermal': False})
    check('Native selected process is exactly 9 U101 + 15 U201 + 3 flagged signal vias',
          Counter(p['reference'] for p in expected) == Counter({'U101': 9, 'U201': 15, '': 3}),
          {'by_reference': dict(Counter(p['reference'] for p in expected)), 'signal_uuids': flagged})
    by_uuid = {r['UUID']: r for r in selected}
    failures = []
    for wanted in expected:
        row = by_uuid.get(wanted['uuid'])
        if row is None:
            failures.append({'uuid': wanted['uuid'], 'error': 'Missing CSV row'})
            continue
        same = (row['Reference'] == wanted['reference'] and row['Pad'] == wanted['pad'] and row['Net'] == wanted['net']
                and abs(float(row['CAM_X_mm'])-wanted['x']) < 1e-8
                and abs(float(row['CAM_Y_mm'])-wanted['y']) < 1e-8
                and abs(float(row['Drill_mm'])-wanted['drill']) < 1e-8
                and abs(float(row['Copper_diameter_mm'])-wanted['copper']) < 1e-8)
        if not same:
            failures.append({'wanted': wanted, 'actual': row})
    check('Selected-hole CSV exactly matches all 27 native UUIDs, coordinates, nets and diameters',
          not failures and len(selected) == len(by_uuid) == len(expected) == 27,
          {'mismatches': failures, 'row_count': len(selected)})
    used, drill_matches = set(), []
    for wanted in expected:
        matches = [i for i, obj in enumerate(pth.objects) if type(obj).__name__ == 'Flash'
                   and abs(obj.aperture.diameter-wanted['drill']) < 1e-8
                   and math.dist((obj.x, obj.y), (wanted['x'], wanted['y'])) <= .000710]
        exact = len(matches) == 1 and matches[0] not in used
        if exact:
            used.add(matches[0])
        drill_matches.append({'uuid': wanted['uuid'], 'matched': exact,
                              'drill_hits': matches, 'expected_CAM_xy': [wanted['x'], wanted['y']]})
    check('Every selected fill/cap hole has exactly one correctly sized plated Excellon drill hit',
          len(used) == 27 and all(r['matched'] for r in drill_matches), drill_matches)

    cu = {layer: unary_union([geometry(o) for o in gerber.objects]) for layer, gerber in copper_files.items()}
    land_results = []
    for wanted in expected:
        for layer in native.layers:
            nominal = Point(wanted['x'], wanted['y']).buffer(wanted['copper']/2, quad_segs=128)
            missing = nominal.difference(cu[layer].buffer(.000002)).area
            land_results.append({'uuid': wanted['uuid'], 'layer': layer, 'copper_diameter_mm': wanted['copper'],
                                 'missing_nominal_land_area_mm2': missing,
                                 'passed': missing < 1e-8 and wanted['copper'] >= .45})
    check('All selected thermal/signal copper lands remain >=0.45 mm and covered on all four exported copper layers',
          all(r['passed'] for r in land_results), land_results)
    check('All thermal drill diameters remain 0.20 mm and signal fill/cap drill diameters 0.25 mm',
          all(abs(p['drill']-(.2 if p['thermal'] else .25)) < 1e-9 for p in expected))
    check('All exported drills/slots are accounted for by the independent native geometry reader',
          len(pth.objects)+len(npth.objects) == len(native.holes),
          {'PTH_drills_and_slots': len(pth.objects), 'NPTH_drills': len(npth.objects), 'native_holes': len(native.holes)})
    populations = list(csv.DictReader((OUT/'bom-populated-by-reference.csv').open()))
    placement = list(csv.DictReader((OUT/'placement-populated.csv').open()))
    groups = list(csv.DictReader((OUT/'bom-populated-grouped.csv').open()))
    omitted = list(csv.DictReader((OUT/'dnp-list.csv').open()))
    refcount = Counter(r['Reference'] for r in populations)
    groupedrefs = Counter(ref.strip() for row in groups for ref in row['References'].split(','))
    native_pop = {f['ref'] for f in native.fps if 'dnp' not in f['attributes']
                  and not f['ref'].startswith(('H', 'TP')) and f['ref'] != 'J102'
                  and 'exclude_from_bom' not in f['attributes']}
    check('Populated BOM, grouped BOM and matched POS cover exactly the same 148 physical components',
          refcount == groupedrefs == Counter(r['Ref'] for r in placement) == Counter(native_pop)
          and len(refcount) == 148 and all(n == 1 for n in refcount.values()),
          {'population_count': len(refcount), 'grouped_rows': len(groups), 'placement_rows': len(placement)})
    check('Six native DNP components are omitted from the populated BOM and POS',
          {r['Reference'] for r in omitted} == {f['ref'] for f in native.fps if 'dnp' in f['attributes']}
          and len(omitted) == 6 and not set(refcount) & {r['Reference'] for r in omitted},
          [r['Reference'] for r in omitted])
    # Ground copper immediately around both original thermal packages. This
    # bounded5mm-radius check does not replace thermal characterization.
    base_path=ROOT/'hardware/pcb/analyzer-v2/Trimix_Analyzer.kicad_pcb'
    base=Native(sexpdata.loads(base_path.read_text()))
    def ground(native,layer):
        return unary_union([p['geo']for p in native.pads_on(layer)if p['net']=='GND']+
            [p['geo']for p in native.tracks if p['net']=='GND'and p['layer']==layer]+
            [p['geo']for p in native.vias if p['net']=='GND']+
            [p['geo']for p in native.zones if p['net']=='GND'and p['layer']==layer])
    # Compare in native PCB coordinates even if an explicitly documented CAM
    # auxiliary origin differs; the case/board datums never derive from this test.
    from shapely import affinity
    old_ground={layer:affinity.translate(ground(base,layer),xoff=base.origin[0],yoff=-base.origin[1])for layer in base.layers}
    new_ground={layer:affinity.translate(ground(native,layer),xoff=native.origin[0],yoff=-native.origin[1])for layer in native.layers}
    thermal_spreading=[]
    for ref,x,y in [('U101',32.9,105.48491),('U201',32.9,82.48491)]:
        window=Point(x,-y).buffer(5,quad_segs=128)
        for layer in base.layers:
            a=old_ground[layer].intersection(window);b=new_ground[layer].intersection(window)
            lost=a.difference(b.buffer(.000002)).area
            thermal_spreading.append({'reference':ref,'layer':layer,'window_radius_mm':5,
                'source_GND_area_mm2':a.area,'candidate_GND_area_mm2':b.area,
                'source_GND_missing_from_candidate_mm2':lost,
                'passed':lost<.0001})
    check('No source GND copper is lost within5mm of either thermal package on any layer',
          all(q['passed']for q in thermal_spreading),thermal_spreading)

    # A deliberately bounded new D+ hop may use In1. Read the actual old core
    # outline and check planar connectivity there; do not infer intact return
    # paths from same-net labels or a logical DRC pass.
    from shapely.ops import polygonize
    original=sexpdata.loads((ROOT/'hardware/pcb/analyzer/Trimix_Analyzer.kicad_pcb').read_text())
    core_edges=[LineString([(float(q[0]),-float(q[1]))for q in (sub(g,'start'),sub(g,'end'))])
                for g in subs(original,'gr_line')if sub(g,'layer')==['Edge.Cuts']]
    core_polygons=list(polygonize(core_edges));assert len(core_polygons)==1
    core=core_polygons[0]
    source_plane=old_ground['In1.Cu'].intersection(core)
    candidate_plane=new_ground['In1.Cu'].intersection(core)
    connectivity=[]
    for i,part in enumerate(polys(source_plane)):
        if part.area<.0001:continue
        remaining=part.intersection(candidate_plane.buffer(.000002))
        fragments=[q for q in polys(remaining)if q.area>=.0001]
        connectivity.append({'source_region':i,'source_area_mm2':part.area,
            'retained_fragment_areas_mm2':[q.area for q in fragments],
            'source_copper_lost_mm2':part.difference(candidate_plane.buffer(.000002)).area,
            'passed':len(fragments)==1})
    check('No new planar GND split or lost source region in retained core In1 copper',
          bool(connectivity)and all(q['passed']for q in connectivity),
          {'comparison':'Each original core GND region must retain one connected planar remainder;2um geometry tolerance and0.0001mm2 minimum fragment area',
           'core_area_mm2':core.area,'source_plane_area_mm2':source_plane.area,
           'candidate_plane_area_mm2':candidate_plane.area,'regions':connectivity,
           'limitation':'Connectivity is not return-path impedance or system EMC certification; intended signal clearance removes finite copper outside thermal windows.'})

    check('All source/CAM/BOM input bytes stayed unchanged during process audit',
          hashes == {str(p.relative_to(ROOT)): digest(p) for p in sorted(input_paths)})
    result = {'purpose': 'Independent exported-byte profile, selected-hole process and full-population consistency review',
              'source_board_sha256': digest(board), 'input_sha256': hashes, 'checks': checks,
              'passed': sum(c['passed'] for c in checks), 'failed': sum(not c['passed'] for c in checks),
              'parser_warnings': [str(w.message) for w in observed_warnings],
              'production_release': False,
              'limitations': ['Selected-hole instructions are explicit but no factory acceptance is claimed.',
                              'Drill coordinates permit only decimal-export quantization, up to 0.000710 mm.',
                              'Gerber copper overlap tolerance is numerical, not a manufacturing tolerance.',
                              'The 44-check independent CAM audit covers detailed drill/slot/pad/track/mask/paste/zone and connectivity parity.',
                              'Native full-board paste is diagnostic and includes DNP apertures; assembler must approve a final matched stencil.',
                              'The5mm ground-spreading comparison is a bounded geometric screen, not proof of equal operating temperature or whole-board cooling.']}
    (OUT/'cam-process-checks.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({'passed': result['passed'], 'failed': result['failed'], 'source_board_sha256': result['source_board_sha256']}))
    for c in checks:
        if not c['passed']:
            print('FAIL:', c['check'], str(c['details'])[:400])
    return 1 if result['failed'] else 0


if __name__ == '__main__':
    sys.exit(main())
