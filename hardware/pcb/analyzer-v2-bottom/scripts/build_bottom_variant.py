#!/usr/bin/env python3
"""Derive bottom USB layout from immutable PCB2. Native KiCad Python required."""
from pathlib import Path
import json,hashlib
import pcbnew as p
HERE=Path(__file__).resolve().parents[1];ROOT=HERE.parents[2];BASE=HERE.parent/'analyzer-v2';NAME='Trimix_Analyzer'
SOURCE=BASE/(NAME+'.kicad_pcb');DEST=HERE/(NAME+'.kicad_pcb')
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()=='1df55664b1f57ec4dcc38bbd2358f5aee306f59696f52271aafb94f233b992bc'
B=p.LoadBoard(str(SOURCE));ADDED=set(json.loads((BASE/'reference/added-copper-uuids.json').read_text()))
BARRELS={'47363557-5ea4-4748-84fa-7e37b39a28ea','c3bc6ac5-4fa3-4fd4-bc4d-99225483322c','6701b800-9295-46f5-a7f3-f18d9c6a0df0','11ada617-abb2-40b0-aa06-6dfbbe5caaba','de2fb6ee-f344-412e-93c2-f24e4335241a','b7aa0b79-4cdd-43df-a6a1-a24f6ce2f0c0','102d484f-fe35-4330-b005-c2d505f45658','14f2b057-f13f-44b7-92a5-bc916b62030d','f7773eaf-c186-4e42-82b0-96442e194ac2'}
def mm(x,y):return p.VECTOR2I(p.FromMM(x),p.FromMM(y))
def xy(v):return list(p.ToMM(v))
def transform(v):x,y=xy(v);return mm(124-y,56.28491+x)
LOCAL=set();REMOVED=set();NEW=[];ZONES=[];DISCARDED=[]
for t in list(B.GetTracks()):
 uid=t.m_Uuid.AsString()
 if uid not in ADDED:continue
 if min(xy(t.GetStart())[0],xy(t.GetEnd())[0])>=51.5-1e-6:
  LOCAL.add(uid)
  if isinstance(t,p.PCB_VIA):t.SetPosition(transform(t.GetPosition()))
  else:a,c=transform(t.GetStart()),transform(t.GetEnd());t.SetStart(a);t.SetEnd(c)
 elif uid not in BARRELS:REMOVED.add(uid);DISCARDED.append(t);B.Remove(t)
assert len(LOCAL)==92 and len(REMOVED)==26
for f in B.GetFootprints():
 if f.GetReference()in('J901','U901','D901'):
  pos=transform(f.GetPosition());f.SetOrientationDegrees(f.GetOrientationDegrees()-90);f.SetPosition(pos)
OUTLINE=[(3.51509,121.28491),(21,121.28491),(21,120.48491),(49.9,120.48491),(49.9,38.48491),(28.8,38.48491),(28.8,21.48491),(19.9,21.48491),(19.9,104.38491),(3.51509,104.38491)]
for d in list(B.GetDrawings()):
 if d.GetLayer()==p.Edge_Cuts:DISCARDED.append(d);B.Remove(d)
for a,c in zip(OUTLINE,OUTLINE[1:]+OUTLINE[:1]):
 s=p.PCB_SHAPE(B);s.SetShape(p.SHAPE_T_SEGMENT);s.SetStart(mm(*a));s.SetEnd(mm(*c));s.SetLayer(p.Edge_Cuts);s.SetWidth(p.FromMM(.05));B.Add(s)
def contour(z,pts,reason):
 before=[[p.ToMM(z.Outline().CVertex(i).x),p.ToMM(z.Outline().CVertex(i).y)] for i in range(z.Outline().TotalVertices())]
 z.Outline().RemoveAllContours();z.Outline().NewOutline()
 for x,y in pts:z.Outline().Append(p.FromMM(x),p.FromMM(y))
 ZONES.append({'uuid':z.m_Uuid.AsString(),'name':z.GetZoneName(),'reason':reason,'before_polygon':before,'after_polygon':pts,'changed_properties':['polygon','filled_polygons'],'all_other_properties':'preserved'})
for z in B.Zones():
 uid=z.m_Uuid.AsString();name=z.GetZoneName()
 if uid=='531f7a80-57bf-49da-926b-ee1da1066b2c':contour(z,OUTLINE,'Continuous GND plane follows revised outline')
 elif uid in('a3ee32fc-511b-4512-acf0-db8842f10939','b76844b1-3616-4d68-9eb1-0974e35bc20d'):contour(z,[(3.51509,104.38491),(21,104.38491),(21,121.28491),(3.51509,121.28491)],'Local USB ground pour follows moved USB wing')
 elif name=='ROUTER_EDGE_RESERVE_0':contour(z,[(19.9,21.48491),(20.4,21.48491),(20.4,104.38491),(19.9,104.38491)],'Former lower left edge becomes internal neck')
 elif name=='ROUTER_EDGE_RESERVE_1':contour(z,[(49.4,38.48491),(49.275,38.48491),(49.275,120.48491),(49.4,120.48491)],'Restore reserve along restored original right edge')
 elif name=='ROUTER_EDGE_RESERVE_7':contour(z,[(3.51509,120.78491),(21,120.78491),(21,121.28491),(3.51509,121.28491)],'Filled former notch: reserve now follows new USB wing front edge')
 elif name=='ROUTER_EDGE_RESERVE_6':contour(z,[(20.5,120.48491),(21,120.48491),(21,121.28491),(20.5,121.28491)],'Filled former notch: reserve follows new0.8mm wing-to-core step')
 elif name=='ROUTER_EDGE_RESERVE_5':contour(z,[(21,119.98491),(49.9,119.98491),(49.9,120.48491),(21,120.48491)],'Filled former notch: bottom-edge reserve extends left toX21')
def track(net,pts,layer,width=.15):
 for a,c in zip(pts,pts[1:]):
  if a==c:continue
  t=p.PCB_TRACK(B);t.SetStart(mm(*a));t.SetEnd(mm(*c));t.SetWidth(p.FromMM(width));t.SetLayer(layer);t.SetNet(B.FindNet(net));B.Add(t);NEW.append(t.m_Uuid.AsString())
def via(net,pos,diameter=.6,drill=.3):
 t=p.PCB_VIA(B);t.SetPosition(mm(*pos));t.SetWidth(p.FromMM(diameter));t.SetDrill(p.FromMM(drill));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(p.F_Cu,p.B_Cu);t.SetNet(B.FindNet(net));B.Add(t);t.SetFrontTentingMode(p.TENTING_MODE_TENTED);t.SetBackTentingMode(p.TENTING_MODE_TENTED);NEW.append(t.m_Uuid.AsString())
# New links are supplied separately so each iteration is reproduced from the frozen source.
routes=HERE/'scripts/bottom-routes.json'
if routes.exists():
 for r in json.loads(routes.read_text()):
  if r['kind']=='via':via(r['net'],r['position'],r.get('diameter',.6),r.get('drill',.3))
  else:track(r['net'],r['points'],B.GetLayerID(r['layer']),r.get('width',.15))
B.GetTitleBlock().SetTitle('Trimix PCB 2.0 — integrated bottom USB-C layout')
B.GetTitleBlock().SetRevision('2.0-bottom-01')
B.GetTitleBlock().SetComment(1,'4 layers /1.6mm /46.38491x99.8mm; filled/capped thermal vias retained.')
B.BuildConnectivity();p.ZONE_FILLER(B).Fill(B.Zones());p.SaveBoard(str(DEST),B)
manifest={'source_board':str(SOURCE.relative_to(ROOT)),'source_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),'output_sha256':hashlib.sha256(DEST.read_bytes()).hexdigest(),'rigid_transform':{'native_formula':['x_new=124-y_old','y_new=56.28491+x_old'],'angle_delta_degrees':-90,'footprints':['J901','U901','D901']},'outline_native_mm':OUTLINE,'moved_local_copper_uuids':sorted(LOCAL),'retained_former_harness_bridge_uuids':sorted(BARRELS),'removed_added_side_link_uuids':sorted(REMOVED),'new_copper_uuids':NEW,'changed_zones':ZONES,'world_transform':{'x':'native_x+30.5','y':'122.58491-native_y','nominal_back_z':24.0,'nominal_top_z':25.6,'step_translation':[30.5,122.58491,24.0529]},'schematics':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in sorted(HERE.glob('*.kicad_sch'))}}
(HERE/'reference/bottom-transform-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'board':str(DEST),'moved':len(LOCAL),'removed':len(REMOVED),'new':len(NEW),'sha256':manifest['output_sha256']}))
