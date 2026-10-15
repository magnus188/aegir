from pathlib import Path
import sys,json,hashlib,collections
import cadquery as cq
ROOT=Path.cwd();sys.path.insert(0,str(ROOT/'hardware/cad/pcb-v2/scripts'))
from step_inputs import load_named_step,bounds
from component_clearance import build_keepouts,build_tail_keepouts,PCB,PCBSTEP,CONTRACT,ORIGINAL

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def box(x0,x1,y0,y1,z0,z1):return cq.Solid.makeBox(x1-x0,y1-y0,z1-z0,cq.Vector(x0,y0,z0))
def intersects_bounds(a,b):return all(a['min'][i]<=b['max'][i]+1e-6 and b['min'][i]<=a['max'][i]+1e-6 for i in range(3))
inputs={str(p.relative_to(ROOT)):sha(p)for p in (PCB,PCBSTEP,CONTRACT,ORIGINAL)}
assert sha(PCB)=='1df55664b1f57ec4dcc38bbd2358f5aee306f59696f52271aafb94f233b992bc'
src=load_named_step(ORIGINAL)
retired={'TMX-A3-P06','TMX-A3-P09','TMX-A3-P10','TMX-A3-B01','TMX-A3-B02','TMX-A3-P03',*(f'TMX-A3-C05-V0{i}'for i in range(1,5))}
fixed=[];excluded=[]
for p in src:
 pid=p['part_id'];ctr=p['bounds']['center']
 if pid in retired or pid in ('TMX-A3-P01','TMX-A3-F03') or (pid in ('TMX-A3-F02','TMX-A3-F05')and abs(ctr[1]-18.25)<.01) or (pid=='TMX-A3-F05' and any(abs(ctr[0]-x)<.01 and abs(ctr[1]-y)<.01 for x,y in ((76,28),(54.8,114)))):
  excluded.append({'part_id':pid,'path':p['path']});continue
 q=dict(p)
 if pid=='TMX-A3-P02':q['shape']=p['shape'].cut(box(37.3,47.7,19.9,24.5,31.1,40.8));q['bounds']=bounds(q['shape'])
 fixed.append(q)
ks=build_keepouts();tails=build_tail_keepouts()
assert len(ks)==24 and sum(k['underside']for k in ks)==19 and sum(k['missing_from_step']for k in ks)==6 and len(tails)==52
shift=(0,-18.9,3.5);moving=[]
for k in ks+tails:
 q=dict(k);q['shape']=k['shape'].translate(shift);q['bounds']=bounds(q['shape']);moving.append(q)
r={'schema':1,'status':'bounded placement screen only; no source PCB/CAD edits',
 'core_shift_world_mm':shift,'nominal_board_back_top_world_z_mm':[24.0,25.6],
 'usb_mouth_world_mm':[42.5,1.3,27.33],
 'source_sha256':inputs,'underside_component_count':19,'missing_model_count':6,'unique_component_envelope_count':24,'individual_tail_envelope_count':52,
 'underside_and_missing_envelope_reserve_mm':{'xy':.5,'outward_z':.5},'tail_envelope_reserve_mm':{'xy':.35,'outward_z':.4},
 'fixed_part_count':len(fixed),'candidate_pair_count':len(moving)*len(fixed),'broad_phase_pair_count':0,
 'collisions':[],'zero_volume_contacts':[],'invalid_booleans':[],'display_retainer_screw_clearance':[],
 'excluded_parts':excluded,'fixed_parts':[{'part_id':p['part_id'],'path':p['path'],'bounds':p['bounds']}for p in fixed],
 'envelopes':[{key:val for key,val in q.items()if key not in ('shape','actual_step_models')}for q in moving],
 'limits':['Housing, carrier, moved PCB fasteners and their inserts require redesign and are deliberately excluded.',
 'The original cover is included after removing only the already-retired USB key box X37.3..47.7/Y19.9..24.5/Z31.1..40.8, matching the selected-candidate template.',
 'This checks 19 underside maximum+assembly bodies and six absent-model bodies (U115 is in both), plus52 actual-model tail envelopes. Other top-side parts are covered by the separate actual STEP screen, not this envelope report.',
 'No assembly path, screwdriver path, strength, cable/hose or print tolerance qualification. USB placement is metadata only here: all screened envelopes belong to the shifted main core.']}
for a in moving:
 for b in fixed:
  if not intersects_bounds(a['bounds'],b['bounds']):continue
  r['broad_phase_pair_count']+=1
  common=a['shape'].intersect(b['shape']);v=sum(abs(s.Volume())for s in common.Solids())
  row={'reference':a['reference'],'tail_index':a.get('tail_index'),'kind':a.get('kind','component_maximum_and_assembly_envelope'),'fixed_part':b['part_id'],'fixed_path':b['path'],'intersection_mm3':v,'valid_common':common.isValid(),'distance_mm':a['shape'].distance(b['shape'])}
  if not common.isValid():r['invalid_booleans'].append(row)
  elif v>1e-6:r['collisions'].append(row)
  elif row['distance_mm']<1e-6:r['zero_volume_contacts'].append(row)
# Required selected display screw, not merely the printed bracket.
selected=[p for p in fixed if p['part_id']=='TMX-A3-P04' or (p['part_id']=='TMX-A3-F02'and abs(p['bounds']['center'][0]-69)<.01 and abs(p['bounds']['center'][1]-12.3)<.01)]
for f in selected:
 nearest=sorted([{'reference':p['reference'],'tail_index':p.get('tail_index'),'distance_mm':p['shape'].distance(f['shape']),'bounds':p['bounds']}for p in moving],key=lambda p:p['distance_mm'])[:8]
 r['display_retainer_screw_clearance'].append({'part_id':f['part_id'],'path':f['path'],'bounds':f['bounds'],'nearest_envelopes':nearest})
r['input_hashes_unchanged']=all(sha(ROOT/p)==v for p,v in inputs.items())
r['pass']=not r['collisions'] and not r['invalid_booleans'] and r['input_hashes_unchanged']
r['status']='pass_for_selected_static_component_reserves'if r['pass']else'fail'
out=ROOT/'hardware/cad/pcb-v2-bottom-study/verification/component-reserve-screen.json';out.write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({k:r[k]for k in ('status','pass','fixed_part_count','candidate_pair_count','broad_phase_pair_count','collisions','invalid_booleans','zero_volume_contacts','display_retainer_screw_clearance')},indent=2),flush=True)
