from pathlib import Path
import sys,json,zipfile,hashlib,copy,xml.etree.ElementTree as E
import numpy as np
import prepare_original_prints as O
ROOT=O.OUT;Q=O.Q
from inspect_seal_paths import objects
N=Q.I.NS;P=Q.I.PROD;OUT=ROOT/'multi-bed';FINAL=OUT/'Trimix_Original_H2D_FitCheck.3mf'
offsets=[(0,0),(420,0),(0,-384),(420,-384)]
def md(el):return {m.get('key'):m.get('value') for m in el.findall('metadata') if m.get('key')}
def verify():
 with zipfile.ZipFile(FINAL) as z:
  assert z.testzip() is None;data={n:z.read(n) for n in z.namelist()};parts=objects(z)
 cfg=E.fromstring(data['Metadata/model_settings.config']);main=E.fromstring(data['3D/3dmodel.model']);sliceinfo=E.fromstring(data['Metadata/slice_info.config']);settings=json.loads(data['Metadata/project_settings.config'])
 assert len(cfg.findall('plate'))==4 and len(cfg.findall('object'))==17
 assert [len(p.findall('model_instance')) for p in cfg.findall('plate')]==[14,1,1,1]
 assert settings['printer_model']=='Bambu Lab H2D' and all(float(n)==.4 for n in settings['nozzle_diameter'])
 assert any('Bambu PLA Basic' in n for n in settings['filament_settings_id'])
 expected={p['id']:p for p in Q.load()['parts'] if p['id']!='D01'};assert set(parts)==set(expected)
 geometry=[]
 for pid,tri in parts.items():
  volume=abs(np.einsum('ij,ij->i',tri[:,0,:],np.cross(tri[:,1,:],tri[:,2,:])).sum()/6);relative=abs(volume-expected[pid]['volume_mm3'])/expected[pid]['volume_mm3'];assert relative<.001,(pid,relative)
  assert abs(tri[:,:,2].min())<.001,(pid,tri[:,:,2].min());assert abs(tri[:,:,2].max()-expected[pid]['size_mm'][2])<.001
  geometry.append({'id':pid,'relative_mesh_volume_error':relative,'floor_z_mm':float(tri[:,:,2].min()),'height_mm':float(tri[:,:,2].max())})
 rows=[]
 for i,(plate,(dx,dy)) in enumerate(zip(cfg.findall('plate'),offsets),1):
  info=md(plate);wanted={int(md(m)['object_id']) for m in plate.findall('model_instance')};own=copy.deepcopy(main);resources=own.find(N+'resources');build=own.find(N+'build')
  for obj in list(resources):
   if int(obj.get('id')) not in wanted:resources.remove(obj)
  for item in list(build):
   if int(item.get('objectid')) not in wanted:build.remove(item);continue
   a=list(map(float,item.get('transform').split()));a[9]-=dx;a[10]-=dy;item.set('transform',' '.join(map(str,a)))
  c=E.Element('config')
  for ob in cfg.findall('object'):
   if int(ob.get('id')) in wanted:c.append(copy.deepcopy(ob))
  c.append(copy.deepcopy(plate));ss=E.Element('config');ss.append(copy.deepcopy(sliceinfo.find('header')));ss.append(copy.deepcopy(sliceinfo.findall('plate')[i-1]))
  subset={n:v for n,v in data.items() if not (n.endswith('.gcode') or n.endswith('.gcode.md5'))}
  code=data[f'Metadata/plate_{i}.gcode'];assert hashlib.md5(code).hexdigest()==data[f'Metadata/plate_{i}.gcode.md5'].decode().strip().lower()
  subset['Metadata/plate_1.gcode']=code;subset['Metadata/plate_1.gcode.md5']=data[f'Metadata/plate_{i}.gcode.md5'];subset['Metadata/plate_1.png']=data[f'Metadata/plate_{i}.png'];subset['Metadata/top_1.png']=data[f'Metadata/top_{i}.png']
  subset['3D/3dmodel.model']=E.tostring(own);subset['Metadata/model_settings.config']=E.tostring(c);subset['Metadata/slice_info.config']=E.tostring(ss)
  folder=OUT/f'plate-{i:02d}-inspection';folder.mkdir(exist_ok=True);path=folder/'audit-extract.3mf'
  with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
   for n,v in subset.items():z.writestr(n,v)
  inspection=Q.I.inspect(path,folder);assert not inspection['slicer_warnings'];assert not inspection['support_conflicts_with_exclusion_volumes']
  from support_review import refine_candidates
  refinements=refine_candidates(code.decode(),inspection,folder)
  assert all(r['unresolved_islands']==0 for r in refinements),refinements
  si=md(sliceinfo.findall('plate')[i-1]);assert si.get('outside')=='false'
  row={'plate':i,'name':info['plater_name'],'objects':len(wanted),'estimated_seconds':int(si['prediction']),'grams':float(si['weight']),'support_conflicts':0,'slicer_warnings':0,'unresolved_start_candidates':0,'refined_candidates':refinements};rows.append(row);print(json.dumps(row),flush=True)
 r=Q.P.run_cli(['--info',FINAL],OUT/'reopen.log',120);assert r['exit_code']==0
 report={'file':str(FINAL),'sha256':Q.P.sha(FINAL),'plates':rows,'geometry':geometry,'crc_and_gcode_md5':'pass','reopen':'pass','scale':1.0,'printer_jobs_sent':False};Q.P.dump(OUT/'verification.json',report);print('Verified four plates and 17 original-size unmarked parts')
if __name__=='__main__':verify()
