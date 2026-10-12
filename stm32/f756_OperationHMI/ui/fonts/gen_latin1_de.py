#!/usr/bin/env python3
"""Generate LVGL bpp4 fallback fonts for German umlauts (Ä Ö Ü ß ä ö ü)."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

CHARS = "ÄÖÜßäöü"
RANGE_START = ord("Ä")  # 0xC4
OUT_DIR = Path(__file__).resolve().parent
FONTS = {
    12: "/System/Library/Fonts/Supplemental/Arial.ttf",
    14: "/System/Library/Fonts/Supplemental/Arial.ttf",
    16: "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    28: "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
}


def pack_bpp4(pixels):
    out = []
    acc = 0
    nibble = 0
    for p in pixels:
        v = min(15, max(0, (p + 8) // 17))
        if nibble == 0:
            acc = v << 4
            nibble = 1
        else:
            acc |= v
            out.append(acc)
            nibble = 0
    if nibble == 1:
        out.append(acc)
    return out


def render_glyph(font, ch, ascent):
    pad = 8
    img = Image.new("L", (font.size * 3 + pad * 2, font.size * 3 + pad * 2), 0)
    draw = ImageDraw.Draw(img)
    x0, y0 = pad, pad + ascent
    draw.text((x0, pad), ch, font=font, fill=255)
    bbox = img.getbbox()
    if bbox is None:
        adv = font.getlength(ch)
        return bytes(), 0, 0, 0, 0, int(round(adv * 16))
    l, t, r, b = bbox
    crop = img.crop((l, t, r, b))
    pixels = list(crop.getdata())
    packed = pack_bpp4(pixels)
    adv = font.getlength(ch)
    ofs_x = l - x0
    ofs_y = ascent - (b - pad)
    return bytes(packed), crop.size[0], crop.size[1], ofs_x, ofs_y, int(round(adv * 16))


def emit_font(size, ttf_path):
    font = ImageFont.truetype(ttf_path, size)
    ascent, descent = font.getmetrics()
    line_height = ascent + descent
    glyphs = []
    bitmap = bytearray()
    # dummy glyph 0
    glyphs.append((0, 0, 0, 0, 0, 0))
    unicode_offs = []
    for ch in CHARS:
        packed, w, h, ox, oy, adv = render_glyph(font, ch, ascent)
        unicode_offs.append(ord(ch) - RANGE_START)
        glyphs.append((len(bitmap), adv, w, h, ox, oy))
        bitmap.extend(packed)

    name = f"latin1_de_{size}"
    bits = ", ".join(f"0x{b:02x}" for b in bitmap)
    bit_lines = []
    for i in range(0, len(bitmap), 12):
        chunk = bitmap[i : i + 12]
        bit_lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    dsc_lines = []
    for gi, (bi, adv, w, h, ox, oy) in enumerate(glyphs):
        dsc_lines.append(
            f"    {{.bitmap_index = {bi}, .adv_w = {adv}, .box_w = {w}, "
            f".box_h = {h}, .ofs_x = {ox}, .ofs_y = {oy}}},"
        )
    u_list = ", ".join(f"0x{o:x}" for o in unicode_offs)
    return f"""#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LATIN1_DE_{size}
#define LATIN1_DE_{size} 1
#endif

#if LATIN1_DE_{size}

static LV_ATTRIBUTE_LARGE_CONST const uint8_t {name}_bitmap[] = {{
{chr(10).join(bit_lines)}
}};

static const lv_font_fmt_txt_glyph_dsc_t {name}_glyph_dsc[] = {{
{chr(10).join(dsc_lines)}
}};

static const uint16_t {name}_unicode_list[] = {{
    {u_list}
}};

static const lv_font_fmt_txt_cmap_t {name}_cmaps[] = {{
    {{
        .range_start = {RANGE_START}, .range_length = {ord('ü') - RANGE_START + 1},
        .glyph_id_start = 1, .unicode_list = {name}_unicode_list,
        .glyph_id_ofs_list = NULL, .list_length = {len(CHARS)},
        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }}
}};

static const lv_font_fmt_txt_dsc_t {name}_dsc = {{
    .glyph_bitmap = {name}_bitmap,
    .glyph_dsc = {name}_glyph_dsc,
    .cmaps = {name}_cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0
}};

const lv_font_t {name} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {line_height},
    .base_line = {descent},
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -1,
    .underline_thickness = 1,
    .dsc = &{name}_dsc,
    .fallback = NULL,
    .user_data = NULL
}};

#endif
"""


def main():
    parts = [
        "/** Auto-generated German umlaut fallback fonts. */\n",
        '#include "latin1_de.h"\n',
    ]
    decls = []
    for size, path in FONTS.items():
        parts.append(emit_font(size, path))
        decls.append(f"extern const lv_font_t latin1_de_{size};")
    (OUT_DIR / "latin1_de.c").write_text("\n".join(parts), encoding="utf-8")
    (OUT_DIR / "latin1_de.h").write_text(
        "#ifndef LATIN1_DE_H\n#define LATIN1_DE_H\n\n#include \"lvgl.h\"\n\n"
        + "\n".join(decls)
        + "\n\n#endif\n",
        encoding="utf-8",
    )
    print("wrote", OUT_DIR / "latin1_de.c")


if __name__ == "__main__":
    main()
