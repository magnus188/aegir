from pathlib import Path
import sys, json, shutil, hashlib
import numpy as np
OUT=Path(__file__).resolve().parents[1]
PREVIOUS=Path('/Users/magnustrandokken/.codex/worktrees/internal-gasket/trimix-analysator/hardware/cad/rev04/internal-seal/fit-tests')
sys.path.insert(0,str(PREVIOUS/'scripts'))
import prepare_prints as Q
Q.ROOT=OUT
Q.P.PRINT=OUT/'bambu-work'

SMALL=[p['id'] for p in Q.load()['parts'] if p['id'] not in ['TMX-A3-P01','TMX-A3-P02','TMX-A3-P07']]
PLATES=[('01_Small_component_fit_checks',SMALL,.1),('03_Original_housing',['TMX-A3-P01'],.1),('04_Original_rear_lid',['TMX-A3-P02'],.1)]

def canonical(tri):
    # Sort vertices within each triangle, then triangles, retaining full 1-micron shape.
    pts=np.round(tri,6)
    order=np.lexsort((pts[:,:,2],pts[:,:,1],pts[:,:,0]),axis=1)
    a=np.take_along_axis(pts,order[:,:,None],axis=1).reshape(-1,9)
    return a[np.lexsort(tuple(a[:,i] for i in range(8,-1,-1)))]

def setup():
    (OUT/'profiles').mkdir(exist_ok=True)
    for name in ['h2d_04.json','fit_0.1.json','fit_0.16.json','pla_basic.json','gas-support-blockers.json']:
        shutil.copy2(PREVIOUS/'profiles'/name,OUT/'profiles'/name)
    previous={p['id']:p for p in json.loads((PREVIOUS/'verification/native-fit-parts.json').read_text())['parts']}
    report=[]
    for p in Q.load()['parts']:
        pid=p['id'];src=OUT/p['source_stl'];dst=OUT/'STL'/(pid+'.stl')
        r=Q.P.orient(src,dst,Q.P.IDENTITY)
        assert abs(r['after']['signed_volume_mm3']-p['volume_mm3'])/p['volume_mm3']<.001
        assert max(abs(x-y) for x,y in zip(r['after']['size_mm'],p['size_mm']))<.001
        reusable=pid not in ['TMX-A3-P01','TMX-A3-P02']
        if reusable:
            old=Q.P.read_stl(PREVIOUS/'STL'/(pid+'.stl'))
            new=Q.P.read_stl(dst)
            same=np.array_equal(canonical(old),canonical(new))
            assert same,('Reusable part geometry differs',pid)
            assert abs(previous[pid]['volume_mm3']-p['volume_mm3'])<.0001
        report.append({'id':pid,'reuse_previous_print':reusable,'mesh':r})
    Q.P.dump(OUT/'verification/reuse-and-meshes.json',{'parts':report,'reprint':['TMX-A3-P01','TMX-A3-P02'],'removed':['C01','C02','C03','C04','C05','C06'],'excluded_alternative':'D01','scale':1.0})
    manifold=OUT/'Bambu Studio/02_Sensor_manifold';manifold.mkdir(parents=True,exist_ok=True)
    shutil.copy2(PREVIOUS/'Bambu Studio/03_Sensor_manifold/03_Sensor_manifold.3mf',manifold/'02_Sensor_manifold.3mf')
    print('17 meshes checked; 15 existing prints are identical and reusable',flush=True)

def slice_all():
    record=OUT/'verification/single-plates.json'
    rows=json.loads(record.read_text()) if record.exists() else []
    for name,ids,height in PLATES:
        prior=next((r for r in rows if r['name']==name),None)
        if prior and Q.P.sha(OUT/prior['project'])==prior['sha256']:
            print('Already verified '+name,flush=True);continue
        print('Slicing '+name,flush=True)
        folder=OUT/'Bambu Studio'/name;folder.mkdir(parents=True,exist_ok=True)
        common=['--load-settings',str(OUT/'profiles/h2d_04.json')+';'+str(OUT/f'profiles/fit_{height:g}.json'),'--load-filaments',OUT/'profiles/pla_basic.json','--orient','0','--arrange','1','--ensure-on-bed']
        template=folder/'template.3mf'
        r=Q.P.run_cli(common+['--slice','0','--mstpp','600','--export-3mf',template.name,'--outputdir',folder]+[OUT/'STL'/(pid+'.stl') for pid in ids],folder/'template.log',900)
        assert r['exit_code']==0 and template.exists(),r
        # Original housing/lid have no rear gasket grooves and need no seal blockers.
        # Manifold uses its separate, previously verified gas-channel blockers.
        prepared=folder/'prepared-original.3mf';Q.modify_object_settings(template,prepared,ids)
        common[common.index('--arrange')+1]='0'
        project=folder/(name+'.3mf')
        r=Q.P.run_cli(common+['--slice','0','--mstpp','600','--export-3mf',project.name,'--outputdir',folder,prepared],folder/'slice.log',900)
        assert r['exit_code']==0 and project.exists(),r
        row=Q.verify_plate(name,ids,height)
        rows.append(row);Q.P.dump(record,rows)
        print(json.dumps(row),flush=True)

if __name__=='__main__':
    {'setup':setup,'slice':slice_all}[sys.argv[1]]()
