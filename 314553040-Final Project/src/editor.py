#!/usr/bin/env python3
# editor.py
#
# Multi-OBJ importer + base-scene editor + "explode" geoms into independent bodies.
#
# Core features:
# - Import multiple OBJ meshes: --obj [--tex] [--pos/--euler/--scale/--rgba] [--pin]
# - Edit existing bodies: --move-body / --rot-body-euler / --rot-body-quat
# - Replace:
#   - Replace existing named geoms: --replace-geom-mesh (requires geom has name=...)
#   - NEW: Replace a body's (direct-child) geom mesh: --replace-body-mesh (works great with exploded bodies)
# - NEW: Explode a body into many bodies, one geom per body:
#        --explode-body venice  (for your file)
#
# Notes:
# - MuJoCo macOS build might not support JPEG textures. Use PNG.

import argparse
from pathlib import Path
import xml.etree.ElementTree as ET
from xml.dom import minidom

import numpy as np
import mujoco
import mujoco.viewer


# ----------------------------
# Utility parsing helpers
# ----------------------------

def broadcast(values, n, name):
    if values is None:
        return [None] * n
    if len(values) == 0:
        return [None] * n
    if len(values) == 1:
        return values * n
    if len(values) != n:
        raise ValueError(f"{name}: expected 1 or {n} values, got {len(values)}")
    return values


def parse_vec3_list(items, n, name, default):
    if items is None or len(items) == 0:
        return [np.array(default, dtype=float)] * n

    def parse_one(s):
        s = s.strip().replace(",", " ")
        parts = [p for p in s.split() if p]
        if len(parts) != 3:
            raise ValueError(f"{name}: expected 3 numbers, got: {s!r}")
        return np.array([float(parts[0]), float(parts[1]), float(parts[2])], dtype=float)

    vecs = [parse_one(s) for s in items]
    if len(vecs) == 1:
        return vecs * n
    if len(vecs) != n:
        raise ValueError(f"{name}: expected 1 or {n} entries, got {len(vecs)}")
    return vecs


def parse_vec4_list(items, n, name, default):
    if items is None or len(items) == 0:
        return [np.array(default, dtype=float)] * n

    def parse_one(s):
        s = s.strip().replace(",", " ")
        parts = [p for p in s.split() if p]
        if len(parts) != 4:
            raise ValueError(f"{name}: expected 4 numbers, got: {s!r}")
        v = np.array([float(parts[0]), float(parts[1]), float(parts[2]), float(parts[3])], dtype=float)
        if np.any(v < 0) or np.any(v > 1):
            raise ValueError(f"{name}: values must be in [0,1], got: {v.tolist()}")
        return v

    vecs = [parse_one(s) for s in items]
    if len(vecs) == 1:
        return vecs * n
    if len(vecs) != n:
        raise ValueError(f"{name}: expected 1 or {n} entries, got {len(vecs)}")
    return vecs


def _fmt_floats(arr):
    return " ".join(str(float(x)) for x in arr)


def pretty_xml(root: ET.Element) -> str:
    raw = ET.tostring(root, encoding="utf-8")
    reparsed = minidom.parseString(raw)
    return reparsed.toprettyxml(indent="  ")


# ----------------------------
# XML helpers
# ----------------------------

def load_xml_root(path: Path) -> ET.Element:
    txt = path.read_text(encoding="utf-8")
    root = ET.fromstring(txt)
    if root.tag != "mujoco":
        raise ValueError(f"Root tag is <{root.tag}>, expected <mujoco>")
    return root


def ensure_child(root: ET.Element, tag: str) -> ET.Element:
    ch = root.find(tag)
    if ch is None:
        ch = ET.SubElement(root, tag)
    return ch


def find_named_unique(root: ET.Element, tag: str, name: str) -> ET.Element:
    hits = [e for e in root.iter(tag) if e.get("name") == name]
    if len(hits) == 0:
        raise ValueError(f"[EDIT] {tag} not found: {name}")
    if len(hits) > 1:
        raise ValueError(f"[EDIT] {tag} name not unique: {name} (found {len(hits)} matches)")
    return hits[0]


def list_named(root: ET.Element, tag: str):
    names = []
    for e in root.iter(tag):
        nm = e.get("name")
        if nm:
            names.append(nm)
    return sorted(set(names))


def ensure_unique_name(existing: set[str], base: str) -> str:
    if base not in existing:
        existing.add(base)
        return base
    k = 2
    while f"{base}_{k}" in existing:
        k += 1
    name = f"{base}_{k}"
    existing.add(name)
    return name


# ----------------------------
# Editing operations (body)
# ----------------------------

def apply_move_body(root: ET.Element, body_name: str, pos_xyz):
    b = find_named_unique(root, "body", body_name)
    b.set("pos", _fmt_floats(pos_xyz))


def apply_rot_body_euler(root: ET.Element, body_name: str, euler_rads):
    b = find_named_unique(root, "body", body_name)
    if "quat" in b.attrib:
        del b.attrib["quat"]
    b.set("euler", _fmt_floats(euler_rads))


def apply_rot_body_quat(root: ET.Element, body_name: str, quat_wxyz):
    b = find_named_unique(root, "body", body_name)
    if "euler" in b.attrib:
        del b.attrib["euler"]
    b.set("quat", _fmt_floats(quat_wxyz))


def apply_replace_geom_mesh(root: ET.Element,
                            geom_name: str,
                            new_obj: Path,
                            new_tex: Path | None,
                            new_scale_xyz):
    """
    Replace a named <geom name="..."> with a new OBJ mesh (+ optional texture).
    - Adds <mesh> (and <texture>/<material>) into <asset>.
    - Updates the target geom to use type="mesh", mesh=..., material=...
    """
    asset = ensure_child(root, "asset")
    g = find_named_unique(root, "geom", geom_name)

    existing_mesh = set(list_named(root, "mesh"))
    existing_tex = set(list_named(root, "texture"))
    existing_mat = set(list_named(root, "material"))

    stem = new_obj.stem.replace(" ", "_")
    mesh_name = ensure_unique_name(existing_mesh, f"repl_{geom_name}_{stem}_mesh")
    tex_name = ensure_unique_name(existing_tex, f"repl_{geom_name}_{stem}_tex")
    mat_name = ensure_unique_name(existing_mat, f"repl_{geom_name}_{stem}_mat")

    mesh_el = ET.SubElement(asset, "mesh")
    mesh_el.set("name", mesh_name)
    mesh_el.set("file", new_obj.as_posix())
    mesh_el.set("scale", _fmt_floats(new_scale_xyz))

    if new_tex is not None:
        tex_el = ET.SubElement(asset, "texture")
        tex_el.set("name", tex_name)
        tex_el.set("type", "2d")
        tex_el.set("file", new_tex.as_posix())

    mat_el = ET.SubElement(asset, "material")
    mat_el.set("name", mat_name)
    if new_tex is not None:
        mat_el.set("texture", tex_name)

    rgba = g.get("rgba", "1 1 1 1")
    mat_el.set("rgba", rgba)

    g.set("type", "mesh")
    g.set("mesh", mesh_name)
    g.set("material", mat_name)
    if "size" in g.attrib:
        del g.attrib["size"]


def apply_replace_body_mesh(root: ET.Element,
                            body_name: str,
                            new_obj: Path,
                            new_tex: Path | None,
                            new_scale_xyz,
                            keep_geom_props: bool = True):
    """
    Replace the first direct-child <geom> under a named body (works well for exploded bodies).
    - Adds new <mesh>/<texture>/<material> in <asset>
    - Updates that geom to use the new mesh/material
    - Keeps original geom attributes (contype/conaffinity/class/rgba/etc.) by default
    """
    asset = ensure_child(root, "asset")
    b = find_named_unique(root, "body", body_name)

    geoms = [c for c in list(b) if c.tag == "geom"]
    if len(geoms) == 0:
        raise ValueError(f"[REPLACE] body '{body_name}' has no direct <geom> child to replace.")
    g = geoms[0]

    existing_mesh = set(list_named(root, "mesh"))
    existing_tex = set(list_named(root, "texture"))
    existing_mat = set(list_named(root, "material"))

    stem = new_obj.stem.replace(" ", "_")
    mesh_name = ensure_unique_name(existing_mesh, f"repl_{body_name}_{stem}_mesh")
    tex_name = ensure_unique_name(existing_tex, f"repl_{body_name}_{stem}_tex")
    mat_name = ensure_unique_name(existing_mat, f"repl_{body_name}_{stem}_mat")

    mesh_el = ET.SubElement(asset, "mesh")
    mesh_el.set("name", mesh_name)
    mesh_el.set("file", new_obj.as_posix())
    mesh_el.set("scale", _fmt_floats(new_scale_xyz))

    if new_tex is not None:
        tex_el = ET.SubElement(asset, "texture")
        tex_el.set("name", tex_name)
        tex_el.set("type", "2d")
        tex_el.set("file", new_tex.as_posix())

    mat_el = ET.SubElement(asset, "material")
    mat_el.set("name", mat_name)
    if new_tex is not None:
        mat_el.set("texture", tex_name)

    rgba = g.get("rgba", "1 1 1 1")
    mat_el.set("rgba", rgba)

    # Update geom
    g.set("type", "mesh")
    g.set("mesh", mesh_name)
    g.set("material", mat_name)
    if "size" in g.attrib:
        del g.attrib["size"]

    # If you want a clean geom, set keep_geom_props=False and reset a minimal set here.
    if not keep_geom_props:
        # keep name if exists; reset other attributes
        nm = g.get("name")
        g.attrib.clear()
        if nm:
            g.set("name", nm)
        g.set("type", "mesh")
        g.set("mesh", mesh_name)
        g.set("material", mat_name)
        g.set("contype", "0")
        g.set("conaffinity", "0")


# ----------------------------
# NEW: Explode a body into many bodies (one geom per body)
# ----------------------------

def explode_body_geoms(root: ET.Element, src_body_name: str, prefix: str, keep_original: bool):
    worldbody = ensure_child(root, "worldbody")
    src_body = find_named_unique(root, "body", src_body_name)

    geoms = [c for c in list(src_body) if c.tag == "geom"]
    if len(geoms) == 0:
        raise ValueError(f"[EXPLODE] body '{src_body_name}' has no direct <geom> children to explode.")

    created = []
    existing = set(list_named(root, "body"))

    for idx, g in enumerate(geoms):
        mesh = g.get("mesh")
        suffix = mesh if mesh else f"geom{idx}"
        new_body_name = ensure_unique_name(existing, f"{prefix}__{suffix}")

        nb = ET.SubElement(worldbody, "body")
        nb.set("name", new_body_name)
        nb.set("pos", "0 0 0")
        nb.set("euler", "0 0 0")

        g_copy = ET.fromstring(ET.tostring(g, encoding="utf-8"))
        nb.append(g_copy)

        created.append((new_body_name, mesh, g.get("material"), g.get("class")))

    if not keep_original:
        for g in geoms:
            src_body.remove(g)

    return created


# ----------------------------
# Import new objects (multi-OBJ)
# ----------------------------

def add_imported_objects(root: ET.Element, obj_paths, tex_paths, names, poses, eulers, scales, rgbs, pins):
    asset = ensure_child(root, "asset")
    worldbody = ensure_child(root, "worldbody")

    for i in range(len(obj_paths)):
        name = names[i]
        mesh_name = f"{name}_mesh"
        tex_name = f"{name}_tex"
        mat_name = f"{name}_mat"
        rgba = rgbs[i]

        mesh_el = ET.SubElement(asset, "mesh")
        mesh_el.set("name", mesh_name)
        mesh_el.set("file", obj_paths[i].as_posix())
        mesh_el.set("scale", _fmt_floats(scales[i]))

        if tex_paths[i] is not None:
            tex_el = ET.SubElement(asset, "texture")
            tex_el.set("name", tex_name)
            tex_el.set("type", "2d")
            tex_el.set("file", tex_paths[i].as_posix())

        mat_el = ET.SubElement(asset, "material")
        mat_el.set("name", mat_name)
        if tex_paths[i] is not None:
            mat_el.set("texture", tex_name)
        mat_el.set("rgba", _fmt_floats(rgba))

        body_el = ET.SubElement(worldbody, "body")
        body_el.set("name", name)
        body_el.set("pos", _fmt_floats(poses[i]))
        body_el.set("euler", _fmt_floats(eulers[i]))

        if not pins[i]:
            ET.SubElement(body_el, "freejoint")

        geom_el = ET.SubElement(body_el, "geom")
        geom_el.set("type", "mesh")
        geom_el.set("mesh", mesh_name)
        geom_el.set("material", mat_name)
        geom_el.set("contype", "0")
        geom_el.set("conaffinity", "0")


# ----------------------------
# CLI parsing for edit args
# ----------------------------

def parse_move_arg(s: str):
    parts = s.strip().split()
    if len(parts) != 4:
        raise ValueError(f'--move-body expects "bodyName x y z", got: {s!r}')
    return parts[0], np.array([float(parts[1]), float(parts[2]), float(parts[3])], dtype=float)


def parse_replace_geom_arg(s: str):
    """
    --replace-geom-mesh expects:
      "geomName new.obj [new.png] [sx sy sz]"
    """
    parts = s.strip().split()
    if len(parts) not in (2, 3, 6):
        raise ValueError('--replace-geom-mesh expects: '
                         '"geomName new.obj [new.png] [sx sy sz]"')
    geom_name = parts[0]
    objp = Path(parts[1]).expanduser().resolve()

    texp = None
    scale = np.array([1.0, 1.0, 1.0], dtype=float)

    if len(parts) == 3:
        texp = Path(parts[2]).expanduser().resolve()
    elif len(parts) == 6:
        texp = Path(parts[2]).expanduser().resolve()
        scale = np.array([float(parts[3]), float(parts[4]), float(parts[5])], dtype=float)

    if texp is not None and texp.suffix.lower() != ".png":
        raise ValueError(f"texture must be .png (macOS build). got: {texp}")

    return geom_name, objp, texp, scale


def parse_replace_body_arg(s: str):
    """
    --replace-body-mesh expects:
      "bodyName new.obj [new.png] [sx sy sz]"
    Works best for exploded bodies where each body has exactly one direct-child geom.
    """
    parts = s.strip().split()
    if len(parts) not in (2, 3, 6):
        raise ValueError('--replace-body-mesh expects: "bodyName new.obj [new.png] [sx sy sz]"')
    body_name = parts[0]
    objp = Path(parts[1]).expanduser().resolve()

    texp = None
    scale = np.array([1.0, 1.0, 1.0], dtype=float)

    if len(parts) == 3:
        texp = Path(parts[2]).expanduser().resolve()
    elif len(parts) == 6:
        texp = Path(parts[2]).expanduser().resolve()
        scale = np.array([float(parts[3]), float(parts[4]), float(parts[5])], dtype=float)

    if texp is not None and texp.suffix.lower() != ".png":
        raise ValueError(f"texture must be .png (macOS build). got: {texp}")

    return body_name, objp, texp, scale


def parse_rot_euler_arg(s: str):
    parts = s.strip().split()
    if len(parts) != 4:
        raise ValueError(f'--rot-body-euler expects "bodyName rx ry rz", got: {s!r}')
    return parts[0], np.array([float(parts[1]), float(parts[2]), float(parts[3])], dtype=float)


def parse_rot_quat_arg(s: str):
    parts = s.strip().split()
    if len(parts) != 5:
        raise ValueError(f'--rot-body-quat expects "bodyName w x y z", got: {s!r}')
    return parts[0], np.array([float(parts[1]), float(parts[2]), float(parts[3]), float(parts[4])], dtype=float)


# ----------------------------
# Main
# ----------------------------

def main():
    ap = argparse.ArgumentParser()

    ap.add_argument("--base", required=True, help="Base MJCF scene xml (e.g., venice.xml)")

    # Import (new objects)
    ap.add_argument("--obj", action="append", default=None, help="OBJ path. Repeat to import multiple objects.")
    ap.add_argument("--tex", action="append", default=None, help="Optional PNG diffuse texture (repeat or once).")
    ap.add_argument("--name", action="append", default=None, help="Optional name prefix (repeat or once).")
    ap.add_argument("--pos", action="append", default=None, help='Position "x y z" (repeat or once).')
    ap.add_argument("--euler", action="append", default=None, help='Euler radians "rx ry rz" (repeat or once).')
    ap.add_argument("--scale", action="append", default=None, help='Scale "sx sy sz" (repeat or once).')
    ap.add_argument("--rgba", action="append", default=None, help='RGBA in [0,1] "r g b a" (repeat or once).')
    ap.add_argument("--pin", action="append", default=None,
                    help='Pin imported object (repeat): "1/0", "true/false". If once, broadcast.')

    # Replace
    ap.add_argument("--replace-geom-mesh", action="append", default=None,
                    help='Replace a named geom with a new mesh: "geomName new.obj [new.png] [sx sy sz]"')
    ap.add_argument("--replace-body-mesh", action="append", default=None,
                    help='Replace a body\'s first direct geom mesh: "bodyName new.obj [new.png] [sx sy sz]"')

    # Listing
    ap.add_argument("--list-bodies", action="store_true", help="List <body name=...> and exit.")

    # Edit existing bodies
    ap.add_argument("--move-body", action="append", default=None, help='Move body: "bodyName x y z"')
    ap.add_argument("--rot-body-euler", action="append", default=None, help='Rotate body (euler rad): "bodyName rx ry rz"')
    ap.add_argument("--rot-body-quat", action="append", default=None, help='Rotate body (quat w x y z): "bodyName w x y z"')

    # Explode
    ap.add_argument("--explode-body", default=None,
                    help='Explode all direct <geom> under this body into separate bodies. e.g. --explode-body venice')
    ap.add_argument("--explode-prefix", default=None,
                    help='Prefix for exploded bodies. Default: <explode-body-name>')
    ap.add_argument("--explode-keep-original", action="store_true",
                    help="Keep original geoms in the source body (duplicate). Default: remove from source body.")

    # Global
    ap.add_argument("--gravity", nargs=3, type=float, default=[0, 0, 0], help='Gravity gx gy gz. Default 0 0 0.')
    ap.add_argument("--out", default="_combined.xml", help="Output combined xml filename (written next to base).")
    ap.add_argument("--no-view", action="store_true", help="Do not open viewer; only write output XML.")

    args = ap.parse_args()

    base_path = Path(args.base).resolve()
    out_path = base_path.parent / args.out

    root = load_xml_root(base_path)

    if args.list_bodies:
        print("[LIST] bodies:")
        for nm in list_named(root, "body"):
            print("  ", nm)
        return

    # 1) explode (do this early, so edits can target new bodies)
    if args.explode_body:
        prefix = args.explode_prefix if args.explode_prefix else args.explode_body
        created = explode_body_geoms(root, args.explode_body, prefix=prefix, keep_original=args.explode_keep_original)
        print(f"[EXPLODE] created {len(created)} bodies from body '{args.explode_body}'. Example names:")
        for i, (bname, mesh, mat, cls) in enumerate(created[:10]):
            print(f"  {i:02d}: body={bname}  (mesh={mesh}, material={mat}, class={cls})")
        if len(created) > 10:
            print("  ...")

    # 2) import new objects (optional)
    if args.obj is not None and len(args.obj) > 0:
        obj_paths = [Path(p).expanduser().resolve() for p in args.obj]
        n = len(obj_paths)

        if args.tex is not None:
            tex_paths_raw = broadcast(args.tex, n, "--tex")
            tex_paths = []
            for p in tex_paths_raw:
                if p is None:
                    tex_paths.append(None)
                    continue
                tp = Path(p).expanduser().resolve()
                if tp.suffix.lower() != ".png":
                    raise ValueError(f"--tex must be PNG (.png). Got: {tp}")
                tex_paths.append(tp)
        else:
            tex_paths = [None] * n

        if args.name is None or len(args.name) == 0:
            names = []
            for i, op in enumerate(obj_paths):
                stem = op.stem.replace(" ", "_")
                names.append(f"import_{stem}_{i}")
        else:
            names = broadcast(args.name, n, "--name")

        poses = parse_vec3_list(args.pos, n, "--pos", default=[0, 0, 0])
        eulers = parse_vec3_list(args.euler, n, "--euler", default=[0, 0, 0])
        scales = parse_vec3_list(args.scale, n, "--scale", default=[1, 1, 1])
        rgbs = parse_vec4_list(args.rgba, n, "--rgba", default=[1, 1, 1, 1])

        def parse_bool(s: str) -> bool:
            s = s.strip().lower()
            if s in ("1", "true", "t", "yes", "y", "on"):
                return True
            if s in ("0", "false", "f", "no", "n", "off"):
                return False
            raise ValueError(f'--pin value must be bool-like, got: {s!r}')

        if args.pin is None or len(args.pin) == 0:
            pins = [False] * n
        else:
            pin_raw = broadcast(args.pin, n, "--pin")
            pins = [parse_bool(s) for s in pin_raw]

        add_imported_objects(root, obj_paths, tex_paths, names, poses, eulers, scales, rgbs, pins)

    # 3) replace (do before pose edits or after; either is fine because body name stays the same)
    if args.replace_geom_mesh:
        for s in args.replace_geom_mesh:
            geom_name, objp, texp, scale = parse_replace_geom_arg(s)
            apply_replace_geom_mesh(root, geom_name, objp, texp, scale)

    if args.replace_body_mesh:
        for s in args.replace_body_mesh:
            body_name, objp, texp, scale = parse_replace_body_arg(s)
            apply_replace_body_mesh(root, body_name, objp, texp, scale, keep_geom_props=True)

    # 4) edits (move/rotate bodies)
    if args.move_body:
        for s in args.move_body:
            nm, pos = parse_move_arg(s)
            apply_move_body(root, nm, pos)

    if args.rot_body_euler:
        for s in args.rot_body_euler:
            nm, euler = parse_rot_euler_arg(s)
            apply_rot_body_euler(root, nm, euler)

    if args.rot_body_quat:
        for s in args.rot_body_quat:
            nm, quat = parse_rot_quat_arg(s)
            apply_rot_body_quat(root, nm, quat)

    # 5) write output
    out_xml = pretty_xml(root)
    out_path.write_text(out_xml, encoding="utf-8")
    print(f"[INFO] wrote xml: {out_path}")

    if args.no_view:
        return

    # 6) load + view
    model = mujoco.MjModel.from_xml_path(str(out_path))
    model.opt.gravity[:] = np.array(args.gravity, dtype=float)
    data = mujoco.MjData(model)

    with mujoco.viewer.launch_passive(model=model, data=data, show_left_ui=False, show_right_ui=False) as viewer:
        while viewer.is_running():
            mujoco.mj_step(model, data)
            viewer.sync()


if __name__ == "__main__":
    main()
