"""Export the twenty bedroom mesh ranges with baked vertex colors for OpenGL.

blender --background "내 방.blend" --python export_geometry.py
"""

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


GROUP_NAMES = (
    "Interior", "Shell", "IntactWall", "BrokenWall", "MiniWalls",
    "Shelf", "Door", "Rpg", "Rack", "MiniBrokenWall", "LatchMounts", "LatchBolt",
    "CabinetLeftDoor", "CabinetRightDoor", "CabinetMiniatures",
    "LockSocket", "SchoolDiorama", "MilitaryDiorama", "OfficeDiorama",
    "HomeDioramaBase",
)
LIGHT_DIRECTION = Vector((-0.65, 0.65, -0.25)).normalized()


def game_coordinates(value):
    return Vector((value.x, value.z, -value.y))


def number(value):
    if not math.isfinite(value):
        raise ValueError("Non-finite mesh coordinate or color")
    value = round(value, 5)
    if value == 0:
        return "0"
    if value == int(value):
        return str(int(value))
    return f"{value:.5f}".rstrip("0").rstrip(".") + "f"


def vector(value):
    return "{" + ",".join(number(component) for component in value) + "}"


def collection_objects(name):
    collection = bpy.data.collections[f"{GROUP_NAMES.index(name):02d} | {name}"]
    return sorted((obj for obj in collection.objects if obj.type == 'MESH'), key=lambda obj: obj.name)


def make_occlusion_tree(depsgraph):
    vertices, triangles = [], []
    for group in ("Interior", "Shell", "IntactWall"):
        for obj in collection_objects(group):
            evaluated = obj.evaluated_get(depsgraph)
            data = evaluated.to_mesh()
            try:
                data.calc_loop_triangles()
                offset = len(vertices)
                vertices.extend(game_coordinates(evaluated.matrix_world @ vertex.co) for vertex in data.vertices)
                triangles.extend(tuple(offset + index for index in triangle.vertices) for triangle in data.loop_triangles)
            finally:
                evaluated.to_mesh_clear()
    return BVHTree.FromPolygons(vertices, triangles, all_triangles=True)


def ambient_occlusion(tree, position, normal):
    right = normal.cross(Vector((0, 1, 0)))
    if right.length_squared < 0.01:
        right = normal.cross(Vector((1, 0, 0)))
    right.normalize()
    up = normal.cross(right)
    origin = position + normal * 0.007
    visibility = 0.0
    for index in range(5):
        angle = index * 2.39996
        direction = (normal * 0.66 + right * (math.cos(angle) * 0.75)
                     + up * (math.sin(angle) * 0.75)).normalized()
        location, _, _, distance = tree.ray_cast(origin, direction, 0.70)
        visibility += 1.0 if location is None else min(1.0, distance / 0.70)
    return 0.73 + visibility * 0.054


def lit_color(surface, position, normal, tree, group):
    base = surface.diffuse_color[:3]
    emission = surface.get("home_emission", 0)
    if emission:
        brightness = 0.92 if emission < 1.0 else 1.0
        return tuple(min(1.0, component ** (1 / 2.2) * brightness) for component in base)
    directional = max(0.0, normal.dot(LIGHT_DIRECTION))
    shade = 0.67 + directional * 0.28
    if group not in ("Shelf", "Door", "Rpg", "Rack", "MiniWalls", "LatchMounts", "LatchBolt",
                     "CabinetLeftDoor", "CabinetRightDoor", "LockSocket", "SchoolDiorama",
                     "MilitaryDiorama", "OfficeDiorama", "HomeDioramaBase"):
        shade *= ambient_occlusion(tree, position, normal)
        # Soft daylight falloff and warm worklamp pools baked into the actual surfaces.
        daylight = math.exp(-((position.x + 5.1) / 3.6) ** 2
                            - ((position.z + 2.1) / 3.0) ** 2)
        shade += daylight * max(0, normal.y * 0.10 + normal.dot(LIGHT_DIRECTION) * 0.07)
        warm = 0.07 * math.exp(-((position.x - 3.8) / 1.6) ** 2
                              - ((position.z - 0.5) / 1.6) ** 2)
    else:
        warm = 0.0
    return tuple(min(1.0, component ** (1 / 2.2) * (shade + warm * tint))
                 for component, tint in zip(base, (1.0, 0.64, 0.25)))


def write_group(output, group, depsgraph, tree):
    count = 0
    for obj in collection_objects(group):
        evaluated = obj.evaluated_get(depsgraph)
        data = evaluated.to_mesh()
        try:
            data.calc_loop_triangles()
            normal_transform = evaluated.matrix_world.to_3x3().inverted().transposed()
            points = [game_coordinates(evaluated.matrix_world @ vertex.co) for vertex in data.vertices]
            output.write(f"    // {obj.name}\n")
            for triangle in data.loop_triangles:
                if triangle.area < 1e-12:
                    continue
                surface = data.materials[triangle.material_index]
                for vertex_index, loop_index in zip(triangle.vertices, triangle.loops):
                    position = points[vertex_index]
                    normal = game_coordinates(normal_transform @ data.corner_normals[loop_index].vector).normalized()
                    color = lit_color(surface, position, normal, tree, group)
                    output.write("    {" + vector(position) + "," + vector(color) + "},\n")
                    count += 1
        finally:
            evaluated.to_mesh_clear()
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "HomeGeometry.inc")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    depsgraph = bpy.context.evaluated_depsgraph_get()
    tree = make_occlusion_tree(depsgraph)
    ranges = []
    offset = 0
    with args.output.open("w", encoding="utf-8", newline="\n") as output:
        output.write("// Generated from design/내 방.blend by design/export_geometry.py.\n")
        output.write("// Presentation collection instances are deliberately excluded.\n")
        output.write("const MeshVertex homeGeometry[] = {\n")
        for group in GROUP_NAMES:
            count = write_group(output, group, depsgraph, tree)
            ranges.append((offset, count))
            offset += count
            print(group, count // 3, "triangles")
        output.write("};\n\nconst HomeMeshRange homeMeshes[] = {\n")
        for group, (first, count) in zip(GROUP_NAMES, ranges):
            output.write(f"    {{{first}, {count}}}, // {group}\n")
        output.write("};\n")
    print("Total", offset // 3, "triangles in", len(ranges), "fixed ranges")


if __name__ == "__main__":
    main()
