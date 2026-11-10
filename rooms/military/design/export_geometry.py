"""Run with Blender on 군대 방 맵.blend to regenerate MilitaryGeometry.inc.

blender --background "군대 방 맵.blend" --python export_geometry.py
"""

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Vector


BARRICADE = "Barricade_Interactable"
MOON = "Moon_Pickup"
MOVABLE_OBJECTS = {BARRICADE, MOON}
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
        shade = 0.63 + 0.37 * max(0.0, normal.normalized().dot(LIGHT))
        if material and any(word in material.name.lower() for word in ("moon", "starlight")):
            shade = 1.0
        color = tuple(min(1.0, max(0.0, component) ** (1.0 / 2.2) * shade) for component in base)
        yield corners, color


def object_triangles(obj, depsgraph):
    evaluated = obj.evaluated_get(depsgraph)
    # Keep the authored silhouette and omit decorative bevel tessellation on
    # static props. Evaluate text, the moon, and the interactive barricade.
    if obj.type == "MESH" and obj.name not in MOVABLE_OBJECTS:
        return list(mesh_triangles(obj.data, evaluated.matrix_world))
    mesh = evaluated.to_mesh()
    try:
        return list(mesh_triangles(mesh, evaluated.matrix_world))
    finally:
        evaluated.to_mesh_clear()


def world_bounds(triangles):
    points = [point for corners, _ in triangles for point in corners]
    minimum = Vector(tuple(min(point[i] for point in points) for i in range(3)))
    maximum = Vector(tuple(max(point[i] for point in points) for i in range(3)))
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

    barricade = object_triangles(scene.objects[BARRICADE], depsgraph)
    barricade_center, barricade_size = world_bounds(barricade)
    moon = object_triangles(scene.objects[MOON], depsgraph)
    moon_center, moon_size = world_bounds(moon)

    with args.output.open("w", encoding="utf-8", newline="\n") as output:
        output.write("// Generated from the Military Entry scene by design/export_geometry.py.\n")
        output.write("const MeshVertex staticGeometry[] = {\n")
        static_count = 0
        for obj in sorted(scene.objects, key=lambda item: item.name):
            if obj.type not in {"MESH", "FONT"} or obj.hide_render or obj.name in MOVABLE_OBJECTS:
                continue
            output.write("    // " + obj.name + "\n")
            static_count += write_triangles(output, object_triangles(obj, depsgraph))
        output.write("};\n\nconst MeshVertex barricadeGeometry[] = {\n")
        barricade_count = write_triangles(output, normalized_triangles(barricade, barricade_center, barricade_size))
        output.write("};\n\n")
        output.write("const glm::vec3 moonWorldCenter = " + cpp_vector(moon_center, 9) + ";\n")
        output.write("const glm::vec3 moonWorldSize = " + cpp_vector(moon_size, 9) + ";\n")
        output.write("const MeshVertex moonGeometry[] = {\n")
        moon_count = write_triangles(output, normalized_triangles(moon, moon_center, moon_size))
        output.write("};\n")

    print("Barricade center", tuple(barricade_center), "dimensions", tuple(barricade_size))
    print("Moon center", tuple(moon_center), "dimensions", tuple(moon_size))
    print("Exported", static_count, "static vertices,", barricade_count, "barricade vertices,", moon_count, "moon vertices")


if __name__ == "__main__":
    main()
