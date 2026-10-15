"""Source-bound PCB2 component keepouts in enclosure world coordinates.

Run with Python containing CadQuery/OCP (see delivery runtime requirements). Import build_keepouts() to obtain labeled CadQuery
box shapes. No source CAD/PCB mutations. All 19 fitted underside components and
all 6 fitted components absent from STEP are covered (U115 belongs to both sets).
Contract is freshly exported from PCB2 + matching netlist; its historical Fusion
XY fields are deliberately ignored because the new board uses an offset origin.

0.5 mm XY clearance and 0.5 mm Z clearance away from board are added to the union
of maximum body+assembly contract and actual STEP AABB. Bottom boxes end exactly
at Z20.5; top boxes start at Z22.1. Thus mount planes remain unchanged outside
explicit component pockets. Actual through-hole pin tails need their own relief.
"""
from pathlib import Path
import hashlib,json,re,csv
import cadquery as cq
from step_inputs import load_named_step,bounds
BASE=Path(__file__).resolve().parents[1]
ROOT=BASE.parents[2]
VERIFICATION=BASE/'verification'
V2=ROOT/'hardware/pcb/analyzer-v2'
PCB=V2/'Trimix_Analyzer.kicad_pcb'
CONTRACT=VERIFICATION/'component-height-contract.json'
PCBSTEP=V2/'review/Trimix_PCB_V2_Mechanical.step'
ORIGINAL=ROOT/'hardware/cad/rev04/original-fit-check/Trimix_Enclosure_A3_Original.step'
TRANSLATION=(50.4,120.0,20.5529)
MOUNT_PLANES={'bottom_mm':20.5,'top_mm':22.1}
MOUNT_AXES=[(76.0,28.0),(54.8,114.0)]
EXPECTED_BOTTOM={'C107','C111','C112','C113','C114','C115','C116','R112','R119',
'R121','R122','R123','R124','R125','R126','R127','R128','R504','U115'}
EXPECTED_MISSING={'L101','L201','L701','U110','U111','U115'}

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def box(low,high):return cq.Solid.makeBox(*(b-a for a,b in zip(low,high)),cq.Vector(*low))
def ref_of(part):
    m=re.search(r' / ([A-Z]+[0-9]+) \[',part['path'])
    return m[1] if m else None

def _step_inventory():
    cache=VERIFICATION/'pcb-step-source-inventory.json'
    if cache.exists():
        data=json.loads(cache.read_text())
        if data['sha256']==sha(PCBSTEP):return data['parts']
    return [{k:v for k,v in p.items() if k!='shape'} for p in load_named_step(PCBSTEP)]

def build_keepouts(lateral_mm=.5,z_clearance_mm=.5):
    if lateral_mm<0 or z_clearance_mm<0:raise ValueError('Negative clearance')
    contract=json.loads(CONTRACT.read_text())
    if contract['board_sha256']!=sha(PCB):raise RuntimeError('Height contract does not match current PCB2')
    if not contract['part_identity_matches']:raise RuntimeError('Part identity gate failed')
    parts=_step_inventory();steprefs={}
    for part in parts:
        ref=ref_of(part)
        if ref:steprefs.setdefault(ref,[]).append(part)
    fitted={r['Ref'] for r in csv.DictReader((V2/'manufacturing/placement-populated.csv').open())}
    rows={r['reference']:r for r in contract['rows'] if not r['DNP']}
    bottom={ref for ref,r in rows.items() if r['side']=='B.Cu'}
    missing=fitted-steprefs.keys()
    if bottom!=EXPECTED_BOTTOM:raise RuntimeError('Unexpected underside population '+str(bottom))
    if missing!=EXPECTED_MISSING:raise RuntimeError('Unexpected missing-model population '+str(missing))
    outputs=[]
    for ref in sorted(bottom|missing):
        r=rows[ref]
        # Raw PCB coordinates; exported STEP is normalized and already reverses Y.
        low=[50.4+r['x_min_mm']-19.9,120-r['y_max_mm']+21.48491,r['Fusion_z_bottom_mm']]
        high=[50.4+r['x_max_mm']-19.9,120-r['y_min_mm']+21.48491,r['Fusion_z_top_mm']]
        contract_bounds=[list(low),list(high)]
        actual=[]
        for p in steprefs.get(ref,[]):
            lo=[a+t for a,t in zip(p['bounds']['min'],TRANSLATION)]
            hi=[a+t for a,t in zip(p['bounds']['max'],TRANSLATION)]
            actual.append({'path':p['path'],'bounds_mm':[lo,hi]})
            low=[min(a,b) for a,b in zip(low,lo)]
            high=[max(a,b) for a,b in zip(high,hi)]
        union=[list(low),list(high)]
        low[:2]=[v-lateral_mm for v in low[:2]]
        high[:2]=[v+lateral_mm for v in high[:2]]
        if r['side']=='B.Cu':
            low[2]-=z_clearance_mm
            if high[2]>20.5+1e-6:raise RuntimeError('Underside actual body crosses mounting plane '+ref)
            high[2]=20.5
        else:
            high[2]+=z_clearance_mm
            if low[2]<22.1-1e-6:raise RuntimeError('Top-side actual body crosses mounting plane '+ref)
            low[2]=22.1
        # Do not silently remove required clearance to preserve a mount.
        mount_conflicts=[]
        for x,y in MOUNT_AXES:
            # Selected 2.3mm hole radius, not an invented screw-head keepout.
            dx=max(low[0]-x,0,x-high[0]);dy=max(low[1]-y,0,y-high[1])
            if dx*dx+dy*dy<1.15**2:mount_conflicts.append([x,y])
        if mount_conflicts:raise RuntimeError(ref+' keepout intersects existing mount bore '+str(mount_conflicts))
        outputs.append({'reference':ref,'side':r['side'],'MPN':r['MPN'],
            'missing_from_step':ref in missing,'underside':ref in bottom,
            'source_contract_bounds_mm':contract_bounds,'actual_step_models':actual,
            'union_before_extra_clearance_mm':union,'keepout_bounds_mm':[low,high],
            'extra_lateral_clearance_mm':lateral_mm,'extra_z_clearance_mm':z_clearance_mm,
            'assembly_allowance_in_contract_mm':r['assembly_allowance_mm'],
            'body_height_basis':r['basis'],'source_url':r['source'],
            'shape':box(low,high)})
    return outputs



def fitted_references():
    """Physical assembly population; use this in every PCB collision/view loop."""
    return {r['Ref'] for r in csv.DictReader((V2/'manufacturing/placement-populated.csv').open())}


def populated_step_parts(normalized_step_parts=None):
    """Return normalized PCB STEP leaf records, excluding all six DNP refs.

    Unreferenced substrate is retained. No world translation is applied here.
    """
    parts=load_named_step(PCBSTEP) if normalized_step_parts is None else normalized_step_parts
    fitted=fitted_references()
    return [p for p in parts if ref_of(p) is None or ref_of(p) in fitted]


def build_tail_keepouts(lateral_mm=.35,z_clearance_mm=.4,normalized_step_parts=None):
    """Individual actual below-board pin-tail boxes, never whole header AABBs.

    Every fitted top-side STEP solid is translated to world and clipped at Z20.5.
    Each resulting solid gets its own bounding box and stated clearance. Output
    ends at the unchanged Z20.5 seating plane. These are geometric process
    allowances, not independently manufacturer-qualified solder volumes.
    """
    if lateral_mm<0 or z_clearance_mm<0:raise ValueError('Negative clearance')
    contract=json.loads(CONTRACT.read_text())
    if contract['board_sha256']!=sha(PCB):raise RuntimeError('Stale component contract')
    side={r['reference']:r['side'] for r in contract['rows'] if not r['DNP']}
    parts=populated_step_parts(normalized_step_parts)
    outputs=[]
    for part in parts:
        ref=ref_of(part)
        if not ref or side.get(ref)!='F.Cu':continue
        world=part['shape'].translate(TRANSLATION)
        wb=bounds(world)
        if wb['min'][2]>=20.5-1e-7:continue
        clip_low=[wb['min'][0]-1,wb['min'][1]-1,wb['min'][2]-1]
        clip_high=[wb['max'][0]+1,wb['max'][1]+1,20.5]
        clipped=world.intersect(box(clip_low,clip_high))
        if not clipped.isValid():raise RuntimeError('Invalid pin-tail clipping '+ref)
        tails=sorted(clipped.Solids(),key=lambda b:tuple(bounds(b)['center']))
        for index,tail in enumerate(tails):
            if tail.Volume()<=1e-8:continue
            tb=bounds(tail)
            low=[tb['min'][0]-lateral_mm,tb['min'][1]-lateral_mm,tb['min'][2]-z_clearance_mm]
            high=[tb['max'][0]+lateral_mm,tb['max'][1]+lateral_mm,20.5]
            if high[2]-low[2]<=1e-7:continue
            outputs.append({'reference':ref,'kind':'actual_through_hole_tail',
                'tail_index':index,'underside':True,'side':'F.Cu',
                'source_step_path':part['path'],'actual_tail_volume_mm3':tail.Volume(),
                'actual_tail_bounds_mm':[tb['min'],tb['max']],
                'extra_lateral_clearance_mm':lateral_mm,'extra_z_clearance_mm':z_clearance_mm,
                'keepout_bounds_mm':[low,high],'shape':box(low,high)})
    if {q['reference'] for q in outputs}!={'J103','J104','J301','J401','J402','J501','J601','J701','J801','J802'}:
        raise RuntimeError('Unexpected physical tail reference set')
    return outputs


def export_tail_report():
    from collections import Counter
    rows=build_tail_keepouts()
    report={'schema':1,'source_step_sha256':sha(PCBSTEP),'source_board_sha256':sha(PCB),
        'reference_counts':dict(sorted(Counter(r['reference'] for r in rows).items())),
        'keepout_count':len(rows),'board_seating_plane_mm':20.5,
        'construction':'Actual fitted top-side solids clipped below Z20.5 and split into connected solids. Each individual tail AABB receives0.35mmXY and0.4mm outwardZ clearance. DNP excluded.',
        'keepouts':[{k:v for k,v in r.items() if k!='shape'} for r in rows],
        'limits':['Generic/modelled tails are not exact purchased pin and solder-fillet qualification.',
                  'Use full-depth windows where a residual carrier membrane would be too thin; this report supplies required minimum voids only.']}
    (VERIFICATION/'tail-clearance-report.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


def _overlap(one,two):
    a=bounds(one);b=bounds(two)
    if any(min(a['max'][i],b['max'][i])-max(a['min'][i],b['min'][i])<=1e-7 for i in range(3)):return 0.0
    result=one.intersect(two)
    if not result.isValid():raise RuntimeError('Invalid Boolean intersection')
    return result.Volume()


def baseline_report():
    keepouts=build_keepouts();parts=load_named_step(ORIGINAL)
    selected=[p for p in parts if p['part_id'] in {'TMX-A3-P01','TMX-A3-P03','TMX-A3-C02'} or p['part_id'].startswith('TMX-A3-C01-')]
    comparisons=[]
    for keepout in keepouts:
        for p in selected:
            vol=_overlap(keepout['shape'],p['shape'])
            if vol>1e-7:
                nominal=box(*keepout['union_before_extra_clearance_mm'])
                comparisons.append({'reference':keepout['reference'],'part_id':p['part_id'],
                    'name':p['name'],'clearance_envelope_intersection_mm3':vol,
                    'body_and_assembly_envelope_intersection_mm3':_overlap(nominal,p['shape']),
                    'interpretation':'Keepout violation; dimensions are conservative AABB union, not measured physical penetration'})
    inv=_step_inventory();fitted={r['Ref'] for r in csv.DictReader((V2/'manufacturing/placement-populated.csv').open())}
    extras=sorted({ref_of(p) for p in inv if ref_of(p)}-fitted)
    tails=[]
    for p in inv:
        ref=ref_of(p)
        if ref in fitted and ref not in EXPECTED_BOTTOM and p['bounds']['min'][2]+TRANSLATION[2]<20.5-1e-6:
            tails.append({'reference':ref,'definition_name':p['definition_name'],
                          'world_model_bounds_mm':[[v+t for v,t in zip(p['bounds'][key],TRANSLATION)]for key in ('min','max')],
                          'note':'Inspect actual below-board solids for tail relief; full AABB includes above-board body and must not be cut blindly.'})
    report={'status':'read_only_clearance_envelopes_ready_for_case_candidate',
        'sources':[{'path':str(p),'sha256':sha(p)}for p in (PCB,PCBSTEP,CONTRACT,ORIGINAL)],
        'registration':{'raw_kicad_origin_mm':[19.9,21.48491],'step_translation_mm':TRANSLATION,'mount_planes':MOUNT_PLANES,'mount_axes_mm':MOUNT_AXES},
        'underside_count':len(EXPECTED_BOTTOM),'absent_fitted_model_count':len(EXPECTED_MISSING),
        'absent_fitted_models':sorted(EXPECTED_MISSING),'extra_step_references_excluded_from_population':extras,
        'keepout_count':len(keepouts),'keepouts':[{k:v for k,v in q.items()if k!='shape'}for q in keepouts],
        'original_case_intersections':comparisons,'through_hole_models_with_underside_tails':tails,
        'limitations':['No source CAD modified.','STEP AABBs and stated maximum/assembly envelope are conservative geometric allocations, not exact physical geometry.',
                      'Through-hole pin tails require separate actual-shape relief.','Fixed mounting datums are retained; no mechanical strength or insertion-load qualification.']}
    path=VERIFICATION/'component-clearance-report.json';path.write_text(json.dumps(report,indent=2)+'\n')
    return report

def full_component_clearance(normalized_step_parts=None):
    """All populated component maximum/allocation boxes, for screening only.

    Complete fitted population. Top-side boxes stop at F.Cu; lower tails are
    separate. Manufacturer-sourced maxima and unqualified/header allocations
    remain explicitly labeled. J901 generic header rule is rejected in favour
    of its actual GCT model plus0.15mm assembly allowance; dimensional tolerance
    still needs the drawing/physical qualification recorded by the USB review.
    """
    parts=populated_step_parts(normalized_step_parts)
    rowmap={r['reference']:r for r in json.loads(CONTRACT.read_text())['rows'] if not r['DNP']}
    grouped={}
    for p in parts:
        ref=ref_of(p)
        if ref:grouped.setdefault(ref,[]).append(p)
    bottom={r['reference']:r for r in build_keepouts() if r['underside']}
    outputs=[]
    for ref in sorted(fitted_references()):
        if ref in bottom:
            outputs.append(bottom[ref]);continue
        row=rowmap[ref]
        low=[50.4+row['x_min_mm']-19.9,120-row['y_max_mm']+21.48491,22.1]
        high=[50.4+row['x_max_mm']-19.9,120-row['y_min_mm']+21.48491,row['Fusion_z_top_mm']]
        basis=row['basis']
        model_bounds=[]
        for p in grouped.get(ref,[]):
            b=bounds(p['shape'].translate(TRANSLATION));model_bounds.append(b)
            low[:2]=[min(a,b) for a,b in zip(low[:2],b['min'][:2])]
            high=[max(a,b) for a,b in zip(high,b['max'])]
        if ref=='J901':
            if not model_bounds:raise RuntimeError('J901 source model required')
            low=[min(b['min'][i] for b in model_bounds) for i in (0,1)]+[22.1]
            high=[max(b['max'][i] for b in model_bounds) for i in (0,1)]+[max(b['max'][2] for b in model_bounds)+.15]
            basis='Actual GCT model bounds plus0.15mm assembly allowance; default generic8.95mm header contract deliberately not used. No independent maximum-tolerance claim.'
        union=[list(low),list(high)]
        low[:2]=[v-.5 for v in low[:2]]
        high[:2]=[v+.5 for v in high[:2]];high[2]+=.5
        outputs.append({'reference':ref,'side':'F.Cu','underside':False,
            'missing_from_step':ref not in grouped,'union_before_extra_clearance_mm':union,
            'keepout_bounds_mm':[low,high],'body_height_basis':basis,'shape':box(low,high)})
    if len(outputs)!=148:raise RuntimeError('Expected148 fitted component envelopes')
    return outputs


def electronics_fit(assembly_path=None):
    """Independent final printed-assembly electronic geometry screen, read-only.

    No builder imports and no rebuilding. Captures immutable input hashes and
    refuses publication if a file changed during the calculation.
    """
    assembly=BASE/'Trimix_Enclosure_PCB2.step' if assembly_path is None else Path(assembly_path)
    inputs=[assembly,PCB,PCBSTEP,CONTRACT,V2/'manufacturing/placement-populated.csv']
    sources=[{'path':str(p.relative_to(ROOT)),'sha256':sha(p)} for p in inputs]
    expected={'TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-A3-P04','TMX-A3-P05',
              'TMX-A3-P07','TMX-A3-P08','TMX-A3-P11','TMX-V2-P12'}
    assembly_parts=load_named_step(assembly);printed={}
    for p in assembly_parts:
        match=re.search(r'(TMX-(?:A3|V2)-P[0-9]+)',p['path'])
        if match:
            pid=match[1]
            if pid in printed:raise RuntimeError('Duplicate final printed instance '+pid)
            printed[pid]=p
    if set(printed)!=expected:raise RuntimeError('Final printed part set differs '+str(set(printed)^expected))
    local=populated_step_parts(load_named_step(PCBSTEP));nominal=[]
    modeled_refs=set()
    for p in local:
        ref=ref_of(p);shape=p['shape'].translate(TRANSLATION)
        if ref:modeled_refs.add(ref)
        nominal.append({'reference':ref or 'PCB_substrate','kind':'actual_STEP',
                        'shape':shape,'source_model_path':p['path']})
    subset=build_keepouts()
    for q in subset:
        if q['missing_from_step']:
            nominal.append({'reference':q['reference'],'kind':'missing_model_maximum_plus_assembly',
                            'shape':box(*q['source_contract_bounds_mm'])})
    if modeled_refs|EXPECTED_MISSING!=fitted_references():raise RuntimeError('Nominal population incomplete')
    clearances=full_component_clearance(local)+build_tail_keepouts(normalized_step_parts=local)
    result={}
    for label,items in [('nominal_models_and_missing_maxima',nominal),('specified_clearance_envelopes',clearances)]:
        hits=[];tested=0
        for q in items:
            for pid,p in printed.items():
                tested+=1
                volume=_overlap(q['shape'],p['shape'])
                if volume>1e-7:
                    hits.append({'reference':q['reference'],'printed_part':pid,
                        'kind':q.get('kind','component_maximum_plus_clearance'),
                        'tail_index':q.get('tail_index'),
                        'intersection_volume_mm3':volume,
                        'electronic_bounds_mm':bounds(q['shape']),
                        'basis':q.get('body_height_basis',q.get('kind'))})
        result[label]={'pair_count':tested,'intersection_count':len(hits),'intersections':hits}
    unchanged=all(sha(ROOT/r['path'])==r['sha256'] for r in sources)
    if not unchanged:raise RuntimeError('Input changed during independent fit review; rerun against frozen files')
    report={'schema':1,'scope':'Independent static electronics versus all nine final printed parts; no builder calls and no source mutations.',
        'source_files':sources,'source_hashes_unchanged':unchanged,
        'fitted_component_count':148,'actual_modeled_fitted_references':len(modeled_refs),
        'missing_fitted_models_covered':sorted(EXPECTED_MISSING),
        'excluded_DNP':['C503','C706','R602','R603','R604','R804'],
        'printed_part_count':len(printed),'printed_parts':sorted(printed),
        'registration_mm':TRANSLATION,'nominal':result['nominal_models_and_missing_maxima'],
        'clearance':result['specified_clearance_envelopes'],
        'nominal_pass':not result['nominal_models_and_missing_maxima']['intersections'],
        'clearance_pass':not result['specified_clearance_envelopes']['intersections'],
        'limits':['Actual STEP models may be generic, simplified or incomplete; six absent models are covered by explicit maximum/assembly allocations.',
                  'Clearance boxes are conservative AABBs; reported overlap does not by itself establish exact physical contact.',
                  'J901 envelope uses actual model plus0.15mm assembly then0.5mm clearance; independent maximum dimensional tolerance remains open.',
                  'No connector mate, cable bend, insertion/removal sweep, support strength, printed tolerance, thermal or waterproof certification.']}
    report['pass']=report['nominal_pass'] and report['clearance_pass']
    port_hits=[q for q in report['clearance']['intersections'] if q['reference']=='J901']
    report['port_clearance_disposition']={'status':'clearance_screen_has_open_hits' if port_hits else 'clearance_screen_clear',
        'generic_clearance_exception_applied':False,'intersection_count':len(port_hits),
        'remaining':'Physical selected cable/full engagement, print and component tolerance checks required. Static envelope clearance is not mechanical or manufacturing release.'}
    (VERIFICATION/'electronics-fit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'report':str(VERIFICATION/'electronics-fit.json'),'pass':report['pass'],
                     'nominal_intersections':report['nominal']['intersection_count'],
                     'clearance_intersections':report['clearance']['intersection_count']}),flush=True)
    return report


def mount_support_review():
    """Compare housing under both original PCB inserts, without changing geometry."""
    source=load_named_step(ORIGINAL)
    old=next(p['shape'] for p in source if p['part_id']=='TMX-A3-P01')
    part_path=BASE/'parts/TMX-A3-P01.step'
    new=cq.importers.importStep(str(part_path)).val()
    evidence=[]
    for x,y in MOUNT_AXES:
        region=box([x-3.6,y-3.6,14.3],[x+3.6,y+3.6,18.5])
        a=old.intersect(region);b=new.intersect(region)
        removed_shape=a.cut(b);added_shape=b.cut(a)
        difference_valid=removed_shape.isValid() and added_shape.isValid()
        removed=removed_shape.Volume() if difference_valid else None
        added=added_shape.Volume() if difference_valid else None
        # Source nominal radius1.65 pilot, local outer radius3.6 web.
        outer=cq.Solid.makeCylinder(3.6,4.1,cq.Vector(x,y,14.4))
        inner=cq.Solid.makeCylinder(1.65,4.1,cq.Vector(x,y,14.4))
        annulus=outer.cut(inner)
        old_ring=old.intersect(annulus).Volume();new_ring=new.intersect(annulus).Volume()
        floor=box([x-1.65,y-1.65,14.3],[x+1.65,y+1.65,14.4])
        old_floor=old.intersect(floor).Volume();new_floor=new.intersect(floor).Volume()
        evidence.append({'axis_mm':[x,y],'support_plane_mm':18.5,
            'comparison_region_mm':[[x-3.6,y-3.6,14.3],[x+3.6,y+3.6,18.5]],
            'source_support_volume_mm3':a.Volume(),'candidate_support_volume_mm3':b.Volume(),
            'source_material_removed_mm3':removed,'material_added_mm3':added,
            'nominal_radial_web_mm':3.6-1.65,'radial_annulus_probe_volume_mm3':annulus.Volume(),
            'source_annulus_occupied_mm3':old_ring,'candidate_annulus_occupied_mm3':new_ring,
            'source_bottom_floor_mm3':old_floor,'candidate_bottom_floor_mm3':new_floor,
            'local_difference_valid':difference_valid,
            'local_difference_limitation':None if difference_valid else 'Coincident STEP faces produced invalid cut results; these results are not used as evidence of removed material or identity.',
            'local_support_volume_matches':abs(a.Volume()-b.Volume())<1e-6,
            'annulus_fully_occupied':abs(new_ring-annulus.Volume())<1e-6,
            'bottom_floor_fully_occupied':abs(new_floor-floor.Volume())<1e-6,
            'annulus_not_reduced':new_ring>=old_ring-1e-6,'bottom_floor_not_reduced':new_floor>=old_floor-1e-6})
    report={'schema':1,'source_housing_sha256':sha(ORIGINAL),'candidate_housing_sha256':sha(part_path),
        'checks':evidence,'pass':all(q['local_support_volume_matches'] and q['annulus_fully_occupied'] and q['bottom_floor_fully_occupied'] and q['annulus_not_reduced'] and q['bottom_floor_not_reduced'] for q in evidence),
        'scope':'Minimum support witnesses: complete1.95mm radial annulus atZ14.4..18.5 and full0.1mm source bottom-floor probe at both original PCB insert axes, plus equal local support volumes. Invalid coincident-face cuts are explicitly not accepted as exact BRep equivalence.',
        'limits':['Retains the source generic OD3.2x4mm insert allocation and original pilot; not a supplier-specific heat-set/pull-out strength qualification.',
                  'A preserved source feature is not independently qualified merely by being unchanged.']}
    (VERIFICATION/'mount-support-review.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument('--electronics-fit',action='store_true')
    parser.add_argument('--baseline',action='store_true')
    parser.add_argument('--tails',action='store_true')
    args=parser.parse_args()
    if args.electronics_fit:electronics_fit()
    elif args.tails:export_tail_report()
    else:baseline_report()
