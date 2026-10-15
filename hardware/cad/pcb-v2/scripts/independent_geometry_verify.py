"""Read-only independent PCB2 enclosure checks; never writes CAD inputs.

Includes exact BREP common-volume checks, separately reported coplanar contacts,
and continuous translation certificates from the 1-Lipschitz distance bound.
Certificates are numerical to the stated kernel tolerance, not strength claims.
"""
from pathlib import Path
import sys, json, hashlib, math, argparse, time, csv, re
import cadquery as cq

ROOT = next(p for p in Path(__file__).resolve().parents if (p/'hardware/pcb/analyzer-v2').is_dir())
sys.path.insert(0, str(ROOT / 'hardware/cad/pcb-v2/scripts'))
from step_inputs import load_named_step, bounds

OUT = ROOT / 'hardware/cad/pcb-v2'
WORK = ROOT/'build/pcb-v2-case-work'
SHIFT = (50.4,120,20.5529)
EPS = 1e-6

def box(a):
    x0,x1,y0,y1,z0,z1 = a
    return cq.Solid.makeBox(x1-x0,y1-y0,z1-z0,cq.Vector(x0,y0,z0))

def common_volume(a,b):
    return sum(abs(s.Volume()) for s in a.intersect(b).Solids())

def bbox_gap(a,b):
    aa,bb = bounds(a),bounds(b)
    return math.sqrt(sum(max(0,aa['min'][i]-bb['max'][i],bb['min'][i]-aa['max'][i])**2 for i in range(3)))

def metrics(a,b):
    gap = a.distance(b)
    result = {'distance_mm':gap,'intersection_volume_mm3':0.0,'coplanar_contact_area_mm2':0.0}
    if gap <= EPS:
        common = a.intersect(b)
        signed = [s.Volume() for s in common.Solids()]
        volume = sum(abs(v) for v in signed)
        result['intersection_volume_mm3'] = volume
        result['common_valid'] = common.isValid()
        result['common_signed_solid_volumes_mm3'] = signed
        result['boolean_orientation_warning'] = any(v < -EPS for v in signed)
        if volume <= EPS:
            result['coplanar_contact_area_mm2'] = sum(f.Area() for f in common.Faces())
            result['contact_edges'] = len(common.Edges())
    result['volume_pass'] = result['intersection_volume_mm3'] <= EPS
    return result

def record(label,shape):
    return {'label':label,'shape':shape}

def sweep_certificate(label,moving,fixed,start,delta,initial_steps=16,max_depth=9):
    """Certify every interval: distance(mid) >= half its translation span.

    Rigid translation changes set distance by at most the translation norm.
    Failed intervals subdivide; nonpositive interval bounds never become a pass
    merely because endpoints are clear. Contact endpoints are checked by volume.
    """
    total = math.sqrt(sum(x*x for x in delta))
    cache = {}
    failures = []
    uncertain = []
    min_distance = float('inf')
    count = 0
    def at(t):
        nonlocal min_distance,count
        key = round(t,14)
        if key in cache:return cache[key]
        offset = tuple(start[i]+delta[i]*t for i in range(3))
        shape = moving.translate(offset)
        gap = shape.distance(fixed)
        min_distance = min(min_distance,gap)
        volume = common_volume(shape,fixed) if gap <= EPS else 0.0
        data = {'t':t,'offset_mm':offset,'distance_mm':gap,'intersection_volume_mm3':volume}
        cache[key] = data;count += 1
        if volume > EPS:failures.append(data)
        return data
    def interval(a,b,depth):
        if failures:return
        if count > 1800:
            uncertain.append({'t0':a,'t1':b,'reason':'evaluation budget reached; no clearance claim'})
            return
        mid = (a+b)/2
        r = at(mid)
        if r['intersection_volume_mm3'] > EPS:return
        radius = (b-a)*total/2
        if r['distance_mm'] >= radius-EPS:return
        if depth >= max_depth:
            uncertain.append({'t0':a,'t1':b,'distance_at_mid_mm':r['distance_mm'],'half_span_mm':radius})
            return
        interval(a,mid,depth+1);interval(mid,b,depth+1)
    at(0);at(1)
    for i in range(initial_steps):
        if failures:break
        interval(i/initial_steps,(i+1)/initial_steps,0)
    return {'label':label,'start_offset_mm':start,'translation_mm':delta,
            'method':'midpoint exact BREP distance with rigid-translation Lipschitz bound; EPS=1e-6 mm',
            'sample_evaluations':count,'min_sample_distance_mm':min_distance,
            'volume_collisions':failures,'uncertified_intervals':uncertain,
            'certified_clear':not failures and not uncertain}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--quick',action='store_true');ap.add_argument('--output',default='independent-geometry-review.json');args=ap.parse_args()
    started=time.time()
    inputs=[OUT/'parts'/f'{name}.step' for name in ['TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-V2-P12']]
    for p in inputs:
        if not p.exists():raise SystemExit('Missing output: '+str(p))
    # A builder may be working concurrently. Bind every check to captured bytes,
    # and disclose if any canonical export changed during this audit.
    snapshot=WORK/'independent-inputs';snapshot.mkdir(parents=True,exist_ok=True)
    hashes={};captured={}
    for p in inputs:
        raw=p.read_bytes();digest=hashlib.sha256(raw).hexdigest()
        hashes[str(p.relative_to(ROOT))]=digest
        saved=snapshot/(p.stem+'-'+digest[:12]+'.step');saved.write_bytes(raw)
        captured[p.stem]=saved
    new={name:cq.importers.importStep(str(p)).val() for name,p in captured.items()}
    h,c,tray,bezel=[new[k] for k in ['TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-V2-P12']]
    source=load_named_step(ROOT/'hardware/cad/rev04/original-fit-check/Trimix_Enclosure_A3_Original.step')
    pcbs=load_named_step(ROOT/'hardware/pcb/analyzer-v2/review/Trimix_PCB_V2_Mechanical.step')
    with (ROOT/'hardware/pcb/analyzer-v2/manufacturing/placement-populated.csv').open() as handle:
        fitted={row['Ref'] for row in csv.DictReader(handle)}
    pcbs=[p for p in pcbs if not (m:=re.search(r' / ([A-Z]+[0-9]+) \[',p['path'])) or m[1] in fitted]
    for p in pcbs:p['shape']=p['shape'].translate(SHIFT)
    originals={p['part_id']:p['shape'] for p in source if p['part_id'].startswith('TMX-A3-P')}
    report={'input_sha256':hashes,'pcb_translation_mm':SHIFT,'geometry':{},'static':[],
            'bearing_patch_checks':{},'thickness_checks':[],'sweeps':[],
            'limits':['No native Fusion save/reopen claim. No strength, fit tolerance, print-process, or weather-seal qualification.',
                      'Missing native component models are not validated by a STEP-only clearance check.',
                      'Static and sweep contacts are separate from positive-volume overlap.',
                      'coplanar_contact_area_mm2 is only the area of faces returned by the solid Boolean common; zero does not mean an actual bearing interface has zero contact area. Zero distance without common-solid volume is reported separately.']}
    for name,s in new.items():
        report['geometry'][name]={'valid':s.isValid(),'solids':len(s.Solids()),'volume_mm3':s.Volume(),'bounds':bounds(s)}
    for a,b in [('TMX-A3-P01','TMX-V2-P12'),('TMX-A3-P01','TMX-A3-P02'),('TMX-V2-P12','TMX-A3-P02'),('TMX-A3-P01','TMX-A3-P03')]:
        report['static'].append({'pair':[a,b],**metrics(new[a],new[b])})
    # Independently relocate the two existing M2x5 BREP screw instances from
    # their source seats (33.2) to the new bridge seat (28.2). A hardware/head
    # collision is not covered by checking only the printed-part interfaces.
    old_usb_screws=[p for p in source if p['part_id']=='TMX-A3-F02' and abs(p['bounds']['center'][1]-18.25)<.01]
    assert len(old_usb_screws)==2
    report['usb_screws_and_driver_access']=[]
    relocated_screw_shapes=[]
    for p,yy in zip(old_usb_screws,(17,42.5)):
        sx,sy=p['bounds']['center'][:2]
        screw=p['shape'].translate((91-sx,yy-sy,-5))
        relocated_screw_shapes.append(screw)
        # This is an explicit selected clearance gauge, not a supplier tool CAD.
        driver=cq.Solid.makeCylinder(2.2,29.8,cq.Vector(91,yy,30.2))
        report['usb_screws_and_driver_access'].append({'axis_xy_mm':[91,yy],
            'source_screw':p['path'],'new_under_head_z_mm':28.2,
            'screw_housing':metrics(screw,h),'screw_bezel':metrics(screw,bezel),
            'driver_gauge_diameter_mm':4.4,'driver_gauge_z_mm':[30.2,60],
            'driver_gauge_housing':metrics(driver,h),'driver_gauge_bezel':metrics(driver,bezel)})
    for name,s in new.items():
        rows=[]
        for p in pcbs:
            if bbox_gap(s,p['shape']) <= EPS:
                r=metrics(s,p['shape'])
                if r['distance_mm'] <= EPS:rows.append({'pcb_instance':p['path'],**r})
        report['static'].append({'part':name,'pcb_contacts_and_intersections':rows})
    # Closed old port: witness fully inside the former rounded opening and wall.
    witness=box([36.5,48.5,.25,2.35,22.5,29.5])
    report['old_port_closure']={'witness_volume_mm3':witness.Volume(),'filled_volume_mm3':common_volume(h,witness)}
    report['old_port_closure']['pass']=abs(report['old_port_closure']['filled_volume_mm3']-witness.Volume())<EPS
    # Check old cover USB attachments vanished, without eating the lid plate.
    key=box([37.4,47.6,20,24.4,31.2,40.799])
    plate=box([37.4,47.6,20,24.4,40.801,42.1])
    report['old_cover_usb_retirement']={'remaining_below_plate_mm3':common_volume(c,key),'plate_witness_missing_mm3':plate.Volume()-common_volume(c,plate)}
    # Nominal bare tab slab; a solid witness is valid because none of these
    # printed supports should enter a PCB mounting/connector hole.
    for thickness in (1.44,1.6,1.76):
        slab=box([80.4,95.5,21,38.48491,20.5,20.5+thickness])
        report['thickness_checks'].append({'thickness_mm':thickness,'bezel':metrics(bezel,slab),'housing':metrics(h,slab)})
    for label,y0,y1 in [('lower',21.8,23.8),('upper',36,38)]:
        lower=box([88,93.8,y0,y1,20.49,20.5]);upper=box([88,93.8,y0,y1,22.36,22.37])
        report['bearing_patch_checks'][label]={'under_support_witness_fill_mm3':common_volume(h,lower),'under_expected_mm3':lower.Volume(),
            'top_capture_witness_fill_mm3':common_volume(bezel,upper),'top_expected_mm3':upper.Volume()}
    # Retained battery envelope and exact source bodies stay clear of additions.
    battery_ids={'TMX-A3-C02','TMX-A3-C03-V01','TMX-A3-C03-V02','TMX-A3-C10-V01','TMX-A3-C10-V02'}
    report['battery']=[{'part_id':p['part_id'],'name':p['name'],'housing':metrics(h,p['shape']),'cover':metrics(c,p['shape'])}
        for p in source if p['part_id'] in battery_ids]
    # Detector controls. Each deliberately introduced interference must be found.
    cube=box([0,1,0,1,0,1]);touch=box([1,2,0,1,0,1]);overlap=box([.5,1.5,0,1,0,1])
    report['controls']={'overlap_detected':common_volume(cube,overlap)>.49,
        'coplanar_zero_volume':common_volume(cube,touch)<EPS,
        'real_bezel_inserted_too_low_detected':common_volume(bezel.translate((0,0,-.5)),h)>EPS,
        'old_usb_key_present_detected':common_volume(originals['TMX-A3-P02'],key)>EPS,
        'old_port_open_detected':common_volume(originals['TMX-A3-P01'],witness)<witness.Volume()-1,
        'previous_end_wall_head_clash_detected':common_volume(relocated_screw_shapes[0],box([87,94.2,13.2,15.6,28.5,40.6]))>1.4}
    print('Static checks complete',flush=True)
    if not args.quick:
        # Cover and its M3 screws have been removed before the bezel operation.
        local=box([85.5,97,12.5,47,14,41])
        fixed=[h.intersect(local)]
        fixed += [p['shape'] for p in pcbs if bbox_gap(p['shape'],local)<EPS]
        port_obstacles=cq.Compound.makeCompound(fixed)
        report['sweeps'].append(sweep_certificate('bezel first +0.10Z seat release',bezel,port_obstacles,(0,0,0),(0,0,.1),initial_steps=2))
        report['sweeps'].append(sweep_certificate('bezel +X withdrawal after seat release',bezel,port_obstacles,(0,0,.1),(15,0,0),initial_steps=80))
        print('Bezel continuous paths complete',flush=True)
        # After opening the rear, cover moves in +Z. Four rear M3 screws must
        # be removed; all fixed electronics and internal hardware stay present.
        retired={'TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-A3-P06','TMX-A3-P09','TMX-A3-P10','TMX-A3-B01','TMX-A3-B02','TMX-A3-F01',*(f'TMX-A3-C05-V0{i}' for i in range(1,5))}
        fixed=[h,tray,bezel]+[p['shape'] for p in pcbs]
        for p in source:
            if p['part_id'] in retired:continue
            if p['part_id'] in ('TMX-A3-F02','TMX-A3-F05') and abs(p['bounds']['center'][1]-18.25)<.01:continue
            fixed.append(p['shape'])
        # Crop away geometry that cannot be touched by this purely upward move.
        cover_region=box([-1,98,-1,181,31.15,54])
        fixed=[f.intersect(cover_region) for f in fixed if bbox_gap(f,cover_region)<EPS]
        fixed=[f for f in fixed if f.Solids()]
        cover_obstacles=cq.Compound.makeCompound(fixed)
        report['sweeps'].append(sweep_certificate('cover +Z removal after four rear screws removed',c,cover_obstacles,(0,0,0),(0,0,13),initial_steps=64))
        print('Cover continuous path complete',flush=True)
        # Full PCB/carrier removal requires disconnected harnesses and removal
        # of the two original PCB screws. Check the inherited side button as a
        # real obstacle first, rather than silently omitting its occupied space.
        moving=cq.Compound.makeCompound([tray]+[p['shape'] for p in pcbs])
        mb=bounds(moving)
        region=box([mb['min'][0]-.01,mb['max'][0]+.01,mb['min'][1]-.01,mb['max'][1]+.01,mb['min'][2]-.01,mb['max'][2]+25.01])
        moving_ids=retired|{'TMX-A3-F03'}
        candidates=[record('new fixed housing',h)]
        for p in source:
            if p['part_id'] in moving_ids:continue
            if p['part_id'] in ('TMX-A3-F02','TMX-A3-F05') and abs(p['bounds']['center'][1]-18.25)<.01:continue
            if bbox_gap(p['shape'],region)<EPS:
                candidates.append({'label':p['path'],'part_id':p['part_id'],'shape':p['shape']})
        obstacles=cq.Compound.makeCompound([p['shape'].intersect(region) for p in candidates])
        lift=sweep_certificate('PCB+carrier +25Z with button retained',moving,obstacles,(0,0,0),(0,0,25),initial_steps=160)
        report['sweeps'].append(lift)
        if lift['volume_collisions']:
            at_first=lift['volume_collisions'][0]['offset_mm']
            moved=moving.translate(at_first)
            report['pcb_lift_first_obstruction']=[{'label':p['label'],'part_id':p.get('part_id'),**metrics(moved,p['shape'])}
                for p in candidates if bbox_gap(moved,p['shape'])<EPS and common_volume(moved,p['shape'])>EPS]
        if not lift['certified_clear']:
            button_removed=[p for p in candidates if not p.get('part_id','').startswith('TMX-A3-C04')]
            obstacles=cq.Compound.makeCompound([p['shape'].intersect(region) for p in button_removed])
            clear_lift=sweep_certificate('PCB+carrier +25Z after inherited button assembly removed',moving,obstacles,(0,0,0),(0,0,25),initial_steps=160)
            clear_lift['additional_prerequisite']='Remove the Original side-button body/nut/cap references (TMX-A3-C04 variants); harnesses disconnected, cover/bezel and PCB screws already removed.'
            report['sweeps'].append(clear_lift)
        print('PCB/carrier continuous path complete',flush=True)
        # Negative control: same algorithm must reject an unsampled midpoint
        # collision (both original endpoints clear).
        control=sweep_certificate('mid-path collision negative control',cube,box([2,3,0,1,0,1]),(0,0,0),(4,0,0),initial_steps=1,max_depth=4)
        report['controls']['midpath_collision_detected']=not control['certified_clear'] and bool(control['volume_collisions'])
    report['elapsed_seconds']=time.time()-started
    report['input_hashes_still_match']=all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h for p,h in hashes.items())
    report['all_controls_pass']=all(report['controls'].values())
    destination=OUT/'verification'/args.output;destination.write_text(json.dumps(report,indent=2)+'\n')
    print(str(destination),flush=True)
    print(json.dumps({'valid_solid_counts':{k:[v['valid'],v['solids']] for k,v in report['geometry'].items()},'static_volume_collisions':[r for r in report['static'] if r.get('intersection_volume_mm3',0)>EPS],
        'sweeps':[{k:v for k,v in s.items() if k not in ('volume_collisions','uncertified_intervals')} for s in report['sweeps']],
        'controls':report['controls'],'elapsed_seconds':report['elapsed_seconds']},indent=2),flush=True)

if __name__=='__main__':main()
