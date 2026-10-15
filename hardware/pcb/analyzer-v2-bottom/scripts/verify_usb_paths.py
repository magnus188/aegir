#!/usr/bin/env python3
"""Bounded independent USB path audit; no board mutation or operating-current rating.

Run using the isolated CAM environment (Gerbonara, sexpdata and Shapely).
Physical connectivity uses copper intersection and plated vias, never same-net
labels as electrical bridges. In particular, U901's NC pads are NOT internally
connected to its active protection pins.
"""
from pathlib import Path
import sys,json,math,hashlib,argparse
from datetime import datetime,timezone
ROOT=next(p for p in Path(__file__).resolve().parents if(p/'hardware/pcb/analyzer-v2').is_dir())
HERE=ROOT/'hardware/pcb/analyzer-v2-bottom';BASE=HERE.parent/'analyzer-v2'
sys.path.insert(0,str(ROOT/'hardware/system-review/electrical/main-final-independent'))
import sexpdata
from cam_geometry import Native,sub,subs,CU,unary_union,LineString,Point,box
sys.path.insert(0,str(HERE/'scripts'));import verify_bottom_variant as invariant

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def uid(raw):return sub(raw,'uuid')[0]
def copper(n):return {uid(r):(t,r)for t,r in list(zip(n.tracks,subs(n.raw,'segment')))+list(zip(n.vias,subs(n.raw,'via')))}

def cut_esd_bridges(n, new_ids=()):
    tracks=list(zip(n.tracks,subs(n.raw,'segment')));rows=[]
    for net,pin,ncpin in [('USB_CC1','1','10'),('USB_CC2','2','9'),('USB_D_P','4','7'),('USB_D_M','5','6')]:
        active=next(p for p in n.pads if p['ref']=='U901'and p['pin']==pin)
        nc=next(p for p in n.pads if p['ref']=='U901'and p['pin']==ncpin)
        bridge=[i for i,(t,r)in enumerate(tracks)if t['net']==net and t['layer']=='F.Cu'
                and t['geo'].intersects(active['geo'])and t['geo'].intersects(nc['geo'])]
        objects=[]
        for layer in CU:
            for p in n.pads_on(layer):
                if p['net']==net:objects.append(dict(id=p['id'],layer=layer,geo=p['geo'],ref=p['ref'],pin=p['pin'],plated=str(p['raw'][2])=='thru_hole'))
            for i,(t,r)in enumerate(tracks):
                if t['net']==net and t['layer']==layer and i not in bridge:objects.append(dict(id=uid(r),layer=layer,geo=t['geo']))
            for v in n.vias:
                if v['net']==net:objects.append(dict(id=v['id'],layer=layer,geo=v['geo'],plated=True))
        groups=list(range(len(objects)))
        def find(i):
            while groups[i]!=i:groups[i]=groups[groups[i]];i=groups[i]
            return i
        for i,x in enumerate(objects):
            for j,y in enumerate(objects[:i]):
                if (x['layer']==y['layer']and x['geo'].distance(y['geo'])<.000002)or(x.get('plated')and y.get('plated')and x['id']==y['id']):groups[find(i)]=find(j)
        def group_for(ref,pin=None):return {find(i)for i,p in enumerate(objects)if p.get('ref')==ref and(pin is None or p.get('pin')==pin)}
        port=group_for('J901');active_groups=group_for('U901',pin);nc_groups=group_for('U901',ncpin)
        core={find(i)for i,p in enumerate(objects)if p.get('ref')not in(None,'J901','U901')}
        port_shapes=[p for i,p in enumerate(objects)if find(i)in port]
        pg=unary_union([p['geo']for p in port_shapes]);bb=pg.bounds
        native_bounds=[bb[0]+n.origin[0],n.origin[1]-bb[3],bb[2]+n.origin[0],n.origin[1]-bb[1]]
        closest=[]
        for t,r in tracks:
            if uid(r) in new_ids and t['net'] in {'USB_CC1','USB_CC2','USB_D_P','USB_D_M'}:
                same_layer=unary_union([p['geo']for p in port_shapes if p['layer']==t['layer']])
                closest.append({'new_link_uuid':uid(r),'new_link_net':t['net'],'new_link_layer':t['layer'],
                    'minimum_XY_projection_gap_mm':t['geo'].distance(pg),
                    'minimum_same_layer_gap_mm':None if same_layer.is_empty else t['geo'].distance(same_layer)})
        nearest=min(closest,key=lambda q:q['minimum_XY_projection_gap_mm'])if closest else None
        # The frozen source runs connector -> NC land -> explicit bridge ->
        # active shunt pad -> controller. NC itself supplies no clamp function.
        good=(len(bridge)==1 and len(port)==len(core)==1 and not port&core and port==nc_groups and core==active_groups)
        rows.append(dict(net=net,active_pin=pin,nc_pin=ncpin,removed_bridge_uuids=[uid(tracks[i][1])for i in bridge],
            port_is_one_physical_component=len(port)==1,core_is_one_physical_component=len(core)==1,
            port_connected_to_nc_pad=port==nc_groups,core_connected_to_active_pad=core==active_groups,
            port_core_separated_after_cut=not bool(port&core),connector_side_copper_bounds_native_mm=native_bounds,nearest_new_signal_link=nearest,passed=good))
    return rows

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--variant-board',type=Path,default=HERE/'Trimix_Analyzer.kicad_pcb');ap.add_argument('--output',type=Path,default=HERE/'reference/verification/usb-path-review.json');args=ap.parse_args()
    paths=[BASE/'Trimix_Analyzer.kicad_pcb',args.variant_board,BASE/'reference/added-copper-uuids.json',Path(__file__)]
    start={str(p):sha(p)for p in paths};old,n=[Native(sexpdata.loads(p.read_text()))for p in paths[:2]]
    oc,nc=copper(old),copper(n);added=set(json.loads(paths[2].read_text()))
    local={i for i in added if min((sub(oc[i][1],'at')if str(oc[i][1][0])=='via'else sub(oc[i][1],k))[0]for k in(['at']if str(oc[i][1][0])=='via'else['start','end']))>=51.5}
    retired=added-local-invariant.BARRELS;new=set(nc)-set(oc)
    oldrows=cut_esd_bridges(old,retired);rows=cut_esd_bridges(n,new);checks=[]
    def check(name,passed,details=None):checks.append(dict(check=name,passed=bool(passed),details=details))
    check('Frozen source establishes all four explicit ESD bridge-cut paths',all(q['passed']for q in oldrows),oldrows)
    check('No new same-net bypass around any U901 shunt-routing node',all(q['passed']for q in rows),rows)
    # Test actual copper extent, not just route endpoint coordinates. Allow only
    # the track's half-width at the deliberately chosen y=110.3 centreline limit.
    forward=box(-1000,-1000,18.8-n.origin[0],n.origin[1]-110.3)
    excursions=[]
    for i in new:
        t,r=nc[i]
        if t['net']not in {'USB_CC1','USB_CC2','USB_D_P','USB_D_M'}:continue
        if str(r[0])=='segment':geo=LineString([t['a'],t['b']])
        else:geo=Point(t['xy'])
        if geo.intersection(forward.buffer(-.000002)).length>.000002 or(str(r[0])=='via'and forward.buffer(-.000002).contains(geo)):excursions.append(i)
    check('New signal link centrelines remain behind the connector-side local routing within the USB wing',not excursions,{'native_region':'x < 18.8 and y > 110.3 excluded for new CC/D centreline routing','violations':excursions})
    oldpower=[t for i,(t,r)in oc.items()if i in retired and str(r[0])=='segment'and t['net']=='USB_5V']
    power=[t for i,(t,r)in nc.items()if i in new and str(r[0])=='segment'and t['net']=='USB_5V']
    vias=[(i,t)for i,(t,r)in nc.items()if i in new and str(r[0])=='via'and t['net']=='USB_5V']
    details=[]
    for i,t in vias:
        both={layer:any(x['layer']==layer and x['geo'].distance(t['geo'])<.000002 for x in power)for layer in('F.Cu','B.Cu')}
        details.append(dict(uuid=i,position_CAM_mm=t['xy'],copper_mm=t['size'],drill_mm=t['drill'],connected_to_new_power_copper=both,passed=t['size']>=.6 and t['drill']>=.3 and all(both.values())))
    paired=len(vias)==2 and math.dist(vias[0][1]['xy'],vias[1][1]['xy'])<=1.0
    check('New VBUS transition has two >=0.6/0.3 mm plated vias genuinely connected on both outer layers',paired and all(q['passed']for q in details),details)
    check('All new VBUS segments are >=1mm wide and on outer copper',bool(power)and all(t['width']>=1 and t['layer']in('F.Cu','B.Cu')for t in power))
    # Ohmic sensitivity only: intentionally count the short parallel-link branch
    # as series copper, which overestimates its contribution. Via plating is a
    # declared process assumption, not a claim of measured finished plating.
    rho=1.724e-5;outer=.035;plating=.020;thickness=1.6
    oldlen=sum(math.dist(t['a'],t['b'])for t in oldpower);newlen=sum(math.dist(t['a'],t['b'])for t in power)
    oldr=sum(rho*math.dist(t['a'],t['b'])/(outer*t['width'])for t in oldpower)
    newr=sum(rho*math.dist(t['a'],t['b'])/(outer*t['width'])for t in power)
    via_r=[rho*thickness/(math.pi*t['drill']*plating)for i,t in vias]
    parallel_r=1/sum(1/x for x in via_r)if via_r else None
    estimate={'method':'20 C bulk-copper DC estimate; sum every new segment including branch; assume balanced parallel via connection; not ampacity or temperature-rise certification',
        'assumptions':{'copper_resistivity_ohm_mm':rho,'outer_copper_thickness_mm':outer,'via_barrel_plating_mm':plating,'barrel_length_mm':thickness},
        'old_link_length_mm':oldlen,'new_total_link_segments_mm':newlen,'old_trace_resistance_ohm':oldr,'new_trace_resistance_ohm':newr,'new_parallel_via_resistance_ohm':parallel_r,
        'scenarios':[]}
    if parallel_r is not None:
        for amps in(1.4,3):
            r=newr+parallel_r;estimate['scenarios'].append(dict(amps=amps,new_link_drop_V=amps*r,new_link_dissipation_W=amps*amps*r,old_link_drop_V=amps*oldr,old_link_dissipation_W=amps*amps*oldr,meaning='qualified-system setting envelope'if amps==1.4 else'sensitivity only; not an authorized 3A operating current'))
    check('New VBUS added link DC estimate is no worse than source predecessor under stated assumptions',parallel_r is not None and newr+parallel_r<=oldr,estimate)
    check('Input files remained stable during path review',start=={str(p):sha(p)for p in paths})
    result={'timestamp_utc':datetime.now(timezone.utc).isoformat(),'production_release':False,'input_sha256':start,'checks':checks,'passed':sum(c['passed']for c in checks),'failed':sum(not c['passed']for c in checks),
        'inherited_limits':[
          'Charging sheet still says 300 ohm gives ~0.57 A. This text is inherited and inconsistent with its unchanged R103 and with the correct note on USB_Input_Limiter. No schematic is edited.',
          'TI BQ25895 KILIM table 320/355/390 A-ohm and 300 ohm +/-1% give a conditional 1.056–1.313 A range (typical1.183 A); lower IINLIM register and upstream hardware limit can dominate; ILIM can be disabled by register. This is not a comprehensive fault-current bound.',
          'Upstream exact TPS22950CQDDCRQ1 limiter defaults to nominal50mA. The unchanged design requires qualified source plus authorization to raise that ceiling. J104 must remain open until cells, NTC and charging are validated.',
          'D+/D- are BC1.2 detection only, not USB payload data. Longer unmatched routes are not claimed USB high-speed compliant.',
          'The port remains a5V-only design. J104 inhibits charging, not all VBUS hot-plug/fault energy. Current-limit dynamics, transient coordination, ESD and full-load temperature still need physical verification.'
        ],
        'sources':[
          {'url':'https://www.ti.com/lit/ds/symlink/tpd4e05u06.pdf','basis':'RevO pin table4-2 (NC pins have no internal signal connection) and section7.4.1 connector placement/separation'},
          {'url':'https://www.ti.com/lit/ds/symlink/tpd1e10b06.pdf','basis':'RevG section7.4.1 short/wide ground-return guidance'},
          {'url':'https://www.ti.com/lit/ds/symlink/bq25895.pdf','basis':'RevC section6 and7.5 ILIM function and KILIM table'},
          {'file':'hardware/pcb/analyzer-v2/USB_Input_Limiter.kicad_sch','basis':'Unchanged source/permission/charging qualification restrictions and corrected300R range'}]}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'passed':result['passed'],'failed':result['failed'],'failures':[c['check']for c in checks if not c['passed']],'report':str(args.output)}))
    return bool(result['failed'])
if __name__=='__main__':sys.exit(main())
