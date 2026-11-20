"""Run with Blender on 군대 방 맵.blend to regenerate MilitaryGeometry.inc.

blender --background "군대 방 맵.blend" --python export_geometry.py
"""

import argparse
import math
import sys
from collections import defaultdict
from pathlib import Path

import bpy
from mathutils import Vector

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'shared' / 'design'))
from material_bake import sample_surface


BARRICADE = "Barricade_Interactable"
MOON = "Moon_Pickup"
MOVABLE_OBJECTS = {BARRICADE, MOON}
GEAR_MODELS = ("pack", "bedding", "mask")
OUTPOST_PREFIXES = (
    "Area 02 ", "Discharge ", "Distant sky ", "Exit ", "Floor direction marker.",
    "Night exterior | ", "Night polish | ", "Open outpost | ", "Outpost ",
    "Return ramp ", "Shadow pillar ", "Shadow-casting pillar", "Trench ",
)
LIGHT = Vector((0.32, 0.87, 0.37)).normalized()


def game_coordinates(point):
    # Blender Z-up to the game's right-handed Y-up, at the same meter scale.
    return Vector((point.x, point.z, -point.y))


def mesh_triangles(mesh, world):
    mesh.calc_loop_triangles()
    points = [game_coordinates(world @ vertex.co) for vertex in mesh.vertices]
    for triangle in mesh.loop_triangles:
        corners = tuple(points[index] for index in triangle.vertices)
        normal = (corners[1] - corners[0]).cross(corners[2] - corners[0])
        if normal.length_squared < 1e-16:
            continue
        slot = triangle.material_index
        material = mesh.materials[slot] if slot < len(mesh.materials) else None
        base = material.diffuse_color[:3] if material else (0.5, 0.5, 0.5)
        base, shading_normal, highlight = sample_surface(material, sum(corners, Vector()) / 3,
                                                         normal.normalized(), LIGHT)
        shade = 0.63 + 0.37 * max(0.0, shading_normal.dot(LIGHT))
        if material and (material.get("game_unlit", False) or
                         any(word in material.name.lower() for word in ("moon", "starlight"))):
            shade = 1.0
            base, highlight = material.diffuse_color[:3], 0.0
        color = tuple(min(1.0, max(0.0, component) ** (1.0 / 2.2) * shade + highlight)
                      for component in base)
        yield corners, color


def object_triangles(obj, depsgraph):
    evaluated = obj.evaluated_get(depsgraph)
    # Keep the authored silhouette and omit decorative bevel tessellation on
    # static props. Evaluate text and the moon.
    if obj.type == "MESH" and obj.name not in MOVABLE_OBJECTS:
        return list(mesh_triangles(obj.data, evaluated.matrix_world))
    mesh = evaluated.to_mesh()
    try:
        return list(mesh_triangles(mesh, evaluated.matrix_world))
    finally:
        evaluated.to_mesh_clear()


def triangle_bounds(triangles):
    points = [point for corners, _ in triangles for point in corners]
    minimum = Vector(tuple(min(point[i] for point in points) for i in range(3)))
    maximum = Vector(tuple(max(point[i] for point in points) for i in range(3)))
    return minimum, maximum


def world_bounds(triangles):
    minimum, maximum = triangle_bounds(triangles)
    return (maximum + minimum) * 0.5, maximum - minimum


def normalized_triangles(triangles, center, dimensions):
    for corners, color in triangles:
        yield tuple(tuple((point[i] - center[i]) / dimensions[i] for i in range(3))
                    for point in corners), color


def cpp_number(value, precision):
    value = round(value, precision)
    if value == 0:
        return "0"
    if value == int(value):
        return str(int(value))
    return f"{value:.{precision}f}".rstrip("0") + "f"


def cpp_vector(values, precision=3):
    return "{" + ",".join(cpp_number(value, precision) for value in values) + "}"


def write_triangles(output, triangles):
    count = 0
    for corners, color in triangles:
        color_literal = cpp_vector(color)
        for corner in corners:
            output.write("    {" + cpp_vector(corner) + "," + color_literal + "},\n")
            count += 1
    return count


def write_static_batches(output, name, static_objects, is_outpost, depsgraph):
    cell = Vector((8, 6, 8)) if is_outpost else Vector((3, 2, 3))
    buckets = defaultdict(list)
    for obj in static_objects:
        if obj.name.startswith(OUTPOST_PREFIXES) != is_outpost:
            continue
        triangles = object_triangles(obj, depsgraph)
        if not triangles:
            continue
        minimum, maximum = triangle_bounds(triangles)
        center = (minimum + maximum) * 0.5
        spans_cells = any(maximum[i] - minimum[i] > cell[i] * 1.5 for i in range(3))
        # Keep broad walls and horizon cards separate so their bounds cannot
        # pull small props from an otherwise off-screen cell into the draw.
        key = (0, 0, 0, 0) if spans_cells else (1, *(math.floor(center[i] / cell[i]) for i in range(3)))
        buckets[key].append((obj.name, triangles, minimum, maximum))

    offset = 0
    batches = []
    output.write(f"const MeshVertex {name}Geometry[] = {{\n")
    for objects in (buckets[key] for key in sorted(buckets)):
        count = 0
        for object_name, triangles, _, _ in objects:
            output.write(f"    // {object_name}\n")
            count += write_triangles(output, triangles)
        minimum = tuple(min(obj[2][axis] for obj in objects) for axis in range(3))
        maximum = tuple(max(obj[3][axis] for obj in objects) for axis in range(3))
        batches.append((offset, count, minimum, maximum))
        offset += count
    output.write("};\n\n")
    output.write(f"const MeshBatch {name}Batches[] = {{\n")
    for first, count, minimum, maximum in batches:
        output.write(f"    {{{first}, {count}, {cpp_vector(minimum)}, {cpp_vector(maximum)}}},\n")
    output.write("};\n\n")
    print(f"Exported {offset} vertices in {len(batches)} {name} batches")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parents[1] / "MilitaryGeometry.inc")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])

    scene = bpy.data.scenes["01 - Military | Entry"]
    bpy.context.window.scene = scene
    for obj in scene.objects:
        if obj.type == "FONT":
            obj.data.resolution_u = 3
            obj.data.render_resolution_u = 3
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()

    moon = object_triangles(scene.objects[MOON], depsgraph)
    moon_center, moon_size = world_bounds(moon)

    static_objects = [
        obj for obj in sorted(scene.objects, key=lambda item: item.name)
        if obj.type in {"MESH", "FONT"} and not obj.hide_render
        and obj.name not in MOVABLE_OBJECTS and obj.get("game_prop") not in GEAR_MODELS
    ]
    with args.output.open("w", encoding="utf-8", newline="\n") as output:
        output.write("// Generated from the Military Entry scene by design/export_geometry.py.\n")
        write_static_batches(output, "barracks", static_objects, False, depsgraph)
        write_static_batches(output, "outpost", static_objects, True, depsgraph)

        output.write("const MeshVertex barracksGear[] = {\n")
        gear_offsets = [0]
        for model in GEAR_MODELS:
            triangles = [triangle
                         for obj in sorted(scene.objects, key=lambda item: item.name)
                         if obj.type == "MESH" and obj.get("game_prop") == model
                         for triangle in object_triangles(obj, depsgraph)]
            if not triangles:
                raise ValueError(f"Interacting gear mesh is missing: {model}")
            center, size = world_bounds(triangles)
            output.write(f"    // {model}\n")
            count = write_triangles(output, normalized_triangles(triangles, center, size))
            gear_offsets.append(gear_offsets[-1] + count)
        output.write("};\n")
        output.write("constexpr GLint gearOffsets[] = " + cpp_vector(gear_offsets) + ";\n\n")
        output.write("const glm::vec3 moonWorldCenter = " + cpp_vector(moon_center, 9) + ";\n")
        output.write("const glm::vec3 moonWorldSize = " + cpp_vector(moon_size, 9) + ";\n")
        output.write("const MeshVertex moonGeometry[] = {\n")
        moon_count = write_triangles(output, normalized_triangles(moon, moon_center, moon_size))
        output.write("};\n")

    print("Moon center", tuple(moon_center), "dimensions", tuple(moon_size))
    print("Exported", moon_count, "moon vertices")


if __name__ == "__main__":
    main()
