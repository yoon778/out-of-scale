"""Rebuild the authored bedroom and its matching linked tabletop model in Blender."""

import math
import random
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


OUTPUT = Path(__file__).resolve().parent
GROUP_NAMES = (
    "Interior", "Shell", "IntactWall", "BrokenWall", "MiniWalls",
    "Shelf", "Door", "Rpg", "Rack", "MiniBrokenWall", "LatchMounts", "LatchBolt",
    "CabinetLeftDoor", "CabinetRightDoor", "CabinetMiniatures",
    "LockSocket", "SchoolDiorama", "MilitaryDiorama", "OfficeDiorama",
    "HomeDioramaBase",
    "PictureDoor", "Crate",
)
DIORAMA_ORIGINS = {
    "SchoolDiorama": (0.40, 0.95, 9.30),
    "MilitaryDiorama": (5.60, 0.95, 9.30),
    "OfficeDiorama": (0.40, 0.95, 14.20),
}
HOME_MODEL_ORIGIN = Vector((5.60, 0.95, 14.20))
HOME_MODEL_CENTER = Vector((0.945, 0, 3.895))
PICTURE_HINGE = Vector((-5.13, 2.00, -6.26))
HIDDEN_SOCKET_CENTER = Vector((-4.48, 1.93, -6.30))
RANDOM = random.Random(20261120)
COLLECTIONS = {}
MATERIALS = {}


def point(position):
    x, y, z = position
    return Vector((x, -z, y))


def home_model_position(position):
    local = Vector(position) - HOME_MODEL_CENTER
    return HOME_MODEL_ORIGIN + Vector((local.z, local.y, -local.x)) * 0.075


def material(name, color, roughness=0.65, metallic=0.0, emission=0.0, texture=None):
    result = bpy.data.materials.new("Home | " + name)
    result.diffuse_color = (*color, 1.0)
    result.use_nodes = True
    result["home_emission"] = emission
    shader = result.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1.0)
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = metallic
    if emission:
        shader.inputs["Emission Color"].default_value = (*color, 1.0)
        shader.inputs["Emission Strength"].default_value = emission
    if texture:
        nodes = result.node_tree.nodes
        links = result.node_tree.links
        coordinates = nodes.new("ShaderNodeTexCoord")
        mapping = nodes.new("ShaderNodeVectorMath")
        mapping.operation = "MULTIPLY"
        mapping.inputs[1].default_value = (8.0, 120.0, 8.0) if texture == "wood" else (120.0,) * 3
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Roughness"].default_value = 0.7
        noise.inputs["Detail"].default_value = 2.0
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.11
        bump.inputs["Distance"].default_value = 0.025 if texture == "wood" else 0.008
        links.new(coordinates.outputs["Generated"], mapping.inputs[0])
        links.new(mapping.outputs["Vector"], noise.inputs["Vector"])
        links.new(noise.outputs["Fac"], bump.inputs["Height"])
        links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    MATERIALS[name] = result
    return result


def mesh(group, name, vertices, faces, surface):
    data = bpy.data.meshes.new(name)
    data.from_pydata([point(vertex) for vertex in vertices], [], faces)
    data.materials.append(MATERIALS[surface])
    data.update()
    obj = bpy.data.objects.new(name, data)
    obj["home_group"] = group
    COLLECTIONS[group].objects.link(obj)
    return obj


def bevel(obj, width, segments=2):
    modifier = obj.modifiers.new("Hand finished edges", "BEVEL")
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = "ANGLE"
    modifier = obj.modifiers.new("Weighted surface normals", "WEIGHTED_NORMAL")
    modifier.keep_sharp = True
    return obj


def box(group, name, center, size, surface, radius=0.0, segments=2):
    x, y, z = center
    a, b, c = (value * 0.5 for value in size)
    vertices = [
        (x - a, y - b, z - c), (x + a, y - b, z - c),
        (x + a, y - b, z + c), (x - a, y - b, z + c),
        (x - a, y + b, z - c), (x + a, y + b, z - c),
        (x + a, y + b, z + c), (x - a, y + b, z + c),
    ]
    result = mesh(group, name, vertices,
                  [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1),
                   (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)], surface)
    if radius:
        bevel(result, radius, segments)
    return result


def cylinder(group, name, start, end, radius, surface, sides=12, end_radius=None):
    axis = (Vector(end) - Vector(start)).normalized()
    right = axis.cross(Vector((0.0, 1.0, 0.0)))
    if right.length < 0.1:
        right = axis.cross(Vector((1.0, 0.0, 0.0)))
    right.normalize()
    up = axis.cross(right)
    vertices = []
    for center, local_radius in ((start, radius), (end, end_radius if end_radius is not None else radius)):
        for index in range(sides):
            angle = index * math.tau / sides
            vertices.append(tuple(Vector(center) + (right * math.cos(angle) + up * math.sin(angle)) * local_radius))
    faces = [tuple(reversed(range(sides))), tuple(range(sides, sides * 2))]
    faces.extend((index, (index + 1) % sides, (index + 1) % sides + sides, index + sides)
                 for index in range(sides))
    return mesh(group, name, vertices, faces, surface)


def rounded_fabric(group, name, center, size, surface, nx=26, nz=38, phase=0.0):
    x, y, z = center
    sx, sy, sz = size
    vertices = []
    for row in range(nz + 1):
        v = row / nz
        for column in range(nx + 1):
            u = column / nx
            edge = math.sin(math.pi * u) * math.sin(math.pi * v)
            wrinkle = (math.sin(u * 31.0 + v * 15.0 + phase) * 0.009
                       + math.cos(v * 43.0 - u * 10.0) * 0.007) * edge
            vertices.append((x + (u - 0.5) * sx,
                             y + sy * math.sqrt(max(0.0, edge)) + wrinkle,
                             z + (v - 0.5) * sz))
    faces = []
    for row in range(nz):
        for column in range(nx):
            a = row * (nx + 1) + column
            faces.append((a, a + nx + 1, a + nx + 2, a + 1))
    obj = mesh(group, name, vertices, faces, surface)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def book(group, name, x, y, z, width, height, depth, surface):
    box(group, name + " pages", (x, y + height * 0.5, z),
        (width - 0.012, height - 0.018, depth - 0.014), "Paper")
    for side in (-1, 1):
        box(group, name + " cover", (x + side * (width * 0.5 - 0.004), y + height * 0.5, z),
            (0.008, height, depth), surface)
    box(group, name + " spine", (x, y + height * 0.5, z + depth * 0.5),
        (width, height, 0.014), surface)
    box(group, name + " cloth headband", (x, y + height * 0.81, z + depth * 0.5 + 0.008),
        (max(0.012, width * 0.65), 0.012, 0.002), "MutedGold")


def add_palette():
    material("Plaster", (0.71, 0.676, 0.575), texture="fabric")
    material("Ceiling", (0.77, 0.75, 0.67))
    material("SageWall", (0.29, 0.367, 0.303))
    material("Skirting", (0.69, 0.642, 0.532))
    material("Walnut", (0.20, 0.105, 0.055), 0.46, texture="wood")
    material("WalnutEdge", (0.29, 0.160, 0.080), 0.43, texture="wood")
    material("DarkRecess", (0.057, 0.055, 0.043))
    material("Oak", (0.39, 0.257, 0.145), texture="wood")
    material("Metal", (0.055, 0.071, 0.068), 0.37, 0.7)
    material("MutedGold", (0.50, 0.32, 0.13), 0.38, 0.55)
    material("Linen", (0.68, 0.647, 0.532), 0.98, texture="fabric")
    material("SageLinen", (0.25, 0.345, 0.255), 0.96, texture="fabric")
    material("DarkLinen", (0.17, 0.245, 0.196), 0.98, texture="fabric")
    material("CreamRug", (0.50, 0.472, 0.371), 0.99, texture="fabric")
    material("RugWeave", (0.34, 0.346, 0.277), 0.99)
    material("Terracotta", (0.39, 0.152, 0.081), 0.84)
    material("Paper", (0.79, 0.753, 0.632), 0.95)
    material("BlueBook", (0.104, 0.199, 0.251), 0.86)
    material("RedBook", (0.305, 0.092, 0.056), 0.86)
    material("GreenBook", (0.175, 0.258, 0.184), 0.86)
    material("OcherBook", (0.48, 0.327, 0.143), 0.86)
    material("BlackPlastic", (0.027, 0.036, 0.033), 0.6)
    material("Screen", (0.055, 0.106, 0.103), 0.4, emission=0.35)
    material("ScreenLight", (0.18, 0.345, 0.29), 0.7, emission=0.3)
    material("WarmLight", (0.95, 0.67, 0.31), 0.5, emission=1.7)
    material("Sky", (0.58, 0.695, 0.66), emission=0.35)
    material("FarMountain", (0.27, 0.43, 0.40), emission=0.14)
    material("NearMountain", (0.12, 0.28, 0.24), emission=0.1)
    material("Leaf", (0.105, 0.245, 0.12), 0.85)
    material("LeafLight", (0.20, 0.337, 0.147), 0.85)
    material("RpgGreen", (0.142, 0.23, 0.095), 0.7)
    material("HoleInner", (0.23, 0.217, 0.177), 0.9)
    for index in range(9):
        k = 0.87 + index * 0.031
        material(f"OakFloor{index}", (0.355 * k, 0.237 * k, 0.133 * k), texture="wood")


def add_floor():
    group = "Interior"
    box(group, "Floor slab and shadowed joints", (0, -0.10, 0), (11.7, 0.20, 13.1), "DarkRecess")
    plank_width = 11.7 / 44
    for row in range(44):
        x = -5.85 + (row + 0.5) * plank_width
        ends = [-6.55]
        edge = -6.55 + (row % 3) * 0.88 + 0.70
        while edge < 6.55:
            ends.append(edge)
            edge += 2.58 + RANDOM.random() * 0.29
        ends.append(6.55)
        for index, (a, b) in enumerate(zip(ends, ends[1:])):
            box(group, f"Oak plank {row:02d}-{index}", (x, -0.009, (a + b) * 0.5),
                (plank_width - 0.003, 0.020, b - a - 0.003), f"OakFloor{RANDOM.randrange(9)}",
                0.0015, 1)
            if row % 3 == 0:
                for streak in (-1, 1):
                    z0 = a + (b - a) * (0.18 if streak < 0 else 0.48)
                    box(group, "Subtle oak growth line", (x + streak * 0.074, 0.0016, z0),
                        (0.002, 0.0005, (b - a) * 0.27), f"OakFloor{max(0, (row + index) % 9 - 1)}")


def add_shell():
    group = "Shell"
    box(group, "Ceiling", (0, 3.31, 0), (12.0, 0.22, 13.35), "Ceiling")
    box(group, "Right plaster wall", (5.87, 1.60, 0), (0.34, 3.20, 13.1), "Plaster")
    box(group, "Front sage wall left of secret passage", (-1.95, 1.60, 6.51), (7.80, 3.20, 0.34), "SageWall")
    box(group, "Front sage wall right of secret passage", (6.695, 1.60, 6.51), (2.29, 3.20, 0.34), "SageWall")
    box(group, "Secret passage lintel", (3.75, 2.95, 6.51), (3.60, 0.50, 0.34), "SageWall")
    # A genuine window recess instead of scenery layered across an unbroken wall.
    box(group, "Left wall under window", (-5.87, 0.49, 0), (0.34, 0.98, 13.1), "Plaster")
    box(group, "Left window lintel", (-5.87, 2.95, 0), (0.34, 0.50, 13.1), "Plaster")
    box(group, "Left wall rear pier", (-5.87, 1.84, -5.13), (0.34, 1.72, 2.84), "Plaster")
    box(group, "Left wall forward pier", (-5.87, 1.84, 2.91), (0.34, 1.72, 7.28), "Plaster")
    for x in (-5.68, 5.68):
        box(group, "Continuous side skirting", (x, 0.075, 0), (0.045, 0.15, 13.08), "Skirting", 0.008)
        box(group, "Cove edge", (x, 3.11, 0), (0.065, 0.15, 13.08), "Skirting", 0.008)
    box(group, "Front skirting left of passage", (-1.8625, 0.075, 6.315), (7.625, 0.15, 0.045), "Skirting", 0.008)
    box(group, "Front skirting right of passage", (5.6125, 0.075, 6.315), (0.125, 0.15, 0.045), "Skirting", 0.008)
    box(group, "Front cove", (0, 3.11, 6.315), (11.35, 0.15, 0.065), "Skirting", 0.008)
    # One continuous face prevents baked AO from outlining the later breakable opening.
    box("IntactWall", "Continuous plain back wall", (0, 1.6, -6.5), (11.7, 3.2, 0.31), "Plaster")
    box("IntactWall", "Continuous back skirting", (0, 0.075, -6.32), (11.35, 0.15, 0.045), "Skirting", 0.006)
    box("IntactWall", "Continuous back cove", (0, 3.11, -6.31), (11.35, 0.15, 0.065), "Skirting", 0.008)
    group = "BrokenWall"
    for side in (-1, 1):
        box(group, "Back wall side", (side * 3.37, 1.6, -6.5), (4.94, 3.2, 0.31), "Plaster")
        box(group, "Back skirting sides", (side * 3.37, 0.075, -6.32), (4.94, 0.15, 0.045), "Skirting", 0.006)
    box(group, "Back lintel wall", (0, 2.9, -6.5), (1.8, 0.6, 0.31), "Plaster")
    box(group, "Back cove", (0, 3.11, -6.31), (11.35, 0.15, 0.065), "Skirting", 0.008)
    for x in (-0.96, 0.96):
        box(group, "Recessed door jamb", (x, 1.31, -6.63), (0.12, 2.66, 0.33), "WalnutEdge", 0.008)
    box(group, "Recessed door header", (0, 2.65, -6.63), (2.04, 0.12, 0.33), "WalnutEdge", 0.008)
    box(group, "Threshold", (0, 0.012, -6.65), (1.8, 0.024, 0.48), "Oak", 0.006)
    box(group, "Exit corridor floor", (0, -0.08, -7.65), (1.8, 0.16, 2.4), "Oak")
    for x in (-1.07, 1.07):
        box(group, "Exit corridor wall", (x, 1.6, -7.65), (0.32, 3.20, 2.4), "HoleInner")
    box(group, "Exit corridor ceiling", (0, 3.12, -7.65), (2.0, 0.16, 2.4), "HoleInner")
    box(group, "Exit warm end", (0, 1.6, -8.85), (1.8, 3.2, 0.08), "WarmLight")
    group = "MiniWalls"
    for side in (-1, 1):
        box(group, "Cutaway side wall", (side * 5.87, 0.30, 0), (0.34, 0.60, 13.1), "Plaster")
        box(group, "Cutaway rear wall", (side * 3.37, 0.30, -6.5), (4.94, 0.60, 0.31), "Plaster")
    box(group, "Cutaway front wall left of passage", (-1.95, 0.30, 6.51), (7.80, 0.60, 0.34), "SageWall")
    box(group, "Cutaway front wall right of passage", (6.695, 0.30, 6.51), (2.29, 0.60, 0.34), "SageWall")


def add_bed():
    group = "Interior"
    box(group, "Bed floating walnut plinth", (-3.75, 0.16, 1.8), (2.21, 0.31, 3.50), "Walnut", 0.045)
    box(group, "Bed upholstered base", (-3.75, 0.37, 1.8), (2.35, 0.28, 3.65), "DarkLinen", 0.075, 3)
    box(group, "Mattress", (-3.75, 0.58, 1.8), (2.19, 0.22, 3.49), "Linen", 0.09, 3)
    box(group, "Upholstered headboard", (-3.75, 0.77, -0.04), (2.35, 1.19, 0.16), "DarkLinen", 0.055, 3)
    rounded_fabric(group, "Soft sage duvet", (-3.75, 0.72, 2.12), (2.20, 0.13, 2.83), "SageLinen", 40, 72)
    rounded_fabric(group, "Folded linen throw", (-3.75, 0.855, 2.85), (2.23, 0.035, 0.69), "Linen", 24, 15, 1.4)
    for side in (-1, 1):
        rounded_fabric(group, "Linen pillow", (-3.75 + side * 0.55, 0.73, 0.46),
                       (0.96, 0.17, 0.62), "Linen", 30, 18, side)
    rounded_fabric(group, "Sage accent cushion", (-3.75, 0.75, 0.87),
                   (0.71, 0.14, 0.45), "DarkLinen", 23, 14)
    for side in (-1, 1):
        for z in (0.35, 3.25):
            cylinder(group, "Bed tapered foot", (-3.75 + side * 0.91, 0.03, z),
                     (-3.75 + side * 0.91, 0.28, z), 0.047, "Walnut", end_radius=0.055)


def add_rug():
    group = "Interior"
    # The shallow rug remains traversable and continues below the bed.
    box(group, "Woven bedside rug", (-2.35, 0.012, 1.95), (3.58, 0.022, 4.38), "CreamRug", 0.015, 2)
    for x in (-4.06, -0.64):
        box(group, "Rug woven border", (x, 0.025, 1.95), (0.045, 0.001, 4.22), "RugWeave")
    for z in (-0.15, 4.05):
        box(group, "Rug woven end", (-2.35, 0.025, z), (3.44, 0.001, 0.045), "RugWeave")
        for index in range(50):
            x = -4.04 + index * 0.069
            cylinder(group, "Cotton rug fringe", (x, 0.012, z),
                     (x + 0.005, 0.012, z + (-0.075 if z < 0 else 0.075)), 0.004, "Linen", 4)


def add_storage():
    group = "Interior"
    box(group, "Bedside carcass", (-4.85, 0.38, -0.55), (0.85, 0.70, 0.78), "Walnut", 0.026)
    box(group, "Bedside top", (-4.85, 0.75, -0.55), (0.86, 0.04, 0.79), "WalnutEdge", 0.010)
    for y in (0.26, 0.54):
        box(group, "Bedside drawer", (-4.85, y, -0.145), (0.76, 0.235, 0.026), "WalnutEdge", 0.007)
        cylinder(group, "Bedside brass pull", (-4.99, y + 0.025, -0.115),
                 (-4.71, y + 0.025, -0.115), 0.012, "MutedGold", 8)
    box(group, "Dresser carcass", (4.90, 0.72, -3.25), (1.20, 1.40, 1.50), "Walnut", 0.03)
    for index in range(4):
        y = 0.235 + index * 0.31
        box(group, "Inset dresser drawer", (4.285, y, -3.25), (0.035, 0.27, 1.37), "WalnutEdge", 0.007)
        cylinder(group, "Dresser brass pull", (4.252, y + 0.03, -3.38),
                 (4.252, y + 0.03, -3.12), 0.011, "MutedGold", 8)
    box(group, "Dresser top lip", (4.90, 1.445, -3.25), (1.24, 0.05, 1.54), "WalnutEdge", 0.012)
    # The chair lives in the front-left corner, away from the route to the model.
    box(group, "Reading chair timber base", (-3.65, 0.25, 4.65), (1.1, 0.14, 0.90), "Walnut", 0.025)
    box(group, "Reading chair seat", (-3.65, 0.42, 4.65), (1.13, 0.24, 0.91), "SageLinen", 0.09, 3)
    box(group, "Reading chair back", (-3.65, 0.81, 5.03), (1.16, 0.71, 0.22), "SageLinen", 0.09, 3)
    for side in (-1, 1):
        box(group, "Chair oak arm", (-3.65 + side * 0.60, 0.65, 4.64), (0.07, 0.08, 1.05), "Oak", 0.025)
        for z in (4.30, 4.98):
            cylinder(group, "Chair leg", (-3.65 + side * 0.50, 0.02, z),
                     (-3.65 + side * 0.50, 0.57, z), 0.038, "Walnut", 10)


def add_desk():
    group = "Interior"
    box(group, "Walnut worktop", (2.90, 0.96, 0.80), (3.30, 0.08, 1.40), "WalnutEdge", 0.022, 3)
    box(group, "Desk inset underside", (2.90, 0.88, 0.80), (3.12, 0.10, 1.26), "Walnut", 0.01)
    for x in (1.43, 4.37):
        for z in (0.25, 1.35):
            cylinder(group, "Desk tapered leg", (x, 0.02, z), (x, 0.91, z),
                     0.047, "Walnut", 10, 0.064)
    # The computer tower stays below the desk while its display mounts on the right wall.
    box(group, "Computer tower enclosure", (3.85, 0.36, 0.80), (0.38, 0.70, 0.65),
        "Metal", 0.014, 2)
    box(group, "Computer tower inset front", (3.85, 0.355, 1.127), (0.32, 0.60, 0.012),
        "BlackPlastic", 0.010)
    box(group, "Computer tower smoked side panel", (3.655, 0.375, 0.80), (0.008, 0.49, 0.49),
        "DarkRecess", 0.012)
    for y in (0.27, 0.48):
        cylinder(group, "Computer fan metal surround", (3.85, y, 1.133), (3.85, y, 1.138),
                 0.076, "Metal", 20)
        cylinder(group, "Computer fan dark recess", (3.85, y, 1.139), (3.85, y, 1.141),
                 0.063, "BlackPlastic", 20)
        for blade in range(5):
            angle = blade * math.tau / 5
            vertices = [(3.85, y, 1.143)]
            for a, radius in ((angle, 0.023), (angle + 0.42, 0.057), (angle + 1.02, 0.052)):
                vertices.append((3.85 + math.cos(a) * radius, y + math.sin(a) * radius, 1.143))
            mesh(group, "Computer fan curved blade", vertices, [(0, 1, 2, 3)], "Metal")
        cylinder(group, "Computer fan central hub", (3.85, y, 1.143), (3.85, y, 1.146),
                 0.018, "BlackPlastic", 12)
    cylinder(group, "Computer power button", (3.85, 0.643, 1.134), (3.85, 0.643, 1.139),
             0.014, "Metal", 12)
    cylinder(group, "Computer power indicator", (3.85, 0.643, 1.140), (3.85, 0.643, 1.142),
             0.004, "ScreenLight", 8)
    for x in (3.75, 3.80):
        box(group, "Computer front USB socket", (x, 0.643, 1.135), (0.026, 0.011, 0.004), "DarkRecess")
    for z in (0.62, 0.69, 0.76, 0.83, 0.90, 0.97):
        box(group, "Computer side ventilation slit", (3.649, 0.60, z),
            (0.003, 0.045, 0.008), "BlackPlastic")
    for x in (3.73, 3.97):
        for z in (0.56, 1.04):
            box(group, "Computer rubber foot", (x, 0.016, z), (0.058, 0.026, 0.062), "BlackPlastic", 0.005, 1)
    cable = ((3.97, 0.39, 0.468), (4.06, 0.35, 0.38), (4.08, 0.31, 0.23),
             (4.02, 0.34, 0.17), (4.01, 0.65, 0.16), (4.01, 0.88, 0.17))
    for start, end in zip(cable, cable[1:]):
        cylinder(group, "Computer rear cable bundle", start, end, 0.012, "BlackPlastic", 8)
    box(group, "Keyboard deck", (3.69, 1.027, 0.82), (0.81, 0.035, 0.29), "BlackPlastic", 0.012)
    for row in range(4):
        for column in range(13):
            box(group, "Keyboard key", (3.337 + column * 0.055, 1.050, 0.722 + row * 0.060),
                (0.045, 0.012, 0.047), "Metal", 0.003, 1)
    box(group, "Keyboard space bar", (3.69, 1.05, 0.976), (0.26, 0.012, 0.025), "Metal", 0.002, 1)
    box(group, "Mouse mat", (4.29, 1.007, 0.85), (0.30, 0.008, 0.39), "DarkLinen", 0.01)
    box(group, "Wireless mouse", (4.29, 1.032, 0.85), (0.09, 0.045, 0.15), "BlackPlastic", 0.02, 3)
    for index in range(3):
        box(group, "Loose drawing paper", (3.98, 1.005 + index * 0.002, 1.27),
            (0.38 + index * 0.007, 0.001, 0.30), "Paper")
    cylinder(group, "Pencil", (4.00, 1.018, 1.22), (4.25, 1.018, 1.28), 0.004, "OcherBook", 6)
    # A restrained articulated task light remains clear of the model on the left half.
    cylinder(group, "Task light base", (4.33, 1.004, 0.30), (4.33, 1.035, 0.30), 0.11, "Metal", 20)
    cylinder(group, "Task light lower arm", (4.33, 1.04, 0.30), (4.30, 1.49, 0.28), 0.017, "Metal", 12)
    cylinder(group, "Task light upper arm", (4.30, 1.49, 0.28), (4.12, 1.69, 0.42), 0.016, "Metal", 12)
    cylinder(group, "Task shade", (4.12, 1.70, 0.42), (4.10, 1.61, 0.44), 0.073, "Metal", 20, 0.125)
    cylinder(group, "Warm task diffuser", (4.101, 1.609, 0.441), (4.100, 1.604, 0.442), 0.113, "WarmLight", 20)


def add_wall_monitor():
    """Reserve the exact 3.20 by 1.80 aperture for the runtime interactive screen."""
    group = "Shell"
    box(group, "Wall monitor mounting plate", (5.696, 1.85, 3.0), (0.048, 0.74, 1.10), "Metal", 0.012)
    for z in (2.65, 3.35):
        box(group, "Wall monitor mounting bracket", (5.667, 1.85, z), (0.065, 0.81, 0.065), "Metal", 0.008)
    box(group, "Wall monitor housing", (5.59, 1.85, 3.0), (0.14, 1.92, 3.32), "BlackPlastic", 0.026, 3)
    box(group, "Wall monitor display aperture", (5.518, 1.85, 3.0), (0.006, 1.80, 3.20), "Screen")
    for y in (0.918, 2.782):
        box(group, "Wall monitor horizontal bezel", (5.520, y, 3.0), (0.012, 0.060, 3.27), "Metal", 0.007)
    for z in (1.370, 4.630):
        box(group, "Wall monitor vertical bezel", (5.520, 1.85, z), (0.012, 1.80, 0.060), "Metal", 0.007)
    cylinder(group, "Wall monitor standby light", (5.512, 0.921, 4.41), (5.508, 0.921, 4.41),
             0.006, "ScreenLight", 10)


def add_high_door_storage():
    """A recessed walnut shelf conceals the flat door behind its front retaining lip."""
    group = "Interior"
    box(group, "High door shelf platform", (5.28, 2.1675, -3.25), (0.80, 0.065, 1.50),
        "WalnutEdge", 0.010, 2)
    box(group, "High door shelf front retaining lip", (4.905, 2.245, -3.25),
        (0.05, 0.09, 1.50), "WalnutEdge", 0.006, 2)
    for z in (-3.9775, -2.5225):
        box(group, "High door shelf side rail", (5.28, 2.255, z), (0.80, 0.11, 0.045),
            "Walnut", 0.006, 2)
    box(group, "High door shelf back rail", (5.665, 2.26, -3.25), (0.03, 0.12, 1.50),
        "Walnut", 0.005, 2)
    # These rests support the small door at its agreed center without touching the underside handle.
    for z in (-3.42, -3.08):
        box(group, "High door shelf raised timber rest", (5.20, 2.21875, z),
            (0.32, 0.0375, 0.028), "Oak")
    for z in (-3.78, -2.72):
        box(group, "High door shelf vertical wall bracket", (5.65, 1.93, z),
            (0.06, 0.42, 0.055), "Walnut", 0.006, 1)
        box(group, "High door shelf supporting bracket arm", (5.40, 2.105, z),
            (0.56, 0.055, 0.055), "Walnut", 0.006, 1)
        cylinder(group, "High door shelf diagonal brace", (5.65, 1.77, z),
                 (5.15, 2.105, z), 0.024, "WalnutEdge", 10)
        for y in (1.80, 2.06):
            cylinder(group, "High door shelf bracket screw", (5.618, y, z),
                     (5.613, y, z), 0.009, "MutedGold", 8)


def add_memory_cabinet():
    """Keep the closed cabinet frame in the shared room and its leaves in hinge-local ranges."""
    group = "Interior"
    for x in (1.99, 5.51):
        box(group, "Memory cabinet side", (x, 1.35, 5.685), (0.08, 2.70, 1.35), "Walnut", 0.014)
    box(group, "Memory cabinet crown", (3.75, 2.66, 5.685), (3.60, 0.08, 1.35), "WalnutEdge", 0.014)
    box("SchoolDiorama", "Memory cabinet school display mat", (2.85, 1.51325, 5.65),
        (1.50, 0.0135, 1.08), "Oak", 0.002, 1)
    box(group, "Memory cabinet top fine molding", (3.75, 2.692, 5.665), (3.60, 0.016, 1.35), "Oak", 0.004, 1)

    for group, sign in (("CabinetLeftDoor", 1), ("CabinetRightDoor", -1)):
        x = sign * 0.90
        box(group, "Memory cabinet plain door leaf", (x, 0, 0), (1.80, 2.50, 0.07), "WalnutEdge", 0.014)
        # A broad uninterrupted center leaves the projected key silhouette unobstructed.
        box(group, "Memory cabinet quiet inset panel", (x, 0, -0.037), (1.60, 2.26, 0.010), "Walnut", 0.012)
        for edge in (-1, 1):
            box(group, "Memory cabinet slender door stile", (x + edge * 0.845, 0, -0.040),
                (0.025, 2.36, 0.015), "WalnutEdge", 0.004, 1)
        cylinder(group, "Memory cabinet brass handle", (sign * 1.67, 0.33, -0.077),
                 (sign * 1.67, 0.57, -0.077), 0.013, "MutedGold", 10)
        for y in (0.33, 0.57):
            cylinder(group, "Memory cabinet handle mount", (sign * 1.67, y, -0.043),
                     (sign * 1.67, y, -0.077), 0.011, "MutedGold", 8)
        for y in (-0.94, 0.94):
            cylinder(group, "Memory cabinet hinge", (0, y - 0.048, 0.0), (0, y + 0.048, 0.0),
                     0.021, "MutedGold", 10)

def add_cabinet_label(text, center):
    x, y, z = center
    group = "CabinetMiniatures"
    box(group, "Cabinet label plate " + text, (x, y - 0.045, z - 1.181),
        (0.43, 0.058, 0.012), "Metal", 0.005, 1)
    curve = bpy.data.curves.new("Cabinet label " + text, 'FONT')
    curve.body = text
    curve.size = 0.033
    curve.align_x = 'CENTER'
    curve.align_y = 'CENTER'
    curve.resolution_u = 1
    curve.extrude = 0
    curve.materials.append(MATERIALS["Paper"])
    source = bpy.data.objects.new("Cabinet label " + text, curve)
    source.location = point((x, y - 0.045, z - 1.190))
    source.rotation_euler = (math.pi * 0.5, 0, math.pi)
    bpy.context.scene.collection.objects.link(source)
    bpy.context.view_layer.update()
    data = bpy.data.meshes.new_from_object(source.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    obj = bpy.data.objects.new("Cabinet engraved label " + text, data)
    obj.matrix_world = source.matrix_world.copy()
    obj["home_group"] = group
    COLLECTIONS[group].objects.link(obj)
    bpy.data.objects.remove(source, do_unlink=True)


def add_school_representative():
    """The three-by-four desks and three boards follow SchoolRoom's current classroom layout."""
    group = "SchoolDiorama"
    origin, scale = Vector((2.85, 1.52, 5.65)), 0.075

    def school_box(name, center, size, surface):
        local = Vector(center) - Vector((0.8, 0, 1.0))
        local.x = -local.x
        local.z = -local.z
        return box(group, "School model | " + name, tuple(origin + local * scale),
                   tuple(Vector(size) * scale), surface)

    school_box("Cutaway board wall", (0.8, 1.65, -5.1), (12.0, 3.3, 0.12), "Plaster")
    for x in (-2.80, -0.85, 1.10):
        school_box("Three panel chalkboard frame", (x, 2.22, -4.83), (1.85, 2.02, 0.09), "Walnut")
        school_box("Sage chalkboard", (x, 2.22, -4.775), (1.74, 1.91, 0.018), "DarkLinen")
        school_box("Chalk ledge", (x, 1.22, -4.70), (1.8, 0.06, 0.16), "Oak")
    for row, z in enumerate((0.65, 2.55, 4.45, 6.35)):
        for column, x in enumerate((-3.05, -0.95, 1.15)):
            school_box("Student desk", (x, 0.74, z), (1.06, 0.08, 0.66), "Oak")
            school_box("Desk storage", (x, 0.58, z), (0.85, 0.20, 0.46), "Walnut")
            for dx in (-0.42, 0.42):
                for dz in (-0.23, 0.23):
                    school_box("Desk leg", (x + dx, 0.32, z + dz), (0.045, 0.64, 0.045), "Metal")
            school_box("Student chair seat", (x, 0.43, z + 0.69), (0.48, 0.07, 0.48), "WalnutEdge")
            school_box("Student chair back", (x, 0.75, z + 0.88), (0.48, 0.30, 0.055), "WalnutEdge")
            for dx in (-0.18, 0.18):
                school_box("Chair frame", (x + dx, 0.22, z + 0.69), (0.038, 0.44, 0.38), "Metal")
            school_box("Closed lesson book", (x - 0.13, 0.80, z - 0.07), (0.32, 0.045, 0.24),
                       ("BlueBook", "RedBook", "GreenBook")[(row + column) % 3])
    school_box("Teacher lectern", (1.0, 0.48, -3.25), (1.0, 0.96, 0.62), "Walnut")
    school_box("Teacher worktop", (1.0, 1.0, -3.25), (1.1, 0.08, 0.70), "Oak")
    school_box("Materials gallery", (4.30, 0.90, -4.43), (4.40, 1.80, 1.20), "WalnutEdge")
    for x in (2.65, 3.75, 4.85, 5.95):
        school_box("Gallery door", (x, 0.87, -3.812), (1.04, 1.64, 0.025), "Linen")
    add_cabinet_label("SCHOOL", DIORAMA_ORIGINS["SchoolDiorama"])


def add_military_representative():
    """Reuse the barracks' authored beds and lockers at a compact, bevel-free display LOD."""
    group = "MilitaryDiorama"
    source_path = OUTPUT.parents[1] / "military" / "design" / "군대 방 맵.blend"
    parts = ("frame", "rounded mattress", "blanket", "pillow", "headboard",
             "leg", "leg.001", "leg.002", "leg.003")
    wanted = [f"Barracks ref | {side} bed {index} {part}"
              for side in ("North", "South") for index in range(1, 6) for part in parts]
    wanted += [f"Barracks steel | {side} locker {index}"
               for side in ("North", "South") for index in range(1, 6)]
    with bpy.data.libraries.load(str(source_path), link=False) as (source, target):
        target.objects = [name for name in wanted if name in source.objects]
    transform = Matrix.Translation(point((2.2275, 0.62, 6.04))) @ Matrix.Scale(0.075, 4)
    for obj in target.objects:
        obj.name = "Military model | " + obj.name
        obj["home_group"] = group
        # Appended objects have their authored TRS before an evaluated world matrix exists.
        obj.matrix_world = transform @ obj.matrix_basis
        obj.modifiers.clear()
        obj.hide_render = False
        COLLECTIONS[group].objects.link(obj)
    box(group, "Military model | Barracks floor", (2.85, 0.615, 5.65),
        (1.245, 0.01, 0.78), "Linen")
    for z in (5.257, 6.043):
        box(group, "Military model | Low cutaway side wall", (2.85, 0.65, z),
            (1.26, 0.06, 0.012), "Plaster")
    box(group, "Military model | End wall", (3.479, 0.74, 5.65), (0.012, 0.24, 0.79), "Plaster")
    for z in (5.51, 5.79):
        box(group, "Military model | Daylight window", (3.471, 0.77, z), (0.003, 0.11, 0.18), "Sky")
    add_cabinet_label("MILITARY", DIORAMA_ORIGINS["MilitaryDiorama"])


def add_office_representative():
    """A representative four-workstation office awaits the teammate's full map integration."""
    group = "OfficeDiorama"
    box(group, "Office model | Floor", (4.65, 0.615, 5.65), (1.30, 0.01, 0.92), "CreamRug")
    for x in (4.34, 4.97):
        for z in (5.43, 5.88):
            box(group, "Office model | Worktop", (x, 0.728, z), (0.48, 0.018, 0.28), "Oak")
            for dx in (-0.195, 0.195):
                box(group, "Office model | Desk frame", (x + dx, 0.67, z), (0.015, 0.10, 0.23), "Metal")
            box(group, "Office model | Screen support", (x, 0.773, z + 0.055), (0.018, 0.08, 0.018), "Metal")
            box(group, "Office model | Display housing", (x, 0.823, z + 0.055), (0.24, 0.15, 0.018), "BlackPlastic")
            box(group, "Office model | Display screen", (x, 0.823, z + 0.044), (0.218, 0.126, 0.003), "Screen")
            box(group, "Office model | Keyboard", (x, 0.743, z - 0.055), (0.17, 0.008, 0.065), "Metal")
            box(group, "Office model | Chair seat", (x, 0.686, z - 0.20), (0.17, 0.027, 0.16), "DarkLinen")
            box(group, "Office model | Chair back", (x, 0.763, z - 0.266), (0.17, 0.145, 0.02), "DarkLinen")
            cylinder(group, "Office model | Chair post", (x, 0.625, z - 0.20), (x, 0.68, z - 0.20), 0.014, "Metal", 8)
            box(group, "Office model | Chair foot", (x, 0.63, z - 0.20), (0.19, 0.012, 0.15), "Metal")
    for x in (4.34, 4.97):
        box(group, "Office model | Felt desk divider", (x, 0.797, 5.65), (0.50, 0.24, 0.015), "SageLinen")
    box(group, "Office model | Central low partition", (4.65, 0.76, 5.65), (0.018, 0.28, 0.87), "Linen")
    add_cabinet_label("OFFICE", DIORAMA_ORIGINS["OfficeDiorama"])
    add_cabinet_label("HOME", HOME_MODEL_ORIGIN)


def relocate_authored_dioramas():
    authored_origins = {
        "SchoolDiorama": Vector((2.85, 1.52, 5.65)),
        "MilitaryDiorama": Vector((2.85, 0.62, 5.65)),
        "OfficeDiorama": Vector((4.65, 0.62, 5.65)),
    }
    for group, old_origin in authored_origins.items():
        delta = Vector(DIORAMA_ORIGINS[group]) - old_origin
        transform = Matrix.Translation(point(delta))
        for obj in COLLECTIONS[group].objects:
            obj.matrix_world = transform @ obj.matrix_basis


def add_secret_alcove():
    group = "Interior"
    box(group, "Secret alcove floor slab", (3.00, -0.10, 11.57), (9.68, 0.20, 10.04), "DarkRecess")
    floor_random = random.Random(20261128)
    for index in range(36):
        x = -1.84 + (index + 0.5) * 9.68 / 36
        box(group, "Secret alcove oak floorboard", (x, -0.009, 11.57),
            (9.68 / 36 - 0.003, 0.020, 10.037), f"OakFloor{floor_random.randrange(9)}", 0.0015, 1)
    displays = (
        ("SCHOOL", 0.40, 9.30, 0.9365), ("MILITARY", 5.60, 9.30, 0.9265),
        ("OFFICE", 0.40, 14.20, 0.9265), ("HOME", 5.60, 14.20, 0.9350),
    )
    for name, x, z, top in displays:
        box(group, "Secret display table " + name, (x, top - 0.04, z),
            (2.65, 0.080, 2.35), "WalnutEdge", 0.012, 2)
        leg_height = top - 0.08
        for side_x in (-1, 1):
            for side_z in (-1, 1):
                box(group, "Secret display table leg " + name,
                    (x + side_x * 1.18, leg_height * 0.5, z + side_z * 1.03),
                    (0.12, leg_height, 0.12), "Walnut", 0.008, 1)
    group = "Shell"
    box(group, "Secret alcove left wall", (-1.84, 1.60, 11.55), (0.18, 3.20, 10.08), "Plaster")
    box(group, "Secret alcove right wall", (7.84, 1.60, 11.55), (0.18, 3.20, 10.08), "Plaster")
    box(group, "Secret alcove rear wall", (3.00, 1.60, 16.59), (9.86, 3.20, 0.18), "SageWall")
    box(group, "Secret alcove ceiling", (3.00, 3.31, 11.6775), (9.86, 0.22, 10.005), "Ceiling")
    box(group, "Secret alcove front return ceiling", (6.89, 3.31, 6.5925), (2.08, 0.22, 0.165), "Ceiling")
    for x in (-1.735, 7.735):
        box(group, "Secret alcove side skirting", (x, 0.075, 11.55),
            (0.035, 0.15, 10.08), "Skirting", 0.006, 1)
    box(group, "Secret alcove rear skirting", (3.00, 0.075, 16.485),
        (9.47, 0.15, 0.035), "Skirting", 0.006, 1)
    group = "MiniWalls"
    for x in (-1.84, 7.84):
        box(group, "Cutaway secret alcove side wall", (x, 0.30, 11.55),
            (0.18, 0.60, 10.08), "Plaster")
    box(group, "Cutaway secret alcove rear wall", (3.00, 0.30, 16.59),
        (9.86, 0.60, 0.18), "SageWall")


def add_picture_door():
    names = ("Decor | Large quiet abstract", "Decor | Abstract ochre sun",
             "Decor | Abstract sage hillside", "Decor | Abstract clay column")
    transform = Matrix.Translation(-point(PICTURE_HINGE))
    for obj in list(COLLECTIONS["Shell"].objects):
        if obj.name.startswith(names):
            obj.data.transform(transform @ obj.matrix_world)
            obj.data.update()
            obj.matrix_world = Matrix.Identity(4)
            COLLECTIONS["Shell"].objects.unlink(obj)
            COLLECTIONS["PictureDoor"].objects.link(obj)
            obj["home_group"] = "PictureDoor"


def add_step_crate():
    group = "Crate"
    box(group, "Movable wooden crate body", (0, 0, 0), (0.96, 0.96, 0.96), "Oak", 0.010, 1)
    for a in (-1, 1):
        for b in (-1, 1):
            box(group, "Crate vertical corner batten", (a * 0.455, 0, b * 0.455),
                (0.09, 1.0, 0.09), "WalnutEdge", 0.004, 1)
            box(group, "Crate horizontal cross batten", (0, a * 0.455, b * 0.455),
                (0.82, 0.09, 0.09), "WalnutEdge", 0.004, 1)
            box(group, "Crate horizontal side batten", (a * 0.455, b * 0.455, 0),
                (0.09, 0.09, 0.82), "WalnutEdge", 0.004, 1)
    for side in (-1, 1):
        for line in (-0.16, 0.16):
            z = side * 0.4805
            mesh(group, "Crate front plank seam", [(line - 0.0015, -0.40, z),
                 (line + 0.0015, -0.40, z), (line + 0.0015, 0.40, z), (line - 0.0015, 0.40, z)],
                 [(0, 1, 2, 3)], "DarkRecess")
            x = side * 0.4805
            mesh(group, "Crate side plank seam", [(x, -0.40, line - 0.0015),
                 (x, -0.40, line + 0.0015), (x, 0.40, line + 0.0015), (x, 0.40, line - 0.0015)],
                 [(0, 1, 2, 3)], "DarkRecess")


def add_portable_display_boards():
    for group, origin in DIORAMA_ORIGINS.items():
        x, y, z = origin
        if group != "SchoolDiorama":
            box(group, group + " | Portable timber display board", (x, y - 0.01675, z),
                (1.50, 0.0135, 1.08), "Oak", 0.003, 1)
        for side in (-1, 1):
            box(group, group + " | Long display rim", (x, y, z + side * 0.531),
                (1.50, 0.020, 0.018), "WalnutEdge", 0.003, 1)
            box(group, group + " | Short display rim", (x + side * 0.741, y, z),
                (0.018, 0.020, 1.044), "WalnutEdge", 0.003, 1)
    group = "HomeDioramaBase"
    for side in (-1, 1):
        box(group, "Home portable long display rail", (side * 0.5375, 0.005, 0),
            (0.075, 0.040, 2.05), "WalnutEdge", 0.003, 1)
        box(group, "Home portable short display rail", (0, 0.005, side * 0.9875),
            (1.00, 0.040, 0.075), "WalnutEdge", 0.003, 1)


def rebase_portable_dioramas():
    for group, origin in DIORAMA_ORIGINS.items():
        offset = Matrix.Translation(-point(origin))
        for obj in COLLECTIONS[group].objects:
            if obj.data.users > 1:
                obj.data = obj.data.copy()
            obj.data.transform(offset @ obj.matrix_world)
            obj.data.update()
            obj.matrix_world = Matrix.Identity(4)


def add_lock_socket():
    """Build an open, extruded keyhole through a chamfered escutcheon without a painted fill."""
    group = "LockSocket"
    material("SocketIvory", (0.66, 0.625, 0.48), 0.42, 0.25)
    outer = [(-0.106, -0.16), (0.106, -0.16), (0.12, -0.146), (0.12, 0.146),
             (0.106, 0.16), (-0.106, 0.16), (-0.12, 0.146), (-0.12, -0.146)]
    radius, center_y, half_slot = 0.033, 0.035, 0.015
    start_angle = -math.acos(half_slot / radius)
    end_angle = math.pi - start_angle
    inner = [(math.cos(start_angle + (end_angle - start_angle) * index / 28) * radius,
              center_y + math.sin(start_angle + (end_angle - start_angle) * index / 28) * radius)
             for index in range(29)]
    slot_bottom = inner[0][1] - 0.085
    inner.extend([(-half_slot, slot_bottom), (half_slot, slot_bottom)])
    ray_origin = (0.0, 0.015)

    def cross(a, b):
        return a[0] * b[1] - a[1] * b[0]

    def ray_boundary(direction, boundary):
        distances = []
        for a, b in zip(boundary, boundary[1:] + boundary[:1]):
            edge = (b[0] - a[0], b[1] - a[1])
            denominator = cross(direction, edge)
            if abs(denominator) < 1e-10:
                continue
            delta = (a[0] - ray_origin[0], a[1] - ray_origin[1])
            distance = cross(delta, edge) / denominator
            fraction = cross(delta, direction) / denominator
            if distance > 0 and -1e-8 <= fraction <= 1 + 1e-8:
                distances.append(distance)
        distance = min(distances)
        return (ray_origin[0] + direction[0] * distance,
                ray_origin[1] + direction[1] * distance)

    angles = sorted({round(math.atan2(y - ray_origin[1], x - ray_origin[0]), 10)
                     for x, y in outer + inner})
    rings = []
    for angle in angles:
        direction = (math.cos(angle), math.sin(angle))
        rings.append((ray_boundary(direction, outer), ray_boundary(direction, inner)))
    vertices = [(x, y, z) for z in (-0.0125, 0.0125) for ring in rings for x, y in ring]
    offset = len(rings) * 2
    faces = []
    for index in range(len(rings)):
        a, b = index * 2, ((index + 1) % len(rings)) * 2
        faces.extend(((a, a + 1, b + 1, b),
                      (a + offset, b + offset, b + offset + 1, a + offset + 1),
                      (a, b, b + offset, a + offset),
                      (a + 1, a + offset + 1, b + offset + 1, b + 1)))
    obj = mesh(group, "Lock socket | Open keyhole escutcheon", vertices, faces, "MutedGold")
    obj.data.materials.append(MATERIALS["SocketIvory"])
    for index, polygon in enumerate(obj.data.polygons):
        if index % 4 == 0:
            polygon.material_index = 1
    for y in (-0.123, 0.123):
        cylinder(group, "Lock socket | Small brass fixing", (0, y, -0.0125), (0, y, -0.0145),
                 0.006, "MutedGold", 10)


def add_window():
    group = "Shell"
    box(group, "Window view sky", (-5.975, 1.84, -2.1), (0.02, 1.75, 3.22), "Sky")
    for x, surface, height in ((-5.95, "FarMountain", 1.90), (-5.93, "NearMountain", 1.54)):
        vertices = []
        for index in range(14):
            z = -3.71 + index * 3.22 / 13
            ridge = height + math.sin(index * 1.2) * 0.12 + math.cos(index * 0.55) * 0.10
            vertices.extend([(x, 0.985, z), (x, ridge, z)])
        faces = [(index * 2, index * 2 + 1, index * 2 + 3, index * 2 + 2) for index in range(13)]
        mesh(group, surface, vertices, faces, surface)
    for y in (1.00, 2.68):
        box(group, "Window horizontal casing", (-5.68, y, -2.1), (0.095, 0.07, 3.35), "Skirting", 0.009)
    for z in (-3.75, -2.1, -0.45):
        box(group, "Window vertical mullion", (-5.68, 1.84, z), (0.095, 1.71, 0.055), "Skirting", 0.008)
    box(group, "Broad window ledge", (-5.56, 0.958, -2.1), (0.40, 0.065, 3.48), "Oak", 0.012)
    cylinder(group, "Curtain bronze rod", (-5.41, 2.92, -4.02), (-5.41, 2.92, -0.18), 0.023, "MutedGold", 12)
    for side in (-1, 1):
        z_center = -2.1 + side * 1.56
        vertices = []
        rows, columns = 29, 34
        for row in range(rows + 1):
            y = 0.36 + row * 2.49 / rows
            for column in range(columns + 1):
                z = z_center + (column / columns - 0.5) * 0.64
                x = -5.38 + math.sin(column / columns * math.pi * 10) * 0.062
                x += math.sin(row * 0.30 + column * 0.27) * 0.009
                vertices.append((x, y, z))
        faces = []
        for row in range(rows):
            for column in range(columns):
                a = row * (columns + 1) + column
                faces.append((a, a + 1, a + columns + 2, a + columns + 1))
        obj = mesh(group, "Pleated sage curtain", vertices, faces, "SageLinen")
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
        for ring in range(6):
            z = z_center + (ring / 5 - 0.5) * 0.56
            cylinder(group, "Curtain hanging tab", (-5.40, 2.83, z), (-5.40, 2.94, z), 0.012, "MutedGold", 8)


def add_decor():
    group = "Shell"
    # Unlabelled landscapes hang on the side wall beside the bed.
    for index, z in enumerate((1.10, 2.45)):
        box(group, "Landscape walnut frame", (-5.67, 2.12, z), (0.055, 0.97, 0.93), "Walnut", 0.009)
        box(group, "Landscape print mat", (-5.636, 2.12, z), (0.008, 0.88, 0.84), "Paper")
        box(group, "Landscape print sky", (-5.630, 2.12, z), (0.003, 0.64, 0.67), "Sky")
        mesh(group, "Printed mountain silhouette", [(-5.625, 1.80, z - 0.335),
             (-5.625, 2.10, z - 0.335), (-5.625, 2.26 + index * 0.05, z - 0.02),
             (-5.625, 2.08, z + 0.18), (-5.625, 2.16, z + 0.335), (-5.625, 1.80, z + 0.335)],
             [(0, 1, 2, 3, 4, 5)], "NearMountain")
    # Small back-wall fixtures avoid the central moving bookcase strip.
    box(group, "Right wall notice frame", (5.67, 2.03, -4.70), (0.055, 1.03, 1.18), "Oak", 0.014)
    box(group, "Right wall linen panel", (5.637, 2.03, -4.70), (0.006, 0.91, 1.06), "Linen")
    for index in range(3):
        box(group, "Pinned small photograph", (5.631, 2.04 + (index % 2) * 0.18, -5.03 + index * 0.30),
            (0.003, 0.22, 0.20), ("Sky", "NearMountain", "Terracotta")[index])
    cylinder(group, "Ceiling rose", (0.0, 3.16, 1.80), (0.0, 3.20, 1.80), 0.18, "Ceiling", 24)
    cylinder(group, "Ceiling pendant cable", (0.0, 2.72, 1.80), (0.0, 3.17, 1.80), 0.009, "Metal", 8)
    cylinder(group, "Pendant linen shade", (0.0, 2.72, 1.80), (0.0, 2.42, 1.80), 0.25, "Linen", 32, 0.48)
    cylinder(group, "Pendant warm diffuser", (0.0, 2.418, 1.80), (0.0, 2.423, 1.80), 0.455, "WarmLight", 32)


def add_small_props():
    group = "Interior"
    # Low bedside ceramic reading light and an unlabelled book.
    cylinder(group, "Bedside ceramic base", (-4.85, 0.77, -0.57), (-4.85, 0.97, -0.57), 0.09, "Terracotta", 24, 0.065)
    cylinder(group, "Bedside lampshade", (-4.85, 0.98, -0.57), (-4.85, 1.22, -0.57), 0.19, "Linen", 24, 0.12)
    cylinder(group, "Bedside shade edge", (-4.85, 0.98, -0.57), (-4.85, 0.986, -0.57), 0.18, "WarmLight", 24)
    box(group, "Bedside closed book", (-4.63, 0.80, -0.44), (0.20, 0.05, 0.29), "BlueBook", 0.006)
    for index in range(3):
        box(group, "Dresser stacked sketchbooks", (4.87, 1.50 + index * 0.055, -3.60),
            (0.58, 0.048, 0.35), ("BlueBook", "Paper", "GreenBook")[index], 0.005)
    cylinder(group, "Plant ceramic pot", (4.90, 1.47, -2.94), (4.90, 1.70, -2.94), 0.12, "Terracotta", 20, 0.155)
    cylinder(group, "Plant soil", (4.90, 1.694, -2.94), (4.90, 1.705, -2.94), 0.137, "DarkRecess", 20)
    for index in range(9):
        angle = index * 2.39996
        length = 0.18 + 0.035 * (index % 4)
        tip = (4.90 + math.cos(angle) * length, 1.83 + (index % 3) * 0.14,
               -2.94 + math.sin(angle) * length)
        cylinder(group, "Plant stem", (4.9, 1.70, -2.94), tip, 0.005, "Leaf", 6)
        side = Vector((math.sin(angle) * 0.055, 0.02, -math.cos(angle) * 0.055))
        tip_vector = Vector(tip)
        base = tip_vector - Vector((math.cos(angle) * 0.09, 0.08, math.sin(angle) * 0.09))
        end = tip_vector + Vector((math.cos(angle) * 0.12, 0.01, math.sin(angle) * 0.12))
        mesh(group, "Sculpted plant leaf", [tuple(base), tuple(tip_vector + side), tuple(end),
             tuple(tip_vector - side), tuple(tip_vector + Vector((0, 0.025, 0)))],
             [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)],
             "LeafLight" if index % 3 == 0 else "Leaf")


def add_shelf():
    group = "Shelf"
    box(group, "Bookcase recessed back", (0, 0, -0.268), (2.05, 2.60, 0.064), "Walnut", 0.007)
    for side in (-1, 1):
        box(group, "Bookcase upright", (side * 0.965, 0, 0), (0.12, 2.60, 0.60), "WalnutEdge", 0.012)
    for y in (-1.25, -0.63, 0, 0.63, 1.25):
        box(group, "Bookcase shelf", (0, y, 0), (1.93, 0.10, 0.60), "WalnutEdge", 0.012)
    for row, bottom in enumerate((-1.20, -0.58, 0.05, 0.68)):
        x = -0.82
        for index in range(7):
            width = 0.065 + RANDOM.random() * 0.05
            height = 0.30 + RANDOM.random() * 0.18
            book(group, f"Shelf book {row}-{index}", x + width * 0.5, bottom, 0.07,
                 width, height, 0.32 + RANDOM.random() * 0.055,
                 ("BlueBook", "RedBook", "GreenBook", "OcherBook", "Linen")[(row + index) % 5])
            x += width + 0.012
        if row % 2 == 0:
            box(group, "Bookcase linen archive box", (0.51, bottom + 0.15, 0.03),
                (0.54, 0.30, 0.43), "Linen", 0.014)
            box(group, "Archive recessed handle", (0.51, bottom + 0.20, 0.25),
                (0.105, 0.018, 0.004), "DarkRecess", 0.006)
        else:
            for index in range(3):
                box(group, "Horizontal journal", (0.50, bottom + 0.035 + index * 0.062, 0.07),
                    (0.54 - index * 0.018, 0.055, 0.38),
                    ("RedBook", "Paper", "BlueBook")[index], 0.005)
    box(group, "Bookcase top crown", (0, 1.273, 0), (2.05, 0.054, 0.60), "WalnutEdge", 0.012)


def add_door():
    group = "Door"
    box(group, "Door leaf", (0, 0, 0), (1.76, 2.60, 0.105), "WalnutEdge", 0.016, 3)
    for y, height in ((0.53, 1.04), (-0.65, 0.81)):
        box(group, "Door recessed panel", (0, y, 0.054), (1.39, height, 0.012), "Walnut", 0.015)
        for side in (-1, 1):
            box(group, "Door raised stile", (side * 0.72, y, 0.066),
                (0.023, height + 0.07, 0.016), "WalnutEdge", 0.006)
        for edge in (-1, 1):
            box(group, "Door raised rail", (0, y + edge * (height * 0.5 + 0.025), 0.066),
                (1.46, 0.023, 0.016), "WalnutEdge", 0.006)
    cylinder(group, "Door handle rosette", (0.63, -0.12, 0.054), (0.63, -0.12, 0.072), 0.045, "MutedGold", 16)
    cylinder(group, "Door handle spindle", (0.63, -0.12, 0.07), (0.63, -0.12, 0.115), 0.017, "MutedGold", 12)
    cylinder(group, "Door lever", (0.63, -0.12, 0.115), (0.46, -0.12, 0.115), 0.02, "MutedGold", 12)
    for y in (-0.96, 0.0, 0.96):
        cylinder(group, "Door hinge", (-0.877, y - 0.055, -0.025), (-0.877, y + 0.055, -0.025),
                 0.023, "MutedGold", 12)


def add_rpg_and_rack():
    """Reuse the F2 gun cabinet, rifles, and RPG in their authored orientation."""
    source_path = OUTPUT.parents[1] / "military" / "design" / "군대 방 맵.blend"
    rpg_prefix = "Barracks storage | RPG display"
    cabinet_prefixes = ("Barracks gun cabinet |", "Barracks storage | K2C1")
    centers = {"Rpg": Vector((13.91, 0.76, -10.055)),
               "Rack": Vector((13.90, 0.99, -10.045))}
    with bpy.data.libraries.load(str(source_path), link=False) as (source, target):
        target.objects = [name for name in source.objects
                          if name.startswith((rpg_prefix,) + cabinet_prefixes)]
    for obj in target.objects:
        group = "Rpg" if obj.name.startswith(rpg_prefix) else "Rack"
        obj.name = "F2 source | " + obj.name
        obj["home_group"] = group
        # Appended objects retain their authored local TRS before world evaluation.
        obj.matrix_world = Matrix.Translation(-point(centers[group])) @ obj.matrix_basis
        obj.modifiers.clear()
        obj.hide_render = False
        COLLECTIONS[group].objects.link(obj)


def add_miniature_broken_wall():
    """Retain the true opening while exposing the reverse face above a low corridor."""
    for original in list(COLLECTIONS["BrokenWall"].objects):
        if original.name.startswith(("Exit corridor ceiling", "Back wall side")):
            continue
        duplicate = original.copy()
        duplicate.data = original.data.copy()
        duplicate.name = "Mini cutaway | " + original.name
        duplicate["home_group"] = "MiniBrokenWall"
        if original.name.startswith(("Exit corridor wall", "Exit warm end", "Recessed door jamb")):
            for vertex in duplicate.data.vertices:
                vertex.co.z = min(vertex.co.z, 0.60)
            duplicate.data.update()
        COLLECTIONS["MiniBrokenWall"].objects.link(duplicate)


def latch_guide(name, center_x):
    """A hollow guide aligned with the horizontal bolt, including its visible bore."""
    sides = 10
    vertices = []
    for x in (center_x - 0.035, center_x + 0.035):
        for radius in (0.031, 0.019):
            for index in range(sides):
                angle = math.tau * index / sides
                vertices.append((x, math.cos(angle) * radius, math.sin(angle) * radius))
    faces = []
    for index in range(sides):
        following = (index + 1) % sides
        faces.extend((
            (index, following, following + sides * 2, index + sides * 2),
            (index + sides, index + sides * 3, following + sides * 3, following + sides),
            (index, index + sides, following + sides, following),
            (index + sides * 2, following + sides * 2,
             following + sides * 3, index + sides * 3),
        ))
    return mesh("LatchMounts", name, vertices, faces, "Metal")


def add_reverse_door_latch():
    """Keep fixed guides and the retractable bolt as independent local-origin assets."""
    group = "LatchMounts"
    box(group, "Reverse latch brass backplate", (0, 0, 0.026), (0.44, 0.09, 0.012), "MutedGold")
    for center_x in (-0.11, 0.18):
        latch_guide("Reverse latch hollow guide", center_x)
        box(group, "Reverse latch guide saddle", (center_x, 0, 0.019),
            (0.07, 0.055, 0.021), "Metal")
    for x in (-0.19, 0.115):
        for y in (-0.031, 0.031):
            cylinder(group, "Latch countersunk screw", (x, y, 0.019), (x, y, 0.014),
                     0.007, "Metal", 6)
            box(group, "Latch screw slot", (x, y, 0.013), (0.008, 0.0015, 0.001), "DarkRecess")
    group = "LatchBolt"
    cylinder(group, "Horizontal sliding brass bolt", (-0.21, 0, 0), (0.21, 0, 0),
             0.0175, "MutedGold", 12)
    cylinder(group, "Sliding bolt raised handle stem", (0.02, 0, -0.011), (0.02, 0, -0.045),
             0.009, "MutedGold", 8)
    cylinder(group, "Sliding bolt tactile handle", (0.02, -0.019, -0.045), (0.02, 0.019, -0.045),
             0.018, "MutedGold", 10)


def add_lived_in_decor():
    """Dress the existing furniture and walls without adding gameplay obstructions."""
    decor_random = random.Random(20261124)
    material("MutedOchreCloth", (0.39, 0.277, 0.133), 0.96, texture="fabric")

    def ceramic_cup(group, name, center, radius, height, surface):
        x, y, z = center
        vertices = []
        sides = 16
        for level, ring_radius in ((0.0, radius * 0.84), (height, radius),
                                   (height, radius - 0.009), (0.012, radius * 0.84 - 0.009)):
            for index in range(sides):
                angle = index * math.tau / sides
                vertices.append((x + math.cos(angle) * ring_radius, y + level,
                                 z + math.sin(angle) * ring_radius))
        faces = []
        for ring in range(3):
            for index in range(sides):
                following = (index + 1) % sides
                faces.append((ring * sides + index, ring * sides + following,
                              (ring + 1) * sides + following, (ring + 1) * sides + index))
        mesh(group, name, vertices, faces, surface)
        return x, y, z

    def flat_picture(name, x, y, width, height, color):
        z = -6.296
        box("Shell", "Decor | " + name + " frame", (x, y, z), (width, height, 0.048), "Walnut", 0.010)
        box("Shell", "Decor | " + name + " mat", (x, y, z + 0.027),
            (width - 0.08, height - 0.08, 0.006), "Paper")
        box("Shell", "Decor | " + name + " print", (x, y, z + 0.032),
            (width - 0.23, height - 0.23, 0.002), color)
        return z + 0.034

    z = flat_picture("Large quiet abstract", -4.48, 2.00, 1.30, 1.38, "Linen")
    cylinder("Shell", "Decor | Abstract ochre sun", (-4.70, 2.19, z), (-4.70, 2.19, z + 0.001),
             0.185, "MutedOchreCloth", 24)
    mesh("Shell", "Decor | Abstract sage hillside", [(-5.00, 1.47, z + 0.003),
         (-5.00, 1.89, z + 0.003), (-4.73, 1.99, z + 0.003), (-4.46, 1.87, z + 0.003),
         (-4.18, 1.77, z + 0.003), (-3.96, 1.86, z + 0.003), (-3.96, 1.47, z + 0.003)],
         [(0, 1, 2, 3, 4, 5, 6)], "SageLinen")
    box("Shell", "Decor | Abstract clay column", (-4.30, 1.73, z + 0.005),
        (0.15, 0.41, 0.001), "Terracotta")
    for index, (x, y, width, height) in enumerate(((2.35, 2.08, 0.72, 0.91),
                                                (3.20, 2.32, 0.88, 0.66),
                                                (3.52, 1.65, 0.54, 0.56))):
        color = ("Sky", "SageLinen", "Terracotta")[index]
        depth = flat_picture("Travel print " + str(index), x, y, width, height, color)
        if index < 2:
            left, right = x - (width - 0.23) * 0.5, x + (width - 0.23) * 0.5
            low = y - (height - 0.23) * 0.5
            mesh("Shell", "Decor | Travel photograph ridgeline", [(left, low, depth + 0.001),
                 (left, y - 0.03, depth + 0.001), (x - 0.06, y + 0.11, depth + 0.001),
                 (x + 0.08, y - 0.025, depth + 0.001), (right, y + 0.045, depth + 0.001),
                 (right, low, depth + 0.001)], [(0, 1, 2, 3, 4, 5)], "NearMountain")

    # Simple raised hands keep the wall clock legible without any type or labels.
    center_y, center_z = 2.25, -0.76
    cylinder("Shell", "Decor | Wall clock walnut rim", (5.658, center_y, center_z),
             (5.595, center_y, center_z), 0.275, "WalnutEdge", 32)
    cylinder("Shell", "Decor | Wall clock ivory dial", (5.593, center_y, center_z),
             (5.588, center_y, center_z), 0.244, "Paper", 32)
    for index in range(12):
        angle = index * math.tau / 12
        cylinder("Shell", "Decor | Clock hour mark",
                 (5.582, center_y + math.cos(angle) * 0.205, center_z + math.sin(angle) * 0.205),
                 (5.582, center_y + math.cos(angle) * 0.227, center_z + math.sin(angle) * 0.227),
                 0.006, "Metal", 4)
    for angle, length in ((-0.85, 0.125), (1.02, 0.182)):
        cylinder("Shell", "Decor | Clock hand", (5.572, center_y, center_z),
                 (5.572, center_y + math.cos(angle) * length, center_z + math.sin(angle) * length),
                 0.009, "Metal", 6)
    cylinder("Shell", "Decor | Clock brass spindle", (5.57, center_y, center_z),
             (5.563, center_y, center_z), 0.015, "MutedGold", 10)

    # Peg rail and draped canvas tote occupy the front wall, above floor clearance.
    box("Shell", "Decor | Oak peg rail", (-2.20, 1.90, 6.285), (1.50, 0.13, 0.055), "Oak", 0.012)
    for x in (-2.75, -2.20, -1.65):
        cylinder("Shell", "Decor | Coat peg", (x, 1.90, 6.25), (x, 1.90, 6.145),
                 0.022, "Walnut", 10, 0.027)
    bag_x = -2.20
    bag_vertices = []
    rows, columns = 12, 14
    for row in range(rows + 1):
        v = row / rows
        for column in range(columns + 1):
            u = column / columns
            half_width = 0.255 - math.sin(v * math.pi * 0.5) * 0.035
            bag_vertices.append((bag_x + (u * 2 - 1) * half_width, 0.88 + v * 0.66,
                                 6.095 + math.sin(u * 17 + v * 3.4) * 0.013 + (v - 0.5) ** 2 * 0.06))
    bag_faces = []
    for row in range(rows):
        for column in range(columns):
            a = row * (columns + 1) + column
            bag_faces.append((a, a + columns + 1, a + columns + 2, a + 1))
    mesh("Shell", "Decor | Soft canvas tote", bag_vertices, bag_faces, "Linen")
    for side in (-1, 1):
        for segment in range(10):
            angle0 = segment * math.pi / 10
            angle1 = (segment + 1) * math.pi / 10
            cylinder("Shell", "Decor | Tote woven handle",
                     (bag_x + side * 0.125 + math.cos(angle0) * 0.055,
                      1.53 + math.sin(angle0) * 0.35, 6.105),
                     (bag_x + side * 0.125 + math.cos(angle1) * 0.055,
                      1.53 + math.sin(angle1) * 0.35, 6.105), 0.009, "Linen", 4)
    box("Shell", "Decor | Tote sewn patch", (bag_x, 1.23, 6.072), (0.12, 0.08, 0.002), "SageLinen")

    # The bed and bedside keep all added volume inside their existing footprints.
    rounded_fabric("Interior", "Decor | Woven ochre accent cushion", (-3.18, 0.81, 0.97),
                   (0.58, 0.115, 0.47), "MutedOchreCloth", 18, 14, 2.5)
    for stripe in range(3):
        z0 = 2.77 + stripe * 0.09
        box("Interior", "Decor | Throw woven stripe", (-3.75, 0.893, z0),
            (2.00, 0.002, 0.015), "RugWeave")
    for index in range(23):
        x = -4.73 + index * 0.086
        cylinder("Interior", "Decor | Throw soft tassel", (x, 0.88, 3.186),
                 (x + 0.006, 0.84, 3.27), 0.003, "Linen", 4)
    ceramic_cup("Interior", "Decor | Bedside ceramic mug", (-5.075, 0.77, -0.39), 0.057, 0.104, "Linen")
    cylinder("Interior", "Decor | Morning coffee", (-5.075, 0.853, -0.39),
             (-5.075, 0.855, -0.39), 0.045, "Walnut", 16)
    for segment in range(9):
        angle0 = -math.pi * 0.5 + segment * math.pi / 9
        angle1 = -math.pi * 0.5 + (segment + 1) * math.pi / 9
        cylinder("Interior", "Decor | Mug rounded handle",
                 (-5.123 - math.cos(angle0) * 0.035, 0.824 + math.sin(angle0) * 0.033, -0.39),
                 (-5.123 - math.cos(angle1) * 0.035, 0.824 + math.sin(angle1) * 0.033, -0.39),
                 0.008, "Linen", 6)
    box("Interior", "Decor | Bedside reading journal", (-4.66, 0.838, -0.44),
        (0.215, 0.023, 0.285), "Paper", 0.004, 1)

    # A low basket fits beneath the reading chair's existing frame and collision box.
    for index in range(7):
        y = 0.020 + index * 0.017
        box("Interior", "Decor | Basket front woven rib", (-3.65, y, 4.875), (0.56, 0.010, 0.015), "Oak")
        box("Interior", "Decor | Basket rear woven rib", (-3.65, y, 4.425), (0.56, 0.010, 0.015), "Oak")
        for x in (-3.92, -3.38):
            box("Interior", "Decor | Basket end woven rib", (x, y, 4.65), (0.015, 0.010, 0.45), "Oak")
    for index in range(11):
        for z in (4.425, 4.875):
            box("Interior", "Decor | Basket vertical weave", (-3.91 + index * 0.052, 0.075, z),
                (0.010, 0.135, 0.019), "WalnutEdge")
    for x in (-3.79, -3.54):
        cylinder("Interior", "Decor | Folded textile roll", (x, 0.118, 4.49),
                 (x, 0.118, 4.81), 0.047, "Linen" if x < -3.7 else "SageLinen", 14)

    # Window plants remain Shell fixtures so the cutaway has no unsupported miniature pots.
    for index, z in enumerate((-2.91, -1.30)):
        center = (-5.52, 0.991, z)
        ceramic_cup("Shell", "Decor | Window glazed planter", center, 0.10, 0.14,
                    "Terracotta" if index == 0 else "Linen")
        cylinder("Shell", "Decor | Window pot soil", (-5.52, 1.11, z), (-5.52, 1.115, z), 0.085, "DarkRecess", 12)
        for leaf in range(7):
            angle = leaf * 2.39996 + index
            tip = Vector((-5.52 + math.cos(angle) * 0.125,
                          1.21 + decor_random.random() * 0.14, z + math.sin(angle) * 0.125))
            cylinder("Shell", "Decor | Window plant stem", (-5.52, 1.11, z), tuple(tip), 0.0035, "Leaf", 5)
            offset = Vector((math.sin(angle) * 0.04, 0, -math.cos(angle) * 0.04))
            upper = tip + Vector((math.cos(angle) * 0.08, 0.03, math.sin(angle) * 0.08))
            lower = tip - Vector((math.cos(angle) * 0.05, 0.04, math.sin(angle) * 0.05))
            mesh("Shell", "Decor | Window broad leaf", [tuple(lower), tuple(tip + offset),
                 tuple(upper), tuple(tip - offset), tuple(tip + Vector((0, 0.018, 0)))],
                 [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)], "LeafLight" if leaf % 3 == 0 else "Leaf")
    for center_z in (-3.66, -0.54):
        for segment in range(17):
            z = center_z - 0.32 + segment * 0.04
            x = -5.38 + math.sin(segment * 0.04 / 0.64 * math.pi * 10) * 0.062
            cylinder("Shell", "Decor | Curtain stitched lower hem", (x, 0.403, z),
                     (x, 0.403, z + 0.035), 0.003, "Linen", 4)

    # Stationery stays on the rear/right edge, beyond the model, RPG, and door targets.
    ceramic_cup("Interior", "Decor | Desk pencil cup", (4.42, 1.003, 0.55), 0.050, 0.125, "Terracotta")
    for index in range(5):
        x = 4.399 + (index % 3) * 0.019
        z = 0.535 + (index // 3) * 0.024
        cylinder("Interior", "Decor | Desk pencil", (x, 1.02, z),
                 (x + (index - 2) * 0.008, 1.20 + index * 0.012, z + 0.008),
                 0.0035, "Oak" if index % 2 else "Metal", 6)
    for index in range(12):
        a0, a1 = index * math.pi / 12, (index + 1) * math.pi / 12
        cylinder("Interior", "Decor | Headphone headband",
                 (4.34 + math.cos(a0) * 0.102, 1.020, 1.16 + math.sin(a0) * 0.14),
                 (4.34 + math.cos(a1) * 0.102, 1.020, 1.16 + math.sin(a1) * 0.14),
                 0.009, "Metal", 6)
    for x in (4.24, 4.44):
        box("Interior", "Decor | Headphone padded earcup", (x, 1.03, 1.16),
            (0.061, 0.046, 0.10), "BlackPlastic", 0.013, 2)


def instance(name, source, translation, scale=1.0, collection=None):
    obj = bpy.data.objects.new(name, None)
    obj.instance_type = "COLLECTION"
    obj.instance_collection = COLLECTIONS[source]
    obj.location = point(translation)
    obj.scale = (scale,) * 3
    (collection or bpy.context.scene.collection).objects.link(obj)
    return obj


def add_presentation(scene):
    if scene.world is None:
        scene.world = bpy.data.worlds.new("Bedroom ambient daylight")
    scene.world.color = (0.20, 0.22, 0.20)
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.32, 0.36, 0.32, 1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.35
    lighting = bpy.data.collections.new("Lighting and cameras | authoring only")
    scene.collection.children.link(lighting)
    for name, position, energy, color, size in (
        ("Overcast window daylight", (-5.2, 2.2, -2.1), 850, (0.81, 0.90, 1.0), 3.2),
        ("Linen pendant warm light", (0.0, 2.35, 1.8), 370, (1.0, 0.78, 0.52), 3.0),
        ("Desk warm pool", (4.1, 1.6, 0.44), 60, (1.0, 0.72, 0.37), 0.5),
        ("Bedside warm reading light", (-4.85, 1.1, -0.57), 38, (1.0, 0.69, 0.38), 0.4),
    ):
        data = bpy.data.lights.new(name, "AREA")
        data.energy, data.color, data.size = energy, color, size
        light = bpy.data.objects.new(name, data)
        lighting.objects.link(light)
        light.location = point(position)
        target = point((0, 1.0, 0)) if "window" in name else light.location + Vector((0, 0, -1))
        light.rotation_euler = (target - light.location).to_track_quat('-Z', 'Y').to_euler()
    cameras = (
        ("01 | Human entry view", (0, 1.65, 5.0), (0, 1.45, -2.0), 21),
        ("02 | Cabinet miniature collection", (3.00, 2.90, 7.00), (3.00, 0.95, 11.75), 14),
        ("03 | Room editorial corner", (-1.0, 2.45, 5.5), (-1.2, 1.1, -2.3), 19),
        ("04 | Cabinet and computer", (-0.4, 2.25, 1.4), (3.75, 1.35, 5.65), 24),
    )
    for name, eye, target, lens in cameras:
        data = bpy.data.cameras.new(name)
        data.lens = lens
        camera = bpy.data.objects.new(name, data)
        lighting.objects.link(camera)
        camera.location = point(eye)
        camera.rotation_euler = (point(target) - camera.location).to_track_quat('-Z', 'Y').to_euler()
        if scene.camera is None:
            scene.camera = camera
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 32
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 800
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = 'AgX'
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                area.spaces.active.region_3d.view_perspective = 'CAMERA'
                area.spaces.active.shading.type = 'MATERIAL'


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "Home | Bedroom and matching tabletop"
    for index, name in enumerate(GROUP_NAMES):
        collection = bpy.data.collections.new(f"{index:02d} | {name}")
        collection["home_group"] = name
        COLLECTIONS[name] = collection
        if name in ("Interior", "Shell", "IntactWall"):
            scene.collection.children.link(collection)
    add_palette()
    add_floor()
    add_shell()
    add_miniature_broken_wall()
    add_bed()
    add_rug()
    add_storage()
    add_desk()
    add_wall_monitor()
    add_window()
    add_decor()
    add_small_props()
    add_shelf()
    add_door()
    add_rpg_and_rack()
    add_reverse_door_latch()
    add_lived_in_decor()
    add_high_door_storage()
    add_secret_alcove()
    add_memory_cabinet()
    add_school_representative()
    add_military_representative()
    add_office_representative()
    relocate_authored_dioramas()
    add_portable_display_boards()
    rebase_portable_dioramas()
    add_lock_socket()
    add_picture_door()
    add_step_crate()
    presentation = bpy.data.collections.new("Presentation | linked instances excluded from game export")
    scene.collection.children.link(presentation)
    COLLECTIONS["Presentation"] = presentation
    instance("Real bookcase default pose", "Shelf", (0, 1.3, -5.95), collection=presentation)
    base = HOME_MODEL_ORIGIN
    content_origin = home_model_position((0, 0, 0))
    for group in ("Interior", "MiniWalls", "IntactWall"):
        home_model = instance("Cabinet same-layout " + group, group, content_origin, 0.075, presentation)
        home_model.rotation_euler.z = math.radians(90.0)
    mini_shelf = instance("Cabinet bookcase same-layout", "Shelf", home_model_position((0, 1.3, -5.95)),
                          0.075, presentation)
    mini_shelf.rotation_euler.z = math.radians(90.0)
    stored_door = instance("Detached miniature door", "Door", (5.20, 2.245, -3.25), 0.16, presentation)
    stored_door.rotation_euler.x = math.radians(-90.0)
    military_origin = Vector(DIORAMA_ORIGINS["MilitaryDiorama"])
    instance("Miniature equipment rack", "Rack", military_origin + Vector((0.420, 0.07425, -0.363375)), 0.075, presentation)
    instance("Miniature RPG", "Rpg", military_origin + Vector((0.42075, 0.057, -0.364125)), 0.075, presentation)
    instance("Cabinet representative rooms", "CabinetMiniatures", (0, 0, 0), 1.0, presentation)
    for group, origin in DIORAMA_ORIGINS.items():
        instance("Cabinet portable " + group, group, origin, 1.0, presentation)
    socket = instance("Hidden lock socket behind painting", "LockSocket", HIDDEN_SOCKET_CENTER, 1.0, presentation)
    socket.rotation_euler.z = math.radians(180.0)
    instance("Secret socket picture closed", "PictureDoor", PICTURE_HINGE, 1.0, presentation)
    instance("Movable step crate", "Crate", (2.60, 0.50, -2.0), 1.0, presentation)
    mini_crate = instance("Room model step crate", "Crate", home_model_position((2.60, 0.50, -2.0)),
                          0.075, presentation)
    mini_crate.rotation_euler.z = math.radians(90.0)
    home_base = instance("Cabinet portable home display board", "HomeDioramaBase", base, 1.0, presentation)
    home_base.rotation_euler.z = math.radians(90.0)
    instance("Cabinet left leaf closed", "CabinetLeftDoor", (1.95, 1.35, 4.99), 1.0, presentation)
    instance("Cabinet right leaf closed", "CabinetRightDoor", (5.55, 1.35, 4.99), 1.0, presentation)
    add_presentation(scene)
    for name, collection in COLLECTIONS.items():
        collection.use_fake_user = True
    bpy.context.view_layer.update()
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / "내 방.blend"))
    print("Saved authored home with", len(bpy.data.objects), "objects")


if __name__ == "__main__":
    main()
