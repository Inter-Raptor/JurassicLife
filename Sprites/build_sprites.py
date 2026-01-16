#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import re
from pathlib import Path
from collections import Counter, deque, defaultdict
from PIL import Image, ImageSequence

# ============================================================
# CONFIG
# ============================================================
OUT_DIR_NAME = "generated_headers"
SINGLE_DIR_NAME = "single"
ANIMS_DIR_NAME = "anims"

ALLOWED_EXTS = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"}

AUTO_REMOVE_BG_IF_NO_ALPHA = True
BG_TOLERANCE = 22
ALPHA_THRESHOLD = 0

TRIM_SINGLE = True
TRIM_ANIMS = "none"  # "none" ou "union"

KEY_CANDIDATES_565 = [0xF81F, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF800, 0x0010]

# Pattern anim: "<perso> <anim> <num>"
# Ex: "dino oeuf 01.png"
# Ex: "triceratops Senior Marche 02.png"
NAME_RE = re.compile(r"^\s*(?P<char>.+?)\s+(?P<anim>.+?)\s+(?P<idx>\d+)\s*$", re.IGNORECASE)

# ============================================================
# BASE DIR (py vs exe)
# ============================================================
def get_base_dir() -> Path:
    if getattr(sys, "frozen", False):  # exe PyInstaller
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent

# ============================================================
# String utils
# ============================================================
def sanitize_name(name: str) -> str:
    name = name.lower()
    name = re.sub(r"[^a-z0-9_]+", "_", name)
    name = re.sub(r"_+", "_", name).strip("_")
    if not name:
        name = "sprite"
    if name[0].isdigit():
        name = "_" + name
    return name

# ============================================================
# Image utils
# ============================================================
def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def load_first_frame(path: Path) -> Image.Image:
    im = Image.open(path)
    try:
        frame0 = next(ImageSequence.Iterator(im))
        return frame0.copy()
    except Exception:
        return im.copy()

def color_dist(c1, c2) -> int:
    return abs(c1[0]-c2[0]) + abs(c1[1]-c2[1]) + abs(c1[2]-c2[2])

def guess_background_color(im_rgba: Image.Image):
    w, h = im_rgba.size
    px = im_rgba.load()
    samples = []
    coords = [
        (0, 0), (w-1, 0), (0, h-1), (w-1, h-1),
        (w//2, 0), (w//2, h-1), (0, h//2), (w-1, h//2),
    ]
    for x, y in coords:
        r, g, b, a = px[x, y]
        samples.append((r, g, b))
    return Counter(samples).most_common(1)[0][0]

def remove_background_to_alpha(im_rgba: Image.Image, tolerance: int) -> Image.Image:
    if im_rgba.mode != "RGBA":
        im_rgba = im_rgba.convert("RGBA")

    w, h = im_rgba.size
    px = im_rgba.load()
    bg = guess_background_color(im_rgba)

    visited = [[False]*w for _ in range(h)]
    q = deque()

    for x in range(w):
        q.append((x, 0))
        q.append((x, h-1))
    for y in range(h):
        q.append((0, y))
        q.append((w-1, y))

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
        q.append((x+1, y))
        q.append((x-1, y))
        q.append((x, y+1))
        q.append((x, y-1))

    return im_rgba

def trim_transparent(im_rgba: Image.Image):
    if im_rgba.mode != "RGBA":
        im_rgba = im_rgba.convert("RGBA")
    alpha = im_rgba.getchannel("A")
    bbox = alpha.getbbox()
    if bbox is None:
        return im_rgba, None
    return im_rgba.crop(bbox), bbox

def alpha_bbox(im_rgba: Image.Image):
    if im_rgba.mode != "RGBA":
        im_rgba = im_rgba.convert("RGBA")
    return im_rgba.getchannel("A").getbbox()

def union_bbox(bboxes):
    b = None
    for bb in bboxes:
        if bb is None:
            continue
        if b is None:
            b = bb
        else:
            b = (min(b[0], bb[0]), min(b[1], bb[1]), max(b[2], bb[2]), max(b[3], bb[3]))
    return b

def choose_best_key_for_images(images_rgba):
    opaque_565 = set()
    for im in images_rgba:
        rgba = im.convert("RGBA")
        for (r, g, b, a) in rgba.getdata():
            if a > ALPHA_THRESHOLD:
                opaque_565.add(rgb888_to_rgb565(r, g, b))
    for k in KEY_CANDIDATES_565:
        if k not in opaque_565:
            return k, False
    return KEY_CANDIDATES_565[0], True

def image_to_rgb565_array(im: Image.Image, key565: int):
    im_rgba = im.convert("RGBA")
    w, h = im_rgba.size
    values = []
    collision = False
    for (r, g, b, a) in im_rgba.getdata():
        if a <= ALPHA_THRESHOLD:
            values.append(key565)
        else:
            v = rgb888_to_rgb565(r, g, b)
            if v == key565:
                collision = True
            values.append(v)
    return w, h, values, collision

# ============================================================
# Write single header: NOM = image.h
# ============================================================
def write_single_header(out_path: Path, var_base: str, w: int, h: int, key565: int, values):
    def fmt(v): return f"0x{v:04X}"

    lines = []
    lines += ["#pragma once", "#include <Arduino.h>", ""]
    lines += ["// Auto-generated: single image -> RGB565 (KEY transparency)", ""]
    lines += [f"static const uint16_t {var_base}_W = {w};"]
    lines += [f"static const uint16_t {var_base}_H = {h};"]
    lines += [f"static const uint16_t {var_base}_KEY = 0x{key565:04X};"]
    lines += [""]
    lines += [f"static const uint16_t {var_base}[{var_base}_W * {var_base}_H] PROGMEM = {{"]

    per_line = 12
    for i in range(0, len(values), per_line):
        chunk = values[i:i+per_line]
        lines.append("  " + ", ".join(fmt(v) for v in chunk) + ("," if i+per_line < len(values) else ""))
    lines += ["};", ""]

    out_path.write_text("\n".join(lines), encoding="utf-8")

# ============================================================
# Write anim header: NOM = dossier.h
# Contient potentiellement plusieurs persos (namespace par perso)
# ============================================================
def write_anim_folder_header(out_path: Path, folder_name: str, data, key565: int, key_warn: bool):
    """
    data: dict char_name -> dict anim_name -> list[(idx, values)]
    """
    def fmt(v): return f"0x{v:04X}"

    lines = []
    lines += ["#pragma once", "#include <Arduino.h>", ""]
    lines += [f"// Auto-generated: anim folder '{folder_name}' -> RGB565 (KEY transparency)"]
    lines += [f"// KEY (RGB565): 0x{key565:04X}"]
    if key_warn:
        lines += ["// WARNING: KEY collides with some opaque pixels."]
    lines += [""]

    for char_name in sorted(data.keys(), key=lambda s: s.lower()):
        char_ns = sanitize_name(char_name)
        anims = data[char_name]

        # récupérer W/H depuis la 1ère frame
        # (stocké dans data via meta, voir build_anims)
        w = anims["_META"]["W"]
        h = anims["_META"]["H"]

        lines += [f"namespace {char_ns} {{",
                  f"  static const uint16_t W = {w};",
                  f"  static const uint16_t H = {h};",
                  f"  static const uint16_t KEY = 0x{key565:04X};",
                  ""]

        # Frames
        for anim_name in [k for k in anims.keys() if k != "_META"]:
            frames = anims[anim_name]
            anim_var = sanitize_name(anim_name)

            for idx, values in frames:
                frame_var = f"{char_ns}_{anim_var}_{idx:03d}"
                lines.append(f"  static const uint16_t {frame_var}[W * H] PROGMEM = {{")
                per_line = 12
                for i in range(0, len(values), per_line):
                    chunk = values[i:i+per_line]
                    lines.append("    " + ", ".join(fmt(v) for v in chunk) + ("," if i+per_line < len(values) else ""))
                lines.append("  };")
                lines.append("")

            # table de pointeurs
            lines.append(f"  static const uint16_t* const {char_ns}_{anim_var}_frames[] PROGMEM = {{")
            ptrs = [f"{char_ns}_{anim_var}_{idx:03d}" for idx, _ in frames]
            lines.append("    " + ", ".join(ptrs))
            lines.append("  };")
            lines.append(f"  static const uint8_t {char_ns}_{anim_var}_count = {len(frames)};")
            lines.append("")

        # Enum + table ANIMS
        anim_list = [k for k in anims.keys() if k != "_META"]
        lines.append("  enum AnimId {")
        for i, an in enumerate(anim_list):
            sep = "," if i < len(anim_list)-1 else ""
            lines.append(f"    ANIM_{sanitize_name(an).upper()}{sep}")
        lines.append("  };")
        lines.append("")
        lines.append("  struct AnimDesc {")
        lines.append("    const uint16_t* const* frames;")
        lines.append("    uint8_t count;")
        lines.append("  };")
        lines.append("")
        lines.append("  static const AnimDesc ANIMS[] PROGMEM = {")
        for i, an in enumerate(anim_list):
            av = sanitize_name(an)
            sep = "," if i < len(anim_list)-1 else ""
            lines.append(f"    {{ {char_ns}_{av}_frames, {char_ns}_{av}_count }}{sep}")
        lines.append("  };")
        lines.append("")
        lines.append(f"}} // namespace {char_ns}")
        lines.append("")

    out_path.write_text("\n".join(lines), encoding="utf-8")

# ============================================================
# Build singles
# ============================================================
def build_single_headers(single_dir: Path, out_dir: Path):
    files = [p for p in sorted(single_dir.iterdir())
             if p.is_file() and p.suffix.lower() in ALLOWED_EXTS]

    if not files:
        print(f"[SINGLE] Aucun fichier dans {single_dir}")
        return

    for path in files:
        im = load_first_frame(path).convert("RGBA")

        if AUTO_REMOVE_BG_IF_NO_ALPHA:
            alpha = im.getchannel("A")
            min_a, _ = alpha.getextrema()
            if min_a != 0:
                im = remove_background_to_alpha(im, tolerance=BG_TOLERANCE)

        if TRIM_SINGLE:
            im, _ = trim_transparent(im)

        key565, key_warn = choose_best_key_for_images([im])
        w, h, values, collision = image_to_rgb565_array(im, key565)

        base = sanitize_name(path.stem)
        out_path = out_dir / f"{base}.h"   # <-- NOM SIMPLE
        write_single_header(out_path, base, w, h, key565, values)
        print(f"[SINGLE OK] {path.name} -> {out_path.name} (warn={key_warn or collision})")

# ============================================================
# Build anims: 1 header par dossier (nom dossier.h)
# ============================================================
def build_anims_headers(anims_dir: Path, out_dir: Path):
    folders = [p for p in sorted(anims_dir.iterdir()) if p.is_dir()]
    if not folders:
        print(f"[ANIMS] Aucun sous-dossier dans {anims_dir}")
        return

    for folder in folders:
        files = [p for p in sorted(folder.iterdir())
                 if p.is_file() and p.suffix.lower() in ALLOWED_EXTS]
        if not files:
            print(f"[ANIMS] {folder.name}: aucun fichier image")
            continue

        grouped = defaultdict(lambda: defaultdict(list))  # char -> anim -> list[(idx, path)]
        images_for_key = []

        for p in files:
            m = NAME_RE.match(p.stem)
            if not m:
                continue
            ch = m.group("char").strip()
            an = m.group("anim").strip()
            idx = int(m.group("idx"))
            grouped[ch][an].append((idx, p))

        if not grouped:
            print(f"[ANIMS] {folder.name}: aucun fichier reconnu (noms ?)")
            continue

        # Charger tout (pour key + cohérence tailles)
        loaded = []  # list[(char, anim, idx, image)]
        for char_name, anims in grouped.items():
            for anim_name, lst in anims.items():
                for idx, path in sorted(lst, key=lambda t: t[0]):
                    im = load_first_frame(path).convert("RGBA")

                    if AUTO_REMOVE_BG_IF_NO_ALPHA:
                        alpha = im.getchannel("A")
                        min_a, _ = alpha.getextrema()
                        if min_a != 0:
                            im = remove_background_to_alpha(im, tolerance=BG_TOLERANCE)

                    loaded.append((char_name, anim_name, idx, im))
                    images_for_key.append(im)

        if not loaded:
            continue

        if TRIM_ANIMS == "union":
            bbs = [alpha_bbox(im) for _, _, _, im in loaded]
            bb = union_bbox(bbs)
            if bb is not None:
                loaded = [(c, a, i, im.crop(bb)) for c, a, i, im in loaded]

        # Choix key globale pour le dossier
        key565, key_warn = choose_best_key_for_images(images_for_key)

        # Construire data finale: char -> anim -> frames
        data = {}
        any_collision = False

        for char_name, anim_name, idx, im in loaded:
            w, h, values, collision = image_to_rgb565_array(im, key565)
            any_collision = any_collision or collision

            if char_name not in data:
                data[char_name] = {"_META": {"W": w, "H": h}}
            # vérif tailles cohérentes par perso
            if data[char_name]["_META"]["W"] != w or data[char_name]["_META"]["H"] != h:
                raise RuntimeError(
                    f"[{folder.name}] Taille incohérente pour '{char_name}': "
                    f"{(w,h)} vs {(data[char_name]['_META']['W'], data[char_name]['_META']['H'])}"
                )

            if anim_name not in data[char_name]:
                data[char_name][anim_name] = []
            data[char_name][anim_name].append((idx, values))

        # Trier les frames par index
        for char_name in data.keys():
            for anim_name in [k for k in data[char_name].keys() if k != "_META"]:
                data[char_name][anim_name] = sorted(data[char_name][anim_name], key=lambda t: t[0])

        out_h = out_dir / f"{sanitize_name(folder.name)}.h"   # <-- NOM = DOSSIER
        write_anim_folder_header(out_h, folder.name, data, key565, key_warn or any_collision)
        print(f"[ANIM OK] {folder.name} -> {out_h.name}")

# ============================================================
# MAIN
# ============================================================
def main():
    base = get_base_dir()
    single_dir = base / SINGLE_DIR_NAME
    anims_dir = base / ANIMS_DIR_NAME
    out_dir = base / OUT_DIR_NAME
    out_dir.mkdir(parents=True, exist_ok=True)

    print("=== JurassicLife Sprites Builder (simple names) ===")
    print(f"Dossier: {base}")
    print(f"Sortie : {out_dir}")
    print("")

    if single_dir.exists():
        build_single_headers(single_dir, out_dir)
    else:
        print("[SINGLE] dossier 'single' absent -> ignoré")

    print("")
    if anims_dir.exists():
        build_anims_headers(anims_dir, out_dir)
    else:
        print("[ANIMS] dossier 'anims' absent -> ignoré")

    print("\nTerminé.")

if __name__ == "__main__":
    try:
        main()
    finally:
        input("\nAppuie sur Entrée pour fermer...")
