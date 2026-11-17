"""Export the military parade ground and its three carryable props for the game.

blender --background 연병장.blend --python export_parade_geometry.py
Use -- --create to regenerate the editable scene from the dimensions below.
"""

import argparse
import math
from random import Random
from pathlib import Path
import runpy
import sys

import bpy
from mathutils import Vector


SCENE = "Military | Parade ground"
STATIC_COLLECTION = "Parade ground"
PROP_COLLECTION = "Carryable models"
PROP_NAMES = ("Jeep", "Tank", "Barracks model")
PROP_SIZES = ((2.20, 2.00, 4.90), (3.60, 2.40, 10.80), (0.90, 0.75, 0.92))
PROP_STARTS = ((-5.0, 1.0, -12.5), (6.5, 1.2, -8.5), (0.0, .375, -10.5))

PALETTE = {
    "asphalt": (0.36, 0.40, 0.44),
    "base_concrete": (0.33, 0.37, 0.39),
    "recess": (0.08, 0.12, 0.16),
    "lit_glass": (0.49, 0.47, 0.35),
    "fir": (0.19, 0.27, 0.24),
    "fir_dark": (0.14, 0.20, 0.18),
    "ridge_2": (0.19, 0.24, 0.28),
    "ridge_3": (0.17, 0.22, 0.26),
    "ridge_4": (0.22, 0.28, 0.33),
    "ridge_5": (0.20, 0.26, 0.32),
    "ridge_6": (0.27, 0.33, 0.39),
    "ridge_7": (0.25, 0.31, 0.37),
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
    mesh.box((-27.0, 27.0), (-0.20, 0.0), (-24.0, 34.0), "asphalt")
    mesh.box((-75.0, 75.0), (-0.45, -0.21), (-78.0, 78.0), "grass")
    for x in range(-25, 26, 5):
        mesh.block((x, 0.002, 5.0), (0.024, 0.004, 56.0), "seam")
    for z in range(-20, 34, 5):
        mesh.block((0.0, 0.001, z), (53.6, 0.002, 0.026), "seam")
    for x in (-26.60, 26.60):
        mesh.block((x, 0.005, 5.0), (0.06, 0.006, 57.3), "mark")
    for z in (8.0, 10.6, 13.2, 15.8):
        mesh.block((0.0, 0.005, z), (0.11, 0.006, 1.20), "mark")
    for (x, _, z), (width, _, length) in zip(PROP_STARTS[:2], PROP_SIZES[:2]):
        bay_width = width + 1.1
        bay_length = length + 1.3
        for side in (-1, 1):
            mesh.block((x + side * bay_width / 2, 0.005, z), (.065, .006, bay_length), "mark")
            mesh.block((x, 0.005, z + side * bay_length / 2), (bay_width, .006, .065), "mark")
    for x in (-2.32, 2.32):
        mesh.block((x, .005, -22.2), (.055, .006, 3.5), "mark")
    mesh.object("Paved parade square and parking lines", collection)


def perimeter(collection):
    rails = Mesh()
    for x in (-27.05, 27.05):
        for z in range(-22, 34, 4):
            rails.block((x, .56, z), (.16, 1.15, .16), "steel")
            rails.block((x, 1.15, z), (.19, .055, .19), "sill")
        for y in (.44, .94):
            rails.block((x, y, 5.0), (.075, .085, 58.0), "rail")
    for x0, x1, z in ((-27.0, -2.3, -24.03), (2.3, 27.0, -24.03), (-27.0, 27.0, 34.03)):
        divisions = math.ceil((x1 - x0) / 4)
        for index in range(divisions + 1):
            x = x0 + index * (x1 - x0) / divisions
            rails.block((x, .56, z), (.16, 1.15, .16), "steel")
        for y in (.44, .94):
            rails.box((x0, x1), (y - .04, y + .04), (z - .04, z + .04), "rail")
    rails.object("Perimeter barrier", collection)

    lights = Mesh()
    for side in (-1, 1):
        for z in (-17.0, -3.0, 11.0, 27.0):
            x = side * 27.30
            lights.block((x, 2.45, z), (.17, 4.94, .17), "steel")
            lights.block((x - side * .32, 4.85, z), (.78, .14, .12), "steel")
            lights.block((x - side * .70, 4.80, z), (.50, .19, .32), "dark")
            lights.block((x - side * .70, 4.69, z), (.37, .03, .26), "lamp")
    lights.object("Perimeter lamp posts", collection)


def return_gate(collection):
    mesh = Mesh()
    for x in (-2.46, 2.46):
        mesh.block((x, 1.59, -23.30), (.32, 3.18, .42), "steel")
        mesh.block((x, 2.63, -23.05), (.15, .21, .08), "lamp")
    mesh.box((-2.62, 2.62), (2.96, 3.23), (-23.55, -23.05), "steel")
    mesh.box((-1.85, 1.85), (3.025, 3.155), (-23.04, -23.015), "edge")
    for x in (-.9, 0.0, .9):
        mesh.block((x, 3.09, -23.005), (.64, .075, .016), "lamp")
    mesh.object("Return gate", collection)


def barracks_building(collection):
    shell = Mesh()
    shell.box((-10.0, 10.0), (0.0, 3.20), (20.0, 32.0), "base_concrete")
    for x0, x1 in ((-10.0, -.90), (.90, 10.0)):
        shell.box((x0, x1), (3.20, 12.80), (20.0, 32.0), "tower")
    # The dark wall behind the second-floor opening closes the building beyond the
    # walkable threshold and leaves usable clearance inside the portal.
    shell.box((-.90, .90), (3.20, 5.4), (20.90, 32.0), "recess")
    shell.box((-.90, .90), (5.4, 12.80), (20.0, 32.0), "tower")
    shell.box((-10.0, 10.0), (12.80, 13.20), (20.0, 32.0), "edge")
    shell.object("Four-story barracks structure", collection)

    facade = Mesh()
    for y in (3.20, 6.40, 9.60, 12.75):
        facade.box((-10.0, 10.0), (y - .105, y), (19.96, 20.01), "concrete")
        facade.box((-10.0, 10.0), (y - .105, y), (31.99, 32.03), "concrete")
        for x in (-10.02, 10.02):
            facade.block((x, y - .0525, 26.0), (.06, .105, 12.0), "concrete")
    for x in (-9.8, -2.27, 2.27, 9.8):
        facade.box((x - .16, x + .16), (.30, 12.77), (19.95, 20.02), "concrete")
    for level in range(4):
        y = 1.62 + 3.20 * level
        for bay, x in enumerate((-8.50, -6.65, -4.85, -3.15, 3.15, 4.85, 6.65, 8.50)):
            for z in (19.924, 32.025):
                facade.block((x, y, z), (1.35, 1.48, .048), "edge")
                facade.block((x, y + .033, z + (-.026 if z < 20 else .026)), (1.14, 1.25, .022),
                             "lit_glass" if (level + bay * 3) % 7 == 3 else "glass")
                facade.block((x, y, z + (-.048 if z < 20 else .048)), (.033, 1.26, .020), "edge")
                facade.block((x, y - .72, z + (-.04 if z < 20 else .04)), (1.46, .095, .09), "concrete")
        for x in (-10.025, 10.025):
            for z in (22.4, 25.95, 29.50):
                facade.block((x, y, z), (.062, 1.48, 1.48), "edge")
                offset = math.copysign(.039, x)
                facade.block((x + offset, y, z), (.021, 1.24, 1.28), "glass")
                facade.block((x + offset * 1.43, y, z), (.019, 1.24, .035), "edge")
    facade.object("Barracks facade bands and window rows", collection)

    entry = Mesh()
    entry.box((-2.30, 2.30), (0.0, 3.075), (16.75, 20.0), "base_concrete")
    entry.box((-2.30, 2.30), (3.075, 3.20), (16.749, 20.0), "concrete")
    entry.box((-2.30, 2.30), (3.200, 3.204), (16.75, 16.92), "sill")
    entry.box((-.90, .90), (3.200, 3.204), (19.84, 20.40), "sill")
    for x in (-.98, .98):
        entry.block((x, 4.30, 19.965), (.16, 2.20, .11), "sill")
    entry.box((-1.06, 1.06), (5.40, 5.57), (19.90, 20.01), "sill")
    for x in (-1.30, 1.30):
        entry.block((x, 5.03, 19.94), (.22, .28, .12), "edge")
        entry.block((x, 5.03, 19.86), (.14, .19, .055), "lamp")
    entry.object("Second-floor entrance and solid landing", collection)


def mountain_ring(collection):
    random = Random(1207)
    for layer, (radius, height, width) in enumerate(((58, 16, 12), (83, 30, 17), (132, 47, 23))):
        mesh = Mesh()
        count = 96
        rows = [[], [], [], []]
        for index in range(count):
            angle = 2 * math.pi * index / count
            radius_noise = 2.8 * math.sin(5 * angle + layer) + 2.0 * math.cos(11 * angle)
            ridge = radius + radius_noise
            peaks = .61 + .19 * math.sin(5 * angle + layer) + .12 * math.sin(13 * angle - .8 * layer)
            peak = height * (peaks + random.uniform(-.08, .12))
            crest_angle = angle + .022 * math.sin(21 * angle)
            section = ((ridge - width, -.22, angle), (ridge - width * .45, peak * .38, angle),
                       (ridge, peak, crest_angle), (ridge + width, -.26, angle))
            for row, (distance, y, bearing) in zip(rows, section):
                row.append((distance * math.cos(bearing), y, 4.0 + distance * math.sin(bearing)))
        for index in range(count):
            other = (index + 1) % count
            for band in range(len(rows) - 1):
                low, high = rows[band], rows[band + 1]
                tone = (layer + 1) * 2
                mesh.add((low[index], high[index], low[other], high[other]),
                         ((0, 1, 2),), f"ridge_{tone}")
                mesh.add((low[index], high[index], low[other], high[other]),
                         ((1, 3, 2),), f"ridge_{tone + 1}")
        mesh.object(f"Mountain ridge layer {layer + 1}", collection)

    trees = Mesh()
    for index in range(111):
        angle = (index + random.uniform(-.35, .35)) * 2 * math.pi / 111
        radius = random.uniform(32.0, 47.0)
        x = radius * math.cos(angle)
        z = 4.0 + radius * math.sin(angle)
        if -28.0 < x < 28.0 and -25.0 < z < 35.0:
            continue
        height = random.uniform(2.1, 5.2)
        trees.block((x, .29, z), (.15, .66, .15), "edge")
        for tier in range(3):
            y = height * (.13 + .23 * tier)
            reach = height * (.22 - .044 * tier)
            ring = [(x + reach * math.cos(side * math.tau / 5 + angle), y,
                     z + reach * math.sin(side * math.tau / 5 + angle)) for side in range(5)]
            vertices = ring + [(x, y + height * .43, z)]
            trees.add(vertices, [((edge + 1) % 5, edge, 5) for edge in range(5)],
                      "fir_dark" if tier == 0 else "fir")
    trees.object("Evergreen treeline", collection)


def jeep():
    mesh = Mesh()
    mesh.box((-.92, .92), (-.58, .12), (-2.45, 2.45), "army")
    mesh.prism([(-.95, -2.40), (.95, -2.40), (1.01, -1.85), (1.01, -.6),
                (-1.01, -.6), (-1.01, -1.85)], -.12, .36, "olive")
    mesh.box((-.94, .94), (.08, .37), (.87, 2.30), "olive")
    mesh.box((-.88, .88), (.16, .65), (-.68, 1.99), "army")
    mesh.box((-.99, .99), (.91, 1.0), (-.64, 2.02), "olive")
    mesh.box((-.87, .87), (.40, .87), (-.72, -.63), "glass")
    mesh.box((-.87, .87), (.43, .87), (1.93, 1.99), "glass")
    for x in (-.82, 0.0, .82):
        mesh.block((x, .65, -.735), (.061, .48, .08), "dark")
    for side in (-1, 1):
        x = side * .92
        mesh.block((x, .65, .56), (.055, .47, 2.29), "glass")
        for z in (-.57, .61, 1.68):
            mesh.block((x, .64, z), (.076, .56, .11), "olive")
        for z in (.18, 1.34):
            mesh.block((x, .29, z), (.085, .07, .22), "dark")
        mesh.block((side * .67, .092, -2.426), (.31, .18, .045), "paint")
        mesh.block((side * .69, -.02, 2.424), (.20, .14, .043), "sill")
        for z in (-1.54, 1.60):
            tire = tuple(sorted((side * .79, side * 1.085)))
            hub = tuple(sorted((side * 1.08, side * 1.10)))
            mesh.cylinder("x", tire, (-.56, z), .44, "rubber", sides=16)
            mesh.cylinder("x", hub, (-.56, z), .22, "steel", sides=12)
            mesh.block((side * .97, -.045, z), (.25, .10, 1.08), "army")
    for x in (-.32, -.20, -.08, .08, .20, .32):
        mesh.block((x, .067, -2.428), (.048, .20, .043), "dark")
    return mesh


def tank():
    mesh = Mesh()
    for side in (-1, 1):
        x = side * 1.46
        track_left, track_right = sorted((x - side * .34, side * 1.79))
        track_outline = [(track_left, -2.0), (track_right, -2.0), (track_right, 5.40), (track_left, 5.40)]
        mesh.prism(track_outline, -1.20, -.20, "rubber")
        hub = tuple(sorted((side * 1.765, side * 1.80)))
        for z in (-1.57, -.51, .55, 1.61, 2.67, 3.73, 4.79):
            mesh.cylinder("x", hub, (-.68, z), .36, "steel")
        for z in (-1.45, -.37, .71, 1.79, 2.87, 3.95, 5.03):
            mesh.block((x, -.185, z), (.63, .065, .14), "dark")
    mesh.prism([(-1.66, -1.65), (-1.33, -2.08), (1.33, -2.08), (1.66, -1.65),
                (1.66, 4.98), (1.43, 5.35), (-1.43, 5.35), (-1.66, 4.98)], -.27, .78, "army")
    mesh.box((-1.53, 1.53), (.76, .84), (-1.66, 5.20), "olive")
    mesh.prism([(-.94, -.81), (.94, -.81), (1.27, -.40), (1.21, 1.75),
                (.91, 2.06), (-.91, 2.06), (-1.21, 1.75), (-1.27, -.40)], .79, 1.20, "olive_light")
    mesh.cylinder("z", (-5.40, -.66), (.955, 0.0), .135, "steel")
    mesh.cylinder("z", (-4.92, -4.41), (.955, 0.0), .186, "army")
    mesh.box((-.37, .35), (.842, .896), (3.55, 4.2), "dark")
    for side in (-1, 1):
        mesh.block((side * .96, .965, -.89), (.20, .12, .15), "dark")
        for z in (.72, 1.12, 1.52):
            mesh.block((side * 1.24, 1.00, z), (.09, .19, .19), "edge")
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
    perimeter(environment)
    return_gate(environment)
    barracks_building(environment)
    mountain_ring(environment)
    for name, make, (x, y, z) in zip(PROP_NAMES, (jeep, tank, barracks), PROP_STARTS):
        prop = make().object(name, props)
        prop.location = (x, -z, y)
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == "VIEW_3D":
                viewport = area.spaces.active
                viewport.shading.color_type = "MATERIAL"
                viewport.region_3d.view_location = (0.0, -10.5, 1.8)
                viewport.region_3d.view_rotation = Vector((0.25, -0.92, -0.29)).to_track_quat("-Z", "Y")
                viewport.region_3d.view_distance = 44.0
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
