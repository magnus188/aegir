"""Photo-grounded Guition display detail, in assembly millimetres.

Run with normal Python + Pillow + NumPy to reproduce texture assets:
    python display_assets.py --prepare
Import in Blender and call build_display_details(). All returned objects are
unparented, in CAD world millimetres, tagged render_group='display'. Retain CAD
C01-V01 (measured casing) and C01-V02 (glass); omit V03 and V04.

This is a render reconstruction, not a replacement manufacturing model. XY is
projectively registered to the measured mounting grid. PCB/component Z remains
illustrative except the measured 13.4 mm glass-to-header-tip depth.
"""
from pathlib import Path
import hashlib
import json
import math

ROOT = Path(__file__).resolve().parents[3]
ASSETS = ROOT / 'media/renders/references/display'
PHOTO = ROOT / 'hardware/system-review/integration-photo/IMG_0571.png'
REGISTRATION = ROOT / 'hardware/system-review/integration-photo/registration.json'
SCREEN = ROOT / 'software/firmware/docs/screenshots/analysis-live.png'
# Observed PCB edges, expressed in registration's local mm coordinate system.
BOARD_RECT = (5.2, 16.0, 64.4, 101.7)
OFFSET = (7.85, 5.2)
PCB_FRONT, PCB_BACK = 8.0, 9.2


def prepare_assets():
    """Rectify the owner's photograph without changing relative component XY."""
    from PIL import Image, ImageOps
    import numpy as np
    ASSETS.mkdir(parents=True, exist_ok=True)
    reg = json.loads(REGISTRATION.read_text())
    original = ImageOps.exif_transpose(Image.open(PHOTO)).convert('RGB')
    # Work directly with original pixels; registration is in 1368 x 1824 pixels.
    H = np.array(reg['homography_annotation_px_to_local_mm'])
    x0, y0, x1, y1 = BOARD_RECT
    w, h = 1421, 2057
    pixel_to_local = np.array([[(x1-x0)/w, 0, x0],
                               [0, -(y1-y0)/h, y1], [0, 0, 1.]])
    to_original = np.diag([original.width/1368, original.height/1824, 1.])
    transform = to_original @ np.linalg.inv(H) @ pixel_to_local
    transform /= transform[2, 2]
    coeff = tuple(transform.flatten()[:8])
    rectified = original.transform((w, h), Image.Transform.PERSPECTIVE, coeff,
                                    Image.Resampling.BICUBIC)
    rectified.save(ASSETS / 'guition-rear-rectified.png', optimize=True)
    source_paths = [PHOTO, REGISTRATION, SCREEN,
                   ROOT / 'hardware/system-review/integration-photo/guition-manufacturer/JC4880P443C_I_W_Y.pdf']
    receipt = {
        'purpose': 'Render-only registered photo texture and geometric display detail',
        'sources': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in source_paths},
        'manufacturer_source_url': 'https://pan.jczn1688.com/directlink/1/HMI%20display/JC4880P443C_I_W.zip',
        'source_photo': 'Owner photograph; original EXIF orientation applied before rectification',
        'registration': 'Existing four-hole 60 x 108 mm registration; no purchased-component rescaling',
        'rear_texture_local_bounds_mm': BOARD_RECT,
        'rear_texture_size_px': [w, h],
        'assembly_offset_xy_mm': OFFSET,
        'screen_bounds_mm': [14.42, 16.8, 70.58, 110.4],
        'screen_surface_z_mm': .38,
        'screen_aspect': '480:800 mapped to 56.16:93.6 mm without stretch or crop',
        'cad_leaves_to_hide': ['TMX-A3-C01-V03', 'TMX-A3-C01-V04'],
        'cad_leaves_to_keep': ['TMX-A3-C01-V01', 'TMX-A3-C01-V02'],
        'geometry': 'PCB perimeter, shield, microSD shell, USB shells, JST connectors, FPC connectors and header locations sampled from registered owner photograph',
        'depth_assumptions_mm': {'PCB': [PCB_FRONT, PCB_BACK], 'header_tip': 13.8},
        'limitations': ['Board/component Z is illustrative; original PCB plane is unmeasured.',
                       'Raised components in the source photograph retain residual parallax.',
                       'Photo texture includes photographic lighting and fine component detail.',
                       'No unverified inter-board harness is rendered.'],
    }
    (ASSETS / 'source-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


def build_display_details():
    import bpy
    from mathutils import Vector
    created = []
    reg = json.loads(REGISTRATION.read_text())
    H = reg['homography_annotation_px_to_local_mm']

    def xy(px, py):
        q = H[2][0]*px + H[2][1]*py + H[2][2]
        return ((H[0][0]*px+H[0][1]*py+H[0][2])/q + OFFSET[0],
                (H[1][0]*px+H[1][1]*py+H[1][2])/q + OFFSET[1])

    def material(name, color, metal=0, rough=.4):
        mat = bpy.data.materials.new(name)
        mat.diffuse_color = (*color, 1)
        mat.use_nodes = True
        p = mat.node_tree.nodes.get('Principled BSDF')
        p.inputs['Base Color'].default_value = (*color, 1)
        p.inputs['Roughness'].default_value = rough
        p.inputs['Metallic'].default_value = metal
        return mat

    def picture_material(name, path, emission=0, rough=.6):
        mat = material(name, (.05, .05, .05), rough=rough)
        nodes = mat.node_tree.nodes
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(str(path), check_existing=True)
        tex.interpolation = 'Linear'
        p = nodes.get('Principled BSDF')
        mat.node_tree.links.new(tex.outputs['Color'], p.inputs['Base Color'])
        p.inputs['Specular IOR Level'].default_value = .18
        if emission:
            mat.node_tree.links.new(tex.outputs['Color'], p.inputs['Emission Color'])
            p.inputs['Emission Strength'].default_value = emission
        return mat

    pcb = material('Guition black solder mask edges', (.016, .021, .02), rough=.43)
    black = material('Guition connector black LCP', (.014, .016, .017), rough=.46)
    white = material('Guition natural JST nylon', (.69, .67, .57), rough=.53)
    nickel = material('Guition brushed connector nickel', (.54, .57, .58), metal=.88, rough=.32)
    gold = material('Guition header plated contacts', (.64, .42, .12), metal=.8, rough=.3)
    board_photo = picture_material('Registered owner Guition backside photograph', ASSETS / 'guition-rear-rectified.png')
    screen = picture_material('Current live-analysis firmware framebuffer', SCREEN, emission=.65, rough=.35)

    def tag(obj, name):
        obj.name = name
        obj['render_group'] = 'display'
        obj['source'] = 'Owner display photograph + manufacturer mounting registration'
        created.append(obj)
        return obj

    def box(name, center, dims, mat, bevel=0):
        bpy.ops.mesh.primitive_cube_add(size=1, location=center)
        obj = tag(bpy.context.object, name)
        obj.dimensions = dims
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        obj.data.materials.append(mat)
        if bevel:
            mod = obj.modifiers.new('Small manufactured edge radius', 'BEVEL')
            mod.width = bevel
            mod.segments = 3
            obj.modifiers.new('Weighted connector normals', 'WEIGHTED_NORMAL')
        return obj

    x0, y0, x1, y1 = [BOARD_RECT[0]+OFFSET[0], BOARD_RECT[1]+OFFSET[1],
                       BOARD_RECT[2]+OFFSET[0], BOARD_RECT[3]+OFFSET[1]]

    def plane(name, bounds, z, mat, facing_front=False, photo_uv=False):
        a, b, c, d = bounds
        verts = [(a,b,z), (c,b,z), (c,d,z), (a,d,z)]
        mesh = bpy.data.meshes.new(name)
        indices = (0,3,2,1) if facing_front else (0,1,2,3)
        mesh.from_pydata(verts, [], [indices])
        mesh.update()
        uv = mesh.uv_layers.new(name='Photo registration UV')
        for poly in mesh.polygons:
            for li in poly.loop_indices:
                v = mesh.vertices[mesh.loops[li].vertex_index].co
                if photo_uv:
                    uv.data[li].uv = ((v.x-x0)/(x1-x0), (v.y-y0)/(y1-y0))
                else:
                    # Seen from the -Z front with +Y up, screen-right is -X.
                    u = (v.x-a)/(c-a)
                    uv.data[li].uv = (1-u if facing_front else u, (v.y-b)/(d-b))
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
        obj.data.materials.append(mat)
        return tag(obj, name)

    # The board substrate is traced to the observed perimeter, not the oversized
    # nine-solid reference PCB envelope. Detailed traces and small parts use the
    # registered actual photograph rather than invented component populations.
    box('Guition display real-photo PCB substrate', ((x0+x1)/2,(y0+y1)/2,(PCB_FRONT+PCB_BACK)/2),
        (x1-x0,y1-y0,PCB_BACK-PCB_FRONT), pcb, .22)
    plane('Guition registered rear PCB surface', (x0,y0,x1,y1), PCB_BACK+.015, board_photo, photo_uv=True)
    plane('Live firmware 480x800 portrait screen', (14.42,16.8,70.58,110.4), .38, screen, facing_front=True)

    def raised_photo_box(name, rect, height, side=nickel, bevel=.12):
        # Photo coordinates bound the visible package; use a common registered
        # texture so the actual part markings are retained on its raised top.
        a, b = xy(rect[0], rect[1]); c, d = xy(rect[2], rect[3])
        bounds = (min(a,c),min(b,d),max(a,c),max(b,d))
        xa, ya, xb, yb = bounds
        box(name, ((xa+xb)/2,(ya+yb)/2,PCB_BACK+height/2), (xb-xa,yb-ya,height), side, bevel)
        plane(name+' photographed top', bounds, PCB_BACK+height+.015, board_photo, photo_uv=True)
        return bounds

    raised_photo_box('Guition ESP32 P4 RF shield', (426,779,715,1064), 2.05)
    raised_photo_box('Guition microSD steel socket', (217,594,397,768), 1.65)
    raised_photo_box('Guition power inductor', (282,471,338,523), 1.6, black)
    raised_photo_box('Guition LCD FPC connector', (453,1128,692,1190), 1.35, white)
    raised_photo_box('Guition touch FPC connector', (784,393,857,439), .95, black)
    # Observed populated white connector housings; no mating cables invented.
    for name, rect, h in [
        ('upper right 3 pin', (675,350,778,400), 3.4),
        ('upper left 2 pin', (207,426,256,483), 3.1),
        ('middle right 3 pin', (848,623,901,699), 3.6),
        ('lower right 4 pin', (848,759,901,841), 3.6),
        ('lower left 2 pin', (211,881,264,934), 3.1),
        ('right small 6 pin', (849,951,901,1017), 1.6),
    ]:
        raised_photo_box('Guition JST '+name, rect, h, white, .16)

    # The two exposed native USB-C shells are grounded in the photographed
    # mouth centers. Separate sheet surfaces leave a dark open connector mouth.
    for i, pos in enumerate([(464,361),(595,366)], 1):
        cx, mouth_y = xy(*pos)
        cy = mouth_y-2.0
        shell_z = PCB_BACK+1.7
        box(f'Guition USB-C {i} lower shell', (cx,cy,PCB_BACK+.16), (8.9,6.4,.32), nickel,.14)
        box(f'Guition USB-C {i} upper shell', (cx,cy,PCB_BACK+3.18), (8.9,6.4,.32), nickel,.14)
        for dx in [-4.28,4.28]:
            box(f'Guition USB-C {i} shell side', (cx+dx,cy,shell_z), (.34,6.4,3.0),nickel,.13)
        box(f'Guition USB-C {i} dark recess', (cx,mouth_y-3.0,shell_z), (8.3,.35,2.7),black,.12)
        box(f'Guition USB-C {i} insulating tongue', (cx,mouth_y-.25,shell_z), (6.1,3.2,.58), black,.18)
        for j in range(12):
            box(f'Guition USB-C {i} tongue contact {j+1}', (cx-2.75+j*.5,mouth_y+.65,shell_z+.305), (.24,1.3,.04),gold)

    # Header locations from registration's 30.48 mm (12 interval) cross-check.
    hx, hy = xy(385,313)
    for row in range(2):
        yy = hy+row*2.54
        box(f'Guition 26-pin header insulator row {row+1}', (hx+15.24,yy,PCB_BACK+.65), (33.02,2.3,1.3),black,.12)
        for col in range(13):
            xx = hx+col*2.54
            box(f'Guition JP1 physical pin {2*col+row+1}', (xx,yy,(10.5+13.8)/2), (.64,.64,3.3),nickel,.045)
    return created


if __name__ == '__main__':
    import sys
    if '--prepare' in sys.argv:
        prepare_assets()
    else:
        raise SystemExit('Use --prepare to regenerate photo texture; import build_display_details in Blender.')
