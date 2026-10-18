"""Native 400x300 1bpp cards. The firmware embeds these exact pixel buffers."""
from functools import lru_cache
import os
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT = 400, 300
@lru_cache(maxsize=32)
def font(size, latin=False, bold=False):
    candidates = ([os.environ.get('WORD_MACHINE_LATIN_FONT'),
                   '/System/Library/Fonts/Supplemental/Arial Bold.ttf' if bold else
                   '/System/Library/Fonts/Supplemental/Arial.ttf',
                   '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'] if latin else
                  [os.environ.get('WORD_MACHINE_CJK_FONT'),
                   '/System/Library/Fonts/STHeiti Medium.ttc',
                   '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'])
    for p in candidates:
        if p and Path(p).is_file(): return ImageFont.truetype(p, size)
    raise RuntimeError('缺少字体；请设置 WORD_MACHINE_CJK_FONT / WORD_MACHINE_LATIN_FONT')
