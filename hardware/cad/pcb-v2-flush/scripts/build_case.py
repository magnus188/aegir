"""Reproducible PCB2 case adaptation; source archives and electronics are read-only.
Run with CadQuery 2.8 / OCP 7.9. All dimensions millimetres in assembly coordinates.
This creates a separate STEP assembly and print meshes, never overwrites Fusion sources.
"""
from pathlib import Path
import json, hashlib, sys
import cadquery as cq
from step_inputs import load_named_step, bounds, serializable_parts
ROOT=Path(__file__).resolve().parents[4]
OUT=Path(__file__).resolve().parents[1]
SRC=ROOT/'hardware/cad/rev04/original-fit-check/Trimix_Enclosure_A3_Original.step'
PCB=ROOT/'hardware/pcb/analyzer-v2/review/Trimix_PCB_V2_Mechanical.step'
SHIFT=(50.4,120,20.5529)

def box(x0,x1,y0,y1,z0,z1):
    return cq.Solid.makeBox(x1-x0,y1-y0,z1-z0,cq.Vector(x0,y0,z0))
def cyl(x,y,z0,z1,r):
    return cq.Solid.makeCylinder(r,z1-z0,cq.Vector(x,y,z0))
def fuse(a,*b):return a.fuse(*b).clean()
def cut(a,*b):return a.cut(*b).clean()
def rounded_slot_x(x0,x1,y,z,w,h,r):
    # YZ aperture with true circular corners, extruded along X.
    s=box(x0,x1,y-w/2+r,y+w/2-r,z-h/2,z+h/2)
    s=fuse(s,box(x0,x1,y-w/2,y+w/2,z-h/2+r,z+h/2-r))
    for yy in (y-w/2+r,y+w/2-r):
        for zz in (z-h/2+r,z+h/2-r):
            s=fuse(s,cq.Solid.makeCylinder(r,x1-x0,cq.Vector(x0,yy,zz),cq.Vector(1,0,0)))
    return s

def outer_envelope(width, straight_right=False):
    """Match the original 0.25 mm rear taper; widened right wall is planar."""
    lower=cq.Workplane('XY').polyline([(0,0),(width,0),(width,180),(0,180)]).close().val()
    right=width if straight_right else width-.25
    upper=cq.Workplane('XY',origin=(0,0,40.6)).polyline([(.25,.25),(right,.25),(right,179.75),(.25,179.75)]).close().val()
    raw=cq.Solid.makeLoft([lower,upper],ruled=True)
    vertical=[e for e in raw.Edges() if e.BoundingBox().zlen>40]
    assert len(vertical)==4
    return raw.fillet(8,vertical)

def rounded_xy(x0,x1,y0,y1,z0,z1,r):
    raw=box(x0,x1,y0,y1,z0,z1)
    return raw.fillet(r,[e for e in raw.Edges() if e.BoundingBox().zlen>z1-z0-.001])

def flush_body(h,c):
    """Continuous wider shell, with protected access wells at original controls.

    The extra material is outside the original envelope; its hollow does not
    cut the retained battery supports, front display mount or main board mounts.
    """
    original_envelope=outer_envelope(85)
    widened=outer_envelope(96.6,straight_right=True)
    extension=cut(widened,original_envelope)
    extension=cut(extension,rounded_xy(2.4,94.2,2.4,177.6,2.4,40.8,5.6))
    h=fuse(h,extension)
    # Closed pocket walls isolate finger access from the added hollow side bay.
    button_pocket=cut(box(84.7,96.6,98.6,127.4,16.6,40.6),box(84.6,96.8,101,125,19,40.7))
    h=fuse(h,button_pocket)
    h=cut(h,box(84.9,96.8,101,125,19,40.7))
    # Gas fitting is preserved; an 18 mm access bore surrounds its 8 mm model.
    gas_outer=cq.Solid.makeCylinder(11.4,11.9,cq.Vector(84.7,162.5,22),cq.Vector(1,0,0))
    gas_access=cq.Solid.makeCylinder(9,11.9,cq.Vector(84.9,162.5,22),cq.Vector(1,0,0))
    gas_liner=cut(gas_outer,cq.Solid.makeCylinder(9,12.3,cq.Vector(84.6,162.5,22),cq.Vector(1,0,0)))
    h=cut(fuse(h,gas_liner),gas_access)
    # Re-open the full service path after adding the continuous outer wall.
    h=cut(h,box(94,96.8,19.8,39.7,18.3,40.7),box(87,96.8,13,46.5,26.2,28.5))
    # Same four screw seats and battery pads; broaden only the cover perimeter.
    old_cover=rounded_xy(.25,84.75,.25,179.75,40.8,43,8)
    new_cover=rounded_xy(.25,96.6,.25,179.75,40.8,43,8)
    def top_roll(s):
        edges=[e for e in s.Edges() if abs(e.BoundingBox().zmin-43)<.0001 and abs(e.BoundingBox().zmax-43)<.0001]
        return s.fillet(.8,edges)
    old_cover=top_roll(old_cover)
    new_cover=top_roll(new_cover)
    c=fuse(c,cut(new_cover,old_cover))
    for y0,y1 in ((55,67),(135,147)):
        c=fuse(c,box(91.9,93.9,y0,y1,38.6,40.9))
    return h,c

def main():
    for d in ('parts','verification','views'): (OUT/d).mkdir(exist_ok=True)
    sources={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (SRC,PCB,SRC.with_suffix('.f3d'))}
    if sources[str(PCB.relative_to(ROOT))]!='3498d386c480d86322c211ac33cb6374d962162a937a167221fb9be26367aefb':
        raise RuntimeError('Unexpected PCB STEP; re-establish mechanical datums')
    from component_clearance import build_keepouts, build_tail_keepouts, populated_step_parts, ref_of
    src=load_named_step(SRC);local_pcbs=populated_step_parts(load_named_step(PCB))
    tail_keepouts=build_tail_keepouts(normalized_step_parts=local_pcbs)
    pcbs=[dict(p) for p in local_pcbs]
    for p in pcbs:p['shape']=p['shape'].translate(SHIFT);p['bounds']=bounds(p['shape'])
    originals={p['part_id']:p['shape'] for p in src if p['part_id'].startswith('TMX-A3-P')}
    h=originals['TMX-A3-P01'];c=originals['TMX-A3-P02'];tray=originals['TMX-A3-P03']
    print('Loaded originals and PCB',flush=True)
    # Local side pod. Existing global enclosure, battery and board mounting datums stay fixed.
    old_mount=h.intersect(box(72.4,82.7,24.4,31.6,14.3,18.5))
    old_display_bracket=h.intersect(box(67.3,82.7,8.5,16,14.3,18.5))
    h=fuse(h,box(82,96.6,12.2,47.3,14.3,40.6))
    h=cut(h,box(80.4,94.2,14.6,44.9,16.7,40.8))
    h=fuse(h,old_mount,old_display_bracket)
    # Retire the old lower daughterboard support floor. Preserve the bottom wall.
    h=cut(h,box(29.35,55.65,2.4,22.05,16.95,19.05))
    # Close the old USB aperture, retaining the original front-to-back wall taper.
    plug=box(34.2,50.8,0,2.5,20.8,31.2)
    outside=cq.Workplane('YZ',origin=(33,0,0)).polyline([(-1,0),(0,0),(.25,40.6),(-1,40.6)]).close().extrude(20).val()
    h=fuse(h,cut(plug,outside))
    # Bare-board ledges rooted on the pod floor, no support below USB solder/stakes.
    for y0,y1 in ((21.8,23.8),(36,38)):
        h=fuse(h,box(88,93.8,y0,y1,16.7,20.5))
    # Supplier selected TC-M2x3.0 insert envelope, two reused M2x5 screw allocations.
    for yy in (17,42.5):h=fuse(h,cyl(91,yy,16.7,26.2,3.8))
    h=cut(h,box(94,96.8,19.8,39.7,18.3,40.7),box(87,96.8,13,46.5,26.2,28.5))
    for yy in (17,42.5):h=cut(h,cyl(91,yy,22.2,26.3,1.6))
    h=fuse(h,box(49,50.3,26,38,20,22.6))
    # Rear cover retains its four screws and all battery capture pads.
    c=cut(c,box(37.3,47.7,19.9,24.5,31.1,40.8))
    c=fuse(c,box(82,96.6,12.2,47.3,40.8,43),box(91.9,93.9,24,35,38.6,40.9))
    # Detachable L bezel. Full-height side opening is cleared when it slides out.
    bezel=fuse(box(87.2,96.6,13.2,46.3,26.2,28.2),box(94.2,96.6,20,39.5,18.5,40.6))
    bezel=cut(bezel,box(93.9,95.7,20.7,38.8,20.2,26.1),rounded_slot_x(95.2,96.8,29.48491,23.83,10.8,5.2,.4))
    for y0,y1 in ((21.8,23.8),(36,38)):
        bezel=fuse(bezel,box(88,93.8,y0,y1,22.36,26.2))
    for yy in (17,42.5):bezel=cut(bezel,cyl(91,yy,26.1,28.3,1.15))
    h,c=flush_body(h,c)
    print('Built continuous flush side wall and retained removable USB support',flush=True)
    # PCB back-side clearances are driven by current STEP and the height contract.
    keepouts=build_keepouts()+tail_keepouts
    # Four-millimetre carrier frame; same board plane and screw engagement.
    # Fixed mounting webs retain their existing 18.5 mm bearing interface.
    tray=cut(fuse(tray,tray.translate((0,0,-2))),h,
        box(72.3,82.8,24.3,31.7,16.4,18.5),box(48.1,58.5,110.3,117.7,16.4,18.5),
        box(50.1,50.4,20.4,120.6,16.4,20.6))
    # These shapes include .5mm XY/Z reserve, and may cut through the carrier.
    for k in keepouts:
        if k.get('underside'):
            bb=bounds(k['shape'])
            lo,hi=bb['min'],bb['max']
            # Open a window to the edge instead of leaving an unprintable sliver.
            if 0 < lo[0]-50.2 < 2: lo[0]=50.1
            if 0 < 80.6-hi[0] < 2: hi[0]=80.7
            window=box(lo[0],hi[0],lo[1],hi[1],16.4,20.6)
            tray=cut(tray,window)
            h=cut(h,k['shape'])
    # Ventilation windows leave a perimeter frame and cross-members.
    for x0,x1,y0,y1 in ((55,76,80,99),(55,76,51,69),(55,76,39,47),(53.5,71.5,24,37.5)):
        tray=cut(tray,box(x0,x1,y0,y1,16.4,20.6))
    solids=sorted(tray.Solids(),key=lambda s:s.Volume(),reverse=True)
    scrap=[{'volume_mm3':s.Volume(),'bounds':bounds(s)} for s in solids[1:]]
    if sum(s['volume_mm3'] for s in scrap)>0.02*solids[0].Volume():raise RuntimeError('Carrier fragmented; redesign load path')
    tray=solids[0] # Remove isolated manufacturing scrap, never a connected load path.
    parts={'TMX-A3-P01':h,'TMX-A3-P02':c,'TMX-A3-P03':tray,'TMX-V2-P12':bezel}
    # A carry-forward part keeps its exact source BREP and placement.
    retired={'TMX-A3-P06','TMX-A3-P09','TMX-A3-P10','TMX-A3-B01','TMX-A3-B02',*(f'TMX-A3-C05-V0{i}' for i in range(1,5))}
    relocated_screws=[];retired_insert_paths=[];retained=[]
    for p in src:
        if p['part_id'] in retired:continue
        b=p['bounds'];ctr=b['center']
        if p['part_id']=='TMX-A3-F02' and abs(ctr[1]-18.25)<.01:
            relocated_screws.append(p);continue
        if p['part_id']=='TMX-A3-F05' and abs(ctr[1]-18.25)<.01:
            retired_insert_paths.append(p['path']);continue
        q=dict(p);q['shape']=parts.get(p['part_id'],p['shape']);retained.append(q)
    assert len(relocated_screws)==2 and len(retired_insert_paths)==2
    retained.append({'part_id':'TMX-V2-P12','name':'USB removable retainer','shape':bezel})
    for idx,(p,yy) in enumerate(zip(relocated_screws,(17,42.5)),1):
        # Original M2x5 under-head seat33.2, new seat28.2.
        bb=p['bounds'];sx,sy=bb['center'][:2]
        retained.append({'part_id':f'TMX-V2-F02-{idx}','name':f'USB M2x5 screw {idx}','shape':p['shape'].translate((91-sx,yy-sy,-5))})
        # Cylindrical occupied-space model only, no invented knurl/thread topology.
        ins=cut(cyl(91,yy,23.2,26.2,1.8),cyl(91,yy,23.1,26.3,1))
        retained.append({'part_id':f'TMX-V2-F05-{idx}','name':f'TC-M2x3 insert envelope {idx}','shape':ins})
    for p in pcbs:retained.append({'part_id':'PCB2-'+(ref_of(p) or 'substrate'),'name':p['name'],'shape':p['shape']})
    report={'sources':sources,'variant':'flush continuous right wall',
      'exterior_width_mm':96.6,'usb_outer_face_x_mm':96.6,'usb_mouth_x_mm':95.5,
      'control_access':{'button_recess_mm':10.1,'button_well_yz_mm':[24,21.8],'gas_access_diameter_mm':18,'gas_tip_recess_mm':5.6},
      'pcb_translation_mm':SHIFT,'nominal_pcb_back_front_mm':[20.5,22.1],
      'carrier_isolated_scrap_removed':scrap,'changed_print_parts':{},'retired_part_ids':sorted(retired),'retired_generic_insert_paths':retired_insert_paths,
      'retained_source_instance_count':len(retained)-len(pcbs)-5,'issues':[],
      'status':'FIT PROTOTYPE; not a load-tested or waterproof production release'}
    assembly=cq.Assembly(name='Trimix_Enclosure_PCB2_Flush_FitPrototype')
    for i,p in enumerate(retained):
        shape=p['shape'];pid=p['part_id']
        color=(.17,.22,.28) if pid=='TMX-A3-P01' else (.58,.63,.68) if pid=='TMX-A3-P02' else (.94,.44,.12) if pid=='TMX-V2-P12' else (.16,.46,.26) if pid.startswith('PCB2-') else (.6,.62,.64)
        assembly.add(shape,name=f'{i:03d}_{pid}',color=cq.Color(*color))
    for pid,s in parts.items():
        print(pid,len(s.Solids()),s.isValid(),s.Volume(),flush=True)
        report['changed_print_parts'][pid]={'valid':s.isValid(),'solids':len(s.Solids()),'volume_mm3':s.Volume(),'bounds':bounds(s)}
        cq.exporters.export(s,str(OUT/'parts'/f'{pid}.step'))
        cq.exporters.export(s,str(OUT/'parts'/f'{pid}.stl'),tolerance=.05,angularTolerance=.1)
        if not s.isValid() or len(s.Solids())!=1:report['issues'].append(pid+' is not one valid solid')
    # Export unchanged print parts too, so the fit assembly is self-contained.
    for pid in ('TMX-A3-P04','TMX-A3-P05','TMX-A3-P07','TMX-A3-P08','TMX-A3-P11'):
        cq.exporters.export(originals[pid],str(OUT/'parts'/f'{pid}.stl'),tolerance=.05,angularTolerance=.1)
    print('Exporting assembly',flush=True)
    assembly.save(str(OUT/'Trimix_Enclosure_PCB2.step'))
    report['assembly_instances']=[{'part_id':p['part_id'],'name':p['name'],'bounds':bounds(p['shape']),'solid_count':len(p['shape'].Solids()),'volume_mm3':p['shape'].Volume()} for p in retained]
    report['source_hashes_unchanged']=all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h for p,h in sources.items())
    (OUT/'verification/build-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Finished export',flush=True)
if __name__=='__main__':main()
