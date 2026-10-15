"""Explicit Fusion MCP stage: derive fit specimens without changing source CAD."""
import adsk, adsk.core as C, adsk.fusion as F
import pathlib,json,math,hashlib,sys
OUT=pathlib.Path(__file__).resolve().parents[1]
SEAL=pathlib.Path('/Users/magnustrandokken/.codex/worktrees/internal-gasket/trimix-analysator/hardware/cad/rev04/internal-seal')
sys.path.insert(0,str(SEAL/'scripts'))
import verify_seal as Q
R={'front_down':[[1,0,0],[0,1,0],[0,0,1]],'rear_down':[[1,0,0],[0,-1,0],[0,0,-1]],'x_up':[[0,0,-1],[0,1,0],[1,0,0]],'y_down':[[1,0,0],[0,0,1],[0,-1,0]]}

def dump(name,data):
 p=OUT/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(data,indent=2))
def signature(d):
 return [(c.name,b.name,b.volume,tuple(b.boundingBox.minPoint.asArray()),tuple(b.boundingBox.maxPoint.asArray())) for c in d.allComponents for b in c.bRepBodies]
def build():
 app=C.Application.get();srcdoc=next(doc for doc in app.documents if doc.name.startswith('Trimix_Enclosure_A3_Original v'));srcdoc.activate();src=F.Design.cast(app.activeProduct)
 assert src.userParameters.itemByName('CaseDepth').expression=='43 mm'
 assert not any(doc.name=='Trimix_Original_FitTests' for doc in app.documents),'Fit kit already exists; inspect before retrying'
 before=signature(src);m=F.TemporaryBRepManager.get()
 policy=json.loads((SEAL.parent/'3d-print/printing/orientations.json').read_text());specs=[]
 for p in policy['parts']:
  c=next(c for c in src.allComponents if c.partNumber==p['part_id']);assert c.bRepBodies.count==1
  specs.append({'id':p['part_id'],'name':c.description or c.name,'source_component':c.name,'source_bodies':[c.bRepBodies.item(0).name], 'orientation':p['orientation'],'role':'assembly_part','body':m.copy(c.bRepBodies.item(0))})
 pcb=next(c for c in src.allComponents if c.partNumber=='TMX-A3-B01');board=next(b for b in pcb.bRepBodies if b.name=='Future shaped PCB')
 # OMIT D01
 # specs.append({'id':'D01','name':'Main PCB outline - 1.60 mm - mechanical allocation','source_component':pcb.name,'source_bodies':[board.name],'orientation':'front_down','role':'pcb_dummy','body':m.copy(board)})
 populated=m.copy(board)
 for b in pcb.bRepBodies:
  if b!=board:assert m.booleanOperation(populated,m.copy(b),F.BooleanTypes.UnionBooleanType)
 assert abs(populated.volume-sum(b.volume for b in pcb.bRepBodies))<1e-6
 specs.append({'id':'D02','name':'Main PCB populated envelope - mechanical allocation','source_component':pcb.name,'source_bodies':[b.name for b in pcb.bRepBodies],'orientation':'front_down','role':'pcb_dummy','body':populated})
 usb=next(c for c in src.allComponents if c.partNumber=='TMX-A3-B02')
 specs.append({'id':'D03','name':'USB daughterboard replica - 0.60 mm','source_component':usb.name,'source_bodies':[usb.bRepBodies.item(0).name],'orientation':'front_down','role':'pcb_dummy','body':m.copy(usb.bRepBodies.item(0))})
 house=next(c for c in src.allComponents if c.partNumber=='TMX-A3-P01').bRepBodies.item(0)
 for pid,name,lo,hi in [('C07','Display edge and seating ledge',[0,55,0],[13,75,20]),('C08','Battery divider and support rail',[44.5,50,14.3],[50.2,70,37])]:
  b=m.copy(house);cut=Q.box(m,*lo,*[y-x for x,y in zip(lo,hi)]);assert m.booleanOperation(b,cut,F.BooleanTypes.IntersectionBooleanType)
  assert b.isSolid and b.lumps.count==1
  specs.append({'id':pid,'name':name,'source_component':'01 Shape A housing','source_bodies':[house.name],'orientation':'front_down','role':'quick_fit_coupon','crop_bounds_mm':[lo,hi],'body':b})
 doc=app.documents.add(C.DocumentTypes.FusionDesignDocumentType);doc.name='Trimix_Original_FitTests';d=F.Design.cast(app.activeProduct);d.designType=F.DesignTypes.ParametricDesignType;d.fusionUnitsManager.distanceDisplayUnits=F.DistanceUnits.MillimeterDistanceUnits
 d.attributes.add('FitTests','source_archive_sha256',hashlib.sha256((OUT/'Trimix_Enclosure_A3_Original.f3d').read_bytes()).hexdigest())
 d.attributes.add('FitTests','purpose','PLA Basic fit specimens; printer H2D 0.4mm; PCB layout provisional; no geometry scaling')
 rows=[];cursor_x=0;cursor_y=0;row_h=0
 def add_part(s):
  nonlocal cursor_x,cursor_y,row_h
  body=s['body'];oldvol=body.volume*1000;ori=R[s['orientation']];tf=C.Matrix3D.create()
  for i in range(3):
   for j in range(3):tf.setCell(i,j,ori[i][j])
  assert m.transform(body,tf)
  low=body.boundingBox.minPoint;dx,dy,dz=-low.x,-low.y,-low.z;shift=C.Matrix3D.create();shift.translation=C.Vector3D.create(dx,dy,dz);assert m.transform(body,shift)
  bb=body.boundingBox;size=[(b-a)*10 for a,b in zip(bb.minPoint.asArray(),bb.maxPoint.asArray())]
  assert body.isSolid and body.lumps.count==1 and abs(body.volume*1000-oldvol)<.0001
  if cursor_x+size[0]>320:cursor_x=0;cursor_y+=row_h+12;row_h=0
  place=C.Matrix3D.create();place.translation=C.Vector3D.create(cursor_x/10,cursor_y/10,0)
  occ=d.rootComponent.occurrences.addNewComponent(place);c=occ.component;c.name=s['id']+' | '+s['name'];c.partNumber=s['id'];c.description=s['name'];c.isOriginFolderLightBulbOn=False
  bf=c.features.baseFeatures.add();bf.name='Exact source geometry - '+s['id'];bf.startEdit();native=c.bRepBodies.add(body,bf);bf.finishEdit();native=c.bRepBodies.item(0);native.name=s['id']+' fit print'
  opts=d.exportManager.createSTLExportOptions(native,str(OUT/'source-stl'/(s['id']+'.stl')));opts.isBinaryFormat=True;opts.sendToPrintUtility=False;opts.unitType=F.DistanceUnits.MillimeterDistanceUnits;opts.surfaceDeviation=.0005;opts.normalDeviation=math.radians(2)
  assert d.exportManager.execute(opts)
  rows.append({k:v for k,v in s.items() if k!='body'}|{'size_mm':size,'volume_mm3':oldvol,'rotation_rows':ori,'translation_mm':[dx*10,dy*10,dz*10],'native_layout_mm':[cursor_x,cursor_y,0],'source_stl':'source-stl/'+s['id']+'.stl','scale':1.0,'native_solid':True,'lump_count':1})
  cursor_x+=size[0]+12;row_h=max(row_h,size[1])
 for s in specs:add_part(s)
 # Blind-hole insert selection coupons, an ordered ladder with no assumed supplier fit.
 for pid,nom,diameters,depth in [('C09','M2',[3.0,3.1,3.2,3.3],4.2),('C10','M3',[4.0,4.1,4.2,4.3],5.2)]:
  body=Q.box(m,0,0,0,56,15,8)
  for i,dia in enumerate(diameters):
   x=7+14*i;tool=m.createCylinderOrCone(C.Point3D.create(x/10,.75,(8-depth)/10),dia/20,C.Point3D.create(x/10,.75,.81),dia/20);assert m.booleanOperation(body,tool,F.BooleanTypes.DifferenceBooleanType)
   # One to four short edge slots provide a tactile left-to-right index.
   for j in range(i+1):assert m.booleanOperation(body,Q.box(m,x-3+j*1.5,0,7.5,.6,1,.6),F.BooleanTypes.DifferenceBooleanType)
  s={'id':pid,'name':nom+' insert pilot ladder - indexed 1 to 4','source_component':'New fit calibration coupon','source_bodies':[],'orientation':'front_down','role':'insert_coupon','hole_diameters_mm_left_to_right':diameters,'blind_depth_mm':depth,'body':body};add_part(s)
 assert len(rows)==17,len(rows)
 for g in [d.timeline.timelineGroups.add(0,d.timeline.count-1)]:g.name='Fit Tests | exact parts, PCB replicas and insert ladders'
 assert d.computeAll()
 assert signature(src)==before,'Source geometry changed'
 exp=d.exportManager;assert exp.execute(exp.createFusionArchiveExportOptions(str(OUT/'Trimix_Original_FitTests.f3d')));assert exp.execute(exp.createSTEPExportOptions(str(OUT/'Trimix_Original_FitTests.step')))
 cam=app.activeViewport.camera;cam.cameraType=C.CameraTypes.OrthographicCameraType;cam.viewOrientation=C.ViewOrientations.TopViewOrientation;cam.isFitView=True;cam.isSmoothTransition=False;app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh()
 opts=C.SaveImageFileOptions.create(str(OUT/'views/fit-kit-fusion.png'));opts.width=1800;opts.height=1600;opts.isAntiAliased=True;assert app.activeViewport.saveAsImageFileWithOptions(opts)
 dump('verification/native-fit-parts.json',{'source_document':srcdoc.name,'source_archive_sha256':hashlib.sha256((OUT/'Trimix_Enclosure_A3_Original.f3d').read_bytes()).hexdigest(),'source_geometry_unchanged':True,'printer':'Bambu Lab H2D','nozzle_mm':.4,'filament':'Bambu PLA Basic','parts':rows,'pcb_basis':'Current mechanical allocation, not routed KiCad PCB. D02 is the populated-envelope main-board specimen; D03 is the provisional USB board.','physical_printed':False})
 print(json.dumps({'native':str(OUT/'Trimix_Original_FitTests.f3d'),'part_count':len(rows),'source_geometry_unchanged':True,'parts':[{'id':r['id'],'size_mm':r['size_mm']} for r in rows]}))

def run(_context):build()
