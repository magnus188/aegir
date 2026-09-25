"""Read-only source/geometry/parity checks plus USB evidence output."""
from pathlib import Path
import sys,json,csv,hashlib,math,datetime,xml.etree.ElementTree as ET,collections
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3];P=ROOT/'hardware/pcb/usb-input'
sys.path.insert(0,str(ROOT/'hardware/tools'))
from analyzer_sheet import sx,child,children

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def file(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p),'bytes':p.stat().st_size}
board=P/'Trimix_USB_Input.kicad_pcb';a=sx.loads(board.read_text());before=sx.loads((HERE/'before'/board.name).read_text());sch=P/'Trimix_USB_Input.kicad_sch';xml=ET.parse(HERE/'usb-netlist.xml').getroot()
expected={(n.attrib['ref'],n.attrib['pin']):net.attrib['name'] for net in xml.findall('./nets/net') for n in net.findall('node')};actual={};fps={}
for fp in children(a,'footprint'):
 pr={v[1]:v[2]for v in children(fp,'property')};ref=pr['Reference'];fps[ref]=fp
 for pad in children(fp,'pad'):
  key=(ref,pad[1]);net=child(pad,'net')[-1]
  assert key not in actual or actual[key]==net
  actual[key]=net
assert actual==expected,(actual,expected)
assert set(fps)=={'J901','J902','U901','D901'}
assert child(fps['J901'],'at')[1:]==[108,110.14]
assert child(fps['J902'],'at')[1:]==[108,104.5]
assert child(child(a,'general'),'thickness')[1]==.6
edges=lambda b:[n for n in b if isinstance(n,list) and child(n,'layer') and child(n,'layer')[1]=='Edge.Cuts']
assert edges(a)==edges(before),'Native board outline changed'
orig=ROOT/'hardware/pcb/analyzer/Trimix_Connectors.pretty/USB_C_GCT_USB4720-03-A_A3.kicad_mod';local=P/'Trimix_USB.pretty/USB4720_RevB.kicad_mod'
assert children(sx.loads(orig.read_text()),'pad')==children(sx.loads(local.read_text()),'pad'),'Manufacturer lands changed'
for pair in [(1,10),(2,9),(4,7),(5,6)]:assert actual['U901',str(pair[0])]==actual['U901',str(pair[1])]
assert actual['U901','3']==actual['U901','8']=='GND'
assert actual['D901','1']=='USB_5V' and actual['D901','2']=='GND'
assert [actual['J902',str(i)]for i in range(1,7)]==['USB_5V','GND','USB_CC1','USB_CC2','USB_D_P','USB_D_M']
# Exact bridge segments prove the model does not rely on internally paired NC lands.
tracks=children(a,'segment');bridges=[]
for net,x in [('USB_CC1',108.5),('USB_CC2',109.0),('USB_D_P',110.0),('USB_D_M',110.5)]:
 match=[t for t in tracks if child(t,'net')[-1]==net and child(t,'layer')[1]=='F.Cu' and sorted([child(t,'start')[1:],child(t,'end')[1:]])==[[x,106.3825],[x,107.2175]]]
 assert match,(net,'missing copper under package');bridges.append({'net':net,'from_mm':[x,106.3825],'to_mm':[x,107.2175],'width_mm':child(match[0],'width')[1]})
drc=json.loads((HERE/'usb-drc.json').read_text());erc=json.loads((HERE/'usb-erc.json').read_text());types=collections.Counter(v['type']for v in drc['violations'])
assert types=={'copper_edge_clearance':20,'annular_width':2},types
assert not drc['unconnected_items'] and not drc['schematic_parity']
assert sum(len(s.get('violations',[]))for s in erc.get('sheets',[]))==0
assert len(tracks)==94 and len(children(a,'via'))==14
height={'schema':1,'units':'mm','board_nominal_size':[16,15.72,.6],'board_finished_thickness':[.5,.7],
 'nominal_Fusion_stack_Z':[24.9,25.5],'nominal_Fusion_support_underside_Z':31.2,
 'registration_hint':{'STEP_origin_native_KiCad':[100,100],'Fusion_X_translation':34.5,'Fusion_Y_translation':18,'previous_Fusion_Z_translation':24.945,'status':'CAD owner must reopen STEP and confirm actual face elevations; nominal stack and existing installation datum remain intended.'},
 'components':[{'ref':'U901','mpn':'TPD4E05U06DQAR','KiCad_XY_rotation':[109.5,106.8,90],'purchased_installed_XY_max':[2.6,1.1],'height_max':.55,'nominal_Fusion_top_Z':26.05,'provisional_assembly_allowance_Z':.1,'max_plus_allowance_top_Z':26.15,'model':'Unchanged KiCad nominal USON; illustrative height0.53. Use source maxima for clearance.'},
 {'ref':'D901','mpn':'TPD1E10B06DYAR','KiCad_XY_rotation':[104,108,0],'purchased_installed_XY_max':[1.7,1.15],'height_max':.77,'nominal_Fusion_top_Z':26.27,'provisional_assembly_allowance_Z':.1,'max_plus_allowance_top_Z':26.37,'model':'Unchanged KiCad nominal SOD523; illustrative height0.63. Marking is generic, not a manufacturer assembly guide.'},
 {'ref':'J902','KiCad_XY_rotation':[108,104.5,0],'pad_pitch':1.8,'pin_centres_X':[103.5,105.3,107.1,108.9,110.7,112.5],'finished_drill':.8,'harness_allowance_height':4.5,'allowance_top_Z':30.0,'physical_maximum_known':False,'hold':'Actual solder tips, insulated wires, bend radius, seating and strain relief remain unmeasured.'}],
 'connector':{'ref':'J901','mpn':'USB4720-03-A','KiCad_XY_rotation':[108,110.14,0],'manufacturer_lands_and_cutout_unchanged':True,'geometry':'Separate existing native Fusion connector/stake assembly retained; not duplicated in exported board STEP.'},
 'physical_fit_verified':False}
(HERE/'height-contract.json').write_text(json.dumps(height,indent=2)+'\n')
rows=[['J901','GCT','USB4720-03-A',1,'Purchased','SMT plus plated through-hole joints; all contacts/stakes require assembler finishing and inspection.'],['U901','Texas Instruments','TPD4E05U06DQAR',1,'Purchased','Factory USON reflow; confirm DQA0010B pad/stencil variant.'],['D901','Texas Instruments','TPD1E10B06DYAR',1,'Purchased','Factory SOD523 reflow; bidirectional clamp, use pin table rather than generic 3D band.'],['J902','','Custom six-wire harness',1,'Supplied / unresolved','Power24AWG,signals28AWG recommendation; wire insulation/OD, length, solder, bend and strain relief to be measured.']]
with (HERE/'manufacturing-diagnostic/purchasing-bom.csv').open('w',newline='')as f:w=csv.writer(f);w.writerow(['Reference','Manufacturer','MPN_or_description','Quantity','Category','Assembly_note']);w.writerows(rows)
with (HERE/'marking-legend.csv').open('w',newline='')as f:
 w=csv.writer(f);w.writerow(['Marking','Meaning']);w.writerows([['USB6','Six-wire USB input board'],['1','USB VBUS; square pad'],['2','Ground'],['3','CC1 to main J101 pin3'],['4','CC2 to main J101 pin4'],['5','D+ to main J101 pin5'],['6','D- to main J101 pin6'],['U901','Four-line signal ESD array'],['D901','VBUS ESD suppressor']])
# Nominal linear copper arithmetic; branches are deliberately all counted in series.
route=json.loads((HERE/'route-intent.json').read_text());by={}
for t in route['tracks']:
 d=by.setdefault(t['net'],{'sum_track_length_mm':0,'nominal_all_tracks_series_ohm_35um_20C':0})
 length=math.dist(*t['xy']);d['sum_track_length_mm']+=length;d['nominal_all_tracks_series_ohm_35um_20C']+=.017241*length/1000/(t['width']*.035)
(HERE/'copper-path-arithmetic.json').write_text(json.dumps({'net_sums':by,'method':'rho=.017241ohm mm2/m,35um nominal finished copper,20C; sums every branch in series. Not an extracted distributed DC network or thermal simulation.','bounds_not_established':['Minimum finished copper thickness','temperature rise','current split across paired connector contacts','contact, harness, plating and solder resistance','transient current/inductance'], 'physical_tested':False},indent=2)+'\n')
manifest={'schema':1,'frozen_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'release_state':'Engineering integration package; manufacturing and physical holds remain','native_checks':{'KiCad_version':drc['kicad_version'],'ERC_violations':0,'unconnected_items':0,'schematic_parity':0,'DRC_types':dict(types),'unique_ref_pin_mappings':len(actual),'footprints':4,'track_segments':len(tracks),'vias':len(children(a,'via')),'original_outline_unchanged':True,'original_GCT_pad_geometry_unchanged':True,'removed_Rd':['R901','R902'],'external_ESD_copper_bridges':bridges},
 'source_files':[file(p)for p in sorted(P.rglob('*'))if p.is_file() and not p.name.endswith(('.lck','.kicad_prl'))],
 'manufacturer_evidence':[file(ROOT/x)for x in ['hardware/pcb/usb-input/reference/GCT_USB4720_RevB_drawing.pdf','hardware/system-review/electrical/usb-protection-research/tpd4e05u06.pdf','hardware/system-review/electrical/usb-protection-research/tpd1e10b06.pdf']],
 'primary_fabrication_source':{'url':'https://jlcpcb.com/capabilities/pcb-capabilities/','accessed':'2026-09-07','quoted_or_ordered':False},
 'artifacts':[file(p)for p in sorted(HERE.rglob('*'))if p.is_file() and p.name!='audit.json' and 'before'not in p.relative_to(HERE).parts and '__pycache__'not in p.parts],
 'visual_review':['Final native top and bottom renders','Final schematic page: pin links and notes readable, no clipped title/comment'],
 'remaining_checks':['Quoted fabrication process or approved alternative GCT land/cutout geometry','Two-layer PTH annular and fine-USON assembly process','GCT slot, hole and routing tolerances','Panel/stencil/placement/reflow and through-hole finishing','STEP reimport and maximum-envelope Fusion check by CAD owner','Physical component/solder/harness bend clearance and removal','Load temperature, contact/harness drops, USB cable/source orientations and detach','Coordinated main protection, hot-plug transients and system ESD','Sealing and enclosure ingress'],
 'physical_testing_performed':False,'main_PCB_or_live_MCP_changed':False}
(HERE/'audit.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'board_sha256':sha(board),'schematic_sha256':sha(sch),'STEP_sha256':sha(HERE/'Trimix_USB_Input.step'),'audit_sha256':sha(HERE/'audit.json'),'native_checks':manifest['native_checks']},indent=2))
