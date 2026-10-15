"""Independent, read-only checks specific to the continuous-width PCB2 case.

No builder imports and no CAD writes. Only verification/flush-specific-review.json
is emitted. Access witnesses test complete corridors, not just exterior apertures.
Run only after the final STEP exports are frozen.
"""
from pathlib import Path
import argparse, hashlib, json, math, re
import cadquery as cq
from step_inputs import load_named_step, bounds
from preservation_geometry import measures
from component_clearance import populated_step_parts, ref_of

BASE=Path(__file__).resolve().parents[1]
ROOT=next(p for p in BASE.parents if (p/'hardware/pcb/analyzer-v2').is_dir())
ORIGINAL=ROOT/'hardware/cad/rev04/original-fit-check/Trimix_Enclosure_A3_Original.step'
PCB=ROOT/'hardware/pcb/analyzer-v2/review/Trimix_PCB_V2_Mechanical.step'
EPS=1e-6
SHIFT=(50.4,120,20.5529)

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def box(x0,x1,y0,y1,z0,z1):
    return cq.Solid.makeBox(x1-x0,y1-y0,z1-z0,cq.Vector(x0,y0,z0))
def cyl_x(x0,x1,y,z,r):
    return cq.Solid.makeCylinder(r,x1-x0,cq.Vector(x0,y,z),cq.Vector(1,0,0))
def slot_x(x0,x1,y,z,w,h,r):
    # Independent gauge is slightly smaller than the nominal aperture.
    s=box(x0,x1,y-w/2+r,y+w/2-r,z-h/2,z+h/2)
    s=s.fuse(box(x0,x1,y-w/2,y+w/2,z-h/2+r,z+h/2-r))
    for yy in (y-w/2+r,y+w/2-r):
        for zz in (z-h/2+r,z+h/2-r):s=s.fuse(cyl_x(x0,x1,yy,zz,r))
    return s.clean()
def overlap(a,b):
    common=a.intersect(b)
    return {'valid':common.isValid(),'volume_mm3':sum(abs(s.Volume())for s in common.Solids())}
def emptiness(label,gauge,parts,extra=None):
    tests=[{'part':pid,**overlap(gauge,s)}for pid,s in parts.items()]
    return {'label':label,'gauge_bounds_mm':bounds(gauge),'intersections':tests,
            'pass':all(r['valid'] and r['volume_mm3']<=EPS for r in tests),**(extra or {})}
def filled(label,gauge,shape):
    common=overlap(gauge,shape)
    return {'label':label,'gauge_bounds_mm':bounds(gauge),'expected_volume_mm3':gauge.Volume(),
            **common,'pass':common['valid'] and abs(common['volume_mm3']-gauge.Volume())<=EPS}
def placement(a,b):
    aa,bb=bounds(a),bounds(b);am,bm=measures(a),measures(b)
    delta=max(abs(x-y)for k in ('min','max')for x,y in zip(aa[k],bb[k]))
    vertices=lambda s:sorted(set(tuple(round(x,6)for x in v.toTuple())for v in s.Vertices()))
    metric=lambda key:abs(am[key]-bm[key])/max(abs(am[key]),1e-12)
    result={'source_bounds':aa,'candidate_bounds':bb,'maximum_bound_difference_mm':delta,
        'unique_world_vertices_match_at_1e_6mm':vertices(a)==vertices(b),
        'relative_adaptive_volume_difference':metric('volume_mm3'),
        'relative_adaptive_area_difference':metric('area_mm2'),
        'source_valid':a.isValid(),'candidate_valid':b.isValid()}
    result['pass']=a.isValid() and b.isValid() and delta<=1e-6 and result['unique_world_vertices_match_at_1e_6mm'] and metric('volume_mm3')<=1e-6 and metric('area_mm2')<=1e-6
    return result

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--assembly',type=Path)
    args=parser.parse_args()
    if args.assembly:assembly=args.assembly.resolve()
    else:
        options=list(BASE.glob('*.step'))
        if len(options)!=1:raise RuntimeError('Specify --assembly; expected exactly one top-level STEP')
        assembly=options[0]
    paths=[assembly,ORIGINAL,PCB]+[BASE/'parts'/f'{pid}.step' for pid in ('TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-V2-P12')]
    inputs={str(p.relative_to(ROOT)):sha(p)for p in paths}
    src=load_named_step(ORIGINAL);new=load_named_step(assembly)
    def final_part(pid):
        hits=[p for p in new if p['definition_name'].endswith('_'+pid)]
        if len(hits)!=1:raise RuntimeError(f'Expected one final {pid}, found {len(hits)}')
        return hits[0]['shape']
    printed={pid:final_part(pid)for pid in ('TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-A3-P04','TMX-A3-P05','TMX-A3-P07','TMX-A3-P08','TMX-A3-P11','TMX-V2-P12')}
    h,lid,tray,bezel=[printed[p]for p in ('TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-V2-P12')]
    shell=cq.Compound.makeCompound(list(printed.values()))
    original_h=next(p['shape']for p in src if p['part_id']=='TMX-A3-P01')
    r={'schema':1,'scope':'Independent continuous-side, access-corridor and retained-datum audit; no CAD mutation.',
       'input_sha256':inputs,'continuous_exterior':[],'access_corridors':[],
       'retained_component_positions':[],'pcb_component_positions':[],
       'hardware_clearance_vs_original':[],'mount_witnesses':[],
       'limits':['Geometric prototype review only; no strength, waterproofing, finger/glove ergonomics, cable-mate or hose qualification.',
         'Only finite side-wall witness regions are tested; uninterrupted cosmetic flatness of every surface is not claimed.',
         'Position checks compare bounds, unique world vertices and adaptive area/volume, not byte-identical STEP topology.',
         'Existing inherited hardware contacts are compared with the source housing. A preserved source contact is not newly qualified.']}
    for label,y,z in [('USB retainer',29.48491,33),('lower side after rounded corner',10,22),('below button',60,22),('middle side',90,22),('above button',140,22),('upper side above gas access opening',170,35)]:
        inside=box(96.55,96.59,y-1,y+1,z-1,z+1)
        outside=box(96.61,96.65,y-1,y+1,z-1,z+1)
        a=filled(label+' material up to X96.6',inside,shell)
        b=emptiness(label+' no material beyond X96.6',outside,printed)
        r['continuous_exterior'].append({'label':label,'nominal_side_x_mm':96.6,'inside':a,'outside':b,'pass':a['pass'] and b['pass']})
    r['printed_width_limit']={'maximum_x_mm':max(bounds(s)['max'][0]for s in printed.values()),'nominal_maximum_x_mm':96.6}
    r['printed_width_limit']['pass']=r['printed_width_limit']['maximum_x_mm']<=96.6+EPS
    r['access_corridors'].append(emptiness('USB opening from connector mouth to exterior',slot_x(95.51,100,29.48491,23.83,10.6,5.0,.4),printed,
        {'nominal_aperture_mm':[10.8,5.2],'gauge_mm':[10.6,5.0],'nominal_mouth_setback_mm':1.1}))
    r['access_corridors'].append(emptiness('complete button access well',box(84.91,98,101.1,124.9,19.1,40.6),printed,
        {'nominal_well_mm':[[84.9,101,19],[96.8,125,40.7]],'button_cap_x_mm':86.5,'cap_recess_from_side_mm':10.1}))
    r['access_corridors'].append(emptiness('button straight rigid access gauge',cyl_x(86.51,102,113,31,7.5),printed,
        {'gauge_diameter_mm':15,'limitation':'A geometric gauge, not a qualified finger or gloved-hand model.'}))
    r['access_corridors'].append(emptiness('gas access tunnel from original wall to exterior',cyl_x(84.91,100,162.5,22,8.9),printed,
        {'nominal_tunnel_diameter_mm':18,'gauge_diameter_mm':17.8,'fitting_end_x_mm':91,'fitting_recess_mm':5.6,
         'limitation':'Printed-material clearance only; physical hose OD, insertion, grip and bend remain unqualified.'}))
    # A full nominal tab slab catches stock accidentally added into PCB space.
    r['access_corridors'].append(emptiness('USB PCB tab including maximum1.76mm thickness',box(80.41,95.49,21.01,38.47491,20.5001,22.2599),printed))
    r['lid_roof_over_button']=filled('button well retains lid roof',box(85.1,96.3,101.2,124.8,41,42.8),lid)
    for yy in (17,42.5):
        r['access_corridors'].append(emptiness(f'USB screw driver with lid removed atY{yy}',cq.Solid.makeCylinder(2.2,29.8,cq.Vector(91,yy,30.2)),{'housing':h,'USB retainer':bezel},
            {'gauge_diameter_mm':4.4,'prerequisite':'Rear cover removed.'}))
    r['retainer_housing_static']=overlap(h,bezel)
    r['retainer_housing_static']['pass']=r['retainer_housing_static']['valid'] and r['retainer_housing_static']['volume_mm3']<=EPS
    # All original purchased/illustrative electronics, controls, gas and seals;
    # only the retired USB daughterboard references are intentionally absent.
    for p in src:
        pid=p['part_id']
        if not pid.startswith(('TMX-A3-C','TMX-A3-S')) or pid.startswith('TMX-A3-C05'):continue
        b=final_part(pid)
        r['retained_component_positions'].append({'part_id':pid,**placement(p['shape'],b)})
        if pid.startswith('TMX-A3-C04') or pid in ('TMX-A3-C11-V01','TMX-A3-C11-V02'):
            old=overlap(original_h,p['shape']);now=overlap(h,b);cover=overlap(lid,b)
            r['hardware_clearance_vs_original'].append({'part_id':pid,'original_housing':old,'new_housing':now,'new_lid':cover,
                'pass':old['valid'] and now['valid'] and cover['valid'] and now['volume_mm3']<=old['volume_mm3']+EPS and cover['volume_mm3']<=EPS})
    for p in populated_step_parts(load_named_step(PCB)):
        ref=ref_of(p)or'substrate';expected=p['shape'].translate(SHIFT);actual=final_part('PCB2-'+ref)
        r['pcb_component_positions'].append({'reference':ref,**placement(expected,actual)})
        if ref=='J901':r['actual_usb_contact_vs_printed']=emptiness('actual J901 solid',actual,printed)
    # Minimum retained support, independent of potentially unreliable coincident cuts.
    for x,y in ((76,28),(54.8,114)):
        outer=cq.Solid.makeCylinder(3.6,4.1,cq.Vector(x,y,14.4));inner=cq.Solid.makeCylinder(1.65,4.1,cq.Vector(x,y,14.4))
        ring=outer.cut(inner)
        witness=filled('original PCB mounting support annulus',ring,h)
        floor=filled('original PCB mounting support floor',box(x-1.65,x+1.65,y-1.65,y+1.65,14.3,14.4),h)
        r['mount_witnesses'].append({'axis_mm':[x,y],'bearing_plane_z_mm':18.5,'annulus':witness,'floor':floor,'pass':witness['pass'] and floor['pass']})
    # The void detector must reject newly filled stock in a corridor.
    test=box(96,97,28,30,23,24)
    control=emptiness('deliberately blocked USB corridor',slot_x(95.51,100,29.48491,23.83,10.6,5.0,.4),{'introduced stock':test})
    r['negative_control']={'blocked_corridor_rejected':not control['pass'],'introduced_overlap_mm3':control['intersections'][0]['volume_mm3']}
    r['inputs_unchanged_during_review']=all(sha(ROOT/p)==v for p,v in inputs.items())
    groups=('continuous_exterior','access_corridors','retained_component_positions','pcb_component_positions','hardware_clearance_vs_original','mount_witnesses')
    failures=[{'group':g,'item':q.get('label',q.get('part_id',q.get('reference',q.get('axis_mm'))))}for g in groups for q in r[g]if not q['pass']]
    for key in ('printed_width_limit','lid_roof_over_button','retainer_housing_static','actual_usb_contact_vs_printed'):
        if not r[key]['pass']:failures.append({'group':key})
    r['failures']=failures
    r['pass']=not failures and r['inputs_unchanged_during_review'] and r['negative_control']['blocked_corridor_rejected']
    r['status']='pass_for_geometric_prototype_review' if r['pass'] else 'fail'
    out=BASE/'verification/flush-specific-review.json';out.write_text(json.dumps(r,indent=2)+'\n')
    print(json.dumps({'report':str(out),'pass':r['pass'],'failures':failures,'assembly_sha256':inputs[str(assembly.relative_to(ROOT))]},indent=2),flush=True)
if __name__=='__main__':main()
