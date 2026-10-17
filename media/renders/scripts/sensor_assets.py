"""Appearance reconstructions of the four installed sensors, in CAD millimeters.

build_sensors() does not clear the scene, add lights, change units, or render.
The caller may parent every returned object to a scene transform.  All surfaces
and labels are authored in original assembly coordinates and all objects carry
render_group='sensors' and sensor_id.  See ../references/sensors/sources.json.
These visual meshes are intentionally separate from manufacturing geometry.
"""
from pathlib import Path
import math
import bpy
from mathutils import Vector, Matrix

_OUT = []
_ID = ""
_REFS = Path(__file__).resolve().parents[1] / "references" / "sensors"


def _material(name, color, metallic=0, roughness=.4):
    name = "Sensor_" + name
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.diffuse_color = (*color, 1)
    bs = mat.node_tree.nodes.get("Principled BSDF")
    bs.inputs["Base Color"].default_value = (*color, 1)
    bs.inputs["Metallic"].default_value = metallic
    bs.inputs["Roughness"].default_value = roughness
    if "Coat Weight" in bs.inputs:
        bs.inputs["Coat Weight"].default_value = .025 if metallic == 0 else 0
    return mat


def _label_material(name, filename):
    mat = _material(name, (.94, .94, .91), roughness=.57)
    nodes = mat.node_tree.nodes
    tex = nodes.get("Reference label") or nodes.new("ShaderNodeTexImage")
    tex.name = "Reference label"
    tex.image = bpy.data.images.load(str(_REFS / filename), check_existing=True)
    mat.node_tree.links.new(tex.outputs["Color"], nodes.get("Principled BSDF").inputs["Base Color"])
    return mat


def _register(ob, name, mat=None):
    ob.name = "Sensor_" + _ID + "_" + name
    ob["render_group"] = "sensors"
    ob["sensor_id"] = _ID
    ob["geometry_basis"] = "manufacturer-reference visual reconstruction"
    if mat:
        ob.data.materials.append(mat)
    _OUT.append(ob)
    return ob


def _smooth(ob):
    if ob.type == 'MESH':
        for p in ob.data.polygons:
            p.use_smooth = True
    return ob


def _bevel(ob, width=.1, segments=3):
    mod = ob.modifiers.new("Soft manufactured edges", 'BEVEL')
    mod.width = width
    mod.segments = segments
    mod.limit_method = 'ANGLE'
    norm = ob.modifiers.new("Face normals", 'WEIGHTED_NORMAL')
    norm.keep_sharp = True
    return ob


def _box(name, center, size, mat, bevel=.07):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    ob = _register(bpy.context.object, name, mat)
    ob.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        _bevel(ob, bevel)
    return ob


def _cyl(name, center, radius, depth, mat, axis='X', vertices=96, bevel=.05):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=center)
    ob = _register(bpy.context.object, name, mat)
    if axis == 'X':
        ob.rotation_euler[1] = math.pi / 2
    elif axis == 'Y':
        ob.rotation_euler[0] = math.pi / 2
    _smooth(ob)
    if bevel:
        _bevel(ob, bevel, 2)
    return ob


def _ring_x(name, x, cy, cz, outer, inner, depth, mat, vertices=96):
    verts = []
    for xx, rad in [(x-depth/2, outer),(x+depth/2, outer),(x-depth/2, inner),(x+depth/2, inner)]:
        verts.extend((xx, cy+rad*math.cos(i*2*math.pi/vertices), cz+rad*math.sin(i*2*math.pi/vertices)) for i in range(vertices))
    faces=[]
    for i in range(vertices):
        j=(i+1)%vertices
        faces.extend([(i,j,vertices+j,vertices+i),(2*vertices+i,3*vertices+i,3*vertices+j,2*vertices+j),
                      (i,2*vertices+i,2*vertices+j,j),(vertices+i,vertices+j,3*vertices+j,3*vertices+i)])
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    ob=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(ob)
    _register(ob,name,mat);_smooth(ob)
    return ob


def _poly_x(name, x0, x1, section, mat, label=False):
    n=len(section)
    verts=[(x,y,z) for x in (x0,x1) for y,z in section]
    faces=[tuple(reversed(range(n))),tuple(range(n,n*2))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    ob=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(ob)
    _register(ob,name,mat)
    for p in mesh.polygons:
        p.use_smooth=p.index>=2
    if label:
        uv=mesh.uv_layers.new(name='Label coordinates')
        lengths=[0]
        for i in range(n):
            y,z=section[i]; yy,zz=section[(i+1)%n]
            lengths.append(lengths[-1]+math.hypot(y-yy,z-zz))
        for p in mesh.polygons:
            if p.index<2:
                for li in p.loop_indices: uv.data[li].uv=(.01,.01)
                continue
            i=p.index-2
            for li,coord in zip(p.loop_indices,[(lengths[i]/lengths[-1],0),(lengths[i+1]/lengths[-1],0),(lengths[i+1]/lengths[-1],1),(lengths[i]/lengths[-1],1)]):
                uv.data[li].uv=coord
    return ob


def _cylinder_label(name, x0,x1,cy,cz,radius,mat):
    section=[(cy+radius*math.cos(i*2*math.pi/192),cz+radius*math.sin(i*2*math.pi/192)) for i in range(192)]
    return _poly_x(name,x0,x1,section,mat,label=True)


def _curve(name, points, radius, mat):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D'
    curve.resolution_u=16;curve.bevel_depth=radius;curve.bevel_resolution=3
    sp=curve.splines.new('POLY');sp.points.add(len(points)-1)
    for p,co in zip(sp.points,points):p.co=(*co,1)
    ob=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(ob)
    return _register(ob,name,mat)


def _text(name, value, point, size, mat, u=(1,0,0),v=(0,1,0)):
    curve=bpy.data.curves.new(name,'FONT');curve.body=value
    curve.size=size;curve.extrude=.002;curve.align_x='CENTER';curve.align_y='CENTER'
    curve.resolution_u=6
    ob=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(ob)
    u=Vector(u);v=Vector(v);n=u.cross(v)
    ob.matrix_world=Matrix(((u.x,v.x,n.x,point[0]),(u.y,v.y,n.y,point[1]),(u.z,v.z,n.z,point[2]),(0,0,0,1)))
    return _register(ob,name,mat)


def _ao2(m):
    global _ID
    _ID='AO2'
    y,z=142.5,21.75
    # 31.75 total shoulder-to-rear length includes the smaller rear hex boss.
    _cyl('red_ABS_barrel',(51.2,y,z),14.55,24.4,m['red'],bevel=.32)
    _cyl('shoulder_flange',(39.35,y,z),14.65,.7,m['red'],bevel=.12)
    _cyl('rear_seam_flange',(63.45,y,z),14.65,.7,m['red'],bevel=.12)
    _cyl('rear_hex_connector_boss',(67.15,y,z),10,7.2,m['red'],vertices=6,bevel=.2)
    _cyl('thread_nose_core',(35.75,y,z),7.5,6.5,m['red'],bevel=.15)
    _ring_x('black_shoulder_seal',38.6,y,z,8.5,7.5,.65,m['black'])
    pts=[]
    for i in range(481):
        t=i/480*5.25*math.tau
        pts.append((32.8+5.25*i/480,y+7.78*math.cos(t),z+7.78*math.sin(t)))
    _curve('M16x1_thread_crest',pts,.22,m['red'])
    _ring_x('sensing_nose_rim',32.63,y,z,7.7,4.9,.26,m['red'])
    _cyl('sensing_membrane',(32.5,y,z),4.8,.06,m['membrane'],bevel=0)
    # Seven gas apertures visible through the molded nose face.
    for i in range(7):
        th=i*math.tau/7
        _cyl('gas_port_%02d'%i,(32.45,y+2.6*math.cos(th),z+2.6*math.sin(th)),.46,.04,m['dark'],bevel=0)
    _cylinder_label('white_identification_wrap',43.1,60.8,y,z,14.61,m['ao2label'])
    # Actual connector construction replaces the old opaque clearance block.
    _box('Molex_header_base',(71.35,y,z),(1.2,8.4,4.5),m['ivory'],.12)
    _box('Molex_friction_lock',(72.2,y,z-2.0),(2.7,8.4,.8),m['ivory'],.08)
    for i in range(3):
        _box('Molex_square_contact_%d'%i,(73.3,y+(i-1)*2.54,z),(3.0,.64,.64),m['gold'],.04)


def _md62(m):
    global _ID
    _ID='MD62'
    cy,cz=167.75,17.5
    def stadium(r=4.75):
        return [(cy+r*math.cos(t),cz+4.75+r*math.sin(t)) for t in [i*math.pi/48 for i in range(49)]]+[(cy+r*math.cos(t),cz-4.75+r*math.sin(t)) for t in [math.pi+i*math.pi/48 for i in range(49)]]
    _poly_x('sintered_metal_capsule',19,33,stadium(),m['bronze'])
    _poly_x('paper_identification_band',20.0,31.7,stadium(4.765),m['mdlabel'],label=True)
    # Both feedthroughs are on the rear metal face; sensing element uses black mark.
    for j,zz in enumerate((12.75,22.25)):
        _cyl('green_epoxy_feedthrough_%d'%j,(33.03,cy,zz),3.15,.08,m['epoxy'],bevel=0)
        _ring_x('feedthrough_metal_rim_%d'%j,33.05,cy,zz,3.45,3.15,.12,m['bronze'])
        for k,yy in enumerate((cy-1.25,cy+1.25)):
            _cyl('lead_solder_joint_%d_%d'%(j,k),(33.18,yy,zz),.6,.36,m['solder'],bevel=.06)
            _cyl('unformed_gold_lead_%d_%d'%(j,k),(46.5,yy,zz),.23,27,m['gold'],bevel=.02,vertices=16)
    _box('detector_square_mark',(31.83,cy+4.76,23.3),(1.25,.025,1.25),m['black'],.01)


def _ze07(m):
    global _ID
    _ID='ZE07_CO'
    y,z=145,16.6
    _box('green_module_PCB',(9.75,145,16.6),(1.6,25.4,22.4),m['pcb'],.14)
    _box('FR4_visible_edge',(9.74,132.32,16.6),(1.48,.05,22.15),m['fr4'],0)
    _cyl('blue_electrochemical_cell',(18.9,y,z),9.87,16.7,m['blue'],bevel=.26)
    _cyl('blue_base_lip',(11.15,y,z),10,1.2,m['blue'],bevel=.12)
    _cylinder_label('cell_identification_wrap',12.0,24.4,y,z,9.91,m['colabel'])
    _cyl('blue_cap_face',(26.71,y,z),9.85,.9,m['blue'],bevel=.14)
    _ring_x('cap_raised_circumference',27.16,y,z,9.85,9.30,.18,m['blue'])
    _cyl('central_gas_aperture',(27.26,y,z),.24,.02,m['dark'],bevel=0,vertices=24)
    _ring_x('molded_cap_inner_ring',27.18,y,z,8.75,8.65,.055,m['blue'])
    # Two edge rows, 15 total terminal positions from the manufacturer drawing.
    for row,(zz,count) in enumerate([(6.45,8),(26.75,7)]):
        start=145-(count-1)*2.54/2
        _box('black_header_strip_%d'%row,(8.4,145,zz),(1.1,(count-1)*2.54+2.2,2.2),m['black'],.05)
        for i in range(count):
            yy=start+i*2.54
            _cyl('header_pad_%d_%d'%(row,i),(10.58,yy,zz),.82,.055,m['solder'],bevel=0,vertices=24)
            _box('gold_header_pin_%d_%d'%(row,i),(7.2,yy,zz),(3.4,.5,.5),m['gold'],.035)
    # Representative back electronics: actual board remains a separate purchased module.
    _box('back_controller',(8.55,145.5,16.5),(.8,5.5,5.5),m['black'],.1)
    for side in [-1,1]:
        for i in range(8):
            _box('controller_pin_%d_%d'%(side,i),(8.6,145.5+side*3.05,14.2+i*.65),(.36,.65,.24),m['solder'],.02)
    for i,(yy,zz) in enumerate([(135.5,12),(136,16),(136.5,21),(153,12),(153.5,21),(150,24)]):
        _box('passive_%d'%i,(8.55,yy,zz),(.8,1.7,.9),m['ceramic'],.05)
        for sy in [-1,1]:_box('passive_terminal_%d_%d'%(i,sy),(8.53,yy+sy*.7,zz),(.84,.3,.96),m['solder'],.02)
    _text('back_silkscreen','ZE07-CO',(8.91,145,24.5),1.2,m['white'],u=(0,-1,0),v=(0,0,1))


def _bme(m):
    global _ID
    _ID='GYBMEP_BME280'
    board=_box('purple_four_pin_breakout',(61.5,168.8,6.8),(17,12,1.6),m['purple'],0)
    # A single mounting eye is characteristic of this four-pin breakout family.
    bpy.ops.mesh.primitive_cylinder_add(vertices=48,radius=1.18,depth=3,location=(67.7,172.4,6.8))
    cutter=bpy.context.object
    mod=board.modifiers.new('Mounting hole','BOOLEAN');mod.operation='DIFFERENCE';mod.object=cutter
    bpy.context.view_layer.objects.active=board
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(cutter,do_unlink=True)
    _bevel(board,.14)
    # Bare Bosch BME280: 2.5 x 2.5 x 0.93 mm metal-capped LGA package.
    _box('BME280_LGA_base',(64.5,168.4,7.70),(2.5,2.5,.2),m['black'],.06)
    _box('BME280_metal_cap',(64.5,168.4,8.165),(2.5,2.5,.73),m['steel'],.11)
    _cyl('BME280_pressure_port',(63.8,169.1,8.535),.18,.015,m['dark'],axis='Z',vertices=24,bevel=0)
    # Four signal pads and short terminals keep the original 5 mm envelope.
    for i,label in enumerate(('VIN','GND','SCL','SDA')):
        yy=165.0+i*2.54
        _cyl('gold_pad_'+label,(54.5,yy,7.63),.8,.055,m['gold'],axis='Z',vertices=32,bevel=0)
        _box('short_header_pin_'+label,(54.5,yy,8.95),(.5,.5,2.6),m['gold'],.035)
        _text('pin_label_'+label,label,(57,yy,7.64),.63,m['white'])
    _box('header_spacer',(54.5,168.81,8.15),(2.0,10,1.0),m['black'],.07)
    _box('voltage_regulator',(60,165.0,8.05),(2.9,1.6,.85),m['black'],.08)
    for i,(xx,yy) in enumerate([(59.2,164.0),(60.8,164.0),(60,166.0)]):
        _box('regulator_contact_%d'%i,(xx,yy,7.8),(.45,.7,.3),m['solder'],.03)
    for i,(xx,yy) in enumerate([(60,170.4),(62.4,171),(67.4,165),(59,172.1)]):
        _box('support_passive_%d'%i,(xx,yy,7.95),(1.6,.8,.65),m['ceramic'] if i%2 else m['black'],.05)
        for side in [-1,1]:_box('support_termination_%d_%d'%(i,side),(xx+side*.65,yy,7.96),(.3,.84,.7),m['solder'],.025)
    _text('board_identity','BME280',(63.1,163.5,7.64),.8,m['white'])
    _text('board_family','GYBMEP',(62.5,173.65,7.64),.78,m['white'])
    board['geometry_basis']='Owner-approved approximate GYBMEP four-pin breakout appearance'


def build_sensors():
    """Return independently parentable visual objects in original CAD millimeters."""
    global _OUT
    _OUT=[]
    m={
        'red':_material('AO2_red_ABS',(.58,.018,.013),roughness=.34),
        'blue':_material('Winsen_blue_polymer',(.006,.33,.62),roughness=.31),
        'black':_material('black_polymer',(.012,.014,.017),roughness=.46),
        'dark':_material('aperture_shadow',(.0015,.002,.0025),roughness=.85),
        'ivory':_material('Molex_nylon',(.81,.79,.68),roughness=.48),
        'white':_material('silkscreen_white',(.9,.91,.84),roughness=.65),
        'pcb':_material('Winsen_green_soldermask',(.013,.19,.063),roughness=.4),
        'purple':_material('GYBMEP_purple_soldermask',(.19,.022,.33),roughness=.4),
        'fr4':_material('exposed_FR4',(.29,.31,.12),roughness=.7),
        'bronze':_material('sintered_bronze',(.48,.29,.083),metallic=.73,roughness=.52),
        'gold':_material('gold_contacts',(.72,.47,.15),metallic=.8,roughness=.28),
        'steel':_material('sensor_steel_cap',(.45,.49,.51),metallic=.85,roughness=.3),
        'solder':_material('solder',(.49,.52,.53),metallic=.85,roughness=.34),
        'ceramic':_material('ceramic_passives',(.48,.32,.16),roughness=.55),
        'epoxy':_material('green_glass_epoxy',(.13,.22,.074),roughness=.26),
        'membrane':_material('oxygen_membrane',(.73,.74,.69),roughness=.83),
        'ao2label':_label_material('AO2_paper_label','ao2-label.png'),
        'mdlabel':_label_material('MD62_paper_label','md62-label.png'),
        'colabel':_label_material('ZE07_paper_label','ze07-label.png'),
    }
    # Fine sintered texture is a shader detail, not geometry that enlarges the envelope.
    mat=m['bronze'];nodes=mat.node_tree.nodes;links=mat.node_tree.links
    noise=nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=115
    bump=nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.25;bump.inputs['Distance'].default_value=.045
    links.new(noise.outputs['Fac'],bump.inputs['Height']);links.new(bump.outputs['Normal'],nodes.get('Principled BSDF').inputs['Normal'])
    _ao2(m);_md62(m);_ze07(m);_bme(m)
    return list(_OUT)
