from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "assets" / "fonts" / "MiSans-Regular.ttf"
OUT = ROOT / "assets" / "icons"
SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)

ICON_REF_SIZE = 36
PROFILE_GLYPH_WIDTH_UNITS = 32
PROFILE_GLYPH_HEIGHT_UNITS = 33
PROFILE_GLYPH_Y_OFFSET_UNITS = 0.5


def font_for_size(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT), round(size * 1.25))


def render_glyph(text: str, size: int, color: tuple[int, int, int]) -> Image.Image:
    scale = 4
    canvas_side = size * scale
    source_side = canvas_side * 2
    source = Image.new("L", (source_side, source_side), 0)
    draw = ImageDraw.Draw(source)
    font = font_for_size(canvas_side)
    bbox = draw.textbbox((0, 0), text, font=font)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    x = (source_side - text_w) / 2 - bbox[0]
    y = (source_side - text_h) / 2 - bbox[1]
    draw.text((x, y), text, font=font, fill=255)

    source_bbox = source.getbbox()
    if source_bbox is None:
        return Image.new("RGBA", (size, size), (0, 0, 0, 0))
    glyph = source.crop(source_bbox)

    target_w = round(canvas_side * PROFILE_GLYPH_WIDTH_UNITS / ICON_REF_SIZE)
    target_h = round(canvas_side * PROFILE_GLYPH_HEIGHT_UNITS / ICON_REF_SIZE)
    target = glyph.resize((target_w, target_h), Image.Resampling.LANCZOS)

    alpha = Image.new("L", (canvas_side, canvas_side), 0)
    left = round((canvas_side - target_w) / 2)
    top = round(
        (canvas_side - target_h) / 2
        + canvas_side * PROFILE_GLYPH_Y_OFFSET_UNITS / ICON_REF_SIZE
    )
    alpha.paste(target, (left, top))

    canvas = Image.new("RGBA", (canvas_side, canvas_side), (*color, 0))
    canvas.putalpha(alpha)
    return canvas.resize((size, size), Image.Resampling.LANCZOS)


def save_icon(path: Path, color: tuple[int, int, int]) -> None:
    images = [render_glyph("\u7545", size, color) for size in SIZES]
    base = images[-1]
    base.info["append_images"] = images[:-1]
    base.save(path, sizes=[(size, size) for size in SIZES])


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    save_icon(OUT / "fluent-pinyin-dark.ico", (245, 245, 245))
    save_icon(OUT / "fluent-pinyin-light.ico", (32, 32, 32))
    save_icon(OUT / "fluent-pinyin.ico", (71, 74, 178))
    render_glyph("\u7545", 256, (245, 245, 245)).save(
        OUT / "fluent-pinyin-dark.preview.png"
    )


if __name__ == "__main__":
    main()
