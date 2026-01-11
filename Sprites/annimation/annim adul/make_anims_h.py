#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
from pathlib import Path
from collections import Counter, deque, defaultdict
from PIL import Image, ImageSequence

# ==========================
# RÉGLAGES
# ==========================
OUT_DIR_NAME = "out_headers"
ALLOWED_EXTS = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"}

# Si tes PNG ont déjà la transparence nickel, laisse à False
AUTO_REMOVE_BG_IF_NO_ALPHA = False

# Tolérance pour enlever le fond (si AUTO_REMOVE_BG_IF_NO_ALPHA=True)
BG_TOLERANCE = 22

# Pixels alpha <= threshold => considérés transparents
ALPHA_THRESHOLD = 0

# "none"  = pas de recadrage (recommandé si tes exports ont déjà la bonne taille)
# "union" = recadre toutes les frames selon la bbox commune (économise mémoire mais change W/H)
TRIM_MODE = "none"  # "none" ou "union"

# Candidats KEY RGB565 (on choisit automatiquement une KEY qui ne collisionne pas)
KEY_CANDIDATES_565 = [0xF81F, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF800, 0x0010]

# Pattern: "<perso> <anim> <num>"
# Ex: "triceratops Marche 02.png"
NAME_RE = re.compile(r"^\s*(?P<char>.+?)\s+(?P<anim>.+?)\s+(?P<idx>\d+)\s*$", re.IGNORECASE)


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
    try:
        frame0 = next(ImageSequence.Iterator(im))
        return frame0.copy()
    except Exception:
        return im.copy()


def color_dist(c1, c2) -> int:
    return abs(c1[0] - c2[0]) + abs(c1[1] - c2[1]) + abs(c1[2] - c2[2])


def guess_background_color(im_rgba: Image.Image):
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
    if im_rgba.mode != "RGBA":
        im_rgba = im_rgba.convert("RGBA")

    w, h = im_rgba.size
    px = im_rgba.load()
    bg = guess_background_color(im_rgba)

    visited = [[False] * w for _ in range(h)]
    q = deque()

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


def alpha_bbox(im_rgba: Image.Image):
    if im_rgba.mode != "RGBA":
        im_rgba = im_rgba.convert("RGBA")
    alpha = im_rgba.getchannel("A")
    return alpha.getbbox()


def union_bbox(bboxes):
    b = None
    for bb in bboxes:
        if bb is None:
            continue
        if b is None:
            b = bb
        else:
            x0 = min(b[0], bb[0])
            y0 = min(b[1], bb[1])
            x1 = max(b[2], bb[2])
            y1 = max(b[3], bb[3])
            b = (x0, y0, x1, y1)
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


def image_to_rgb565_array(im: Image.Image, key565: int, alpha_threshold: int = 0):
    im_rgba = im.convert("RGBA")
    w, h = im_rgba.size

    values = []
    has_key_collision = False

    for (r, g, b, a) in im_rgba.getdata():
        if a <= alpha_threshold:
            values.append(key565)
        else:
            v = rgb888_to_rgb565(r, g, b)
            if v == key565:
                has_key_collision = True
            values.append(v)

    return w, h, values, has_key_collision


# ==========================
# MANIFEST "AIDE CODE .INO"
# ==========================
def write_manifest_guide_txt(out_path: Path, header_filename: str, char_name: str,
                            w: int, h: int, key565: int, trim_mode: str, namespace: str, anims_dict):
    """
    Génère un TXT orienté "comment je l'utilise dans mon .ino"
    anims_dict: anim -> list[(idx, values)]
    """
    ns = namespace
    lines = []
    lines.append("=== Sprite Animations - Guide d'utilisation ===")
    lines.append("")
    lines.append(f"Header à inclure : {header_filename}")
    lines.append(f"Personnage       : {char_name}")
    lines.append(f"Namespace C++     : {ns}")
    lines.append("")
    lines.append(f"Taille d'une frame: {w} x {h} pixels")
    lines.append(f"Couleur KEY (RGB565) pour transparence : 0x{key565:04X}")
    lines.append(f"Trim mode         : {trim_mode}")
    lines.append("")
    lines.append("Constantes disponibles :")
    lines.append(f" - {ns}::W   (largeur)")
    lines.append(f" - {ns}::H   (hauteur)")
    lines.append(f" - {ns}::KEY (couleur transparente)")
    lines.append("")
    lines.append("Animations disponibles :")
    # On liste aussi les noms d'IDs dans l'enum
    anim_list = list(anims_dict.keys())
    for anim_name in sorted(anim_list, key=lambda s: s.lower()):
        av = sanitize_name(anim_name)
        enum_name = f"{ns}::ANIM_{av.upper()}"
        count_name = f"{ns}::{sanitize_name(char_name)}_{av}_count".replace("__", "_")
        frames_name = f"{ns}::{sanitize_name(char_name)}_{av}_frames".replace("__", "_")
        nb = len(anims_dict[anim_name])
        lines.append(f" - {anim_name} : {nb} frame(s)")
        lines.append(f"    enum id      : {enum_name}")
        lines.append(f"    frames table : {frames_name}  (pointeurs vers frames)")
        lines.append(f"    frame count  : {count_name}")
    lines.append("")
    lines.append("Lecture générique (table globale) :")
    lines.append(f" - {ns}::ANIMS[animId].frames")
    lines.append(f" - {ns}::ANIMS[animId].count")
    lines.append("")
    lines.append("Exemple minimal (LovyanGFX) :")
    lines.append("")
    lines.append("  #include \"" + header_filename + "\"")
    lines.append("  // ... LGFX lcd;")
    lines.append("  ")
    lines.append("  void drawAnimFrame(int x, int y, " + ns + "::AnimId anim, uint8_t frameIndex) {")
    lines.append("    // Récupère la description de l'anim")
    lines.append("    " + ns + "::AnimDesc ad;")
    lines.append("    memcpy_P(&ad, &" + ns + "::ANIMS[anim], sizeof(ad));")
    lines.append("")
    lines.append("    if (ad.count == 0) return;")
    lines.append("    frameIndex %= ad.count;")
    lines.append("")
    lines.append("    // Récupère le pointeur de frame")
    lines.append("    const uint16_t* framePtr;")
    lines.append("    memcpy_P(&framePtr, &ad.frames[frameIndex], sizeof(framePtr));")
    lines.append("")
    lines.append("    // Dessin (color-key)")
    lines.append("    // Note: pushImage n'a pas toujours le key, selon ta conf.")
    lines.append("    // Si tu utilises un Sprite LovyanGFX, tu peux dessiner pixel par pixel en sautant KEY.")
    lines.append("    lcd.pushImage(x, y, " + ns + "::W, " + ns + "::H, framePtr);")
    lines.append("  }")
    lines.append("")
    lines.append("Notes :")
    lines.append(" - Les frames sont stockées en PROGMEM (flash).")
    lines.append(" - Si tu veux une transparence parfaite (KEY), dessine en ignorant les pixels == KEY.")
    lines.append(" - Pour animer: incrémente frameIndex toutes les X ms (ex: 80-120ms).")
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="utf-8")


# ==========================
# GÉNÉRATION HEADER
# ==========================
def write_character_header(out_path: Path, char_name: str, anims_dict, w: int, h: int, key565: int, key_warn):
    char_var = sanitize_name(char_name)

    def fmt(v): return f"0x{v:04X}"

    lines = []
    lines.append("#pragma once")
    lines.append("#include <Arduino.h>")
    lines.append("")
    lines.append("// Auto-generated: animations (frames) -> RGB565 (KEY transparency)")
    lines.append(f"// Character: {char_name}")
    lines.append(f"// Frame size: {w}x{h}")
    lines.append(f"// KEY (RGB565): 0x{key565:04X}")
    if key_warn:
        lines.append("// WARNING: KEY collides with opaque pixels (auto couldn't find a perfect KEY).")
    lines.append("")
    lines.append(f"namespace {char_var} {{")
    lines.append(f"  static const uint16_t W = {w};")
    lines.append(f"  static const uint16_t H = {h};")
    lines.append(f"  static const uint16_t KEY = 0x{key565:04X};")
    lines.append("")

    # Frames + table de pointeurs
    for anim_name, frames in anims_dict.items():
        anim_var = sanitize_name(anim_name)

        for idx, values in frames:
            frame_var = f"{char_var}_{anim_var}_{idx:03d}"
            lines.append(f"  static const uint16_t {frame_var}[W * H] PROGMEM = {{")
            per_line = 12
            for i in range(0, len(values), per_line):
                chunk = values[i:i + per_line]
                lines.append("    " + ", ".join(fmt(v) for v in chunk) + ("," if i + per_line < len(values) else ""))
            lines.append("  };")
            lines.append("")

        lines.append(f"  static const uint16_t* const {char_var}_{anim_var}_frames[] PROGMEM = {{")
        ptrs = [f"{char_var}_{anim_var}_{idx:03d}" for idx, _ in frames]
        lines.append("    " + ", ".join(ptrs))
        lines.append("  };")
        lines.append(f"  static const uint8_t {char_var}_{anim_var}_count = {len(frames)};")
        lines.append("")

    # Enum + table globale ANIMS[]
    anim_list = list(anims_dict.keys())

    lines.append("  enum AnimId {")
    for i, an in enumerate(anim_list):
        sep = "," if i < len(anim_list) - 1 else ""
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
        sep = "," if i < len(anim_list) - 1 else ""
        lines.append(f"    {{ {char_var}_{av}_frames, {char_var}_{av}_count }}{sep}")
    lines.append("  };")
    lines.append("")

    lines.append(f"}} // namespace {char_var}")
    lines.append("")

    out_path.write_text("\n".join(lines), encoding="utf-8")


# ==========================
# MAIN
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

    grouped = defaultdict(lambda: defaultdict(list))

    for p in files:
        m = NAME_RE.match(p.stem)
        if not m:
            print(f"[SKIP] nom non reconnu: {p.name}  (attendu: <perso> <anim> <num>.png)")
            continue
        ch = m.group("char").strip()
        an = m.group("anim").strip()
        idx = int(m.group("idx"))
        grouped[ch][an].append((idx, p))

    if not grouped:
        print("Aucun fichier avec un nom valide.")
        return

    for char_name, anims in grouped.items():
        loaded = []

        for anim_name, lst in anims.items():
            for idx, path in sorted(lst, key=lambda t: t[0]):
                im = load_first_frame(path).convert("RGBA")

                if AUTO_REMOVE_BG_IF_NO_ALPHA:
                    alpha = im.getchannel("A")
                    min_a, _ = alpha.getextrema()
                    if min_a != 0:
                        im = remove_background_to_alpha(im, tolerance=BG_TOLERANCE)

                loaded.append((anim_name, idx, im))

        if not loaded:
            continue

        if TRIM_MODE == "union":
            bbs = [alpha_bbox(im) for _, _, im in loaded]
            bb = union_bbox(bbs)
            if bb is not None:
                loaded = [(an, idx, im.crop(bb)) for an, idx, im in loaded]

        w0, h0 = loaded[0][2].size
        for an, idx, im in loaded:
            if im.size != (w0, h0):
                raise RuntimeError(
                    f"Taille incohérente pour {char_name}: {an} {idx:02d} "
                    f"= {im.size}, attendu {(w0, h0)}. "
                    f"(Conseil: exporte toutes les frames avec la même taille, ou mets TRIM_MODE='union')"
                )

        key565, key_warn = choose_best_key_for_images([im for _, _, im in loaded])

        anim_arrays = defaultdict(list)
        any_collision = False
        for anim_name, idx, im in loaded:
            w, h, values, collision = image_to_rgb565_array(im, key565, alpha_threshold=ALPHA_THRESHOLD)
            any_collision = any_collision or collision
            anim_arrays[anim_name].append((idx, values))

        for anim_name in list(anim_arrays.keys()):
            anim_arrays[anim_name] = sorted(anim_arrays[anim_name], key=lambda t: t[0])

        # Génération du .h
        out_h = out_dir / f"{sanitize_name(char_name)}_anims_{w0}x{h0}.h"
        write_character_header(out_h, char_name, anim_arrays, w0, h0, key565, key_warn or any_collision)
        print(f"[OK] {char_name} -> {out_h.name} ({w0}x{h0})")

        # Génération du guide TXT (orienté code .ino)
        out_txt = out_dir / f"{sanitize_name(char_name)}_guide.txt"
        write_manifest_guide_txt(
            out_txt,
            header_filename=out_h.name,
            char_name=char_name,
            w=w0,
            h=h0,
            key565=key565,
            trim_mode=TRIM_MODE,
            namespace=sanitize_name(char_name),
            anims_dict=anim_arrays
        )
        print(f"[OK] Guide -> {out_txt.name}")

    print(f"\nTerminé. Fichiers générés dans: {out_dir}")


if __name__ == "__main__":
    main()
