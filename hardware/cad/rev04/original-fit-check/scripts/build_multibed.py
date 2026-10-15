from pathlib import Path
import sys,zipfile,json,copy,uuid,xml.etree.ElementTree as E
import prepare_original_prints as O
ROOT=O.OUT;Q=O.Q
N='{http://schemas.microsoft.com/3dmanufacturing/core/2015/02}';P='{http://schemas.microsoft.com/3dmanufacturing/production/2015/06}';REL='{http://schemas.openxmlformats.org/package/2006/relationships}'
E.register_namespace('',N[1:-1]);E.register_namespace('p',P[1:-1]);
SOURCES=[('01_Small_component_fit_checks','01 - SMALL FIT CHECKS - existing prints reusable',(0,0)),('02_Sensor_manifold','02 - SENSOR MANIFOLD - existing print reusable',(420,0)),('03_Original_housing','03 - ORIGINAL HOUSING - reprint',(0,-384)),('04_Original_rear_lid','04 - ORIGINAL REAR LID - reprint',(420,-384))]
def metadata(el,key,value):
 m=next((m for m in el.findall('metadata') if m.get('key')==key),None)
 if m is None:m=E.SubElement(el,'metadata',{'key':key})
 m.set('value',str(value))
def xml(el):return E.tostring(el,encoding='utf-8',xml_declaration=True)
def merge():
 out=ROOT/'multi-bed';out.mkdir(exist_ok=True)
 paths=[ROOT/'Bambu Studio'/s[0]/(s[0]+'.3mf') for s in SOURCES]
 source=[]
 for p in paths:
  with zipfile.ZipFile(p) as z:source.append({n:z.read(n) for n in z.namelist()})
 data={n:v for n,v in source[0].items() if not n.startswith('3D/Objects/')}
 main=E.fromstring(data['3D/3dmodel.model']);res=main.find(N+'resources');res.clear();build=main.find(N+'build');build.clear();build.set(P+'UUID',str(uuid.uuid4()))
 config=E.Element('config');plates=[];objects=[];sliceinfo=E.fromstring(source[0]['Metadata/slice_info.config'])
 for p in list(sliceinfo.findall('plate')):sliceinfo.remove(p)
 modelrels=E.Element('Relationships',{'xmlns':REL[1:-1]});gcode_rels=E.Element('Relationships',{'xmlns':REL[1:-1]});seq={}
 for i,(src,(name,title,(dx,dy))) in enumerate(zip(source,SOURCES),1):
  offset=i*1000;original=E.fromstring(src['3D/3dmodel.model']);cfg=E.fromstring(src['Metadata/model_settings.config'])
  pathmap={n:'3D/Objects/plate%d_%s'%(i,Path(n).name) for n in src if n.startswith('3D/Objects/')}
  for oldpath,newpath in pathmap.items():
   leaf=E.fromstring(src[oldpath])
   for o in leaf.iter(N+'object'):o.set('id',str(int(o.get('id'))+offset))
   data[newpath]=xml(leaf);E.SubElement(modelrels,'Relationship',{'Target':'/'+newpath,'Id':f'rel-{len(modelrels)+1}','Type':'http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel'})
  for o in original.find(N+'resources'):
   o.set('id',str(int(o.get('id'))+offset));o.set(P+'UUID',str(uuid.uuid4()))
   for c in o.find(N+'components'):
    c.set('objectid',str(int(c.get('objectid'))+offset));c.set(P+'path','/'+pathmap[c.get(P+'path').lstrip('/')]);c.set(P+'UUID',str(uuid.uuid4()))
   res.append(o)
  for b in original.find(N+'build'):
   b.set('objectid',str(int(b.get('objectid'))+offset));v=list(map(float,b.get('transform').split()));v[9]+=dx;v[10]+=dy;b.set('transform',' '.join(f'{x:.12g}' for x in v));b.set(P+'UUID',str(uuid.uuid4()));build.append(b)
  for o in cfg.findall('object'):
   o.set('id',str(int(o.get('id'))+offset))
   for part in o.findall('part'):
    part.set('id',str(int(part.get('id'))+offset))
    if part.get('uuid'):part.set('uuid',str(uuid.uuid4()))
   if i==2:
    for key,val in [('layer_height','.16'),('bottom_shell_layers',5),('top_shell_layers',5)]:metadata(o,key,val)
   objects.append(o)
  plate=cfg.find('plate');metadata(plate,'plater_id',i);metadata(plate,'plater_name',title)
  for m in plate.findall('metadata'):
   v=m.get('value','')
   if v.startswith('Metadata/'):m.set('value',v.replace('_1.','_%d.'%i))
  for mi in plate.findall('model_instance'):
   for m in mi.findall('metadata'):
    if m.get('key')=='object_id':m.set('value',str(int(m.get('value'))+offset))
  plates.append(plate)
  si=E.fromstring(src['Metadata/slice_info.config']).find('plate');metadata(si,'index',i);sliceinfo.append(si)
  for n,v in src.items():
   if n.startswith('Metadata/') and any(n.startswith('Metadata/'+stem) for stem in ['plate_1.','plate_1_','plate_no_light_1.','top_1.','pick_1.']):
    new=n.replace('_1.','_%d.'%i).replace('plate_1_','plate_%d_'%i);data[new]=v
  seq[f'plate_{i}']=json.loads(src['Metadata/filament_sequence.json'])['plate_1'];E.SubElement(gcode_rels,'Relationship',{'Target':f'/Metadata/plate_{i}.gcode','Id':f'rel-{i}','Type':'http://schemas.bambulab.com/package/2021/gcode'})
 for o in objects:config.append(o)
 for p in plates:config.append(p)
 E.SubElement(config,'assemble')
 for m in main.findall(N+'metadata'):
  if m.get('name') in ['Title','ProfileTitle']:m.text='Trimix ORIGINAL H2D fit check - no rear gasket - four plates'
  if m.get('name') in ['Description','ProfileDescription']:m.text='Original pre-gasket enclosure. Plate 1: 14 small pieces including D02 PCB replica. Plate 2: manifold. Plates 1-2 reuse existing prints. Reprint only plates 3-4: original housing and lid. Bambu PLA Basic / H2D 0.4 mm; 100% scale.'
 data['3D/3dmodel.model']=xml(main);data['3D/_rels/3dmodel.model.rels']=xml(modelrels);data['Metadata/model_settings.config']=xml(config);data['Metadata/slice_info.config']=xml(sliceinfo);data['Metadata/_rels/model_settings.config.rels']=xml(gcode_rels);data['Metadata/filament_sequence.json']=json.dumps(seq).encode()
 dest=out/'assembled-input.3mf'
 with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
  for n,v in data.items():z.writestr(n,v)
 print('Assembled four plates, 17 objects',flush=True)
 common=['--load-settings',str(ROOT/'profiles/h2d_04.json')+';'+str(ROOT/'profiles/fit_0.1.json'),'--load-filaments',ROOT/'profiles/pla_basic.json','--orient','0','--arrange','0','--ensure-on-bed','--slice','0','--export-3mf','Trimix_Original_H2D_FitCheck.3mf','--outputdir',out,'--mstpp','600',dest]
 r=Q.P.run_cli(common,out/'slice.log',900);Q.P.dump(out/'slice-result.json',r);print(json.dumps(r));assert r['exit_code']==0
if __name__=='__main__':merge()
