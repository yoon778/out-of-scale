"""Export the military parade ground and its three carryable props for the game.

blender --background 연병장.blend --python export_parade_geometry.py
Use -- --create to regenerate the editable scene from the dimensions below.
"""

import argparse
import math
from pathlib import Path
import runpy
import sys

import bpy
from mathutils import Vector


SCENE = "Military | Parade ground"
STATIC_COLLECTION = "Parade ground"
PROP_COLLECTION = "Carryable models"
PROP_NAMES = ("Jeep", "Tank", "Barracks model")
PROP_SIZES = ((0.70, 0.40, 0.98), (0.85, 0.55, 1.10), (0.90, 0.75, 0.92))

PALETTE = {
    "asphalt": (0.39, 0.44, 0.47),
    "concrete": (0.47, 0.51, 0.50),
    "edge": (0.27, 0.34, 0.34),
    "seam": (0.28, 0.33, 0.35),
    "mark": (0.65, 0.66, 0.53),
    "rail": (0.34, 0.44, 0.45),
    "steel": (0.35, 0.42, 0.41),
    "dark": (0.15, 0.19, 0.21),
    "tower": (0.56, 0.56, 0.46),
    "sill": (0.73, 0.63, 0.37),
    "lamp": (1.0, 0.73, 0.32),
    "glass": (0.27, 0.41, 0.43),
    "grass": (0.25, 0.30, 0.23),
    "gravel": (0.34, 0.37, 0.32),
    "rubber": (0.18, 0.20, 0.18),
    "olive": (0.47, 0.54, 0.30),
    "army": (0.37, 0.44, 0.29),
    "olive_light": (0.55, 0.58, 0.34),
    "canvas": (0.63, 0.64, 0.46),
    "sand": (0.65, 0.58, 0.42),
    "paint": (0.80, 0.75, 0.56),
}
BOX_FACES = ((3, 0, 1, 2), (7, 6, 5, 4), (0, 4, 5, 1),
             (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0))


def material(name):
    existing = bpy.data.materials.get(name)
    if existing:
        return existing
    result = bpy.data.materials.new(name)
    result.diffuse_color = (*(component ** 2.2 for component in PALETTE[name]), 1)
    if name == "lamp":
        result["game_unlit"] = True
    return result


class Mesh:
    def __init__(self):
        self.vertices = []
        self.faces = []
        self.colors = []

    def add(self, vertices, faces, color):
        origin = len(self.vertices)
        self.vertices.extend((x, -z, y) for x, y, z in vertices)
        self.faces.extend(tuple(origin + idx for idx in face) for face in faces)
        self.colors.extend([color] * len(faces))

    def box(self, x, y, z, color):
        x0, x1 = x
        y0, y1 = y
        z0, z1 = z
        self.add(((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1),
                  (x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)),
                 BOX_FACES, color)

    def block(self, center, size, color):
        bounds = [(value - length / 2, value + length / 2) for value, length in zip(center, size)]
        self.box(*bounds, color)

    def cylinder(self, axis, interval, center, radius, color, sides=12):
        vertices = []
        for along in interval:
            for index in range(sides):
                angle = 2 * math.pi * index / sides
                a = center[0] + radius * math.cos(angle)
                b = center[1] + radius * math.sin(angle)
                vertices.append({"x": (along, a, b), "y": (b, along, a), "z": (a, b, along)}[axis])
        faces = [tuple(reversed(range(sides))), tuple(range(sides, 2 * sides))]
        for index in range(sides):
            after = (index + 1) % sides
            faces.append((index, after, after + sides, index + sides))
        self.add(vertices, faces, color)

    def prism(self, xz, bottom, top, color):
        count = len(xz)
        vertices = [(x, y, z) for y in (bottom, top) for x, z in xz]
        faces = [tuple(range(count)), tuple(reversed(range(count, 2 * count)))]
        for index in range(count):
            after = (index + 1) % count
            faces.append((index, index + count, after + count, after))
        self.add(vertices, faces, color)

    def object(self, name, collection):
        data = bpy.data.meshes.new(name)
        data.from_pydata(self.vertices, [], self.faces)
        slots = {}
        for polygon, color in zip(data.polygons, self.colors):
            if color not in slots:
                slots[color] = len(data.materials)
                data.materials.append(material(color))
            polygon.material_index = slots[color]
        data.update()
        instance = bpy.data.objects.new(name, data)
        collection.objects.link(instance)
        return instance


def ground(collection):
    mesh = Mesh()
    mesh.box((5.2, 16.2), (-0.20, 0.0), (16.18, 29.0), "asphalt")
    mesh.box((5.2, 16.2), (-0.004, 0.002), (16.18, 17.08), "concrete")
    for x in (8.0, 10.7, 13.4):
        mesh.block((x, 0.001, 22.96), (0.025, 0.002, 11.40), "seam")
    for z in (17.25, 20.08, 22.86, 25.66, 28.4):
        mesh.block((10.7, 0.001, z), (10.84, 0.002, 0.027), "seam")
    for x in (5.42, 15.98):
        mesh.block((x, 0.003, 22.57), (0.043, 0.004, 12.45), "mark")
    for x in (9.84, 11.16):
        mesh.block((x, 0.003, 17.07), (0.043, 0.004, 1.44), "mark")
    for z in (23.5, 24.16, 24.82, 25.48):
        mesh.block((10.5, 0.003, z), (0.055, 0.004, 0.38), "mark")
    for x in (7.8, 8.9, 10.0):
        # Flat paint defines the initial staging bays, without an obstacle to approach.
        for side in (-1, 1):
            mesh.block((x + side * 0.49, 0.003, 18.5), (0.036, 0.004, 1.22), "mark")
            mesh.block((x, 0.003, 18.5 + side * 0.61), (1.01, 0.004, 0.036), "mark")
    mesh.object("Paved square and staging paint", collection)

    margin = Mesh()
    margin.box((3.7, 5.2), (-0.23, -0.09), (16.18, 30.30), "gravel")
    margin.box((16.2, 17.7), (-0.23, -0.09), (16.18, 30.30), "gravel")
    margin.box((5.2, 16.2), (-0.23, -0.09), (29.0, 30.30), "grass")
    for x, z in ((4.30, 19.28), (4.15, 25.60), (17.18, 22.34), (17.28, 27.38), (7.1, 29.74), (14.5, 29.78)):
        margin.prism([(x - .29, z - .22), (x + .12, z - .27), (x + .33, z + .12),
                      (x + .08, z + .30), (x - .35, z + .18)], -.10, .12, "grass")
    margin.object("Gravel shoulders", collection)


def edge_rails(collection):
    mesh = Mesh()
    for x in (5.13, 16.27):
        for z in (16.23, 18.34, 20.46, 22.58, 24.70, 26.82, 28.94):
            mesh.block((x, 0.52, z), (0.105, 1.20, 0.105), "steel")
            mesh.block((x, 1.14, z), (0.12, 0.045, 0.12), "sill")
        for y in (0.41, 0.88):
            mesh.block((x, y, 22.585), (0.07, 0.075, 12.71), "rail")
    for x0, x1, z in ((5.2, 9.73, 16.13), (11.27, 16.2, 16.13), (5.2, 16.2, 29.05)):
        for x in (x0, (x0 + x1) / 2, x1):
            mesh.block((x, 0.52, z), (0.10, 1.20, 0.10), "steel")
        for y in (0.41, 0.88):
            mesh.box((x0, x1), (y - 0.038, y + 0.038), (z - 0.035, z + 0.035), "rail")
    mesh.object("Perimeter barrier", collection)


def watch_post(collection):
    mesh = Mesh()
    mesh.box((9.35, 11.65), (0.0, 2.87), (26.85, 28.8), "edge")
    mesh.box((9.347, 11.653), (0.025, 0.21), (26.847, 28.803), "steel")
    for y in (0.90, 1.78, 2.66):
        mesh.box((9.35, 11.65), (y, y + 0.022), (26.847, 26.851), "seam")
    mesh.box((9.35, 11.65), (2.87, 3.05), (26.85, 28.8), "concrete")
    mesh.box((9.35, 11.65), (3.048, 3.053), (26.85, 26.96), "sill")
    mesh.box((9.68, 11.32), (3.048, 3.053), (28.245, 28.32), "sill")
    for x in (9.48, 11.52):
        for z in (27.17, 28.6):
            mesh.block((x, 1.43, z), (0.21, 2.89, 0.21), "steel")
            mesh.block((x, 0.11, z), (0.32, 0.22, 0.32), "concrete")
        mesh.block((x, 2.74, 27.88), (0.16, 0.19, 1.63), "steel")
        mesh.block((x, 0.98, 27.88), (0.12, 0.11, 1.63), "steel")
        mesh.block((x, 3.1, 27.15), (0.095, 0.10, 0.10), "sill")
    mesh.block((10.50, 2.70, 28.58), (2.15, 0.16, 0.17), "steel")

    # Two jambs stop exactly at the edges of the door opening.
    mesh.box((9.35, 9.8), (3.05, 5.20), (28.27, 28.45), "tower")
    mesh.box((11.2, 11.65), (3.05, 5.20), (28.27, 28.45), "tower")
    mesh.box((9.35, 9.45), (3.05, 5.41), (28.45, 28.88), "tower")
    mesh.box((11.55, 11.65), (3.05, 5.41), (28.45, 28.88), "tower")
    mesh.box((9.35, 11.65), (5.20, 5.41), (28.27, 28.80), "tower")
    mesh.box((9.8, 11.2), (3.05, 5.20), (28.80, 28.88), "dark")
    mesh.box((9.28, 11.72), (5.41, 5.51), (28.03, 28.90), "steel")
    for x in (9.61, 11.39):
        mesh.block((x, 4.97, 28.228), (0.14, 0.12, 0.12), "dark")
        mesh.block((x, 4.97, 28.156), (0.101, 0.084, 0.028), "lamp")
    for x in (9.397, 11.603):
        for z in (27.18, 27.68):
            mesh.block((x, 3.49, z), (0.087, 0.88, 0.087), "steel")
        mesh.block((x, 3.82, 27.47), (0.06, 0.059, 0.75), "rail")
        mesh.block((x, 3.43, 27.47), (0.05, 0.054, 0.75), "rail")
    mesh.object("Elevated watch post and open door", collection)

    lights = Mesh()
    for x in (5.02, 16.41):
        for z in (19.98, 26.5):
            lights.block((x, 1.45, z), (0.09, 2.92, 0.09), "steel")
            arm = .17 if x < 10 else -.17
            lights.block((x + arm / 2, 2.88, z), (abs(arm) + .09, .08, .09), "steel")
            lights.block((x + arm, 2.84, z), (.24, .105, .20), "dark")
            lights.block((x + arm, 2.78, z), (.19, .02, .15), "lamp")
    lights.object("Perimeter lamp posts", collection)


def jeep():
    mesh = Mesh()
    mesh.box((-.31, .31), (-.108, .01), (-.46, .46), "army")
    mesh.box((-.323, .323), (-.005, .097), (.13, .462), "olive")
    mesh.box((-.31, .31), (-.005, .068), (-.442, .132), "olive")
    mesh.box((-.318, .318), (-.072, -.037), (.46, .49), "dark")
    mesh.box((-.31, .31), (-.072, -.037), (-.49, -.452), "steel")
    mesh.box((-.272, .272), (.092, .161), (.165, .189), "glass")
    mesh.box((-.325, .325), (.162, .20), (-.40, .33), "canvas")
    for side in (-1, 1):
        mesh.block((side * .289, .112, -.367), (.047, .12, .046), "army")
        mesh.block((side * .284, .127, .171), (.043, .074, .044), "army")
        mesh.block((side * .317, .037, .003), (.020, .025, .118), "dark")
        mesh.block((side * .227, .037, .465), (.107, .049, .013), "paint")
        mesh.block((side * .228, .015, -.451), (.062, .038, .017), "sill")
        tire = tuple(sorted((side * .239, side * .348)))
        hub = tuple(sorted((side * .346, side * .350)))
        for z in (-.29, .30):
            mesh.cylinder("x", tire, (-.087, z), .113, "rubber")
            mesh.cylinder("x", hub, (-.087, z), .048, "steel")
    for x in (-.13, -.065, 0.0, .065, .13):
        mesh.block((x, .031, .466), (.023, .052, .016), "dark")
    return mesh


def tank():
    mesh = Mesh()
    for side in (-1, 1):
        x = side * .315
        mesh.box((x - .11, x + .11), (-.275, -.048), (-.55, .55), "rubber")
        hub = tuple(sorted((side * .419, side * .425)))
        for z in (-.406, -.203, 0, .203, .406):
            mesh.cylinder("x", hub, (-.17, z), .085, "steel")
        for z in (-.43, -.23, -.03, .17, .37):
            mesh.block((x, -.034, z), (.22, .024, .055), "dark")
    mesh.prism([(-.41, -.47), (.41, -.47), (.406, .38), (.305, .54),
                (-.305, .54), (-.406, .38)], -.048, .222, "army")
    mesh.box((-.376, .376), (.221, .235), (-.453, .39), "olive")
    mesh.prism([(-.265, -.25), (.265, -.25), (.305, -.14), (.27, .14),
                (.18, .21), (-.18, .21), (-.27, .14), (-.305, -.14)], .235, .275, "olive_light")
    mesh.cylinder("z", (.17, .545), (0.0, .181), .042, "steel")
    mesh.cylinder("z", (.493, .544), (0.0, .181), .060, "army")
    mesh.box((.137, .249), (.234, .250), (-.396, -.284), "dark")
    return mesh


def barracks():
    mesh = Mesh()
    mesh.box((-.45, .45), (-.375, -.327), (-.46, .46), "edge")
    mesh.box((-.414, .414), (-.327, .308), (-.425, .425), "sand")
    mesh.box((-.45, .45), (.308, .375), (-.46, .46), "army")
    mesh.box((-.419, .419), (-.303, -.257), (-.427, .427), "concrete")
    mesh.block((-.135, -.068, -.431), (.211, .498, .020), "dark")
    mesh.block((-.135, -.07, -.443), (.174, .447, .012), "olive")
    mesh.block((-.07, -.094, -.453), (.033, .025, .013), "paint")
    for z in (-.19, .19):
        for x in (-.419, .419):
            mesh.block((x, .015, z), (.024, .190, .245), "dark")
            beyond = -.014 if x < 0 else .014
            mesh.block((x + beyond, .015, z), (.008, .147, .195), "glass")
            mesh.block((x + beyond * 1.35, .015, z), (.014, .155, .015), "paint")
    mesh.block((.152, .015, -.431), (.252, .194, .022), "dark")
    mesh.block((.152, .015, -.445), (.201, .149, .008), "glass")
    return mesh


def create_scene(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = SCENE
    scene.world = bpy.data.worlds.new("Night sky")
    scene.world.color = (0.045, 0.071, 0.112)
    environment = bpy.data.collections.new(STATIC_COLLECTION)
    props = bpy.data.collections.new(PROP_COLLECTION)
    scene.collection.children.link(environment)
    scene.collection.children.link(props)
    ground(environment)
    edge_rails(environment)
    watch_post(environment)
    for name, make, size, x in zip(PROP_NAMES, (jeep, tank, barracks), PROP_SIZES, (7.8, 8.9, 10.0)):
        prop = make().object(name, props)
        prop.location = (x, -18.50, size[1] / 2)
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == "VIEW_3D":
                viewport = area.spaces.active
                viewport.shading.color_type = "MATERIAL"
                viewport.region_3d.view_location = (10.52, -23.3, 0.65)
                viewport.region_3d.view_rotation = Vector((0.32, -0.91, -0.36)).to_track_quat("-Z", "Y")
                viewport.region_3d.view_distance = 15.5
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(path), compress=True)


def export(path):
    pipeline = runpy.run_path(str(Path(__file__).with_name("export_geometry.py")))
    triangles = pipeline["object_triangles"]
    write = pipeline["write_triangles"]
    bounds = pipeline["world_bounds"]
    normalize = pipeline["normalized_triangles"]
    scene = bpy.data.scenes[SCENE]
    bpy.context.window.scene = scene
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write("// Generated from the Parade Ground scene by design/export_parade_geometry.py.\n")
        output.write("const MeshVertex paradeStaticGeometry[] = {\n")
        for obj in sorted(bpy.data.collections[STATIC_COLLECTION].objects, key=lambda item: item.name):
            output.write(f"    // {obj.name}\n")
            write(output, triangles(obj, depsgraph))
        output.write("};\n\nconst MeshVertex paradeMovableGeometry[] = {\n")
        offsets = [0]
        for name, expected_size in zip(PROP_NAMES, PROP_SIZES):
            data = triangles(bpy.data.collections[PROP_COLLECTION].objects[name], depsgraph)
            center, size = bounds(data)
            if any(abs(actual - expected) > 0.002 for actual, expected in zip(size, expected_size)):
                raise ValueError(f"{name} export dimensions {tuple(size)} differ from {expected_size}")
            output.write(f"    // {name}\n")
            offsets.append(offsets[-1] + write(output, normalize(data, center, size)))
            print(name, "dimensions", tuple(round(value, 3) for value in size), "vertices", offsets[-1] - offsets[-2])
        output.write("};\nconstexpr GLint paradePropOffsets[] = " + pipeline["cpp_vector"](offsets) + ";\n")


def main():
    directory = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--create", action="store_true")
    parser.add_argument("--scene", type=Path, default=directory / "연병장.blend")
    parser.add_argument("--output", type=Path, default=directory.parent / "ParadeGeometry.inc")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    if args.create:
        create_scene(args.scene)
    export(args.output)


if __name__ == "__main__":
    main()
