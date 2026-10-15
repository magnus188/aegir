"""Restore the pre-gasket baseline without modifying the preserved seal design."""
import adsk, adsk.core as C, adsk.fusion as F
from pathlib import Path
import json, hashlib

OUT=Path(__file__).resolve().parents[1]
SOURCE=Path('/Users/magnustrandokken/.codex/worktrees/internal-gasket/trimix-analysator/hardware/cad/rev04/3d-print')
NAME='Trimix_Enclosure_A3_Original'

def write(name,data):
    (OUT/'verification'/name).write_text(json.dumps(data,indent=2))

def open_original():
    app=C.Application.get()
    seal=next(d for d in app.documents if d.name.startswith('Trimix_Enclosure_A3_InternalSeal v'))
    assert seal.isSaved and not seal.isModified and seal.dataFile.isComplete
    folder=seal.dataFile.parentFolder
    assert not any(f.name==NAME for f in folder.dataFiles), 'Original variant already exists; inspect before retry'
    write('preserved-gasket.json',{'name':seal.name,'id':seal.dataFile.id,'version':seal.dataFile.versionNumber,'complete':seal.dataFile.isComplete,'editable':True})
    path=SOURCE/'Trimix_Enclosure_A3_PrintReview.f3d'
    doc=app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(str(path)))
    assert doc
    doc.name=NAME
    d=F.Design.cast(doc.products.itemByProductType('DesignProductType'))
    assert d.computeAll()
    assert d.userParameters.itemByName('CaseDepth').expression=='43 mm'
    assert not any(p.name.startswith('Seal') for p in d.userParameters)
    expected=json.loads((SOURCE/'verification/print-mesh-manifest.json').read_text())['parts']
    rows=[]
    for p in expected:
        c=next(c for c in d.allComponents if c.partNumber==p['part_id'])
        assert c.bRepBodies.count==1
        b=c.bRepBodies.item(0)
        assert abs(b.volume*1000-p['volume_mm3'])<.01,(p['part_id'],b.volume*1000,p['volume_mm3'])
        rows.append({'id':p['part_id'],'volume_mm3':b.volume*1000,'matches_original':True})
    issues=[]
    for i in range(d.timeline.count):
        e=d.timeline.item(i).entity
        if hasattr(e,'healthState') and e.healthState!=F.FeatureHealthStates.HealthyFeatureHealthState:
            issues.append({'index':i,'name':e.name,'message':e.errorOrWarningMessage})
    assert not issues,issues
    d.rootComponent.attributes.add('TrimixVariant','basis','Pre-gasket PrintReview archive; internal gasket deferred 2026-10-15')
    cam=app.activeViewport.camera
    cam.cameraType=C.CameraTypes.OrthographicCameraType
    cam.viewOrientation=C.ViewOrientations.IsoTopRightViewOrientation
    cam.isFitView=True;cam.isSmoothTransition=False
    app.activeViewport.camera=cam
    adsk.doEvents();app.activeViewport.refresh()
    assert d.exportManager.execute(d.exportManager.createFusionArchiveExportOptions(str(OUT/(NAME+'.f3d'))))
    assert d.exportManager.execute(d.exportManager.createSTEPExportOptions(str(OUT/(NAME+'.step'))))
    assert doc.saveAs(NAME,folder,'Current original enclosure, before gasket changes. InternalSeal retained as a separate future design.','')
    write('original-source.json',{'source_archive':str(path),'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'source_preserved':True,'parts':rows,'timeline':d.timeline.count,'health':issues})
    print('ORIGINAL_SAVED',doc.name,d.timeline.count)

def finish():
    app=C.Application.get()
    doc=next(d for d in app.documents if d.name.startswith(NAME+' v'))
    assert doc.isSaved and not doc.isModified and doc.dataFile.isComplete
    for x in list(app.documents):
        if x.name=='Trimix_Original_FitTests':
            assert (OUT/'Trimix_Original_FitTests.f3d').stat().st_size>10000
            assert x.close(False)
        elif x.name.startswith('Trimix_Enclosure_A3_InternalSeal v'):
            assert x.isSaved and not x.isModified and x.dataFile.isComplete
            assert x.close(False)
    doc.activate()
    write('fusion-saved.json',{'name':doc.name,'id':doc.dataFile.id,'version':doc.dataFile.versionNumber,'complete':doc.dataFile.isComplete,'modified':doc.isModified,'project':doc.dataFile.parentProject.name})
    print('ORIGINAL_READY',json.dumps({'name':doc.name,'id':doc.dataFile.id,'version':doc.dataFile.versionNumber,'complete':doc.dataFile.isComplete}))
