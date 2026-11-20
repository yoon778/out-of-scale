"""Rebuild the authored bedroom and its matching linked tabletop model in Blender."""

import math
import random
from pathlib import Path

import bpy
from mathutils import Vector


OUTPUT = Path(__file__).resolve().parent
GROUP_NAMES = (
    "Interior", "Shell", "IntactWall", "BrokenWall", "MiniWalls",
    "Shelf", "Door", "Rpg", "Rack", "MiniBrokenWall", "LatchMounts", "LatchBolt",
)
RANDOM = random.Random(20261120)
COLLECTIONS = {}
MATERIALS = {}


def point(position):
    x, y, z = position
    return Vector((x, -z, y))


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
    box(group, "Front sage wall", (0, 1.60, 6.51), (11.7, 3.20, 0.34), "SageWall")
    # A genuine window recess instead of scenery layered across an unbroken wall.
    box(group, "Left wall under window", (-5.87, 0.49, 0), (0.34, 0.98, 13.1), "Plaster")
    box(group, "Left window lintel", (-5.87, 2.95, 0), (0.34, 0.50, 13.1), "Plaster")
    box(group, "Left wall rear pier", (-5.87, 1.84, -5.13), (0.34, 1.72, 2.84), "Plaster")
    box(group, "Left wall forward pier", (-5.87, 1.84, 2.91), (0.34, 1.72, 7.28), "Plaster")
    for x in (-5.68, 5.68):
        box(group, "Continuous side skirting", (x, 0.075, 0), (0.045, 0.15, 13.08), "Skirting", 0.008)
        box(group, "Cove edge", (x, 3.11, 0), (0.065, 0.15, 13.08), "Skirting", 0.008)
    box(group, "Front skirting", (0, 0.075, 6.315), (11.35, 0.15, 0.045), "Skirting", 0.008)
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
    box(group, "Cutaway front wall", (0, 0.30, 6.51), (11.7, 0.60, 0.34), "SageWall")


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
    # Screen and small tools occupy only the right-rear corner, leaving the cutaway model unobstructed.
    box(group, "Monitor base", (3.75, 1.025, 0.36), (0.45, 0.045, 0.27), "BlackPlastic", 0.014)
    box(group, "Monitor support", (3.75, 1.25, 0.29), (0.058, 0.43, 0.055), "Metal", 0.006)
    box(group, "Monitor housing", (3.75, 1.57, 0.30), (1.03, 0.64, 0.06), "BlackPlastic", 0.014, 3)
    box(group, "Dim monitor glass", (3.75, 1.57, 0.333), (0.96, 0.562, 0.008), "Screen", 0.008)
    box(group, "Monitor landscape silhouette", (3.75, 1.41, 0.340), (0.94, 0.20, 0.002), "ScreenLight")
    cylinder(group, "Monitor status LED", (4.16, 1.274, 0.331), (4.16, 1.274, 0.338), 0.006, "WarmLight", 8)
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
    box(group, "Right wall notice frame", (5.67, 2.03, -2.98), (0.055, 1.03, 1.18), "Oak", 0.014)
    box(group, "Right wall linen panel", (5.637, 2.03, -2.98), (0.006, 0.91, 1.06), "Linen")
    for index in range(3):
        box(group, "Pinned small photograph", (5.631, 2.04 + (index % 2) * 0.18, -3.31 + index * 0.30),
            (0.003, 0.22, 0.20), ("Sky", "NearMountain", "Terracotta")[index])
    box(group, "Front interior door surround", (3.48, 1.12, 6.30), (1.28, 2.26, 0.045), "Walnut")
    box(group, "Front interior flush door", (3.48, 1.10, 6.27), (1.13, 2.12, 0.04), "Oak", 0.01)
    cylinder(group, "Entry brass lever", (3.88, 1.06, 6.23), (3.72, 1.06, 6.23), 0.014, "MutedGold", 10)
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
    group = "Rpg"
    cylinder(group, "RPG launch tube", (-0.205, 0.025, 0), (0.205, 0.025, 0), 0.043, "RpgGreen", 16)
    cylinder(group, "RPG rear venturi", (-0.285, 0.025, 0), (-0.19, 0.025, 0), 0.071, "Metal", 16, 0.046)
    cylinder(group, "RPG rear dark mouth", (-0.287, 0.025, 0), (-0.285, 0.025, 0), 0.056, "DarkRecess", 16)
    cylinder(group, "RPG muzzle ring", (0.195, 0.025, 0), (0.218, 0.025, 0), 0.052, "Metal", 16)
    cylinder(group, "RPG muzzle dark", (0.219, 0.025, 0), (0.221, 0.025, 0), 0.037, "DarkRecess", 16)
    for x in (-0.145, -0.055):
        cylinder(group, "RPG wood heatshield", (x - 0.025, 0.025, 0), (x + 0.025, 0.025, 0),
                 0.050, "WalnutEdge", 12)
    box(group, "RPG pistol grip", (0.073, -0.042, 0), (0.045, 0.104, 0.052), "WalnutEdge", 0.008)
    box(group, "RPG sight base", (0.018, 0.087, 0), (0.081, 0.035, 0.026), "Metal", 0.004)
    box(group, "RPG optical sight", (-0.025, 0.106, 0), (0.085, 0.023, 0.028), "BlackPlastic", 0.004)
    group = "Rack"
    box(group, "Miniature equipment rack base", (0, -0.18, 0), (0.80, 0.04, 0.55), "RpgGreen", 0.012)
    box(group, "Miniature equipment rack back", (0, 0.02, -0.23), (0.80, 0.36, 0.045), "RpgGreen", 0.008)
    for x in (-0.36, 0.36):
        box(group, "Equipment rack end", (x, -0.05, 0.0), (0.055, 0.28, 0.51), "RpgGreen", 0.008)
    for x in (-0.19, 0.19):
        box(group, "Equipment cradle", (x, -0.11, 0.02), (0.045, 0.12, 0.23), "Metal", 0.005)


def add_miniature_broken_wall():
    """Retain the true opening while exposing the reverse face above a low corridor."""
    for original in list(COLLECTIONS["BrokenWall"].objects):
        if original.name.startswith("Exit corridor ceiling"):
            continue
        duplicate = original.copy()
        duplicate.data = original.data.copy()
        duplicate.name = "Mini cutaway | " + original.name
        duplicate["home_group"] = "MiniBrokenWall"
        if original.name.startswith(("Exit corridor wall", "Exit warm end")):
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
        ("02 | Desk and matching miniature", (0.6, 2.10, 2.70), (2.2, 1.05, 0.58), 30),
        ("03 | Room editorial corner", (-1.0, 2.45, 5.5), (-1.2, 1.1, -2.3), 19),
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
    add_window()
    add_decor()
    add_small_props()
    add_shelf()
    add_door()
    add_rpg_and_rack()
    add_reverse_door_latch()
    presentation = bpy.data.collections.new("Presentation | linked instances excluded from game export")
    scene.collection.children.link(presentation)
    instance("Real bookcase default pose", "Shelf", (0, 1.3, -5.95), collection=presentation)
    base = Vector((1.99, 1.04, 0.77))
    for group in ("Interior", "MiniWalls", "IntactWall"):
        instance("Tabletop same-layout " + group, group, base, 0.10, presentation)
    instance("Tabletop bookcase same-layout", "Shelf", base + Vector((0, 1.3, -5.95)) * 0.10,
             0.10, presentation)
    instance("Detached miniature door", "Door", (4.05, 1.23, 0.56), 0.16, presentation)
    instance("Miniature equipment rack", "Rack", (3.15, 1.20, 0.67), 1.0, presentation)
    instance("Miniature RPG", "Rpg", (3.16, 1.21, 0.59), 1.0, presentation)
    add_presentation(scene)
    for name, collection in COLLECTIONS.items():
        collection.use_fake_user = True
    bpy.context.view_layer.update()
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / "내 방.blend"))
    print("Saved authored home with", len(bpy.data.objects), "objects")


if __name__ == "__main__":
    main()
