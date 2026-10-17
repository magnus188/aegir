"""Read-only checks of packed scenes; writes only the verification report."""
from pathlib import Path
import hashlib
import json
import bpy

HERE=Path(__file__).resolve().parent

out=HERE.parent
reports={}
for view in ('portrait','exploded'):
    file=out/f'aegir-{view}.blend'
    source_sha256=hashlib.sha256(file.read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(file))
    objects=[ob for ob in bpy.data.objects if 'render_group' in ob]
    missing=[]
    for im in bpy.data.images:
        if im.source=='FILE' and not im.packed_file and not Path(bpy.path.abspath(im.filepath)).exists():
            missing.append(im.filepath)
    if missing: raise RuntimeError('Missing scene textures: '+str(missing))
    sensor_ids=sorted({ob.get('sensor_id') for ob in objects if ob.get('sensor_id')})
    refs=sorted({ob.get('reference') for ob in objects if ob.get('reference')})
    if len(sensor_ids)!=4: raise RuntimeError('Expected four distinct sensors')
    if len(refs)!=6: raise RuntimeError('Expected all six supplemental PCB packages')
    scene=bpy.context.scene
    if hashlib.sha256(file.read_bytes()).hexdigest()!=source_sha256:
        raise RuntimeError('Source scene changed during read-only checks: '+str(file))
    reports[view]={
        'source_sha256':source_sha256,
        'source_file_unchanged':True,
        'geometry_objects':len(objects),
        'sensor_ids':sensor_ids,
        'supplemental_pcb_references':refs,
        'packed_file_images':sum(bool(im.packed_file) for im in bpy.data.images if im.source=='FILE'),
        'missing_textures':missing,
        'render_engine':scene.render.engine,
        'render_samples':scene.cycles.samples,
        'resolution':[scene.render.resolution_x,scene.render.resolution_y],
        'assembly_collections':sorted(c.name for c in bpy.data.collections if c.name.startswith('Assembly /')),
    }
(out/'verification/scene-checks.json').write_text(json.dumps(reports,indent=2)+'\n')
print(json.dumps(reports,indent=2))
