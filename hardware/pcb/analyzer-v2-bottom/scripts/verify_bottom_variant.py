#!/usr/bin/env python3
"""Independent bottom-USB delta audit. Reads native text, never edits PCB/sheets.

The source PCB2, original-source manifest and allowed USB rigid transform are
pinned independently. This is preservation evidence, not a manufacturing release.
"""
from pathlib import Path
import sys,json,hashlib,copy,math,subprocess,argparse,xml.etree.ElementTree as ET
from collections import Counter
from datetime import datetime,timezone
ROOT=next(p for p in Path(__file__).resolve().parents if (p/'hardware/pcb/analyzer-v2').is_dir())
BASE=ROOT/'hardware/pcb/analyzer-v2';HERE=ROOT/'hardware/pcb/analyzer-v2-bottom'
sys.path.insert(0,str(BASE/'scripts'));import verify_variant as v
NAME='Trimix_Analyzer';USB={'J901','U901','D901'}
BASE_HASH='1df55664b1f57ec4dcc38bbd2358f5aee306f59696f52271aafb94f233b992bc'
MANIFEST_HASH='12e91986281369c36cefdda2c8eca19386d702f23d3b1ba262d95105bdefa832'
LINK_NETS={'USB_CC1','USB_CC2','USB_D_P','USB_D_M','USB_5V'}
# Final, explicitly authorized D+ hop: retain existing power pours and terminate
# on the original J101 barrel. Every other new signal on In1 remains forbidden.
IN1_DPLUS=[((39.85,111.9),(37.375,114.375)),((37.375,114.375),(37.375,118.475)),((37.375,118.475),(37.35,118.48491))]
BARRELS={'47363557-5ea4-4748-84fa-7e37b39a28ea','c3bc6ac5-4fa3-4fd4-bc4d-99225483322c','6701b800-9295-46f5-a7f3-f18d9c6a0df0','11ada617-abb2-40b0-aa06-6dfbbe5caaba','de2fb6ee-f344-412e-93c2-f24e4335241a','b7aa0b79-4cdd-43df-a6a1-a24f6ce2f0c0','102d484f-fe35-4330-b005-c2d505f45658','14f2b057-f13f-44b7-92a5-bc916b62030d','f7773eaf-c186-4e42-82b0-96442e194ac2'}
OUTLINE=[(3.51509,121.28491),(21,121.28491),(21,120.48491),(49.9,120.48491),(49.9,38.48491),(28.8,38.48491),(28.8,21.48491),(19.9,21.48491),(19.9,104.38491),(3.51509,104.38491)]
checks=[]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check(name,ok,details=None):checks.append({'check':name,'passed':bool(ok),'details':details})
def T(p):return [round(124-p[1],8),round(56.28491+p[0],8)]
def close(a,b,tol=3e-6):return len(a)==len(b) and all(abs(x-y)<=tol for x,y in zip(a,b))
def copper(b):return {v.uid(t):t for kind in ('segment','via','arc')for t in v.children(b,kind)}
def strip(t,keys):return [q for q in t if not(isinstance(q,list) and q and q[0]in keys)]
def thermal(b):
 f={v.props(f)['Reference'][2]:f for f in v.children(b,'footprint')};rows=[]
 for ref,pin in [('U101','25'),('U201','15')]:
  for p in v.children(f[ref],'pad'):
   if p[1:3]==[pin,'thru_hole']:rows.append({'reference':ref,'uuid':v.uid(p),'world_xy_mm':v.world_pad(f[ref],p),'raw_pad':p})
 return rows

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--variant-board',type=Path,default=HERE/(NAME+'.kicad_pcb'));ap.add_argument('--output-dir',type=Path,default=HERE/'reference/verification');args=ap.parse_args()
 dest=args.output_dir.resolve();dest.mkdir(parents=True,exist_ok=True)
 board=args.variant_board.resolve();manifest=BASE/'reference/original-source-hashes.json'
 inputs={board,BASE/(NAME+'.kicad_pcb'),manifest,Path(__file__).resolve(),BASE/'scripts/verify_variant.py',BASE/'reference/added-copper-uuids.json'}
 inputs|=set(BASE.glob('*.kicad_sch'))|set(HERE.glob('*.kicad_sch'))
 inputs|={d/(NAME+s)for d in (BASE,HERE)for s in ('.kicad_pro','.kicad_dru')}
 inputs|={HERE/'reference/final-drc.json'}
 start={p:sha(p)for p in inputs if p.exists()}
 check('PCB2 source is the independently frozen original integration',sha(BASE/(NAME+'.kicad_pcb'))==BASE_HASH)
 check('51-file canonical manifest is the independently pinned manifest',sha(manifest)==MANIFEST_HASH)
 canonical=json.loads(manifest.read_text());changes=[p for p,h in canonical.items()if not(ROOT/p).exists()or sha(ROOT/p)!=h]
 check('All51 canonical files are byte-identical',len(canonical)==51 and not changes,{'count':len(canonical),'changed':changes})
 inputs|={ROOT/p for p in canonical};start.update({ROOT/p:sha(ROOT/p)for p in canonical})
 names={p.name for p in BASE.glob('*.kicad_sch')};changed_sch=[n for n in names if not(HERE/n).exists()or sha(BASE/n)!=sha(HERE/n)]
 check('All14 derived schematics are byte-identical to PCB2',len(names)==14 and names=={p.name for p in HERE.glob('*.kicad_sch')} and not changed_sch,changed_sch)
 expected_sch_errors=[]
 for n in names:
  expected,_=v.expected_symbol_metadata(v.read_sexpr(BASE.parent/'analyzer'/n),n)
  if expected!=v.read_sexpr(BASE/n):expected_sch_errors.append(n)
 check('PCB2 reference sheets still contain only the prior explicit metadata/annotation adaptations to original51 sources',not expected_sch_errors,expected_sch_errors)
 before=v.read_sexpr(BASE/(NAME+'.kicad_pcb'));after=v.read_sexpr(board)
 oldfp={v.props(f)['Reference'][2]:f for f in v.children(before,'footprint')};newfp={v.props(f)['Reference'][2]:f for f in v.children(after,'footprint')}
 check('Footprint reference membership is identical',oldfp.keys()==newfp.keys(),{'source':len(oldfp),'candidate':len(newfp)})
 core_errors=[];pad_errors=[];usb_errors=[];net_errors=[]
 for ref,f in oldfp.items():
  if ref not in newfp:continue
  g=newfp[ref]
  if ref not in USB:
   for key in ('at','layer','path','attr','uuid'):
    if v.child(f,key)!=v.child(g,key):core_errors.append({'reference':ref,'field':key})
   if f[1]!=g[1]:core_errors.append({'reference':ref,'field':'library footprint'})
  else:
   a,b=v.child(f,'at')[1:],v.child(g,'at')[1:]
   if not close(T(a[:2]),b[:2])or abs(((b[2]if len(b)>2 else 0)-(a[2]if len(a)>2 else 0)+90)%360)>1e-6:usb_errors.append({'reference':ref,'field':'rigid placement','old':a,'new':b,'expected_xy':T(a[:2])})
   for key in ('layer','path','attr','uuid'):
    if v.child(f,key)!=v.child(g,key):usb_errors.append({'reference':ref,'field':key})
   if f[1]!=g[1]:usb_errors.append({'reference':ref,'field':'library footprint'})
  for prop in ('Reference','Value','MPN','Footprint','Assembly'):
   a,b=v.props(f).get(prop),v.props(g).get(prop)
   if (a[2]if a else None)!=(b[2]if b else None):core_errors.append({'reference':ref,'field':prop})
  op,np=v.pad_signature(f),v.pad_signature(g)
  if op.keys()!=np.keys():pad_errors.append({'reference':ref,'field':'pad UUID set'})
  for pid,p in op.items():
   q=np.get(pid)
   if q is None:continue
   if v.board_net(p)!=v.board_net(q):net_errors.append({'reference':ref,'pad':p[1]})
   if ref not in USB:
    if p!=q:pad_errors.append({'reference':ref,'pad':p[1],'differences':v.diff(p,q)})
   else:
    if strip(p,{'at'})!=strip(q,{'at'}):usb_errors.append({'reference':ref,'pad':p[1],'field':'nonposition pad data','differences':v.diff(strip(p,{'at'}),strip(q,{'at'}))})
    if not close(T(v.world_pad(f,p)),v.world_pad(g,q)):usb_errors.append({'reference':ref,'pad':p[1],'field':'world pad rigid placement'})
    pa,pb=v.child(p,'at')[1:],v.child(q,'at')[1:]
    oldangle=pa[2]if len(pa)>2 else (v.child(f,'at')[3]if len(v.child(f,'at'))>3 else 0)
    newangle=pb[2]if len(pb)>2 else (v.child(g,'at')[3]if len(v.child(g,'at'))>3 else 0)
    if abs((newangle-oldangle+90)%360)>1e-6:usb_errors.append({'reference':ref,'pad':p[1],'field':'pad angle','angles':[oldangle,newangle]})
 check('All retained core footprints retain native placement, identity, values and population',not core_errors,core_errors)
 check('Every retained core pad is identical, including thermal lands, layers, nets and holes',not pad_errors,pad_errors)
 check('All35 USB pads and three footprints undergo only the approved rigid transform',not usb_errors and sum(len(v.children(oldfp[r],'pad'))for r in USB)==35,usb_errors)
 check('Every pad-to-net assignment is unchanged',not net_errors,net_errors)
 oldcu,newcu=copper(before),copper(after)
 added=set(json.loads((BASE/'reference/added-copper-uuids.json').read_text()))
 # Independently reconstruct the92 local objects from frozen added-copper provenance.
 local={i for i in added if all(v.child(oldcu[i],k)[1]>=51.5 for k in (('at',)if oldcu[i][0]=='via'else('start','end')))}
 check('Independent local USB selection is exactly92 items (76tracks,16vias)',len(local)==92 and Counter(oldcu[i][0]for i in local)==Counter({'segment':76,'via':16}),{'count':len(local)})
 protected=(set(oldcu)-added)|BARRELS;allowed=added-local-BARRELS
 preserved_errors=[{'uuid':i,'differences':v.diff(oldcu[i],newcu.get(i))}for i in protected if oldcu[i]!=newcu.get(i)]
 check('All2199 non-USB-added copper objects plus9 retained former-J101 replacements are exact',len(set(oldcu)-added)==2199 and not preserved_errors,{'protected_count':len(protected),'mismatches':preserved_errors})
 local_errors=[]
 for i in local:
  p,q=oldcu[i],newcu.get(i);expected=copy.deepcopy(p)
  for key in (('at',)if p[0]=='via'else('start','end')):v.child(expected,key)[1:]=T(v.child(expected,key)[1:])
  if q is None or v.diff(expected,q):
   # Tolerate only numeric serialization at the transformed endpoint coordinates.
   same=q is not None and strip(expected,{'at','start','end'})==strip(q,{'at','start','end'})
   if same:same=all(close(v.child(expected,k)[1:],v.child(q,k)[1:])for k in(('at',)if p[0]=='via'else('start','end')))
   if not same:local_errors.append({'uuid':i,'differences':v.diff(expected,q)})
 check('All92 USB-local copper items preserve topology, widths, layers, drills and treatment under rotation',not local_errors,local_errors)
 missing=set(oldcu)-set(newcu)
 check('Only the26 superseded USB-link copper objects may be removed/rerouted',len(allowed)==26 and missing<=allowed,{'allowed_uuids':sorted(allowed),'removed_uuids':sorted(missing),'unexpected_removals':sorted(missing-allowed)})
 changed={i for i in oldcu.keys()&newcu.keys()if oldcu[i]!=newcu[i]};new_ids=set(newcu)-set(oldcu)
 check('No copper outside USB-local and superseded-link scope changes',changed<=local|allowed,{'unexpected_changed':sorted(changed-local-allowed)})
 routes=[newcu[i]for i in new_ids|(allowed&set(newcu))];route_errors=[]
 for t in routes:
  net=v.board_net(t)
  if net not in LINK_NETS|{'GND'}:route_errors.append({'uuid':v.uid(t),'error':'unexpected net','net':net})
  if t[0]=='segment':
   width=v.child(t,'width')[1]
   if width+1e-8 < (1 if net=='USB_5V'else.15):route_errors.append({'uuid':v.uid(t),'error':'narrow rerouted link','width':width,'net':net})
   if v.child(t,'layer')==['layer','In1.Cu']and net!='GND':
    ends=[v.child(t,k)[1:]for k in('start','end')]
    allowed_in1=net=='USB_D_P' and abs(width-.15)<1e-8 and any((close(ends[0],a)and close(ends[1],b))or(close(ends[1],a)and close(ends[0],b))for a,b in IN1_DPLUS)
    if not allowed_in1:route_errors.append({'uuid':v.uid(t),'error':'signal outside exact approved In1 D+ hop'})
  elif t[0]=='via':
   if v.child(t,'drill')[1]<.2 or v.child(t,'size')[1]<.45 or v.child(t,'layers')!=['layers','F.Cu','B.Cu']:route_errors.append({'uuid':v.uid(t),'error':'new via below standard bounds or not through'})
  else:route_errors.append({'uuid':v.uid(t),'error':'new unsupported copper kind'})
 in1=[t for t in routes if t[0]=='segment' and v.child(t,'layer')==['layer','In1.Cu'] and v.board_net(t)!='GND']
 if len(in1)!=len(IN1_DPLUS):route_errors.append({'error':'Exact three-segment In1 hop is incomplete or duplicated','actual_count':len(in1)})
 check('New links preserve1mm VBUS minimum,0.15mm signals and ordinary vias; only exact bounded D+ hop uses In1',not route_errors,{'new_copper_count':len(new_ids),'link_item_count':len(routes),'approved_In1_Dplus_endpoints':IN1_DPLUS,'mismatches':route_errors})
 anchors={'USB_CC1':([13.35,108.20991],[33.75,118.48491]),'USB_CC2':([12.6,108.28491],[35.55,118.48491]),'USB_5V':([17.2,108.48491],[30.15,118.48491]),'USB_D_P':([11.4,108.28491],[37.35,118.48491]),'USB_D_M':([10.65,108.68491],[39.15,118.48491])}
 anchor_results=[]
 for net,targets in anchors.items():
  points=[v.child(t,k)[1:]for t in routes if v.board_net(t)==net for k in (('at',)if t[0]=='via'else('start','end'))]
  core_targets=[targets[1]]
  if net=='USB_D_P':core_targets.append([48.835,115.52462])
  # D+ in the frozen PCB2 source actually entered retained original copper at
  # (48.835,115.52462); former J101 is not its sole legitimate core tie point.
  # Require an exact retained-source endpoint, not an arbitrary same-net point.
  proven=[]
  for target in core_targets:
   for i in protected:
    t=oldcu[i]
    if t[0]=='segment' and v.board_net(t)==net and any(close(v.child(t,k)[1:],target)for k in('start','end')):proven.append(target);break
  local_ok=any(close(p,targets[0])for p in points)
  core_ok=any(any(close(p,t)for p in points)for t in proven)
  anchor_results.append({'net':net,'protected_local_exit_native_mm':targets[0],'allowed_retained_core_entries_native_mm':core_targets,'independently_proven_source_entries_native_mm':proven,'both_anchors_explicit':local_ok and core_ok})
 check('All five rerouted links explicitly use preserved post-ESD exits and exact retained-source core-entry anchors',all(q['both_anchors_explicit']for q in anchor_results),anchor_results)

 # Explicit copper bridge proof: same net labels alone do not join NC pass-through pads.
 esdp={p[1]:p for p in v.children(newfp['U901'],'pad')if p[1]};bridges=[]
 for a,b,net in [('1','10','USB_CC1'),('2','9','USB_CC2'),('4','7','USB_D_P'),('5','6','USB_D_M')]:
  p,q=v.world_pad(newfp['U901'],esdp[a]),v.world_pad(newfp['U901'],esdp[b])
  hits=[v.uid(t)for t in v.children(after,'segment')if v.board_net(t)==net and v.child(t,'layer')==['layer','F.Cu']and v.child(t,'width')[1]>=.15 and ((close(v.child(t,'start')[1:],p)and close(v.child(t,'end')[1:],q))or(close(v.child(t,'end')[1:],p)and close(v.child(t,'start')[1:],q)))]
  bridges.append({'pins':[a,b],'net':net,'direct_segments':hits})
 check('U901 four NC pairs retain actual direct copper bridges',all(r['direct_segments']for r in bridges),bridges)
 mapping={'J901':{'A1':'GND','A12':'GND','B1':'GND','B12':'GND','SH':'GND','A4':'USB_5V','A9':'USB_5V','B4':'USB_5V','B9':'USB_5V','A5':'USB_CC1','B5':'USB_CC2','A6':'USB_D_P','B6':'USB_D_P','A7':'USB_D_M','B7':'USB_D_M','A8':'unconnected-(J901-SBU1-PadA8)','B8':'unconnected-(J901-SBU2-PadB8)'},'D901':{'1':'USB_5V','2':'GND'}}
 mapping['U901']=dict(zip(map(str,range(1,11)),('USB_CC1','USB_CC2','GND','USB_D_P','USB_D_M','USB_D_M','USB_D_P','GND','USB_CC2','USB_CC1')))
 me=[{'ref':ref,'pin':p[1],'net':v.board_net(p)}for ref,expected in mapping.items()for p in v.children(newfp[ref],'pad')if p[1]and expected.get(p[1])!=v.board_net(p)]
 check('USB-C and ESD pin mappings retain polarity, channels and VBUS/GND protection',not me,me)
 t0,t1=thermal(before),thermal(after)
 check('All24 thermal drills/lands retain exact location,0.20mm bores and ground mapping',t0==t1 and len(t1)==24 and all(v.child(p['raw_pad'],'drill')==['drill',.2]for p in t1),t1)
 check('All layers, stackup,1.6mm general thickness and global setup are retained',v.child(before,'layers')==v.child(after,'layers')and v.child(before,'general')==v.child(after,'general')and v.child(before,'setup')==v.child(after,'setup'))
 pro0=json.loads((BASE/(NAME+'.kicad_pro')).read_text());pro1=json.loads((HERE/(NAME+'.kicad_pro')).read_text())
 check('Design settings/netclasses and custom rules are unchanged',all(pro0.get(k)==pro1.get(k)for k in('board','net_settings')) and sha(BASE/(NAME+'.kicad_dru'))==sha(HERE/(NAME+'.kicad_dru')))
 z0={v.uid(z):z for z in v.children(before,'zone')};z1={v.uid(z):z for z in v.children(after,'zone')};ze=[]
 expected_zone_polygons={
 '531f7a80-57bf-49da-926b-ee1da1066b2c':OUTLINE,
 'a3ee32fc-511b-4512-acf0-db8842f10939':[(3.51509,104.38491),(21,104.38491),(21,121.28491),(3.51509,121.28491)],
 'b76844b1-3616-4d68-9eb1-0974e35bc20d':[(3.51509,104.38491),(21,104.38491),(21,121.28491),(3.51509,121.28491)],
 'b759ef5f-747e-431e-a3b1-071e532148d4':[(19.9,21.48491),(20.4,21.48491),(20.4,104.38491),(19.9,104.38491)],
 '98a237f8-fda4-45e0-a4cd-1f55a012a4ca':[(49.4,38.48491),(49.275,38.48491),(49.275,120.48491),(49.4,120.48491)],
 '752a4e3f-d3fd-4923-bf89-43e311b9cfa3':[(3.51509,120.78491),(21,120.78491),(21,121.28491),(3.51509,121.28491)],
 '1a6f506a-b52d-4161-ba79-54fc7dc85e8e':[(20.5,120.48491),(21,120.48491),(21,121.28491),(20.5,121.28491)],
 '5f563f3d-c3bf-4282-ad7f-ce7feec2097c':[(21,119.98491),(49.9,119.98491),(49.9,120.48491),(21,120.48491)]}
 if z0.keys()!=z1.keys():ze.append({'error':'Zone UUID set changed'})
 for i,z in z0.items():
  q=z1.get(i)
  if q is None:ze.append({'uuid':i,'error':'zone removed'});continue
  if strip(z,{'polygon','filled_polygon'})!=strip(q,{'polygon','filled_polygon'}):ze.append({'uuid':i,'error':'zone net/layer/fill/thermal semantics changed'})
  if i in expected_zone_polygons:
   expected=[['polygon',['pts',*[['xy',x,y]for x,y in expected_zone_polygons[i]]]]]
   if v.children(q,'polygon')!=expected:ze.append({'uuid':i,'error':'polygon differs from independent exact adaptation'})
  elif v.children(z,'polygon')!=v.children(q,'polygon'):ze.append({'uuid':i,'error':'unapproved source zone polygon change'})
  if v.child(z,'keepout') and i not in expected_zone_polygons and z!=q:ze.append({'uuid':i,'error':'retained keepout changed'})
 check('All zone semantics retained; only three exact GND contours and five matching edge-reserve contours adapt',not ze,{'allowed_polygon_changes':expected_zone_polygons,'mismatches':ze})
 edges=[e for e in v.children(after,'gr_line')if v.child(e,'layer')==['layer','Edge.Cuts']]
 segments={frozenset(tuple(v.child(e,k)[1:])for k in('start','end'))for e in edges}
 wanted={frozenset((p,OUTLINE[(i+1)%len(OUTLINE)]))for i,p in enumerate(OUTLINE)}
 dims=[max(p[i]for p in OUTLINE)-min(p[i]for p in OUTLINE)for i in(0,1)]
 check('Outline is the exact approved10-segment46.38491×99.8mm closed profile',segments==wanted and len(edges)==len(wanted),{'dimensions_mm':dims,'actual_edge_count':len(edges)})
 drills=[v.child(p,'drill')[1:]for f in newfp.values()for p in v.children(f,'pad')if v.child(p,'drill')]+[v.child(t,'drill')[1:]for t in v.children(after,'via')]
 sizes=[n for d in drills for n in d if isinstance(n,(int,float))]
 check('Smallest native drill remains0.20mm',bool(sizes)and abs(min(sizes)-.2)<1e-8,{'minimum_mm':min(sizes),'drill_item_count':len(drills)})
 # Fresh graph comparison is independent of board DRC and references current sheet bytes.
 cli='/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli';netpaths=[];exports=[]
 for label,base in [('baseline',BASE),('bottom',HERE)]:
  target=dest/(label+'-netlist.xml');run=subprocess.run([cli,'sch','export','netlist','--format','kicadxml','--output',str(target),str(base/(NAME+'.kicad_sch'))],capture_output=True,text=True)
  exports.append({'source':str(base.relative_to(ROOT)),'returncode':run.returncode,'stderr':run.stderr.strip()})
  if run.returncode:raise RuntimeError('Fresh netlist export failed '+str(exports[-1]))
  netpaths.append(target)
 n0,n1=[ET.parse(p).getroot()for p in netpaths]
 check('Fresh schematic graphs and component metadata remain identical',v.net_graph(n0)==v.net_graph(n1)and{e.attrib['ref']:v.component_signature(e)for e in n0.findall('./components/comp')}=={e.attrib['ref']:v.component_signature(e)for e in n1.findall('./components/comp')})
 drcpath=HERE/'reference/final-drc.json';drc=json.loads(drcpath.read_text())if drcpath.exists()else{}
 olddrc=json.loads((BASE/'reference/final-drc.json').read_text())
 check('Frozen native DRC reports zero violations,unconnected items and parity errors without new ignored checks',bool(drc)and all(drc.get(k)==[]for k in('violations','unconnected_items','schematic_parity'))and drc.get('ignored_checks')==olddrc.get('ignored_checks')and set(olddrc.get('included_severities',[]))<=set(drc.get('included_severities',[])),{'source_report':str(drcpath.relative_to(ROOT)),'date':drc.get('date'),'included_severities':drc.get('included_severities'),'baseline_included_severities':olddrc.get('included_severities'),'inspection_only':True})
 stable=[str(p.relative_to(ROOT))for p,h in start.items()if not p.exists()or sha(p)!=h]
 check('All captured native/source inputs remained stable throughout audit',not stable,stable)
 report={'timestamp_utc':datetime.now(timezone.utc).isoformat(),'purpose':'Independent bottom-USB preservation audit; not production release',
  'input_sha256':{str(p.relative_to(ROOT)):sha(p)for p in sorted(inputs|set(netpaths))if p.exists()},'fresh_netlist_exports':exports,
  'checks':checks,'passed':sum(c['passed']for c in checks),'failed':sum(not c['passed']for c in checks),
  'limitations':['Local USB ESD topology and source geometry are rigidly retained; longer rerouted links still require native DRC and independent CAM connectivity review.',
   'Thermal drill/land and fill-rule identity does not by itself prove identical cooling. Independent filled copper/CAM comparison and physical temperature tests remain separate.',
   'The DRC result is independently inspected, not rerun by this script. No fabrication, ESD, charger or mechanical release is claimed.']}
 out=dest/'bottom-invariants.json';out.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({'passed':report['passed'],'failed':report['failed'],'failures':[c for c in checks if not c['passed']]},indent=2),flush=True)
 return bool(report['failed'])
if __name__=='__main__':sys.exit(main())
