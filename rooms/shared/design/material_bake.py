"""Offline material approximation for the game's existing position/RGB meshes.

Keep subtle procedural grain, bump shading and a fixed roughness/metallic
highlight without adding textures, vertices or shader work at runtime.
This is an appearance bake, not a complete Blender node evaluator or PBR shader.
"""

from functools import cache

from mathutils import Vector, noise


@cache
def parameters(surface):
    if surface is None:
        return (0.5, 0.5, 0.5), 0.8, 0.0, 0.0, (1.0, 1.0, 1.0)
    nodes = surface.node_tree.nodes if surface.use_nodes else ()
    shader = next((node for node in nodes if node.type == 'BSDF_PRINCIPLED'), None)
    base = tuple(surface.diffuse_color[:3])
    roughness, metallic = surface.roughness, surface.metallic
    if shader is not None:
        # The authored export palette is authoritative: legacy props can retain
        # a white Principled default even with an explicitly colored diffuse value.
        if not shader.inputs['Roughness'].is_linked:
            roughness = shader.inputs['Roughness'].default_value
        if not shader.inputs['Metallic'].is_linked:
            metallic = shader.inputs['Metallic'].default_value
    bump = next((node for node in nodes if node.type == 'BUMP'), None)
    has_noise = any(node.type == 'TEX_NOISE' for node in nodes)
    strength = bump.inputs['Strength'].default_value if bump and has_noise else 0.0
    mapping = next((node for node in nodes
                    if node.type == 'VECT_MATH' and node.operation == 'MULTIPLY'), None)
    # Band-limit the authored grain to the density of the existing game mesh.
    scale = (1.5, 1.5, 1.5)
    if mapping is not None:
        x, y, z = mapping.inputs[1].default_value
        scale = tuple(max(0.25, min(6.0, value * 0.05)) for value in (x, z, y))
    return base, min(1.0, max(0.0, roughness)), metallic, strength, scale


def sample_surface(surface, position, normal, light):
    base, roughness, metallic, strength, scale = parameters(surface)
    shading_normal = normal
    if strength > 0.0:
        def height(point):
            return noise.noise(Vector(tuple(point[i] * scale[i] for i in range(3))))

        value = height(position)
        variation = max(0.94, min(1.06, 1.0 + value * strength * 0.45))
        base = tuple(component * variation for component in base)
        tangent = normal.cross(Vector((0, 1, 0)))
        if tangent.length_squared < 0.01:
            tangent = normal.cross(Vector((1, 0, 0)))
        tangent.normalize()
        bitangent = normal.cross(tangent)
        step = 0.03
        slope = (tangent * (height(position + tangent * step) - value)
                 + bitangent * (height(position + bitangent * step) - value)) / step
        shading_normal = (normal - slope * min(strength, 1.0) * 0.08).normalized()
    # A fixed broad highlight is appropriate for a color bake; it is not view dependent.
    half_vector = (light + Vector((0.2, 0.5, 0.84))).normalized()
    highlight = max(0.0, shading_normal.dot(half_vector)) ** (8.0 + (1.0 - roughness) * 48.0)
    highlight *= (0.02 + 0.14 * metallic) * (1.0 - roughness * 0.75)
    return base, shading_normal, highlight
