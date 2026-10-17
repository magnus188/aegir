"""Check packed textures and organize existing scenes without re-rendering."""
from pathlib import Path
import json
import sys
import bpy

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from build_scene import organize_collections

out=HERE.parent
reports={}
for view in ('portrait','exploded'):
    file=out/f'aegir-{view}.blend'
    bpy.ops.wm.open_mainfile(filepath=str(file))
    objects=[ob for ob in bpy.data.objects if 'render_group' in ob]
    organize_collections(objects)
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
    reports[view]={
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
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(file),compress=True)
(out/'verification/scene-checks.json').write_text(json.dumps(reports,indent=2)+'\n')
print(json.dumps(reports,indent=2))
