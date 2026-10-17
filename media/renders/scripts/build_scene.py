"""Build reproducible CAD-based Ægir studio photographs in Blender 4.2+.

blender -b --factory-startup --python media/renders/scripts/build_scene.py -- --draft
Source geometry and procedural details use the original assembly millimetres.
One root maps them to Blender metres, with the device standing upright.
"""
from pathlib import Path
import argparse
import json
import math
import sys

import bpy
from mathutils import Vector, Matrix
from bpy_extras.object_utils import world_to_camera_view

HERE = Path(__file__).resolve().parent
OUT = HERE.parent
sys.path.insert(0, str(HERE))


def material(name, color, roughness=.4, metallic=0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*color, 1)
    p.inputs['Roughness'].default_value = roughness
    p.inputs['Metallic'].default_value = metallic
    return m


def print_material(name, color):
    m = material(name, color, .38)
    n, l = m.node_tree.nodes, m.node_tree.links
    p = n.get('Principled BSDF')
    coord = n.new('ShaderNodeTexCoord')
    noise = n.new('ShaderNodeTexNoise')
    noise.inputs['Scale'].default_value = 18
    noise.inputs['Detail'].default_value = 2
    l.new(coord.outputs['Object'], noise.inputs['Vector'])
    bump = n.new('ShaderNodeBump')
    bump.inputs['Strength'].default_value = .15
    bump.inputs['Distance'].default_value = .000012
    l.new(noise.outputs['Fac'], bump.inputs['Height'])
    split=n.new('ShaderNodeSeparateXYZ'); l.new(coord.outputs['Object'],split.inputs[0])
    freq=n.new('ShaderNodeMath'); freq.operation='MULTIPLY'; freq.inputs[1].default_value=2*math.pi/.2
    l.new(split.outputs['Y'],freq.inputs[0])
    wave=n.new('ShaderNodeMath'); wave.operation='SINE'; l.new(freq.outputs[0],wave.inputs[0])
    layers=n.new('ShaderNodeBump'); layers.inputs['Strength'].default_value=.10; layers.inputs['Distance'].default_value=.000009
    l.new(wave.outputs[0],layers.inputs['Height']); l.new(bump.outputs['Normal'],layers.inputs['Normal'])
    l.new(layers.outputs['Normal'], p.inputs['Normal'])
    return m


def material_for(part, mats):
    name = (part['name'] + ' ' + part.get('description', '') + ' ' + part.get('source_name', '')).lower()
    pid = part['part_id']
    if part['render_group'] == 'logo': return mats['white']
    if '-F04' in pid or '-F05' in pid: return mats['brass']
    if part['render_group']=='fasteners': return mats['steel']
    if 'insert' in name: return mats['brass']
    if 'screw' in name: return mats['steel']
    if '18650' in name: return mats['cell']
    if 'rcy' in name: return mats['red']
    if 'button' in name and 'body' in name: return mats['steel']
    if 'button' in name and 'cap' in name: return mats['button']
    if 'nut' in name: return mats['steel']
    if 'front glass' in name: return mats['glass']
    if 'factory casing' in name: return mats['molded']
    if 'holder' in name: return mats['molded']
    if pid in ('TMX-A3-P03', 'TMX-V2-P12'): return mats['internal']
    if part['render_group'] == 'manifold': return mats['internal']
    return mats['shell']


def import_geometry(mats):
    data = json.loads((OUT / 'assets/geometry.json').read_text())
    obs = []
    for p in data['parts']:
        if p['part_id'].startswith('TMX-A3-C10'):
            # Connector envelopes have no verified owned mating part yet.
            continue
        mesh = bpy.data.meshes.new(p['name'])
        mesh.from_pydata(p['vertices'], [], p['faces'])
        mesh.update()
        mesh.set_sharp_from_angle(angle=math.radians(38))
        for poly in mesh.polygons: poly.use_smooth = True
        ob = bpy.data.objects.new(p['name'], mesh)
        bpy.context.collection.objects.link(ob)
        ob.data.materials.append(material_for(p, mats))
        ob['part_id'] = p['part_id']
        ob['render_group'] = p['render_group']
        ob['source_name'] = p.get('source_name', p['name'])
        ob['description'] = p.get('description','')
        ob['source_bounds'] = json.dumps(p['bounds'])
        obs.append(ob)
    return obs


def import_pcb():
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(OUT / 'assets/pcb.glb'))
    obs = list(set(bpy.data.objects) - before)
    # KiCad GLB is metre-based Y-up; Blender importer remaps to Z-up.
    # Registration is read from the independently checked geometry report.
    report = json.loads((OUT / 'verification/geometry.json').read_text())
    matrix = report.get('blender_import_to_cad_mm')
    if matrix is None:
        raise RuntimeError('Missing independently verified PCB transform')
    transform = Matrix(matrix)
    saved = {ob: ob.matrix_world.copy() for ob in obs}
    for ob in obs:
        ob.parent = None
        ob.matrix_world = transform @ saved[ob]
        ob['render_group'] = 'pcb'
        ob['source'] = 'KiCad native GLB export'
        if ob.type == 'MESH':
            for p in ob.data.polygons: p.use_smooth = True
            ob.data.set_sharp_from_angle(angle=math.radians(45))
    # KiCad leaves component metallicFactor unspecified, which glTF defines
    # as metallic=1. Correct plastics/ceramics; retain actual plated metals.
    for mat in {m for ob in obs if ob.type=='MESH' for m in ob.data.materials if m}:
        p=mat.node_tree.nodes.get('Principled BSDF') if mat.use_nodes else None
        if not p: continue
        c=p.inputs['Base Color'].default_value
        if max(c[:3])<.13 or (c[0]>.1 and c[0]>c[1]*1.5 and c[1]>c[2]*1.3):
            p.inputs['Metallic'].default_value=0
            p.inputs['Roughness'].default_value=.48
    return obs


def cylinder(name, loc, radius, depth, mat, axis='Z'):
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=radius, depth=depth, location=loc)
    ob = bpy.context.object
    ob.name = name
    if axis == 'Y': ob.rotation_euler.x = math.pi / 2
    if axis == 'X': ob.rotation_euler.y = math.pi / 2
    ob.data.materials.append(mat)
    for p in ob.data.polygons: p.use_smooth = len(p.vertices) == 4
    return ob


def battery_details(mats):
    obs = []
    for x in (16.2, 36.6):
        for y in (32.05, 100.95):
            ob = cylinder('18650 nickel end contact', (x,y,26.75), 7.7, .18, mats['steel'], 'Y')
            ob['render_group'] = 'battery'; obs.append(ob)
        ob = cylinder('18650 positive terminal', (x,101.05,26.75), 3.6, .25, mats['steel'], 'Y')
        ob['render_group'] = 'battery'; obs.append(ob)
    # Owned illuminated 12mm momentary switch: thin green LED annulus.
    bpy.ops.mesh.primitive_torus_add(major_radius=4.95, minor_radius=.28,
                                   major_segments=72, minor_segments=12,
                                   location=(86.34,113,31), rotation=(0,math.pi/2,0))
    ob=bpy.context.object; ob.name='Power switch green LED ring'
    ob.data.materials.append(mats['green']); ob['render_group']='shell'; obs.append(ob)
    return obs


def look_at(ob, target):
    ob.rotation_euler = (Vector(target) - ob.location).to_track_quat('-Z','Y').to_euler()


def organize_collections(objects):
    """Keep physical subassemblies selectable for subsequent animation work."""
    for ob in objects:
        name='Assembly / '+ob.get('render_group','shell').title()
        collection=bpy.data.collections.get(name)
        if collection is None:
            collection=bpy.data.collections.new(name)
            bpy.context.scene.collection.children.link(collection)
        for old in list(ob.users_collection): old.objects.unlink(ob)
        collection.objects.link(ob)


def area(name, pos, target, power, size, color):
    data=bpy.data.lights.new(name, 'AREA'); data.energy=power; data.shape='DISK'; data.size=size; data.color=color
    ob=bpy.data.objects.new(name,data); bpy.context.collection.objects.link(ob); ob.location=pos; look_at(ob,target)
    return ob


def studio():
    ground=material('Studio charcoal',(.024,.028,.035),.72)
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.0008))
    bpy.context.object.name='Seamless charcoal studio'; bpy.context.object.data.materials.append(ground)
    world=bpy.data.worlds.new('Soft charcoal environment'); world.use_nodes=True
    world.node_tree.nodes['Background'].inputs[0].default_value=(.13,.16,.21,1)
    world.node_tree.nodes['Background'].inputs[1].default_value=.28
    bpy.context.scene.world=world
    area('Large soft key',(-.22,.24,.36),(0,0,.09),1.8,.22,(.88,.93,1))
    area('Warm edge',(.18,-.18,.3),(0,0,.09),1.5,.16,(1,.9,.76))
    area('Front fill',(.22,.3,.16),(0,0,.09),.55,.22,(.85,.92,1))
    area('Top softbox',(-.03,-.02,.48),(0,0,.05),.8,.2,(1,1,1))
    camera=bpy.data.cameras.new('Product camera'); camera.lens=80; camera.clip_start=.005; camera.clip_end=1000
    ob=bpy.data.objects.new('Product camera',camera); bpy.context.collection.objects.link(ob)
    bpy.context.scene.camera=ob
    return ob


def fit_camera(camera, objects, direction, margin=.10):
    bpy.context.view_layer.update()
    points=[ob.matrix_world @ Vector(c) for ob in objects if ob.type in ('MESH','FONT','CURVE') and not ob.hide_render for c in ob.bound_box]
    lower=Vector(tuple(min(p[i] for p in points) for i in range(3)))
    upper=Vector(tuple(max(p[i] for p in points) for i in range(3)))
    target=(lower+upper)/2
    direction=Vector(direction).normalized()
    distance=(upper-lower).length*2.8
    scene=bpy.context.scene
    for _ in range(35):
        camera.location=target+direction*distance; look_at(camera,target); bpy.context.view_layer.update()
        projected=[world_to_camera_view(scene,camera,p) for p in points]
        extent=max(max(abs(p.x-.5),abs(p.y-.5)) for p in projected)
        desired=.5-margin
        ratio=extent/desired
        if abs(ratio-1)<.002: break
        distance*=max(.75,min(1.3,ratio))
    return {'location':list(camera.location),'target':list(target),'lens_mm':camera.data.lens}


def explode(objects):
    offsets={
        'shell': (0,0,0), 'logo': (0,0,0),
        'display': (-88,0,8),
        'frame': (28,0,45), 'pcb':(38,0,95),
        'battery':(-32,0,70),
        'manifold':(0,42,45),
        'sensors':(0,42,95),
        'cover':(80,0,165),
        'fasteners': (0,0,195),
    }
    for ob in objects:
        group=ob.get('render_group','shell')
        delta=Vector(offsets.get(group,(0,0,0)))
        name=ob.name.lower()
        pid=ob.get('part_id','')
        if pid=='TMX-A3-P08': delta=Vector((0,42,145))
        if group=='sensors' and ob.get('sensor_id')=='GYBMEP_BME280': delta+=Vector((7,17,5))
        if group=='fasteners':
            bounds=json.loads(ob.get('source_bounds','{}'))
            center=bounds.get('center',[(a+b)/2 for a,b in zip(bounds.get('min',[42,90,30]),bounds.get('max',[42,90,30]))])
            if '-F04' in pid or '-F05' in pid:
                # Heat-set inserts stay in their receiving printed component.
                delta=Vector((80,0,165)) if 'SIDE' in pid else Vector((0,42,45)) if center[1]>130 else Vector((0,0,0))
            elif 'SIDE' in pid: delta=Vector((102,0,165))
            elif center[1]>130: delta=Vector((0,42,167))
            elif pid=='TMX-A3-F03': delta=Vector((38,0,125))
            elif pid.startswith('TMX-V2-F02'): delta=Vector((28,0,65))
            elif pid=='TMX-A3-F02': delta=Vector((-88,0,32))
            else: delta=Vector((80,0,188))
        ob.location += delta
        ob['exploded_offset_mm']=list(delta)


def write_view_reports(path, reports):
    """Preserve metadata for views not rendered during this invocation."""
    previous = json.loads(path.read_text()) if path.is_file() else {}
    if not isinstance(previous, dict):
        raise RuntimeError('Expected a view-keyed render settings object: '+str(path))
    previous.update(reports)
    path.write_text(json.dumps(previous, indent=2)+'\n')


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--draft',action='store_true'); ap.add_argument('--view',choices=['portrait','exploded','both'],default='both'); ap.add_argument('--samples',type=int)
    args=ap.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    mats={
        'shell':print_material('Printed black polymer',(.018,.021,.025)),
        'internal':print_material('Printed internal graphite',(.026,.032,.038)),
        'white':material('Flush white logo polymer',(.82,.84,.86),.34),
        'molded':material('Molded black plastic',(.012,.016,.019),.32),
        'steel':material('Satin stainless steel',(.45,.48,.51),.25,.85),
        'brass':material('Brass heat-set inserts',(.42,.25,.067),.3,.78),
        'cell':material('Unbranded blue cell insulation',(.018,.16,.24),.31),
        'red':material('Battery connector red',(.5,.012,.007),.35),
        'glass':material('Black display cover glass',(.003,.006,.01),.19),
        'button':material('Black momentary cap',(.014,.018,.02),.28),
        'green':material('Power switch green LED',(.015,.4,.1),.3),
    }
    p=mats['green'].node_tree.nodes.get('Principled BSDF'); p.inputs['Emission Color'].default_value=(.01,.6,.09,1); p.inputs['Emission Strength'].default_value=.4
    objects=import_geometry(mats)+import_pcb()+battery_details(mats)
    from sensor_assets import build_sensors
    from display_assets import build_display_details
    objects += build_sensors()+build_display_details()
    try:
        from pcb_details import build_pcb_details
        objects += build_pcb_details()
    except ImportError:
        if not args.draft: raise
        print('DRAFT ONLY: PCB supplement not yet available')
    root=bpy.data.objects.new('Ægir CAD assembly — millimetres',None); bpy.context.collection.objects.link(root)
    root.scale=(.001,.001,.001); root.rotation_euler.x=math.pi/2; root.location.x=-.0425
    for ob in objects:
        ob.parent=root
        ob['assembled_location_mm']=list(ob.location)
    organize_collections(objects)
    camera=studio(); scene=bpy.context.scene
    scene.render.engine='CYCLES'; scene.cycles.samples=args.samples or (32 if args.draft else 160)
    scene.cycles.use_denoising=True; scene.cycles.preview_samples=16
    scene.cycles.max_bounces=8
    # Blender 4.2's Metal kernel compiler aborts on this host OS. CPU Cycles
    # gives the same materials and path-traced lighting without that driver issue.
    scene.cycles.device='CPU'
    scene.view_settings.view_transform='AgX'; scene.view_settings.look='AgX - Medium High Contrast'
    scene.view_settings.exposure=0
    scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGB'; scene.render.image_settings.color_depth='8'
    scene.render.film_transparent=False
    scene.render.resolution_percentage=40 if args.draft else 100
    scene.unit_settings.system='METRIC'; scene.unit_settings.length_unit='MILLIMETERS'
    reports={}
    for view in ['portrait','exploded']:
        if view=='exploded': explode(objects)
        if args.view not in (view,'both'): continue
        scene.render.resolution_x,scene.render.resolution_y=(2400,3000) if view=='portrait' else (3200,2400)
        direction=(.62,1,.30) if view=='portrait' else (-.9,-1,.6)
        reports[view]=fit_camera(camera,objects,direction,.11 if view=='portrait' else .08)
        if view=='exploded':
            # Wider softboxes illuminate the separated assembly from its rear.
            area('Exploded broad rear key',(-.28,-.42,.43),(-.03,-.09,.10),2,.32,(.93,.96,1))
            area('Exploded right fill',(.32,-.20,.25),(0,-.10,.11),1.2,.25,(1,.92,.82))
        folder=OUT/('previews' if args.draft else 'masters'); folder.mkdir(exist_ok=True,parents=True)
        scene.render.filepath=str(folder/f'aegir-{view}.png')
        # Every still has an independently editable scene with packed textures.
        if not args.draft:
            bpy.ops.file.pack_all()
            bpy.ops.wm.save_as_mainfile(filepath=str(OUT/f'aegir-{view}.blend'),compress=True)
        bpy.ops.render.render(write_still=True)
        reports[view]['resolution']=[scene.render.resolution_x,scene.render.resolution_y]
        reports[view]['samples']=scene.cycles.samples
    write_view_reports(OUT/'verification'/('draft-settings.json' if args.draft else 'render-settings.json'), reports)


if __name__=='__main__': main()
