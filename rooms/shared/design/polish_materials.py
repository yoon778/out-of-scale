"""Shared authoring palette for the military room and bedroom.

Preserve mesh data, assignments, procedural links and emissive puzzle lighting.
The OpenGL exporter still uses diffuse_color as its authoritative color palette.
"""

import json
import bpy


HOME_PALETTE = {
    'Plaster': ((.74,.71,.64),.86,0),
    'Ceiling': ((.80,.79,.74),.86,0),
    'SageWall': ((.30,.38,.34),.84,0),
    'Skirting': ((.66,.62,.54),.62,0),
    'Walnut': ((.18,.105,.065),.48,0),
    'WalnutEdge': ((.25,.15,.09),.46,0),
    'Oak': ((.40,.29,.18),.60,0),
    'Linen': ((.74,.71,.64),.95,0),
    'SageLinen': ((.29,.37,.31),.96,0),
    'DarkLinen': ((.19,.25,.25),.98,0),
    'CreamRug': ((.56,.53,.46),.99,0),
    'RugWeave': ((.36,.37,.33),.99,0),
    'Metal': ((.065,.078,.080),.36,.75),
    'BlackPlastic': ((.025,.031,.034),.66,0),
    'BlueBook': ((.13,.21,.27),.83,0),
    'RedBook': ((.31,.13,.10),.87,0),
    'GreenBook': ((.21,.28,.22),.86,0),
    'OcherBook': ((.44,.32,.19),.87,0),
    'Paper': ((.81,.78,.70),.95,0),
    'Terracotta': ((.35,.17,.11),.84,0),
}
MILITARY_PALETTE = {
    'Warm white powder-coated steel': ((.74,.755,.72),.50,0),
    'MAT_Metal_off-white_bed_frame': ((.62,.65,.62),.46,0),
    'MAT_Metal_Radiator_Paint': ((.62,.64,.60),.54,0),
    'MAT_Metal_painted_steel_cabinet': ((.57,.60,.57),.52,0),
    'MAT_Metal_locker_hardware': ((.15,.19,.19),.45,.70),
    'MAT_Paint_painted_wall': ((.64,.665,.64),.86,0),
    'MAT_BarracksDetail_Tile4': ((.47,.49,.455),.88,0),
    'MAT_Fabric_field_green_bedding': ((.13,.18,.09),.92,0),
    'MAT_Fabric_folded_blanket': ((.16,.205,.11),.94,0),
    'MAT_Fabric_Window_Curtain': ((.16,.245,.23),.94,0),
    'MAT_Fabric_gray-green_curtain': ((.17,.25,.23),.94,0),
    'MAT_Fabric_cotton_pillow': ((.51,.53,.43),.96,0),
    'Hardware | dark steel': ((.055,.075,.09),.42,.72),
    'MAT_OpenOutpost_granite': ((.027,.047,.064),.94,0),
    'MAT_OpenOutpost_slate': ((.035,.057,.078),.96,0),
    'MAT_Night_Rock_Drainage_Bed': ((.039,.057,.065),.97,0),
    'MAT_OpenOutpost_sandbags': ((.11,.10,.075),.98,0),
    'MAT_NightExterior_Conifer': ((.006,.014,.013),.98,0),
    'OOS military prop 0.120-0.140-0.140': ((.07,.10,.105),.54,.55),
    'OOS military prop 0.130-0.170-0.190': ((.11,.15,.18),.90,0),
    'OOS military prop 0.320-0.343-0.250': ((.30,.34,.25),.90,0),
}


def _state(material, shader):
    values = [*material.diffuse_color, material.roughness, material.metallic]
    for key in ('Base Color', 'Roughness', 'Metallic'):
        value = shader.inputs[key].default_value
        values.extend(value if hasattr(value, '__iter__') else (value,))
    for node in material.node_tree.nodes:
        if node.type == 'VALTORGB':
            for element in node.color_ramp.elements:
                values.extend(element.color)
    return tuple(round(float(value),6) for value in values)


def _recipe(material, room):
    palettes = dict(MILITARY_PALETTE)
    if room == 'home':
        palettes.update({'Home | '+key:value for key,value in HOME_PALETTE.items()})
    name = material.name
    if name in palettes:
        return palettes[name]
    # Appended Blender data can have .001 suffixes; retain the original IDs.
    base, dot, suffix = name.rpartition('.')
    if dot and len(suffix)==3 and suffix.isdigit():
        return palettes.get(base)
    return None


def polish_scene(room):
    if room not in ('home', 'military'):
        raise ValueError('Expected home or military')
    if room == 'home' and not any(m.name.startswith('Home | ') for m in bpy.data.materials):
        raise RuntimeError('The bedroom asset file must be loaded first')
    if room == 'military' and '01 - Military | Entry' not in bpy.data.scenes:
        raise RuntimeError('The military asset file must be loaded first')
    changed, repaired, protected = [], [], []
    for material in bpy.data.materials:
        if not material.use_nodes:
            continue
        shader = next((node for node in material.node_tree.nodes if node.type=='BSDF_PRINCIPLED'),None)
        if shader is None:
            continue
        emission = shader.inputs.get('Emission Strength')
        if (material.get('game_unlit',False) or material.get('home_emission',0) or
                (emission and (emission.is_linked or emission.default_value>0))):
            protected.append(material.name)
            continue
        before = _state(material,shader)
        old_color = tuple(material.diffuse_color[:3])
        color = old_color
        roughness = (material.roughness if shader.inputs['Roughness'].is_linked
                     else shader.inputs['Roughness'].default_value)
        metallic = (material.metallic if shader.inputs['Metallic'].is_linked
                    else shader.inputs['Metallic'].default_value)
        if material.name.startswith('OOS military prop '):
            roughness, metallic = .84, 0.0
        recipe = _recipe(material,room)
        if recipe is not None:
            color, roughness, metallic = recipe
        base = shader.inputs['Base Color']
        if not base.is_linked:
            if max(abs(base.default_value[i]-old_color[i]) for i in range(3))>.00001:
                repaired.append(material.name)
            base.default_value = (*color,material.diffuse_color[3])
        elif recipe is not None and max(abs(color[i]-old_color[i]) for i in range(3))>.000001:
            source = base.links[0].from_node
            if source.type == 'VALTORGB':
                ratios = [color[i]/old_color[i] if old_color[i]>.000001 else 1 for i in range(3)]
                for element in source.color_ramp.elements:
                    element.color = (*[min(1,max(0,element.color[i]*ratios[i])) for i in range(3)],
                                     element.color[3])
        material.diffuse_color = (*color,material.diffuse_color[3])
        material.roughness, material.metallic = roughness, metallic
        if not shader.inputs['Roughness'].is_linked:
            shader.inputs['Roughness'].default_value = roughness
        if not shader.inputs['Metallic'].is_linked:
            shader.inputs['Metallic'].default_value = metallic
        if _state(material,shader) != before:
            changed.append(material.name)
    return {'room':room,'materials':len(bpy.data.materials),'changed':changed,
            'repaired_shader_colors':repaired,'protected_emissive':protected,
            'mesh_objects':sum(obj.type=='MESH' for obj in bpy.data.objects)}


if __name__ == '__main__':
    import argparse
    import sys
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--room',choices=('home','military'),required=True)
    parser.add_argument('--save',action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    print(json.dumps(polish_scene(args.room),ensure_ascii=False))
    if args.save:
        if not bpy.data.filepath:
            raise RuntimeError('Refusing to save over an unspecified file')
        bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath,compress=True,check_existing=False)
