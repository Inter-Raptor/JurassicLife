#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
from pathlib import Path
from collections import Counter, deque

from PIL import Image, ImageSequence

# ==========================
# RÉGLAGES "AUTO" (tu ne touches à rien)
# ==========================
OUT_DIR_NAME = "out_headers"
ALLOWED_EXTS = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"}

# Tolérance pour enlever le fond (plus haut = enlève plus, plus bas = plus strict)
BG_TOLERANCE = 22

# Pixels alpha <= threshold => considérés transparents (utile si bords semi-transparents)
ALPHA_THRESHOLD = 0

# Candidats KEY RGB565 (on choisit automatiquement une KEY qui ne collisionne pas)
KEY_CANDIDATES_565 = [0xF81F, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF800, 0x0010]


# ==========================
# OUTILS
# ==========================
def sanitize_name(name: str) -> str:
    name = name.lower()
    name = re.sub(r"[^a-z0-9_]+", "_", name)
    name = re.sub(r"_+", "_", name).strip("_")
    if not name:
        name = "sprite"
    if name[0].isdigit():
        name = "_" + name
    return name


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def load_first_frame(path: Path) -> Image.Image:
    im = Image.open(path)
    # Si GIF/APNG etc, on prend la première frame
    try:
        frame0 = next(ImageSequence.Iterator(im))
        return frame0.copy()
    except Exception:
        return im.copy()


def trim_transparent(im_rgba: Image.Image):
    """
    Retourne (cropped_image, bbox) ou (original, None) si rien à trim.
    bbox = (x0, y0, x1, y1) (right/bottom exclusives)
    """
    if im_rgba.mode != "RGBA":
        im_rgba = im_rgba.convert("RGBA")
    alpha = im_rgba.getchannel("A")
    bbox = alpha.getbbox()  # bbox des pixels alpha > 0
    if bbox is None:
        return im_rgba, None
    return im_rgba.crop(bbox), bbox


def color_dist(c1, c2) -> int:
    # distance Manhattan sur RGB (rapide)
    return abs(c1[0] - c2[0]) + abs(c1[1] - c2[1]) + abs(c1[2] - c2[2])


def guess_background_color(im_rgba: Image.Image):
    """
    Devine la couleur de fond à partir des coins + milieux de bords.
    """
    w, h = im_rgba.size
    px = im_rgba.load()

    samples = []
    coords = [
        (0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1),
        (w // 2, 0), (w // 2, h - 1), (0, h // 2), (w - 1, h // 2),
    ]
    for x, y in coords:
        r, g, b, a = px[x, y]
        samples.append((r, g, b))

    return Counter(samples).most_common(1)[0][0]


def remove_background_to_alpha(im_rgba: Image.Image, tolerance: int) -> Image.Image:
    """
    Enlève le fond par flood-fill depuis les bords (très utile pour JPG/WEBP sans alpha).
    Ne supprime que ce qui est connecté aux bords.
    """
    if im_rgba.mode != "RGBA":
        im_rgba = im_rgba.convert("RGBA")

    w, h = im_rgba.size
    px = im_rgba.load()

    bg = guess_background_color(im_rgba)

    visited = [[False] * w for _ in range(h)]
    q = deque()

    # init : tous les pixels du bord
    for x in range(w):
        q.append((x, 0))
        q.append((x, h - 1))
    for y in range(h):
        q.append((0, y))
        q.append((w - 1, y))

    def is_bg(x, y) -> bool:
        r, g, b, a = px[x, y]
        if a == 0:
            return True
        return color_dist((r, g, b), bg) <= tolerance

    while q:
        x, y = q.popleft()
        if x < 0 or x >= w or y < 0 or y >= h:
            continue
        if visited[y][x]:
            continue
        visited[y][x] = True

        if not is_bg(x, y):
            continue

        r, g, b, a = px[x, y]
        px[x, y] = (r, g, b, 0)

        q.append((x + 1, y))
        q.append((x - 1, y))
        q.append((x, y + 1))
        q.append((x, y - 1))

    return im_rgba


def choose_best_key(im: Image.Image):
    """
    Choisit une KEY (RGB565) qui ne collisionne pas avec les pixels opaques.
    Retourne (key565, warn_bool)
    """
    rgba = im.convert("RGBA")
    opaque_565 = set()

    for (r, g, b, a) in rgba.getdata():
        if a > ALPHA_THRESHOLD:
            opaque_565.add(rgb888_to_rgb565(r, g, b))

    for k in KEY_CANDIDATES_565:
        if k not in opaque_565:
            return k, False

    return KEY_CANDIDATES_565[0], True


def image_to_rgb565_array(im: Image.Image, key565: int, alpha_threshold: int = 0):
    """
    Retourne (w, h, values, has_key_collision)
    alpha_threshold: <= ce seuil => pixel considéré transparent -> KEY
    """
    im_rgba = im.convert("RGBA")
    w, h = im_rgba.size
    pixels = list(im_rgba.getdata())

    values = []
    has_key_collision = False

    for (r, g, b, a) in pixels:
        if a <= alpha_threshold:
            values.append(key565)
        else:
            v = rgb888_to_rgb565(r, g, b)
            if v == key565:
                has_key_collision = True
            values.append(v)

    return w, h, values, has_key_collision


def write_header(out_path: Path, var_base: str, w: int, h: int, key565: int,
                 values, bbox=None, warn_key_collision=False):
    def fmt(v): return f"0x{v:04X}"

    lines = []
    lines.append("#pragma once")
    lines.append("#include <Arduino.h>")
    lines.append("")
    lines.append("// Auto-generated: image -> RGB565 (KEY transparency)")
    if bbox is not None:
        x0, y0, x1, y1 = bbox
        lines.append(f"// Trim bbox (original): x0={x0} y0={y0} x1={x1} y1={y1} (right/bottom exclusive)")
    lines.append(f"// Size: {w}x{h}")
    lines.append(f"// KEY (RGB565): 0x{key565:04X}")
    if warn_key_collision:
        lines.append("// WARNING: KEY collides with opaque pixels (auto couldn't find a perfect KEY).")
    lines.append("")
    lines.append(f"static const uint16_t {var_base}_W = {w};")
    lines.append(f"static const uint16_t {var_base}_H = {h};")
    lines.append(f"static const uint16_t {var_base}_KEY = 0x{key565:04X};")
    lines.append("")
    lines.append(f"static const uint16_t {var_base}[{var_base}_W * {var_base}_H] PROGMEM = {{")

    per_line = 12
    for i in range(0, len(values), per_line):
        chunk = values[i:i + per_line]
        lines.append("  " + ", ".join(fmt(v) for v in chunk) + ("," if i + per_line < len(values) else ""))
    lines.append("};")
    lines.append("")

    out_path.write_text("\n".join(lines), encoding="utf-8")


# ==========================
# MAIN (zéro argument)
# ==========================
def main():
    script_dir = Path(__file__).resolve().parent
    out_dir = script_dir / OUT_DIR_NAME
    out_dir.mkdir(parents=True, exist_ok=True)

    files = [p for p in sorted(script_dir.iterdir())
             if p.is_file() and p.suffix.lower() in ALLOWED_EXTS]

    if not files:
        print("Aucune image trouvée dans le dossier du script.")
        return

    for path in files:
        im = load_first_frame(path).convert("RGBA")

        # 1) Si pas d'alpha déjà présent, on tente de supprimer le fond (bords)
        alpha = im.getchannel("A")
        min_a, max_a = alpha.getextrema()
        has_any_transparent = (min_a == 0)

        if not has_any_transparent:
            im = remove_background_to_alpha(im, tolerance=BG_TOLERANCE)

        # 2) Trim (recadrage) autour du sprite
        im, bbox = trim_transparent(im)

        # 3) Choix KEY auto (évite collision avec pixels opaques)
        key565, key_warn = choose_best_key(im)

        # 4) Conversion RGB565 + KEY
        w, h, values, key_collision = image_to_rgb565_array(
            im, key565, alpha_threshold=ALPHA_THRESHOLD
        )

        base = sanitize_name(path.stem)
        out_path = out_dir / f"{base}_{w}x{h}.h"

        write_header(
            out_path, base, w, h, key565, values,
            bbox=bbox,
            warn_key_collision=(key_warn or key_collision)
        )

        print(f"[OK] {path.name} -> {out_path.name} ({w}x{h})")

    print(f"\nTerminé. Headers dans: {out_dir}")


if __name__ == "__main__":
    main()
