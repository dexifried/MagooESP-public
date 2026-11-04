from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
SCALE = 4
FONT = Path('tools/Roboto.ttf')


def make_font(name, size, chars, bold=False):
    font = ImageFont.truetype(str(FONT), size * SCALE)
    try: font.set_variation_by_name('Bold' if bold else 'Regular')
    except (OSError, ValueError): pass
    height = (sum(font.getmetrics()) + SCALE - 1) // SCALE
    blob, entries = [], []
    for ch in chars:
        advance = max(1, round(font.getlength(ch) / SCALE))
        width = max(advance + 1, (font.getbbox(ch)[2] + SCALE - 1) // SCALE + 1)
        canvas = Image.new('L', (width * SCALE, height * SCALE))
        ImageDraw.Draw(canvas).text((0, 0), ch, font=font, fill=255, stroke_width=0)
        canvas = canvas.resize((width, height), Image.Resampling.LANCZOS)
        values = [(v * 15 + 127) // 255 for v in list(canvas.getdata())]
        if len(values) % 2: values.append(0)
        offset = len(blob)
        blob.extend((values[i] << 4) | values[i+1] for i in range(0, len(values), 2))
        entries.append((offset, width, advance))
    rows = [','.join(str(v) for v in blob[i:i+24]) for i in range(0, len(blob), 24)]
    records = ',\n'.join('{%d,%d,%d}' % entry for entry in entries)
    return f'constexpr uint8_t {name}_HEIGHT = {height};\nconst GlyphInfo {name}_GLYPHS[] PROGMEM = {{\n{records}\n}};\nconst uint8_t {name}_DATA[] PROGMEM = {{\n' + ',\n'.join(rows) + '\n};\n'

ascii_chars=''.join(chr(i) for i in range(32, 127))
# Keep digit font indexed by ASCII too, so drawing code is shared.
out = '#pragma once\n#include <Arduino.h>\nstruct GlyphInfo { uint16_t offset; uint8_t width; uint8_t advance; };\n'
for name, size, chars, bold in [('FONT_SMALL',9,ascii_chars,False),('FONT_MEDIUM',11,ascii_chars,True),('FONT_DIGIT',36,ascii_chars,True)]:
    # Large type only needs numeric characters. Other entries are blank.
    if name=='FONT_DIGIT':
        # Restrict symbols in the same ASCII-indexed table to keep flash small.
        font=ImageFont.truetype(str(FONT), size*SCALE)
        try: font.set_variation_by_name('Bold')
        except (OSError, ValueError): pass
        height=(sum(font.getmetrics())+SCALE-1)//SCALE
        blob=[]; entries=[]
        for ch in ascii_chars:
            if ch not in '0123456789.': entries.append((0,0,0));continue
            advance=max(1,round(font.getlength(ch)/SCALE));width=max(advance+1,(font.getbbox(ch)[2]+SCALE-1)//SCALE+1)
            im=Image.new('L',(width*SCALE,height*SCALE));ImageDraw.Draw(im).text((0,0),ch,font=font,fill=255);im=im.resize((width,height),Image.Resampling.LANCZOS)
            vals=[(v*15+127)//255 for v in list(im.getdata())]
            if len(vals)%2:vals.append(0)
            offset=len(blob);blob.extend((vals[i]<<4)|vals[i+1] for i in range(0,len(vals),2));entries.append((offset,width,advance))
        rows=[','.join(str(v) for v in blob[i:i+24]) for i in range(0,len(blob),24)];records=',\n'.join('{%d,%d,%d}'%e for e in entries)
        out+=f'constexpr uint8_t {name}_HEIGHT = {height};\nconst GlyphInfo {name}_GLYPHS[] PROGMEM = {{\n{records}\n}};\nconst uint8_t {name}_DATA[] PROGMEM = {{\n'+',\n'.join(rows)+'\n};\n'
    else: out += make_font(name,size,chars,bold)
Path('include/RobotoFont.h').write_text(out,encoding='utf-8')
print('Generated',len(out),'header characters')
