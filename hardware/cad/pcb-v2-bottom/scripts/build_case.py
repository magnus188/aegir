"""Bottom USB case prototype. Source CAD and PCB files are read-only.
Run with the project CadQuery runtime. The builder states whether final PCB STEP
or a source-model placement study was used; neither is silently substituted.
"""
from pathlib import Path
import csv,json,hashlib,sys
import cadquery as cq
from step_inputs import load_named_step,bounds
from component_clearance import populated_step_parts,ref_of,build_keepouts,build_tail_keepouts
ROOT=Path(__file__).resolve().parents[4];OUT=Path(__file__).resolve().parents[1]
SRC=ROOT/'hardware/cad/rev04/original-fit-check/Trimix_Enclosure_A3_Original.step'
OLDPCB=ROOT/'hardware/pcb/analyzer-v2/review/Trimix_PCB_V2_Mechanical.step'
FINALPCB=ROOT/'hardware/pcb/analyzer-v2-bottom/review/Trimix_PCB_V2_Bottom_Mechanical.step'
CORE_SHIFT=(0,-18.9,3.5);BOSSES=((30.01509,5.8),(42.5,22.5));PATCHES=((34.81509,36.81509,3,8.8),(48.5,50.5,3,8.8))
def box(x0,x1,y0,y1,z0,z1):return cq.Solid.makeBox(x1-x0,y1-y0,z1-z0,cq.Vector(x0,y0,z0))
def cyl(x,y,z0,z1,r):return cq.Solid.makeCylinder(r,z1-z0,cq.Vector(x,y,z0))
def cyl_x(x0,x1,y,z,r):return cq.Solid.makeCylinder(r,x1-x0,cq.Vector(x0,y,z),cq.Vector(1,0,0))
def fuse(a,*b):return a.fuse(*b).clean()
def cut(a,*b):return a.cut(*b).clean()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def lower_right_wire(xright,yfront,yback,z,r=4):
 xc=xright-r;yc=yfront+r;d=r/(2**.5)
 return (cq.Workplane('XY',origin=(0,0,z)).moveTo(77.6,yfront)
  .lineTo(xc,yfront).threePointArc((xc+d,yc-d),(xright,yc))
  .lineTo(xright,yback).lineTo(77.6,yback).close().val())
def rounded_xy(x0,x1,y0,y1,z0,z1,r):
 s=box(x0,x1,y0,y1,z0,z1)
 return s.fillet(r,[e for e in s.Edges() if e.BoundingBox().zlen>z1-z0-.001])
def outside_front(x0,x1):
 return cq.Workplane('YZ',origin=(x0,0,0)).polyline([(-1,0),(0,0),(.25,40.6),(-1,40.6)]).close().extrude(x1-x0).val()
def rounded_slot_y(x,z,w,h,r,y0,y1):
 s=fuse(box(x-w/2+r,x+w/2-r,y0,y1,z-h/2,z+h/2),box(x-w/2,x+w/2,y0,y1,z-h/2+r,z+h/2-r))
 for xx in (x-w/2+r,x+w/2-r):
  for zz in (z-h/2+r,z+h/2-r):s=fuse(s,cq.Solid.makeCylinder(r,y1-y0,cq.Vector(xx,y0,zz),cq.Vector(0,1,0)))
 return s

def load_pcbs():
 old=populated_step_parts(load_named_step(OLDPCB));rows=[]
 if FINALPCB.exists():
  for p in populated_step_parts(load_named_step(FINALPCB)):
   q=dict(p);q['ref']=ref_of(p);q['shape']=p['shape'].translate((30.5,122.58491,24.0529));rows.append(q)
  substrate=next(p['shape'] for p in rows if p['ref'] is None)
  bb=bounds(substrate)
  expected=((34.01509,1.3,24.0529),(80.4,101.1,25.5471))
  if any(abs(a-b)>.001 for key,ex in zip(('min','max'),expected) for a,b in zip(bb[key],ex)):
   raise RuntimeError('Final PCB export registration does not match frozen geometry: '+str(bb))
  connector=next(p['shape'] for p in rows if p['ref']=='J901');jb=bounds(connector)
  jexpected=((38.03,1.3,24.6821),(46.97,9.08,28.9421))
  if any(abs(a-b)>.001 for key,ex in zip(('min','max'),jexpected) for a,b in zip(jb[key],ex)):
   raise RuntimeError('USB connector body registration is wrong: '+str(jb))
  notch_probe=box(51.6,56.1,2.2,6.2,24.1,25.5)
  filled=sum(abs(s.Volume()) for s in substrate.intersect(notch_probe).Solids())
  return rows,old,{'mode':'ACTUAL BOTTOM PCB STEP; mechanical placements frozen, routing release separate','source':str(FINALPCB.relative_to(ROOT)),'source_sha256':sha(FINALPCB),'translation_mm':[30.5,122.58491,24.0529],'substrate_bounds':bb,'connector_bounds':jb,'filled_notch_probe':{'expected_mm3':notch_probe.Volume(),'filled_mm3':filled,'pass':abs(filled-notch_probe.Volume())<1e-5}}
 # Until final export is agreed with the PCB owner, retain an explicit study.
 for p in old:
  ref=ref_of(p);q=dict(p);w=p['shape'].translate((50.4,120,20.5529))
  if ref in ('J901','U901','D901'):
   w=w.translate((-95.5,-29.48491,0)).rotate((0,0,0),(0,0,1),-90).translate((42.5,1.3,3.5))
  elif ref is None:
   w=w.intersect(box(50.3,80.4,20,121,20,23)).translate(CORE_SHIFT)
   w=fuse(w,box(34.01509,51.5,1.3,18.2,24.0529,25.5471))
  else:w=w.translate(CORE_SHIFT)
  q['shape']=w;q['ref']=ref;rows.append(q)
 return rows,old,{'mode':'EXPLICIT STUDY TRANSFORMS; FINAL PCB STEP NOT YET REGISTERED','source':str(OLDPCB.relative_to(ROOT)),'source_sha256':sha(OLDPCB)}

def main():
 for d in ('parts','verification'): (OUT/d).mkdir(exist_ok=True)
 source_paths=[p for p in (SRC,SRC.with_suffix('.f3d'),OLDPCB,FINALPCB) if p.exists()]
 sources_before={str(p.relative_to(ROOT)):sha(p) for p in source_paths}
 src=load_named_step(SRC);pcs,oldpcs,pcb_receipt=load_pcbs()
 originals={p['part_id']:p['shape'] for p in src if p['part_id'].startswith('TMX-A3-P')}
 h,c,tray=(originals[k] for k in ('TMX-A3-P01','TMX-A3-P02','TMX-A3-P03'))
 print('Loaded read-only originals and populated PCB models',flush=True)
 # Add material inside the unchanged85x180 bounding rectangle. Only the lower
 # right outer corner changes fromR8 toR4, keeping the original rear taper.
 # This supports the new internal withdrawal channel without a thin R8 corner.
 corner=cq.Solid.makeLoft([lower_right_wire(85,0,8,0),lower_right_wire(84.75,.25,8.25,40.6)],ruled=True)
 corner=cut(corner,rounded_xy(2.4,82.6,2.4,177.6,14.5,40.8,5.6))
 h=fuse(h,corner)
 cover_corner=cq.Solid.makeLoft([lower_right_wire(84.75,.25,8.25,40.8),lower_right_wire(84.75,.25,8.25,43)],ruled=True)
 roll=[]
 for e in cover_corner.Edges():
  b=e.BoundingBox()
  if abs(b.zmin-43)<1e-6 and abs(b.zmax-43)<1e-6 and (e.geomType()=='CIRCLE' or abs(b.ymax-.25)<1e-6 or abs(b.xmin-84.75)<1e-6):roll.append(e)
 cover_corner=cover_corner.fillet(.8,roll)
 c=fuse(c,cover_corner)
 # Remove retired daughterboard floor and original PCB webs only; keep battery
 # cradle/divider and display-retainer support bodies in their source positions.
 h=cut(h,box(29.35,55.65,2.4,22.05,16.95,19.05),box(72.3,82.6,24.3,31.7,14.29,18.51),box(50.2,58.5,110.3,117.7,14.29,18.51))
 # Retire the fixed lower-right cover boss, which would trap upward PCB
 # removal. Its M3 insert moves into a lid-owned leg, with the same screw
 # turned horizontally through a flush right-wall counterbore. The fixed rib
 # remains beyond boardX80.4 and clears the lid leg by0.3mm.
 h=cut(h,box(72.7,81.3,2.4,12.3,29.9,39.1))
 h=fuse(h,box(80.7,82.7,12.8,21.2,32.8,40.6))
 h=cut(h,cyl_x(80.6,85.1,17,37,1.65),cyl_x(83.1,85.1,17,37,3))
 # Fill the retired top screw hole; the other three original seats stay exact.
 c=cut(c,box(72,82,3,13,39.1,40.8),box(37.3,47.7,19.9,24.5,31.1,40.8))
 c=fuse(c,cyl(77,8,40.8,43,3.1),box(72.6,80.4,12.8,21.2,32.8,40.8))
 c=cut(c,cyl_x(75.4,80.5,17,37,2.15),cyl_x(74.8,75.5,17,37,1.6))
 # New main-board seats atZ22; 4mm carrier ends atZ24. Both posts are rooted
 # to original side structure, and the lower post clears the retained P04 clip.
 h=fuse(h,box(72.7,82.7,5.5,12.7,14.3,22),box(49.8,58.4,91.5,98.7,14.3,22))
 for x,y in ((76,9.1),(54.8,95.1)):h=cut(h,cyl(x,y,18,22.1,1.6))
 # USB support floor joins front wall and original battery-cradle crossmember.
 h=fuse(h,box(25.3,50.2,2.4,26.5,16.7,20.7))
 for x,y in BOSSES:h=fuse(h,cyl(x,y,19,30.3,3.8))
 for x0,x1,y0,y1 in PATCHES:h=fuse(h,box(x0,x1,y0,y1,20.2,24))
 for x,y in BOSSES:h=cut(h,cyl(x,y,26.3,30.4,1.6))
 # Upper edge takes insertion force after0.1mm travel. It contacts FR4 edge,
 # not component roofs, and is anchored into the unchanged battery divider.
 h=fuse(h,box(49.8,57.4,101.2,103.2,20,26.3))
 # Clear a rear-open vertical channel for the main PCB front edge. It is a
 # shallow internal pocket, not a through-opening across the external face.
 h=cut(h,box(50.0,81.1,1.7,4.6,19.7,40.7))
 # Rebuild old port area before cutting the new full-height removable insert.
 h=fuse(h,cut(box(32.1,52.9,0,2.5,20.7,40.6),outside_front(32,53)))
 h=cut(h,box(32.2,52.8,-.1,2.6,22,40.7))
 h=cut(h,box(50.0,81.1,1.7,4.6,19.7,40.7))
 # Restore the full bare-FR4 lower bearing after the channel cut. It ends at
 # PCBbackZ24 and therefore cannot obstruct upward withdrawal.
 for x0,x1,y0,y1 in PATCHES:h=fuse(h,box(x0,x1,y0,y1,20.2,24))
 # Rear-accessible USB retainer. Four-mm frame; recessed seats retain3mm M2x5
 # engagement in3mm inserts. Two screws lie wholly outside the proposed PCB.
 bezel=fuse(box(32.4,52.6,0,2.4,22.2,40.6),box(29.3,51.9,2.4,10,30.3,34.3),box(28.5,33.5,5.8,22.5,30.3,34.3),box(28.5,47,18.2,26.3,30.3,34.3))
 for x,y in BOSSES:bezel=fuse(bezel,cyl(x,y,30.3,34.3,3.8))
 # Rear relief is open through the lower edge so the panel can lift in+Z
 # without sweeping a lower skirt through the PCB's projecting front edge.
 bezel=cut(bezel,outside_front(25,54),box(33.71509,52.7,1.1,2.6,22.1,30.3),rounded_slot_y(42.5,27.33,10.8,5.2,.4,-.1,1.4),box(25,32.4,-.1,2.6,30.2,34.4))
 for x0,x1,y0,y1 in PATCHES:bezel=fuse(bezel,box(x0,x1,y0,y1,25.86,30.3))
 for x,y in BOSSES:bezel=cut(bezel,cyl(x,y,30.2,34.4,1.15),cyl(x,y,32.3,34.4,2.4))
 print('Built first bottom port housing, new mounts and removable retainer',flush=True)
 # Carrier follows original outline but keeps0.5mm setback at the front and
 #0.2mm at rear for the stop. Component/tail relief uses unchanged source parts.
 tray=fuse(tray.translate(CORE_SHIFT),tray.translate((0,-18.9,1.5)))
 tray=cut(tray,box(50.1,50.4,1,103,19.9,24.1),box(50.1,81,1,2.6,19.9,24.1),box(50.1,81,100.9,102,19.9,24.1))
 keepouts=build_keepouts()+build_tail_keepouts(normalized_step_parts=oldpcs)
 for k in keepouts:
  if not k.get('underside'):continue
  shape=k['shape'].translate(CORE_SHIFT);b=bounds(shape);lo,hi=b['min'],b['max']
  x0,x1=lo[0],hi[0]
  if 0<x0-50.4<2:x0=50.3
  if 0<80.6-x1<2:x1=80.7
  tray=cut(tray,box(x0,x1,lo[1],hi[1],19.9,24.1))
  h=cut(h,shape)
 # Local seats receive the original carrier bearing load atZ22. Explicit
 # pockets avoid Boolean ambiguity around the insert bore and contact faces.
 tray=cut(tray,box(72.6,82.8,5.4,12.8,19.9,22),box(49.7,58.5,91.4,98.8,19.9,22),box(65,73,5.2,16.5,19.9,24.1))
 tray=cut(tray,box(50.3,50.7,2.9,8.9,19.9,24.1))
 for x0,x1,y0,y1 in ((55,76,61.1,80.1),(55,76,32.1,50.1),(55,76,20.1,28.1),(53.5,71.5,5.1,18.6)):
  tray=cut(tray,box(x0,x1,y0,y1,19.9,24.1))
 solids=sorted(tray.Solids(),key=lambda s:s.Volume(),reverse=True)
 scrap=[{'volume_mm3':s.Volume(),'bounds':bounds(s)} for s in solids[1:]]
 if len(solids)>1:print('Carrier disconnected pieces',scrap,flush=True)
 # Never silently discard a structural fragment: keep the compound for review.
 parts={'TMX-A3-P01':h,'TMX-A3-P02':c,'TMX-A3-P03':tray,'TMX-V2-P12':bezel}
 retired={'TMX-A3-P06','TMX-A3-P09','TMX-A3-P10','TMX-A3-B01','TMX-A3-B02',*(f'TMX-A3-C05-V0{i}' for i in range(1,5))}
 retained=[];usb_screws=[];main_screws=[]
 for p in src:
  pid=p['part_id'];ctr=p['bounds']['center'];q=dict(p)
  if pid in retired:continue
  if pid in ('TMX-A3-F02','TMX-A3-F05') and abs(ctr[1]-18.25)<.01:
   if pid=='TMX-A3-F02':usb_screws.append(p)
   continue
  if pid=='TMX-A3-F03':q['shape']=p['shape'].translate(CORE_SHIFT);main_screws.append(q)
  if pid=='TMX-A3-F05' and any(abs(ctr[0]-x)<.01 and abs(ctr[1]-y)<.01 for x,y in ((76,28),(54.8,114))):continue
  if pid in ('TMX-A3-F01','TMX-A3-F04') and abs(ctr[0]-77)<.01 and abs(ctr[1]-8)<.01:
   oldz,newx=(41.35,83.1) if pid=='TMX-A3-F01' else (39,80.4)
   q['shape']=p['shape'].translate((-77,-8,-oldz)).rotate((0,0,0),(0,1,0),90).translate((newx,17,37))
   q['part_id']='TMX-V2-F01-SIDE' if pid=='TMX-A3-F01' else 'TMX-V2-F04-SIDE'
  if pid in parts:q['shape']=parts[pid]
  retained.append(q)
 retained.append({'part_id':'TMX-V2-P12','name':'Removable supported bottom USB retainer','shape':bezel})
 assert len(usb_screws)==2
 for i,(p,(x,y)) in enumerate(zip(usb_screws,BOSSES),1):
  sx,sy=p['bounds']['center'][:2]
  retained.append({'part_id':f'TMX-V2-F02-{i}','name':'USB reused M2x5','shape':p['shape'].translate((x-sx,y-sy,-.9))})
  retained.append({'part_id':f'TMX-V2-F05-{i}','name':'TC-M2x3 insert envelope','shape':cut(cyl(x,y,27.3,30.3,1.8),cyl(x,y,27.2,30.4,1))})
 for i,(x,y) in enumerate(((76,9.1),(54.8,95.1)),3):
  retained.append({'part_id':f'TMX-V2-F05-{i}','name':'Main PCB TC-M2x3 insert envelope','shape':cut(cyl(x,y,19,22,1.8),cyl(x,y,18.9,22.1,1))})
 for p in pcs:retained.append({'part_id':'PCB2-'+(p['ref'] or 'substrate'),'name':p['name'],'shape':p['shape']})
 sources=sources_before
 report={'status':'FIT PROTOTYPE; independent component/load-path/removal checks pending','pcb':pcb_receipt,'sources':sources,'core_shift_mm':CORE_SHIFT,'main_pcb_nominal_planes_mm':[24,25.6], 'usb_mouth_mm':[42.5,1.3,27.33], 'carrier_scrap':scrap,'changed_print_parts':{},'assembly_file':'Trimix_Enclosure_PCB2_Bottom.step','usb_boss_axes_mm':BOSSES,'usb_insert_bottom_top_mm':[27.3,30.3],'usb_screw_underhead_tip_mm':[32.3,27.3],'usb_bearing_patches_xy_mm':PATCHES,'usb_top_capture_bottom_mm':25.86,'main_mount_axes_mm':[(76,9.1),(54.8,95.1)],'main_insert_bottom_top_mm':[19,22],'main_screw_underhead_tip_mm':[25.6,18.6],'moved_cover_screw_axis_mm':{'direction':'+X head / -X insertion','yz':[17,37],'underhead_x':83.1,'tip_x':75.1,'head_outer_x':84.75,'insert_x_extent':[75.4,80.4],'lid_leg_box':[72.6,80.4,12.8,21.2,32.8,40.8],'fixed_bearing_rib_box':[80.7,82.7,12.8,21.2,32.8,40.6],'leg_rib_gap_mm':.3},'local_outer_corner_change':{'corner':'lower-right','old_radius_mm':8,'new_radius_mm':4,'stock_extent_xy_mm':[77.6,85,0,8.25],'original_rear_taper_preserved':True,'matching_lid_corner':True},'main_withdrawal_channel_box_mm':[50,81.1,1.7,4.6,19.7,40.7], 'front_wall_skin':{'flat_min_mm':1.45,'corner_horizontal_min_mm':4-(.35**2+2.55**2)**.5,'conservative_surface_normal_lower_bound_mm':(4-(.35**2+2.55**2)**.5)/(1+2*(.25/40.6)**2)**.5,'derivation':'At rearZ40.6, R4 corner centre(80.75,4.25), nearest channel corner(81.1,1.7); thickness4-sqrt(0.35^2+2.55^2). Dividing by sqrt(1+2*(0.25/40.6)^2) conservatively accounts for loft slope. This is channel skin, not a blanket minimum over all designed holes or cosmetic rims.'},'usb_load_path':{'bearing_area_each_mm2':11.6,'floor_thickness_mm':4,'retainer_frame_thickness_mm':4,'screw_seat_thickness_mm':2,'upper_capture_nominal_gap_mm':.26,'upper_capture_max_board_gap_mm':.10,'insertion_stop_travel_mm':.1,'extraction_stop_travel_mm':.2,'description':'Insertion+Y: connector board to bare FR4 rear-edge stop, into original divider. Extraction-Y: bare front FR4 margins to removable panel rear face, into retainer frame and two M2 screws/inserts/floor. BendingZ: bare USB-wing margins captured between full lower ledges and upper feet; no component or pin contact. Tolerances, printing anisotropy, insert retention and real cable loads require prototype checks.'},'assembly_removal_steps':['remove three top cover screws plus one horizontal right-side M3x8, then lift cover+Z (its M3 insert leg leaves with it)','remove two USB M2x5 and lift USB retainer+Z','disconnect harnesses, remove two PCB M2x7, then lift PCB+carrier+Z'], 'issues':[]}
 assembly=cq.Assembly(name='Trimix_Enclosure_PCB2_Bottom_FitPrototype')
 for i,p in enumerate(retained):
  color=(.17,.22,.28) if p['part_id']=='TMX-A3-P01' else (.58,.63,.68) if p['part_id']=='TMX-A3-P02' else (.94,.44,.12) if p['part_id']=='TMX-V2-P12' else (.16,.46,.26) if p['part_id'].startswith('PCB2-') else (.6,.62,.64)
  assembly.add(p['shape'],name=f"{i:03d}_{p['part_id']}",color=cq.Color(*color))
 for pid,s in parts.items():
  print(pid,len(s.Solids()),s.isValid(),s.Volume(),flush=True)
  report['changed_print_parts'][pid]={'valid':s.isValid(),'solids':len(s.Solids()),'volume_mm3':s.Volume(),'bounds':bounds(s)}
  cq.exporters.export(s,str(OUT/'parts'/f'{pid}.step'));cq.exporters.export(s,str(OUT/'parts'/f'{pid}.stl'),tolerance=.05,angularTolerance=.1)
  if not s.isValid() or len(s.Solids())!=1:report['issues'].append(pid+' is not one valid solid')
 for pid in ('TMX-A3-P04','TMX-A3-P05','TMX-A3-P07','TMX-A3-P08','TMX-A3-P11'):cq.exporters.export(originals[pid],str(OUT/'parts'/f'{pid}.stl'),tolerance=.05,angularTolerance=.1)
 assembly.save(str(OUT/report['assembly_file']))
 report['assembly_instances']=[{'part_id':p['part_id'],'name':p['name'],'bounds':bounds(p['shape']),'solid_count':len(p['shape'].Solids()),'volume_mm3':p['shape'].Volume()} for p in retained]
 report['source_hashes_unchanged']=all(sha(ROOT/p)==v for p,v in sources.items())
 if not report['source_hashes_unchanged']:report['issues'].append('An input changed during build; rebuild against frozen input before review')
 (OUT/'verification/build-report.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Export complete',flush=True)
if __name__=='__main__':main()
