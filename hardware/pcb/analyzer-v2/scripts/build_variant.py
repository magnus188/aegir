"""Reproducible PCB 2.0 layout candidate; never writes the reviewed source projects.

Run using the local KiCad Python environment. The only schematic differences in
this derived project are board/BOM membership and the USB receptacle's metadata.
All wire, label, junction, pin and symbol UUID/position data are preserved.
"""
from pathlib import Path
import sys, json, hashlib, shutil, re, uuid
import pcbnew as p

HERE = Path(__file__).resolve().parents[1]
ROOT = HERE.parents[2]
SRC = HERE.parent / 'analyzer'
USB = HERE.parent / 'usb-input'
sys.path.insert(0, str(ROOT / 'hardware/tools'))
from analyzer_sheet import sx, S, node, child, children, save

NAME = 'Trimix_Analyzer'
FP_USB = 'USB_C_Receptacle_GCT_USB4105_GF_A_GroundLandOffset025'
SYMBOLS = {}
USB_NOTES = {
    '01  SEALED USB-C RECEPTACLE': '01  USB-C RECEPTACLE | PCB 2.0',
    '03  SIX-WIRE OUTPUT': '03  NET REFERENCES | J101/J902 NOT FITTED',
    'TRIMIX USB INPUT | GCT + ESD + SIX-WIRE HARNESS': 'TRIMIX PCB 2.0 | USB-C + EXISTING ESD CIRCUIT',
    'Dedicated 0.60 +/-0.10 mm board. Nominal 5 V input; USB-C attachment and BC1.2 detection are on the main PCB.':
        'Integrated 4-layer / 1.6 mm main PCB. Nominal 5 V input; USB-C attachment and BC1.2 detection are unchanged.',
    'GCT Rev B cutout and fixed connector pose retained.\nNo copper-edge exception rule;0.10mm GCT edge margin needs closure.\nFactory stencil/reflow for U901; support the port internally.\nMeasure harness/solder height inside the 5.7 mm gap.\nNo 9/12/20 V request; tests required for both cable orientations,\nsource change, hot plug, overcurrent and assembled-port ESD.':
        'USB4105-GF-A top mount; no daughterboard or harness.\nGND lands offset 0.025 mm; supplier DFM approval pending.\nFactory stencil/reflow for U901 and connector shell slots.\nSupport the new PCB tab and socket inside the revised case.\nNo 9/12/20 V request; test both cable orientations, source\nchange, hot plug, temperature, overcurrent and port ESD.',
    '1 = USB_5V   2 = GND\n3 = CC1       4 = CC2\n5 = D+        6 = D-\nDo not swap CC/data wires or populate old Rd.':
        'Net references only: no harness is assembled in PCB 2.0.\n1 = USB_5V   2 = GND\n3 = CC1       4 = CC2       5 = D+       6 = D-\nNo extra Rd: existing TUSB320LAI supplies termination.',
}
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def mm(x, y): return p.VECTOR2I(p.FromMM(x), p.FromMM(y))
def xy(v): return tuple(p.ToMM(v))

def prepare():
    sources = [q for src in (SRC, USB) for q in src.rglob('*') if q.is_file()
               and q.suffix in ('.kicad_pcb', '.kicad_sch', '.kicad_pro', '.kicad_dru', '.kicad_mod')]
    hashes = {str(q.relative_to(ROOT)): sha(q) for q in sources}
    receipt = HERE / 'reference/original-source-hashes.json'
    if receipt.exists():
        assert json.loads(receipt.read_text()) == hashes, 'Reviewed source files changed: rebase/review explicitly.'
    else:
        receipt.write_text(json.dumps(hashes, indent=2) + '\n')
    for q in SRC.iterdir():
        if q.is_file() and (q.suffix in ('.kicad_sch', '.kicad_pro', '.kicad_dru', '.kicad_sym')
                           or q.name in ('fp-lib-table', 'sym-lib-table')):
            shutil.copy2(q, HERE/q.name)
        elif q.is_dir() and q.suffix in ('.pretty', '.3dshapes'):
            shutil.copytree(q, HERE/q.name, dirs_exist_ok=True)
    for suffix in ('.pretty', '.3dshapes'):
        shutil.copytree(USB/('Trimix_USB'+suffix), HERE/('Trimix_USB'+suffix), dirs_exist_ok=True)
    shutil.copy2(USB/'Trimix_USB.kicad_sym', HERE/'Trimix_USB.kicad_sym')
    for filename in ('fp-lib-table', 'sym-lib-table'):
        q = HERE/filename
        q.write_text(q.read_text().replace('${KIPRJMOD}/../usb-input/', '${KIPRJMOD}/'))
    table = sx.loads((HERE/'fp-lib-table').read_text())
    table.append(node('lib', node('name','USB4105'), node('type','KiCad'),
                      node('uri','${KIPRJMOD}/USB4105.pretty'), node('options',''),
                      node('descr','KiCad footprint checked against GCT USB4105 Rev B4')))
    save(HERE/'fp-lib-table', table)
    # Same thermal drilling and solder lands; larger copper annuli only.
    q=HERE/'Trimix_Power.pretty/TI_DSJ_14.kicad_mod'
    text=q.read_text()
    assert text.count('(size 0.4 0.4)')==15
    q.write_text(text.replace('(size 0.4 0.4)','(size 0.45 0.45)').replace(
        '0.4mm via lands and bottom tenting are engineering choices.',
        '0.45mm via lands; retain resin fill and copper cap. PCB 2.0 cost variant.'))
    for filename in ('Charging.kicad_sch', 'USB_Input.kicad_sch'):
        q=HERE/filename
        a=sx.loads(q.read_text())
        if filename=='USB_Input.kicad_sch':
            title=child(a,'title_block')
            child(title,'title')[1]='Trimix PCB 2.0 / integrated USB input and ESD'
            child(title,'rev')[1]='2.0-layout-01'
            child(title,'date')[1]='2026-10-15'
            next(v for v in children(title,'comment') if v[1]==2)[2]='Same electrical nets; new connector/layout require DFM and prototype verification.'
            for t in children(a,'text'):
                if t[1] in USB_NOTES:t[1]=USB_NOTES[t[1]]
        for s in children(a,'symbol'):
            props={v[1]:v for v in children(s,'property')}
            ref=props['Reference'][2]
            if ref=='J101':
                child(s,'on_board')[1]=S('no')
                child(s,'in_bom')[1]=S('no')
            if ref in ('J901','U901','D901'):
                child(s,'on_board')[1]=S('yes')
                child(s,'in_bom')[1]=S('yes')
                props['Assembly'][2]='Integrated main PCB 2.0'
                if ref=='J901':
                    for k,v in {'Value':'USB4105-GF-A','MPN':'USB4105-GF-A',
                                'Footprint':'USB4105:'+FP_USB,
                                'Datasheet':'https://gct.co/files/drawings/usb4105.pdf',
                                'Board thickness':'1.6 mm; top mount, plated shell slots',
                                'Interface':'GCT B4 / KiCad lands with 0.025 mm rearward GND land offset; DFM review required'}.items():
                        props[k][2]=v
                path=child(child(child(s,'instances'),'project'),'path')[1]
                SYMBOLS[ref]={'path':path+'/'+child(s,'uuid')[1],
                              'value':props['Value'][2], 'footprint':props['Footprint'][2],
                              'properties':{k:v[2] for k,v in props.items() if k not in ('Reference','Value','Footprint')}}
        # Preserve schematic topology, including the historical harness symbols
        # as excluded assembly references; their six net labels remain unchanged.
        save(q,a)
    with (HERE/(NAME+'.kicad_dru')).open('a') as rules:
        rules.write('''
# PCB 2.0 J901 only: JLC published NPTH-to-copper minimum 0.20 mm.
# GND lands moved rearward 0.025 mm (sizes/holes retained): measured 0.2187 mm.
# This is an explicit footprint adaptation, not a manufacturer approval.
(rule "USB4105 locating holes within adapted footprint"
 (condition "A.Type == 'Pad' && B.Type == 'Pad' && A.memberOfFootprint('J901') && B.memberOfFootprint('J901')")
 (constraint hole_clearance (min 0.20mm)))
''')

def initial_board():
    text=(SRC/(NAME+'.kicad_pcb')).read_text()
    # Change exactly the U201 thermal PTH lands (not the SMD thermal land/paste).
    a=sx.loads(text)
    for f in children(a,'footprint'):
        props={v[1]:v[2] for v in children(f,'property')}
        if props.get('Reference')=='U201':
            desc=child(f,'descr')
            if desc:desc[1]=desc[1].replace('0.4mm via lands and bottom tenting are engineering choices.',
                         '0.45mm via lands; retain resin fill and copper cap. PCB 2.0 cost variant.')
            changed=0
            for pad in children(f,'pad'):
                if str(pad[2])=='thru_hole' and pad[1]=='15':
                    assert child(pad,'size')[1:]==[.4,.4] and child(pad,'drill')[1:]==[.2]
                    child(pad,'size')[1:]=[.45,.45]; changed+=1
            assert changed==15
    for t in children(a,'segment'):
        uid=child(t,'uuid')[1]
        if uid in ('18845727-8852-493c-99fc-786af6a05a06','1b8f78a3-4291-426a-99ac-a5700ffa3132','a21ed52b-daff-45ba-9248-a2e63742ae14'):
            for endpoint in ('start','end'):
                pt=child(t,endpoint)
                if pt[1:]==[34.11651,80.89651]: pt[1:]=[34.16651,80.84651]
                if pt[1:]==[34.1007,80.89651]: pt[1:]=[34.1507,80.84651]
    outline=[(19.9,21.48491),(28.8,21.48491),(28.8,38.48491),
             (49.9,38.48491),(49.9,103),(65,103),(65,120.48491),
             (25.7,120.48491),(25.7,116.28491),(19.9,116.28491)]
    a[:]=[n for n in a if not (isinstance(n,list) and str(n[0]).startswith('gr_')
                               and child(n,'layer') and child(n,'layer')[1]=='Edge.Cuts')]
    for u,v in zip(outline,outline[1:]+outline[:1]):
        a.append(node('gr_line',node('start',*u),node('end',*v),
                      node('stroke',node('width',.05),node('type',S('default'))),
                      node('layer','Edge.Cuts'),node('uuid',str(uuid.uuid4()))))
    a[:]=[n for n in a if not (isinstance(n,list) and str(n[0])=='footprint'
          and any(q[1:3]==['Reference','J101'] for q in children(n,'property')))]
    # Redundant short D- spur entirely inside the removed J101.6 land.
    a[:]=[n for n in a if not (isinstance(n,list) and str(n[0])=='segment'
          and child(n,'uuid')[1]=='a38913a3-74e5-452c-a094-3e0a9727ca4d')]
    for z in children(a,'zone'):
        if child(z,'net') and child(z,'net')[1]=='GND':
            poly=child(z,'polygon')
            child(poly,'pts')[1:]=[node('xy',*u) for u in outline]
        if child(z,'name') and child(z,'name')[1]=='ROUTER_EDGE_RESERVE_1':
            # The previous right edge becomes internal to the integrated board.
            # Retain the existing edge reserve only above the added USB tab.
            for pt in child(child(z,'polygon'),'pts')[1:]:
                if pt[2]>103:pt[2]=103
    save(HERE/(NAME+'.kicad_pcb'),a)
    b=p.LoadBoard(str(HERE/(NAME+'.kicad_pcb')))
    b.GetTitleBlock().SetTitle('Trimix PCB 2.0 — integrated USB-C layout candidate')
    b.GetTitleBlock().SetRevision('2.0-layout-01')
    b.GetTitleBlock().SetComment(0,'Review candidate. Not released for manufacture; source circuits preserved.')
    b.GetTitleBlock().SetComment(1,'4 layers / 1.6 mm / 45.1 x 99 mm; filled/capped thermal vias retained.')
    return b

B=None
ADDED=[]
def net(name):
    n=B.FindNet(name)
    if n is None or n.GetNetCode()<0:
        n=p.NETINFO_ITEM(B,name);B.Add(n)
    return n
def track(name, pts, layer=p.F_Cu, width=.15):
    for u,v in zip(pts,pts[1:]):
        if u==v:continue
        t=p.PCB_TRACK(B);t.SetStart(mm(*u));t.SetEnd(mm(*v));t.SetWidth(p.FromMM(width));t.SetLayer(layer);t.SetNet(net(name));B.Add(t)
        ADDED.append(t.m_Uuid.AsString())
def via(name, pos, diameter=.6, drill=.3):
    v=p.PCB_VIA(B);v.SetPosition(mm(*pos));v.SetWidth(p.FromMM(diameter));v.SetDrill(p.FromMM(drill));v.SetViaType(p.VIATYPE_THROUGH);v.SetLayerPair(p.F_Cu,p.B_Cu);v.SetNet(net(name));B.Add(v)
    v.SetFrontTentingMode(p.TENTING_MODE_TENTED);v.SetBackTentingMode(p.TENTING_MODE_TENTED)
    ADDED.append(v.m_Uuid.AsString());return v
def component(ref, lib, fpname, pos, angle, pins):
    f=p.FootprintLoad(str(HERE/(lib+'.pretty')),fpname)
    assert f is not None
    f.SetReference(ref);f.SetValue(SYMBOLS[ref]['value']);f.SetFPID(p.LIB_ID(lib,fpname))
    f.SetPath(p.KIID_PATH(SYMBOLS[ref]['path']))
    f.SetSheetfile('USB_Input.kicad_sch');f.SetSheetname('12  USB INPUT + ESD')
    f.SetOrientationDegrees(angle);f.SetPosition(mm(*pos));f.Value().SetVisible(False)
    for k,v in SYMBOLS[ref]['properties'].items():
        f.SetField(k,v);f.GetField(k).SetVisible(False)
    B.Add(f)
    for pad in f.Pads():
        number=pad.GetNumber()
        if not number:continue
        name=pins[number]
        # Preserve separate schematic NC nets on the two unused SBU contacts.
        pad.SetNet(net(name))
    return f

def integrate():
    pins={n:'GND' for n in ['A1','A12','B1','B12','SH']}
    pins.update({n:'USB_5V' for n in ['A4','A9','B4','B9']})
    pins.update({'A5':'USB_CC1','B5':'USB_CC2','A6':'USB_D_P','B6':'USB_D_P',
                 'A7':'USB_D_M','B7':'USB_D_M','A8':'unconnected-(J901-SBU1-PadA8)',
                 'B8':'unconnected-(J901-SBU2-PadB8)'})
    component('J901','USB4105',FP_USB,(61.325,112),90,pins)
    component('U901','Trimix_USB','TI_DQA0010B',(54,112),0,
              dict(zip(map(str,[1,2,3,4,5,6,7,8,9,10]),
                       ['USB_CC1','USB_CC2','GND','USB_D_P','USB_D_M','USB_D_M','USB_D_P','GND','USB_CC2','USB_CC1'])))
    component('D901','Trimix_USB','TI_DYA0002A',(55.6,107.3),90,{'1':'USB_5V','2':'GND'})
    # Explicit PCB copper links: U901 NC pads are not internally connected.
    for name,y in [('USB_CC1',111),('USB_CC2',111.5),('USB_D_P',112.5),('USB_D_M',113)]:
        track(name,[(53.5825,y),(54.4175,y)])
    for end in [(52.95,112),(55.05,112)]:
        track('GND',[(54,112),end],width=.2);via('GND',end)
    track('GND',[(55.6,106.56),(55.6,105.8)],width=.3);via('GND',(55.6,105.8))
    # Retain all original core tracks. Ordinary through vias replace the removed
    # harness pad barrels at the existing copper endpoints.
    for x in (29.55,30.15,30.75):via('USB_5V',(x,118.48491))
    track('USB_5V',[(29.55,118.48491),(30.75,118.48491)],p.B_Cu,.6)
    track('USB_5V',[(29.55,118.48491),(30.75,118.48491)],p.F_Cu,.6)
    for name,x in [('USB_CC1',33.75),('USB_CC2',35.55),('USB_D_P',37.35),('USB_D_M',39.15)]:
        via(name,(x,118.48491),.5,.25)
    # Outer-layer power trunk: no thin inner-plane replacement for VBUS.
    track('USB_5V',[(30.15,118.48491),(30.15,119.45),(42.8,119.45),
                   (43.875,118.375),(45.4,118.375),(46.475,119.45),
                   (48.2,119.45),(51,116.65),(51,108),
                   (52.2,106.8),(53.26,106.8)],p.B_Cu,1.0)
    track('USB_5V',[(53.26,106.8),(54.6,108.14),(54.6,109.6),(56.3,109.6)],p.B_Cu,1.0)
    # VBUS ESD shunt is directly beside the connector-side VBUS via pair.
    track('USB_5V',[(55.6,108.04),(55.6,109.5),(55.7,109.6)],width=.4)
    # Both connector VBUS lands receive their own pair of ordinary vias.
    for y in (109.6,114.4):
        track('USB_5V',[(57.645,y),(56.8,y)],width=.4)
        track('USB_5V',[(56.8,y),(55.7,y)],width=.6)
        for x in (55.7,56.3):via('USB_5V',(x,y))
        track('USB_5V',[(55.7,y),(56.3,y)],p.B_Cu,.6)
    track('USB_5V',[(56.3,114.4),(56.3,113.75),(60,113.75),
                   (60,110.25),(56.3,110.25),(56.3,109.6)],p.B_Cu,1.0)
    # Connector-side signal fanout. Every signal reaches the matching U901 clamp
    # before the protected-side route; D+/D- are used only for BC1.2 detection.
    track('USB_CC2',[(54.4175,111.5),(54.7,111.5),(55,111.2),
                     (55.6,111.2),(56.55,110.25),(57.645,110.25)])
    track('USB_CC1',[(54.4175,111),(54.65,111),(55.05,110.6),(55.3,110.6)])
    track('USB_CC1',[(57.645,113.25),(58.3,113.25),(58.8,112.75)])
    for pos in [(55.3,110.6),(58.8,112.75)]:via('USB_CC1',pos,.5,.25)
    track('USB_CC1',[(55.3,110.6),(57.2,110.6),(57.6,111),
                     (57.6,112.2),(58.8,112.75)],p.In2_Cu)
    track('USB_D_P',[(54.4175,112.5),(54.7,112.5),(54.9,112.7),(56,112.7)])
    track('USB_D_P',[(57.645,112.25),(56.85,112.25),(56.4,112.7),(56,112.7)])
    track('USB_D_P',[(57.645,111.25),(56.4,111.25)])
    for pos in [(56,112.7),(56.4,111.25)]:via('USB_D_P',pos,.5,.25)
    track('USB_D_P',[(56,112.7),(55.8,112.5),(55.8,111.85),(56.4,111.25)],p.In2_Cu)
    track('USB_D_M',[(54.4175,113),(54.7,113),(55,113.3),
                     (55.15,113.3),(55.35,113.5)])
    track('USB_D_M',[(57.645,112.75),(56.95,112.75),(56.45,113.25),
                     (55.6,113.25),(55.35,113.5)])
    track('USB_D_M',[(57.645,111.75),(58.8,111.75)])
    for pos in [(55.35,113.5),(58.8,111.75)]:via('USB_D_M',pos,.5,.25)
    track('USB_D_M',[(58.8,111.75),(59.5,112.45),(59.5,114),
                     (57.5,114),(57.1,113.6),(55.35,113.6),(55.35,113.5)],p.In2_Cu)
    # Add local ground copper on the new tab. The original core zones remain.
    for layer in (p.F_Cu,p.B_Cu):
        z=p.ZONE(B);z.SetLayer(layer);z.SetNet(net('GND'));z.SetLocalClearance(p.FromMM(.2))
        z.SetPadConnection(p.ZONE_CONNECTION_FULL);z.SetMinThickness(p.FromMM(.2))
        poly=z.Outline();poly.NewOutline()
        for pt in [(49.9,103),(65,103),(65,120.48491),(49.9,120.48491)]:poly.Append(*mm(*pt))
        B.Add(z)

def main():
    global B
    prepare();B=initial_board();integrate()
    routes=json.loads((HERE/'scripts/signal-routes.json').read_text())
    layers={'F.Cu':p.F_Cu,'B.Cu':p.B_Cu,'In2.Cu':p.In2_Cu}
    for t in routes['tracks']:
        track(t['net'],t['points'],layers[t['layer']],t['width'])
    for v in routes['vias']:
        via(v['net'],v['position'],v['diameter'],v['drill'])
    B.BuildConnectivity()
    p.SaveBoard(str(HERE/(NAME+'.kicad_pcb')),B)
    (HERE/'reference/added-copper-uuids.json').write_text(json.dumps(ADDED,indent=2)+'\n')
    print('Built',HERE/(NAME+'.kicad_pcb'))

if __name__=='__main__':main()
