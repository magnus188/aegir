"""Read-only exact component placement study; creates no routed PCB or case geometry.
Excludes explicitly retired/requiring-redesign mounting geometry, retains purchased parts.
"""
from pathlib import Path
import sys,json,re,csv,math,hashlib
import cadquery as cq
ROOT=next(p for p in Path(__file__).resolve().parents if (p/'hardware/pcb/analyzer-v2').is_dir());sys.path.insert(0,str(ROOT/'hardware/cad/pcb-v2/scripts'))
source_paths=[ROOT/'hardware/cad/rev04/original-fit-check/Trimix_Enclosure_A3_Original.step',ROOT/'hardware/pcb/analyzer-v2/review/Trimix_PCB_V2_Mechanical.step',ROOT/'hardware/pcb/analyzer-v2/Trimix_Analyzer.kicad_pcb',ROOT/'hardware/pcb/analyzer-v2/manufacturing/placement-populated.csv']
source_hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths}
from step_inputs import load_named_step,bounds
from component_clearance import populated_step_parts,ref_of

def box(x0,x1,y0,y1,z0,z1):return cq.Solid.makeBox(x1-x0,y1-y0,z1-z0,cq.Vector(x0,y0,z0))
def vol(s):return sum(abs(x.Volume()) for x in s.Solids())
def bbox_overlap(a,b):
 aa,bb=bounds(a),bounds(b)
 return all(aa['min'][i]<=bb['max'][i]+1e-6 and bb['min'][i]<=aa['max'][i]+1e-6 for i in range(3))
def result(a,b):
 common=a.intersect(b);v=vol(common)
 return {'intersection_mm3':v,'valid_common':common.isValid(),'distance_mm':a.distance(b),'signed_mm3':[x.Volume() for x in common.Solids()]}
src=load_named_step(ROOT/'hardware/cad/rev04/original-fit-check/Trimix_Enclosure_A3_Original.step')
pcs=populated_step_parts(load_named_step(ROOT/'hardware/pcb/analyzer-v2/review/Trimix_PCB_V2_Mechanical.step'))
pcs=[{**p,'ref':ref_of(p),'shape':p['shape'].translate((50.4,120,20.5529))} for p in pcs]
core=next(p['shape'] for p in pcs if p['ref'] is None).intersect(box(50.3,80.4,20,121,20,23))
port=[p for p in pcs if p['ref'] in ('J901','D901','U901')]
main=[p for p in pcs if p['ref'] not in (None,'J901','D901','U901')]
# Remove already-retired daughterboard and its parts; retain original display,
# battery, gas, power button and all original housing/cover hardware references.
retired={'TMX-A3-P06','TMX-A3-P09','TMX-A3-P10','TMX-A3-B01','TMX-A3-B02','TMX-A3-P03',*(f'TMX-A3-C05-V0{i}' for i in range(1,5))}
fixed=[]
for p in src:
 if p['part_id'] in retired:continue
 if p['part_id'] in ('TMX-A3-F02','TMX-A3-F05') and abs(p['bounds']['center'][1]-18.25)<.01:continue
 q=dict(p)
 if p['part_id']=='TMX-A3-P01':q['shape']=p['shape'].cut(box(29.35,55.65,2.4,22.05,16.95,19.05))
 if p['part_id']=='TMX-A3-P02':q['shape']=p['shape'].cut(box(37.3,47.7,19.9,24.5,31.1,40.8))
 fixed.append(q)


# Exact selected candidate. Housing posts/port pocket and carrier are explicitly
# outside this fixed-part check because they require redesign; bought parts stay.
fixed=[p for p in fixed if p['part_id'] not in ('TMX-A3-P01','TMX-A3-F03') and not (p['part_id']=='TMX-A3-F05' and any(abs(p['bounds']['center'][0]-x)<.01 and abs(p['bounds']['center'][1]-y)<.01 for x,y in ((76,28),(54.8,114))))]
xc,ym,dy,dz=42.5,1.3,-18.9,3.5
port_parts=[]
for p in port:
 moved=p['shape'].translate((-95.5,-29.48491,0)).rotate((0,0,0),(0,0,1),-90).translate((xc,ym,dz))
 port_parts.append({'ref':p['ref'],'shape':moved})
core_m=core.translate((0,dy,dz))
tab=box(xc-8.48491,xc+9,ym,17,20.5529+dz,22.0471+dz)
bridge=box(xc-8.48491,51.5,15.5,21,20.5529+dz,22.0471+dz)
substrate=core_m.fuse(tab,bridge).clean()
moving=port_parts+[{'ref':p['ref'],'shape':p['shape'].translate((0,dy,dz))} for p in main]+[{'ref':'candidate_substrate','shape':substrate}]
r={'status':'Exact selected placement only; no CAD/PCB/schematic modification. Housing pocket, board mounts, carrier, USB support and cable-fit design remain open.', 'core_shift':[0,dy,dz],'mouth':[xc,ym,23.83+dz],'substrate_bounds':bounds(substrate),'substrate_valid':substrate.isValid(),'substrate_solids':len(substrate.Solids()),'collisions':[],'near_contacts':[],'moved_component_bounds':{p['ref']:bounds(p['shape']) for p in moving if p['ref']!='candidate_substrate'}}
for a in moving:
 for b in fixed:
  if not bbox_overlap(a['shape'],b['shape']):continue
  rr=result(a['shape'],b['shape'])
  item={'moving':a['ref'],'fixed':b['part_id'],'fixed_name':b['name'],**rr}
  if rr['intersection_mm3']>1e-6:r['collisions'].append(item)
  elif rr['distance_mm']<.500001:r['near_contacts'].append(item)
# Exact minimum gaps to the retained lower display retainer and its M2 screw.
selected_fixed=[p for p in fixed if p['part_id']=='TMX-A3-P04' or (p['part_id']=='TMX-A3-F02' and abs(p['bounds']['center'][0]-69)<.01 and abs(p['bounds']['center'][1]-12.3)<.01)]
r['lower_display_minimum_gaps']=[]
for f in selected_fixed:
 vals=sorted([(p['shape'].distance(f['shape']),p['ref']) for p in moving],key=lambda x:x[0])[:5]
 r['lower_display_minimum_gaps'].append({'fixed':f['part_id'],'five_closest':[{'moving':p,'distance_mm':d} for d,p in vals]})
r['input_sha256']=source_hashes
r['input_hashes_still_match']=all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==sha for p,sha in source_hashes.items())
r['tested_retained_part_count']=len(fixed)
r['tested_populated_model_count']=len(moving)-1
r['excluded_for_required_case_redesign']=['housing and its original fixed PCB posts','carrier','original PCB mounting screws and inserts','retired separate USB board and support parts','old USB cover key below lid removed as in prior adaptation']
r['missing_component_envelopes']='Separate component-envelope review required; this report validates current populated STEP solids only.'
r['pass_fixed_solids']=not r['collisions'] and r['substrate_valid'] and r['substrate_solids']==1
path=ROOT/'hardware/cad/pcb-v2-bottom-study/verification/nominal-placement-review.json';path.write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({k:v for k,v in r.items() if k!='moved_component_bounds'},indent=2),flush=True)
