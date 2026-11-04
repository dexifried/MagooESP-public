from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
S = 4

def render(text, font_path, size, tracking=0):
    font = ImageFont.truetype(font_path, size * S)
    widths = [round(font.getlength(ch)) for ch in text]
    box = font.getbbox(text)
    width = sum(widths) + tracking * S * (len(text) - 1)
    img = Image.new('L', (width + 8 * S, (size + 12) * S))
    draw = ImageDraw.Draw(img)
    x = 4 * S
    for ch, advance in zip(text, widths):
        draw.text((x, 0), ch, font=font, fill=255)
        x += advance + tracking * S
    crop = img.getbbox()
    img = img.crop(crop)
    return img.resize(((img.width + S - 1) // S, (img.height + S - 1) // S), Image.Resampling.LANCZOS)

def packed(name, image):
    pixels = list(image.getdata())
    values = [(x * 15 + 127) // 255 for x in pixels]
    if len(values) % 2: values.append(0)
    data = [(values[i] << 4) | values[i+1] for i in range(0, len(values), 2)]
    rows = [','.join(str(v) for v in data[i:i+24]) for i in range(0, len(data), 24)]
    return f'constexpr int {name}_WIDTH = {image.width}, {name}_HEIGHT = {image.height};\nconst uint8_t {name}[] PROGMEM = {{\n' + ',\n'.join(rows) + '\n};\n'
logo = render('JEEP', 'C:/Windows/Fonts/segoeuiz.ttf', 54)
caption = render('MAGOO EDITION', 'C:/Windows/Fonts/segoeuib.ttf', 9, 2)
Path('include/StartupArtwork.h').write_text('#pragma once\n#include <Arduino.h>\n' + packed('STARTUP_LOGO', logo) + packed('STARTUP_CAPTION', caption), encoding='utf-8')
print('Logo',logo.size,'caption',caption.size)
