"""Appearance reconstructions for six populated parts missing KiCad models.

This never edits the board. Placement comes from the current saved KiCad PCB,
captured by prepare_geometry.py. Dimensions use the repository's manufacturer-
backed package contract, with no assembly/clearance allowance added to bodies.
These are explicit package reconstructions, not claimed vendor STEP models.
All returned objects are in native enclosure-world millimeters.
"""
from pathlib import Path
import json
import math

import bpy

ROOT = Path(__file__).resolve().parents[3]
SOURCE_REPORT = ROOT / 'media/renders/verification/geometry.json'
# Local X/Y and maximum package body Z, from the sourced height contract.
PACKAGES = {
    'L101': {'size': (7.6, 6.9, 5.0), 'style': 'inductor', 'mark': '1R0'},
    'L201': {'size': (4.3, 4.3, 3.1), 'style': 'inductor', 'mark': '152'},
    'L701': {'size': (4.3, 4.3, 2.1), 'style': 'inductor', 'mark': '102'},
    'U110': {'size': (1.6, 1.6, 0.4), 'style': 'qfn'},
    'U111': {'size': (2.0, 2.0, 0.65), 'style': 'qfn'},
    'U115': {'size': (2.0, 2.0, 1.0), 'style': 'hotrod'},
}


def material(name, color, metallic=0, roughness=0.45):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Metallic'].default_value = metallic
    shader.inputs['Roughness'].default_value = roughness
    return mat


def build_pcb_details():
    rows = json.loads(SOURCE_REPORT.read_text())['population']['missing_visuals_details']
    if {r['reference'] for r in rows} != set(PACKAGES):
        raise RuntimeError('Missing-model list changed; review package reconstructions')
    materials = {
        'ferrite': material('PCB_detail_ferrite', (0.055, 0.062, 0.066), 0.08, 0.6),
        'epoxy': material('PCB_detail_epoxy', (0.012, 0.014, 0.016), 0, 0.4),
        'tin': material('PCB_detail_tin', (0.62, 0.66, 0.7), 0.88, 0.24),
        'mark': material('PCB_detail_mark', (0.54, 0.56, 0.53), 0, 0.65),
    }
    objects = []
    for row in rows:
        ref = row['reference']
        spec = PACKAGES[ref]
        w, d, h = spec['size']
        cx, cy = row['cad_xy_mm']
        angle = math.radians(row['rotation_deg'])
        bottom = row['side'] == 'B.Cu'
        direction = -1 if bottom else 1
        seating = 23.9679 if bottom else 25.6321

        def point(x, y, z):
            # Board-local drawing Y points down; body geometry is symmetric and
            # expressed in the CAD plane here. These coordinates are millimeters.
            return (cx+math.cos(angle)*x-math.sin(angle)*y,
                    cy+math.sin(angle)*x+math.cos(angle)*y,
                    seating+direction*z)

        def register(obj):
            obj['render_group'] = 'pcb'
            obj['reference'] = ref
            obj['MPN'] = row['MPN']
            obj['model_provenance'] = 'Manufacturer-dimension appearance reconstruction; not vendor CAD'
            obj['dimension_source'] = row['datasheet']
            objects.append(obj)
            return obj

        def box(label, pos, size, mat, bevel=0):
            bpy.ops.mesh.primitive_cube_add(size=1, location=point(*pos))
            obj = bpy.context.object
            obj.name = f'PCB_detail_{ref}_{label}'
            obj.dimensions = size
            bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
            obj.rotation_euler[2] = angle
            obj.data.materials.append(mat)
            if bevel:
                mod = obj.modifiers.new('Package edge radii', 'BEVEL')
                mod.width = bevel
                mod.segments = 3
                mod.affect = 'EDGES'
                normals = obj.modifiers.new('Package normals', 'WEIGHTED_NORMAL')
                normals.keep_sharp = True
            return register(obj)

        body = box('package', (0, 0, h/2), (w, d, h),
                   materials['ferrite'] if spec['style'] == 'inductor' else materials['epoxy'],
                   min(0.22 if spec['style'] == 'inductor' else 0.045, h/7))
        body['package_dimensions_mm'] = [w, d, h]
        body['package_dimensions_basis'] = 'Sourced maximum body dimensions without clearance allowance'

        if spec['style'] == 'inductor':
            terminal_w = 1.0 if ref == 'L101' else 0.55
            terminal_d = 3.5 if ref == 'L101' else 3.0
            for side in [-1, 1]:
                box(f'terminal_{side}', (side*(w/2-terminal_w/2), 0, 0.13),
                    (terminal_w, terminal_d, 0.26), materials['tin'], 0.045)
            # Small grey top legend identifies nominal inductance. Typography is
            # illustrative; package geometry and actual board position are sourced.
            curve = bpy.data.curves.new(f'{ref}_legend', 'FONT')
            curve.body = spec['mark']
            curve.align_x = 'CENTER'
            curve.align_y = 'CENTER'
            curve.size = 1.1 if ref == 'L101' else 0.7
            curve.extrude = 0
            obj = bpy.data.objects.new(f'PCB_detail_{ref}_legend', curve)
            bpy.context.collection.objects.link(obj)
            obj.location = point(0, 0, h+0.005)
            obj.rotation_euler[2] = angle
            obj.data.materials.append(materials['mark'])
            register(obj)
            # The winding-start stripe is a visible package convention already
            # documented in the board MPN assembly notes for the Coilcraft parts.
            if ref != 'L101':
                box('start_stripe', (w/2-0.55, 0, h+0.004),
                    (0.12, 2.4, 0.006), materials['mark'])
        else:
            # Terminations remain small and mostly concealed under each body;
            # the GLB already provides the actual board pads and solder lands.
            side_offsets = [-0.2, 0.2] if ref != 'U115' else [-0.7, -0.225, 0.225, 0.7]
            for side in [-1, 1]:
                for j, offset in enumerate(side_offsets):
                    box(f'lead_x_{side}_{j}', (side*(w/2-0.06), offset, 0.055),
                        (0.16, 0.19, 0.11), materials['tin'], 0.018)
            if ref != 'U115':
                for side in [-1, 1]:
                    for j, offset in enumerate([-0.6, -0.2, 0.2, 0.6]):
                        box(f'lead_y_{side}_{j}', (offset, side*(d/2-0.06), 0.055),
                            (0.19, 0.16, 0.11), materials['tin'], 0.018)
            else:
                for side in [-1, 1]:
                    box(f'central_power_pad_{side}', (side*0.25, 0, 0.04),
                        (0.28, 1.9, 0.08), materials['tin'], 0.018)
            bpy.ops.mesh.primitive_cylinder_add(vertices=20, radius=0.075, depth=0.006,
                                               location=point(-w*0.3, d*0.3, h+0.004))
            dot = bpy.context.object
            dot.name = f'PCB_detail_{ref}_pin1'
            dot.data.materials.append(materials['mark'])
            register(dot)
    return objects
