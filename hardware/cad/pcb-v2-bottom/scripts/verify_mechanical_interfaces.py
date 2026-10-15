"""Independent bottom-port mechanical checks against exported assembly BReps.

No case-builder imports. Fastener hardware is the actual delivered assembly.
Sweeps use the predecessor's independently implemented continuous-distance test.
"""
from pathlib import Path
import argparse,hashlib,json,re,sys,itertools
import cadquery as cq
ROOT=next(p for p in Path(__file__).resolve().parents if (p/'hardware/pcb/analyzer-v2').exists())
sys.path.insert(0,str(ROOT/'hardware/cad/pcb-v2/scripts'))
from step_inputs import load_named_step,bounds
import independent_geometry_verify as sweep_impl
from independent_geometry_verify import box,bbox_gap,metrics,sweep_certificate
OUT=ROOT/'hardware/cad/pcb-v2-bottom'
EPS=1e-6
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def pid(p):
 m=re.search(r'(TMX-(?:A3|V2)-[A-Z]\d+(?:-V\d+|-[1-4]|-SIDE)?|PCB2-[A-Za-z0-9]+)$',p['definition_name'])
 if not m:raise ValueError(p['definition_name'])
 return m[1]
def compound(ss):return cq.Compound.makeCompound(ss)
def volume(s):return sum(abs(x.Volume())for x in s.Solids())
def strict_common_volume(a,b):
 s=a.intersect(b)
 if not s.isValid():raise RuntimeError('Invalid Boolean in continuous path audit; no clearance claim')
 return volume(s)
sweep_impl.common_volume=strict_common_volume
def safe_metrics(a,b):
 r=metrics(a,b)
 r['pass']=r['volume_pass'] and r.get('common_valid',True) and not r.get('boolean_orientation_warning',False)
 return r
def main():
 ap=argparse.ArgumentParser();ap.add_argument('assembly');ap.add_argument('--quick',action='store_true');ap.add_argument('--out',default=str(OUT/'verification/mechanical-interfaces-independent.json'));a=ap.parse_args()
 path=Path(a.assembly).resolve();before=sha(path);parts=load_named_step(path)
 rows=[{'id':pid(p),'shape':p['shape'],'path':p['path']}for p in parts]
 # Candidate03 retained source IDs; final export uses explicit side IDs.
 for r in rows:
  b=bounds(r['shape'])
  if r['id']=='TMX-A3-F01'and abs(b['size'][0]-9.65)<.001:r['id']='TMX-V2-F01-SIDE'
  if r['id']=='TMX-A3-F04'and abs(b['size'][0]-5)<.001:r['id']='TMX-V2-F04-SIDE'
 by={}
 for p in rows:by.setdefault(p['id'],[]).append(p['shape'])
 def one(i):
  if len(by[i])!=1:raise ValueError(i)
  return by[i][0]
 h,c,tray,port=[one(i)for i in ('TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-V2-P12')]
 pcb=[p['shape']for p in rows if p['id'].startswith('PCB2-')]
 substrate=one('PCB2-substrate')
 checks=[]
 for ia,ib in itertools.combinations(('TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-V2-P12'),2):
  checks.append({'pair':[ia,ib],**safe_metrics(one(ia),one(ib))})
 bearing=[]
 for x0,x1 in ((34.81509,36.81509),(48.5,50.5)):
  lower=box([x0,x1,3,8.8,23.99,24]);upper=box([x0,x1,3,8.8,25.86,25.87])
  lo=h.intersect(lower);hi=port.intersect(upper)
  r={'patch_xy_mm':[x0,x1,3,8.8],'lower_missing_mm3':volume(lower)-volume(lo),'upper_missing_mm3':volume(upper)-volume(hi),'valid':lo.isValid()and hi.isValid()}
  r['pass']=r['valid']and abs(r['lower_missing_mm3'])<EPS and abs(r['upper_missing_mm3'])<EPS;bearing.append(r)
 # Re-extrude the actual final substrate outline, including its actual holes,
 # to each thickness while retaining the nominal lower support datum Z24.
 faces=[f for f in substrate.Faces()if f.geomType()=='PLANE' and abs(f.normalAt().z)>.999 and abs(f.Center().z-bounds(substrate)['min'][2])<.001]
 bottom=max(faces,key=lambda f:f.Area()).translate((0,0,24-bounds(substrate)['min'][2]))
 thickness=[]
 for t in (1.44,1.6,1.76):
  slab=cq.Solid.extrudeLinear(bottom.outerWire(),bottom.innerWires(),cq.Vector(0,0,t))
  thickness.append({'thickness_mm':t,'housing':safe_metrics(slab,h),'carrier':safe_metrics(slab,tray),'retainer':safe_metrics(slab,port),'capture_clearance_mm':25.86-(24+t)})
 # Heads and driver access are separate: assembled shaft/insert engagement is
 # intentional and must not hide a head clash or tool obstruction.
 fasteners=[]
 configurations=[('TMX-V2-F02-1',(30.01509,5.8),32.3,5),('TMX-V2-F02-2',(42.5,22.5),32.3,5)]
 main=by['TMX-A3-F03']
 for i,s in enumerate(main):
  bb=bounds(s);configurations.append((f'main-{i}',bb['center'][:2],25.6,7));by[f'main-{i}']=[s]
 for ident,xy,seat,length in configurations:
  s=one(ident);x,y=xy;head=s.intersect(box([x-3,x+3,y-3,y+3,seat,seat+2.01]))
  driver=cq.Solid.makeCylinder(2.2,60-seat,cq.Vector(x,y,seat))
  # Cover is removed for access; PCB screws are reached after retainer removal.
  excluded={'TMX-A3-P02','TMX-A3-F01','TMX-V2-F01-SIDE','TMX-V2-F04-SIDE',ident}
  if ident.startswith('main-'):excluded.add('TMX-V2-P12')
  fixed=[]
  for p in rows:
   if p['id']in excluded or p['id'].startswith('TMX-V2-F05')or p['id']=='TMX-A3-F03' or p['id'].startswith('TMX-V2-F02'):continue
   if bbox_gap(driver,p['shape'])<=EPS:fixed.append(p)
  hits=[{'id':p['id'],**safe_metrics(driver,p['shape'])}for p in fixed]
  head_checks=[{'id':p['id'],**safe_metrics(head,p['shape'])}for p in rows if p['id'].startswith('PCB2-')and bbox_gap(head,p['shape'])<=EPS]
  b=bounds(s)
  fasteners.append({'id':ident,'axis_xy_mm':xy,'under_head_z_mm':seat,'tip_z_mm':b['min'][2],'nominal_length_mm':length,'length_pass':abs(b['min'][2]-(seat-length))<.001,'driver_diameter_mm':4.4,'driver_checks':hits,'head_to_pcb':head_checks})
 side_screw=one('TMX-V2-F01-SIDE');side_insert=one('TMX-V2-F04-SIDE')
 sb,ib=bounds(side_screw),bounds(side_insert)
 side_driver=cq.Solid.makeCylinder(2.2,16.9,cq.Vector(83.1,17,37),cq.Vector(1,0,0))
 side_checks={'screw_housing':safe_metrics(side_screw,h),'screw_lid':safe_metrics(side_screw,c),'driver_housing':safe_metrics(side_driver,h),'driver_lid':safe_metrics(side_driver,c)}
 side={'screw_bounds_mm':sb,'insert_bounds_mm':ib,'under_head_x_mm':83.1,'head_outer_x_mm':84.75,'insert_x_mm':[75.4,80.4],'thread_engagement_mm':min(83.1,ib['max'][0])-max(sb['min'][0],ib['min'][0]),'fixed_bearing_depth_mm':83.1-80.7,'driver_diameter_mm':4.4,'checks':side_checks}
 side['pass']=all(r['pass']for r in side_checks.values()) and abs(side['thread_engagement_mm']-5)<.001 and abs(sb['min'][0]-75.1)<.001 and abs(sb['max'][0]-84.75)<.001 and all(abs(sb['center'][i]-v)<.001 for i,v in ((1,17),(2,37)))
 battery=[]
 for p in rows:
  if p['id'] in ('TMX-A3-C02','TMX-A3-C03-V01','TMX-A3-C03-V02','TMX-A3-C10-V01','TMX-A3-C10-V02'):
   battery.append({'id':p['id'],'housing':safe_metrics(h,p['shape']),'retainer':safe_metrics(port,p['shape']),'carrier':safe_metrics(tray,p['shape']),'pcb':safe_metrics(compound(pcb),p['shape'])})
 report={'assembly_sha256':before,'scope':'Actual exported printed interfaces, nominal substrate thickness limits, fastener heads/tools, retained battery, and continuous removal paths. No case-builder imports.','static_interfaces':checks,'bearing_patches':bearing,'board_thickness':thickness,'fasteners':fasteners,'side_cover_attachment':side,'battery':battery,'sweeps':[]}
 if not a.quick:
  # Each operation explicitly removes only its prerequisite fasteners/parts.
  # Crop obstacles to a swept bounding box to reduce exact BRep work.
  def sweep(label,moving,excluded,dz):
   bb=bounds(moving);region=box([bb['min'][0]-.01,bb['max'][0]+.01,bb['min'][1]-.01,bb['max'][1]+.01,bb['min'][2]-.01,bb['max'][2]+dz+.01])
   candidates=[p for p in rows if p['id']not in excluded and bbox_gap(p['shape'],region)<=EPS]
   ss=[p['shape'].intersect(region)for p in candidates];ss=[s for s in ss if s.Solids()]
   r=sweep_certificate(label,moving,compound(ss),(0,0,0),(0,0,dz),initial_steps=100,max_depth=10)
   if r['volume_collisions']:
    moved=moving.translate(r['volume_collisions'][0]['offset_mm'])
    r['first_collision_parts']=[{'id':p['id'],**safe_metrics(moved,p['shape'])}for p in candidates if bbox_gap(moved,p['shape'])<=EPS and volume(moved.intersect(p['shape']))>EPS]
   report['sweeps'].append(r);print(label,r['certified_clear'],flush=True)
  sweep('cover and its side insert +Z after three rear and one side M3 screws removed',compound([c,side_insert]),{'TMX-A3-P02','TMX-A3-F01','TMX-V2-F01-SIDE','TMX-V2-F04-SIDE'},15)
  sweep('USB retainer +Z after cover and two USB screws removed',port,{'TMX-A3-P02','TMX-A3-F01','TMX-V2-F01-SIDE','TMX-V2-F04-SIDE','TMX-V2-P12','TMX-V2-F02-1','TMX-V2-F02-2'},25)
  excluded={'TMX-A3-P02','TMX-A3-F01','TMX-V2-F01-SIDE','TMX-V2-F04-SIDE','TMX-V2-P12','TMX-V2-F02-1','TMX-V2-F02-2','TMX-A3-F03','TMX-A3-P03'}|{p['id']for p in rows if p['id'].startswith('PCB2-')}
  sweep('PCB and carrier +Z; harnesses unplugged and four internal screws removed',compound([tray]+pcb),excluded,27)
  control=sweep_certificate('mid-path interference detector control',box([0,1,0,1,0,1]),box([2,3,0,1,0,1]),(0,0,0),(4,0,0),initial_steps=1,max_depth=4)
  report['mid_path_negative_control_pass']=bool(control['volume_collisions']) and not control['certified_clear']
 report['inputs_unchanged']=sha(path)==before
 report['static_pass']=side['pass']and all(r['pass']for r in checks+bearing)and all(r[k]['pass']for r in thickness for k in ('housing','carrier','retainer'))and all(r[k]['pass']for r in battery for k in ('housing','retainer','carrier','pcb'))and all(r['length_pass']and all(x['pass']for x in r['driver_checks']+r['head_to_pcb'])for r in fasteners)
 report['pass']=report['static_pass']and report['inputs_unchanged']and not a.quick and all(s['certified_clear']for s in report['sweeps'])and report.get('mid_path_negative_control_pass',False)
 Path(a.out).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'pass':report['pass'],'static_pass':report['static_pass'],'report':a.out}),flush=True)
if __name__=='__main__':main()
