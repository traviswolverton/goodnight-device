#!/usr/bin/env python3
"""Pack the button-label blanks into one two-colour 3MF plate for Bambu Studio / Orca.

Each icon button is one object with two parts: the blank itself (filament 1) and the
icon inlaid flush in its face, 0.8 mm deep (filament 2). The blanks print face down.

Export the STLs first, one pair per icon (needs OpenSCAD):

    for ic in moon toilet fan bulb tv playpause eye; do
      for L in base icon; do
        openscad -D 'part="label"' -D "icon=\\"$ic\\"" -D "layer=\\"$L\\"" -o DIR/${ic}_$L.stl bedside_enclosure.scad
      done
    done
    openscad -D 'part="label"' -D 'icon="none"' -D 'layer="base"' -o DIR/plain_base.stl bedside_enclosure.scad

then:  python3 labels_3mf.py DIR bedside_labels.3mf
"""
import re
import sys
import zipfile
from xml.sax.saxutils import escape

# button number, name, icon (None = plain blank, one colour)
BUTTONS = [
    (1, "Goodnight", "moon"),
    (2, "Bathroom trip", "toilet"),
    (3, "Fan", "fan"),
    (4, "Bedroom light", "bulb"),
    (5, "TV", "tv"),
    (6, "TV play-pause", "playpause"),
    (7, "Spare", None),
    (8, "Key echo", "eye"),
]
PITCH = 18.5            # blank_d + 3, as the "blanks" part lays them out
COLS = 4
PLATE_CENTRE = (128.0, 128.0)   # a 256 x 256 bed; use Arrange in the slicer for another
COLOURS = ("#2B2B2B", "#FFFFFF")


def read_stl(path):
    """Indexed mesh from an ASCII STL: (vertices, triangles)."""
    pts = re.findall(r"vertex\s+(\S+)\s+(\S+)\s+(\S+)", open(path).read())
    index, verts, tris = {}, [], []
    corners = []
    for p in pts:
        key = tuple(round(float(c), 4) for c in p)
        if key not in index:
            index[key] = len(verts)
            verts.append(key)
        corners.append(index[key])
    for i in range(0, len(corners), 3):
        a, b, c = corners[i:i + 3]
        if len({a, b, c}) == 3:
            tris.append((a, b, c))
    return verts, tris


def check_closed(name, tris):
    edges = {}
    for a, b, c in tris:
        for e in ((a, b), (b, c), (c, a)):
            edges[e] = edges.get(e, 0) + 1
    bad = [e for e, n in edges.items() if n != 1 or (e[1], e[0]) not in edges]
    if bad:
        print(f"warning: {name} has {len(bad)} open or doubled edges")


def mesh_xml(verts, tris):
    v = "".join(f'<vertex x="{x:.4f}" y="{y:.4f}" z="{z:.4f}"/>' for x, y, z in verts)
    t = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in tris)
    return f"<mesh><vertices>{v}</vertices><triangles>{t}</triangles></mesh>"


def main(stl_dir, out):
    resources, items, parts_cfg = [], [], []
    next_id = 2   # 1 is the materials group

    def add_mesh(path, material):
        nonlocal next_id
        verts, tris = read_stl(path)
        check_closed(path, tris)
        oid = next_id
        next_id += 1
        resources.append(f'<object id="{oid}" type="model" pid="1" pindex="{material}">{mesh_xml(verts, tris)}</object>')
        return oid

    for k, (num, name, icon) in enumerate(BUTTONS):
        body = add_mesh(f"{stl_dir}/{icon or 'plain'}_base.stl", 0)
        part_ids = [(body, f"{name} blank", 1)]
        if icon:
            part_ids.append((add_mesh(f"{stl_dir}/{icon}_icon.stl", 1), f"{name} icon", 2))
        gid = next_id
        next_id += 1
        comps = "".join(f'<component objectid="{pid}"/>' for pid, _, _ in part_ids)
        resources.append(f'<object id="{gid}" type="model"><components>{comps}</components></object>')
        col, row = k % COLS, k // COLS
        x = PLATE_CENTRE[0] + (col - (COLS - 1) / 2) * PITCH
        y = PLATE_CENTRE[1] + (row - 0.5) * PITCH
        items.append(f'<item objectid="{gid}" transform="1 0 0 0 1 0 0 0 1 {x:.3f} {y:.3f} 0"/>')
        parts = "".join(
            f'<part id="{pid}" subtype="normal_part"><metadata key="name" value="{escape(pname)}"/>'
            f'<metadata key="extruder" value="{ext}"/></part>' for pid, pname, ext in part_ids)
        parts_cfg.append(
            f'<object id="{gid}"><metadata key="name" value="Button {num} {escape(name)}"/>'
            f'<metadata key="extruder" value="1"/>{parts}</object>')

    materials = "".join(f'<base name="{n}" displaycolor="{c}"/>' for n, c in zip(("Blank", "Icon"), COLOURS))
    model = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
        '<metadata name="Title">Bedside dashboard button labels</metadata>'
        f'<resources><basematerials id="1">{materials}</basematerials>{"".join(resources)}</resources>'
        f'<build>{"".join(items)}</build></model>'
    )
    settings = '<?xml version="1.0" encoding="UTF-8"?>\n<config>' + "".join(parts_cfg) + "</config>"
    content_types = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
        '<Default Extension="config" ContentType="text/xml"/></Types>'
    )
    rels = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Target="/3D/3dmodel.model" Id="rel0" '
        'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>'
    )
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", content_types)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)
        z.writestr("Metadata/model_settings.config", settings)
    print(f"wrote {out}: {len(BUTTONS)} objects")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
