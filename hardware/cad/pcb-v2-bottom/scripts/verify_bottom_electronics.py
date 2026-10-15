"""Independent static electrical-part fit audit using frozen pre-change geometry.

No case-builder imports. Transform the independently qualified predecessor's
model/maximum-height inputs, then check the actual final exported print parts.
This depends on the separate PCB preservation audit proving all main placement
and component identities unchanged and the three USB poses as specified.
"""
from pathlib import Path
import argparse,hashlib,json,re,sys
import cadquery as cq
ROOT=next(p for p in Path(__file__).resolve().parents if (p/'hardware/pcb/analyzer-v2').exists())
sys.path.insert(0,str(ROOT/'hardware/cad/pcb-v2/scripts'))
from step_inputs import load_named_step,bounds
from component_clearance import (populated_step_parts,ref_of,build_keepouts,
    full_component_clearance,build_tail_keepouts,EXPECTED_MISSING)
OLD=ROOT/'hardware/pcb/analyzer-v2'
OUT=ROOT/'hardware/cad/pcb-v2-bottom'
OLD_SHIFT=(50.4,120,20.5529)
USB={'J901','U901','D901'}
CORE_SHIFT=(0,-18.9,3.5)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def move(s,ref):
    if ref in USB:
        return s.translate((-95.5,-29.48491,0)).rotate((0,0,0),(0,0,1),-90).translate((42.5,1.3,3.5))
    return s.translate(CORE_SHIFT)
def box(lo,hi):return cq.Solid.makeBox(*(b-a for a,b in zip(lo,hi)),cq.Vector(*lo))
def overlap(a,b):
    aa,bb=bounds(a),bounds(b)
    if any(min(aa['max'][i],bb['max'][i])-max(aa['min'][i],bb['min'][i])<1e-7 for i in range(3)):return 0.,True
    common=a.intersect(b)
    return sum(abs(s.Volume())for s in common.Solids()),common.isValid()
def part_id(p):
    m=re.search(r'TMX-(?:A3|V2)-P\d+',p['path'])
    return m[0] if m else None
def component_ref(p):
    m=re.search(r'PCB(?:2|B|_BOTTOM)?[-_]([A-Z]+\d+)(?:\b|[_ /])',p['path'])
    return m[1] if m else None
def main():
    ap=argparse.ArgumentParser();ap.add_argument('assembly');ap.add_argument('--out',default=str(OUT/'verification/electronics-fit-independent.json'));args=ap.parse_args()
    assembly=Path(args.assembly).resolve()
    inputs=[assembly,OLD/'Trimix_Analyzer.kicad_pcb',OLD/'review/Trimix_PCB_V2_Mechanical.step',
       ROOT/'hardware/cad/pcb-v2/verification/component-height-contract.json',
       ROOT/'hardware/pcb/analyzer-v2-bottom/Trimix_Analyzer.kicad_pcb']
    hashes={str(p.relative_to(ROOT)):sha(p)for p in inputs}
    if hashes[str((OLD/'Trimix_Analyzer.kicad_pcb').relative_to(ROOT))]!='1df55664b1f57ec4dcc38bbd2358f5aee306f59696f52271aafb94f233b992bc':raise RuntimeError('Predecessor changed')
    assembly_parts=load_named_step(assembly);printed={}
    for p in assembly_parts:
        pid=part_id(p)
        if pid:
            if pid in printed:raise RuntimeError('Duplicate print part '+pid)
            printed[pid]=p['shape']
    if not {'TMX-A3-P01','TMX-A3-P02','TMX-A3-P03','TMX-V2-P12'}<=printed.keys():raise RuntimeError('Missing case parts')
    old=populated_step_parts(load_named_step(OLD/'review/Trimix_PCB_V2_Mechanical.step'))
    nominal=[];expected={}
    for p in old:
        ref=ref_of(p)
        if ref is None:continue
        shape=move(p['shape'].translate(OLD_SHIFT),ref)
        nominal.append({'reference':ref,'kind':'preserved_actual_model','shape':shape})
        expected.setdefault(ref,[]).append(shape)
    # Use the actual final exported substrate, not an approximate blank.
    substrates=[p for p in assembly_parts if re.search(r'PCB.*substrate',p['path'],re.I)]
    if len(substrates)!=1:raise RuntimeError('Expected one final substrate: '+str([p['path']for p in substrates]))
    nominal.append({'reference':'PCB_substrate','kind':'final_exported_substrate','shape':substrates[0]['shape']})
    for q in build_keepouts():
        if q['missing_from_step']:
            nominal.append({'reference':q['reference'],'kind':'missing_model_maximum_and_assembly',
                'shape':move(box(*q['source_contract_bounds_mm']),q['reference'])})
    envelopes=[]
    for q in full_component_clearance(old)+build_tail_keepouts(normalized_step_parts=old):
        envelopes.append({**q,'shape':move(q['shape'],q['reference'])})
    actual={}
    for p in assembly_parts:
        ref=component_ref(p)
        if ref:actual.setdefault(ref,[]).append(p['shape'])
    pose=[]
    for ref,ss in expected.items():
        if ref not in actual:raise RuntimeError('Missing modeled fitted ref '+ref)
        a=bounds(cq.Compound.makeCompound(ss));b=bounds(cq.Compound.makeCompound(actual[ref]))
        delta=max(abs(a[k][i]-b[k][i])for k in ('min','max')for i in range(3))
        pose.append({'reference':ref,'max_bounds_delta_mm':delta,'pass':delta<.001})
    results={}
    for label,items in [('nominal',nominal),('clearance',envelopes)]:
        hits=[];invalid=[];pairs=0
        for q in items:
            for pid,s in printed.items():
                pairs+=1;v,valid=overlap(q['shape'],s)
                item={'reference':q['reference'],'part_id':pid,'volume_mm3':v,'tail_index':q.get('tail_index')}
                if not valid:invalid.append(item)
                if v>1e-7:hits.append(item)
        results[label]={'pairs':pairs,'intersections':hits,'invalid_common_operations':invalid,'pass':not hits and not invalid}
        print(label,results[label]['pass'],len(hits),flush=True)
    r={'input_sha256':hashes,'inputs_unchanged':all(sha(ROOT/p)==h for p,h in hashes.items()),
       'scope':'Static component and clearance-envelope fit plus independent assembly pose check. No case builder imports.',
       'nominal':results['nominal'],'clearance':results['clearance'],'pose_checks':pose,
       'modeled_fitted_refs':len(expected),'missing_fitted_refs':sorted(EXPECTED_MISSING),
       'print_part_ids':sorted(printed),'core_shift_mm':CORE_SHIFT,'nominal_board_planes_mm':[24,25.6],
       'limitations':['Must accompany exact independent PCB preservation/routing checks.',
        'Six absent models use existing documented maximum/assembly allocations.',
        'J901 uses actual model plus0.15mm assembly then0.5mm clearance; physical cable/component tolerance remains to be tested.',
        'No strength, thermal, cable-mate, water-seal or assembly-path qualification.']}
    r['pass']=r['inputs_unchanged'] and all(q['pass'] for q in pose) and all(q['pass'] for q in results.values())
    destination=Path(args.out);destination.parent.mkdir(parents=True,exist_ok=True);destination.write_text(json.dumps(r,indent=2)+'\n')
    print(json.dumps({'pass':r['pass'],'report':str(destination)}),flush=True)
if __name__=='__main__':main()
