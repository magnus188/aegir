"""Independent exported preservation checks; do not equate failed Booleans to change.

STEP conversion can make coincident Boolean operations unreliable. Retained parts
must instead pass explicit independent world-geometry, metric and sampled checks.
The one observed P07 topology split is bounded and disclosed, not called identity.
"""
from pathlib import Path
import json,hashlib
import sys
ROOT_IMPORT=next(p for p in Path(__file__).resolve().parents if (p/'hardware/pcb/analyzer-v2').exists())
sys.path.insert(0,str(ROOT_IMPORT/'hardware/cad/pcb-v2/scripts'))
import cadquery as cq
import trimesh
from step_inputs import load_named_step,bounds
from preservation_geometry import compare,sampled_boundary_distance,surface
ROOT=next(p for p in Path(__file__).resolve().parents if (p/'hardware/pcb/analyzer-v2').exists());OUT=ROOT/'hardware/cad/pcb-v2-bottom'
RETAINED=('TMX-A3-C02','TMX-A3-C03-V01','TMX-A3-C03-V02','TMX-A3-C10-V01','TMX-A3-C10-V02','TMX-A3-P04','TMX-A3-P05','TMX-A3-P07','TMX-A3-P08','TMX-A3-P11')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def volume(shape):return sum(abs(s.Volume()) for s in shape.Solids())
def raw_boolean(a,b):
    x=a.cut(b);y=b.cut(a);valid=x.isValid() and y.isValid()
    raw=volume(x)+volume(y)
    return {'source_minus_candidate_valid':x.isValid(),'candidate_minus_source_valid':y.isValid(),
            'raw_summed_cut_volumes_mm3':raw,'zero_valid_difference':valid and raw<1e-5,
            'interpretation':'Diagnostic only: coincident STEP cuts may be invalid or retain the entire solid. Nonzero volume alone is not evidence of a physical change.'}
def main():
    source_path=ROOT/'hardware/cad/rev04/original-fit-check/Trimix_Enclosure_A3_Original.step'
    assembly_path=OUT/'Trimix_Enclosure_PCB2_Bottom.step'
    start={str(p.relative_to(ROOT)):sha(p) for p in (source_path,assembly_path,OUT/'scripts/build_case.py')}
    source=load_named_step(source_path);result=load_named_step(assembly_path)
    def find_new(pid):
        hits=[p for p in result if p['definition_name'].endswith('_'+pid)]
        if len(hits)!=1:raise ValueError((pid,[p['definition_name']for p in hits]))
        return hits[0]['shape']
    checks=[]
    for pid in RETAINED:
        a=next(p['shape']for p in source if p['part_id']==pid);b=find_new(pid)
        geom=compare(a,b);raw=raw_boolean(a,b);samples=sampled_boundary_distance(a,b)
        relative_volume=geom['volume_difference_mm3']/abs(geom['source_measures']['volume_mm3'])
        relative_area=geom['area_difference_mm2']/abs(geom['source_measures']['area_mm2'])
        invariant_ok=relative_volume<=1e-6 and relative_area<=1e-6
        split=None
        topology_ok=geom['topology_identical']
        if pid=='TMX-A3-P07' and not topology_ok:
            solids=sorted(b.Solids(),key=lambda s:abs(s.Volume()),reverse=True)
            split={'source_topology':geom['source_topology'],'candidate_topology':geom['candidate_topology'],
                'secondary_solids':[{'volume_mm3':abs(s.Volume()),'bounds':bounds(s)}for s in solids[1:]],
                'permitted_secondary_total_volume_mm3':1e-8,
                'limitation':'Explicit STEP roundtrip split; exact topology and exact shape identity are not established.'}
            topology_ok=(geom['source_topology']=={'solids':1,'faces':186,'edges':481,'vertices':309}
                and geom['candidate_topology']=={'solids':2,'faces':187,'edges':481,'vertices':310}
                and sum(abs(s.Volume())for s in solids[1:])<=1e-8)
        accepted=geom['strict_signature_match'] and invariant_ok and topology_ok and samples['within_acceptance']
        row={'part_id':pid,'raw_boolean':raw,'geometry_signature':geom,'sampled_boundary':samples,
            'relative_volume_difference':relative_volume,'relative_area_difference':relative_area,
            'metric_relative_acceptance':1e-6,'metric_invariants_pass':invariant_ok,
            'strict_shape_identity_established':raw['zero_valid_difference'] and geom['strict_signature_match'] and geom['topology_identical'],
            'preserved_within_roundtrip_tolerance':accepted,'topology_split':split,'bounds':bounds(b)}
        if pid in ('TMX-A3-C03-V01','TMX-A3-C03-V02'):
            supports=sorted(surface(f)for f in b.Faces())
            row['battery_analytic_geometry']={'surface_geometry':supports,
                'analytic_cylinder_and_two_end_planes_match':geom['strict_signature_match'] and [s[0]for s in supports]==['CYLINDER','PLANE','PLANE'] and geom['topology_identical'],
                'scope':'World cylinder axis, radius, two end planes, trimmed circular/linear boundaries, and vertices match at 1e-6 mm; battery geometry and placement preserved.'}
            row['preserved_within_roundtrip_tolerance'] &= row['battery_analytic_geometry']['analytic_cylinder_and_two_end_planes_match']
        checks.append(row)
        print(pid,accepted,flush=True)
    # Source cradle/capture volumes, not merely purchased battery reference models.
    for pid,limits in [('TMX-A3-P01',(2.2,48.1,24,110,14.2,36.1)),('TMX-A3-P02',(5.3,47.5,38.9,93.1,37.2,41))]:
        x0,x1,y0,y1,z0,z1=limits;region=cq.Solid.makeBox(x1-x0,y1-y0,z1-z0,cq.Vector(x0,y0,z0))
        a=next(p['shape']for p in source if p['part_id']==pid).intersect(region);b=find_new(pid).intersect(region)
        raw=raw_boolean(a,b)
        preserved=a.isValid() and b.isValid() and raw['zero_valid_difference']
        addition=None
        if pid=='TMX-A3-P01':
            # The new USB floor/boss is allowed in formerly empty space just
            # in front of the holder. No original cradle material may vanish.
            allowed=cq.Solid.makeBox(48.1-25.3,26.5-24,34.3-16.7,cq.Vector(25.3,24,16.7))
            removed=a.cut(b);added=b.cut(a);outside=added.cut(allowed)
            addition={'removed_original_volume_mm3':volume(removed),'added_volume_mm3':volume(added),
                'addition_outside_declared_zone_mm3':volume(outside),'allowed_zone_mm':[25.3,48.1,24,26.5,16.7,34.3],
                'minimum_holder_y_mm':27,'nominal_y_clearance_to_additions_mm':0.5,
                'all_boolean_results_valid':all(q.isValid()for q in (removed,added,outside))}
            preserved=addition['all_boolean_results_valid'] and volume(removed)<1e-5 and volume(outside)<1e-5
        checks.append({'part_id':pid,'protected_battery_region':limits,'raw_boolean':raw,
            'source_region_valid':a.isValid(),'candidate_region_valid':b.isValid(),'declared_usb_support_addition':addition,
            'preserved_within_roundtrip_tolerance':preserved})
    # Transient negative control: detect a real 0.01 mm position change, no file writes.
    cell=next(p['shape']for p in source if p['part_id']=='TMX-A3-C03-V01')
    control_same=compare(cell,cell);control_shift=compare(cell,cell.translate((.01,0,0)))
    controls={'identical_cell_signature_accepted':control_same['strict_signature_match'],
              'cell_shifted_0_01mm_signature_rejected':not control_shift['strict_signature_match'],
              'shift_detected_bounds_mm':control_shift['world_bounds_max_delta_mm']}
    meshes=[]
    expected_meshes={'TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-A3-P04','TMX-A3-P05','TMX-A3-P07','TMX-A3-P08','TMX-A3-P11','TMX-V2-P12'}
    mesh_paths=sorted((OUT/'parts').glob('*.stl'))
    for p in mesh_paths:
        m=trimesh.load_mesh(p,process=True)
        meshes.append({'file':str(p.relative_to(OUT)),'watertight':bool(m.is_watertight),'winding_consistent':bool(m.is_winding_consistent),'connected_bodies':int(m.body_count),'positive_volume':bool(m.volume>0),'volume_mm3':float(m.volume),'sha256':sha(p)})
    build=json.loads((OUT/'verification/build-report.json').read_text())
    src_ok=all(sha(ROOT/p)==h for p,h in build['sources'].items())
    inputs_stable=all(sha(ROOT/p)==h for p,h in start.items())
    mesh_set_ok={p.stem for p in mesh_paths}==expected_meshes
    passed=(all(c['preserved_within_roundtrip_tolerance']for c in checks) and all(m['watertight'] and m['winding_consistent'] and m['connected_bodies']==1 and m['positive_volume']for m in meshes) and src_ok and inputs_stable and mesh_set_ok and controls['identical_cell_signature_accepted'] and controls['cell_shifted_0_01mm_signature_rejected'])
    r={'schema':2,'status':'pass_with_documented_STEP_roundtrip_variation' if passed else 'fail','pass':passed,
       'input_sha256':start,'inputs_unchanged_during_check':inputs_stable,'preservation_checks':checks,'control_checks':controls,
       'meshes':meshes,'expected_mesh_set_matches':mesh_set_ok,'source_hashes_unchanged':src_ok,
       'source_copy_assurance':{'builder':'scripts/build_case.py','builder_sha256':start[str((OUT/'scripts/build_case.py').relative_to(ROOT))],
          'reviewed_route':"if pid in parts:q['shape']=parts[pid]; retained.append(q)",
          'review':'Retained IDs above are absent from the builder replacement dictionary and retirement set; source world BReps are passed directly to assembly export. This is static source review plus independent export checks, not byte identity of reserialized STEP.'},
       'scope':'Source geometry preservation within explicit STEP roundtrip bounds and printable mesh topology. No exact BRep identity claim for P07; raw Boolean failures retained. No print-strength, pressure/gas-seal, waterproof or complete purchased-component qualification.',
       'limits':['World support/edge/vertex signatures use 1e-6 mm rounding; sampled boundary acceptance is 0.001 mm and is not a continuous surface maximum proof.',
                 'Adaptive integrated area/volume must agree within 1 ppm. Only P07 permits its explicitly measured tiny secondary topology split.',
                 'Nine delivered STL meshes are independently watertight, one body and positive volume; this does not qualify the STEP manifold for production.']}
    (OUT/'verification/delivery-checks.json').write_text(json.dumps(r,indent=2)+'\n')
    print(json.dumps({'status':r['status'],'pass':passed,'assembly_sha256':start[str(assembly_path.relative_to(ROOT))]}),flush=True)
if __name__=='__main__':main()
