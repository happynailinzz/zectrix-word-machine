#!/usr/bin/env python3
"""Dashboard-only bitmap subset; leaves frozen dictionary/font assets untouched."""
import json,math,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from PIL import Image,ImageDraw,ImageFont
from app.render import font
ROOT=Path(__file__).resolve().parents[1]
TEXT='（）月人教版·七八九年级上下册缺页索引全书今日新词到期待补中文英文已学日期校准周一二三四五六日继续菜单选词书'
SIZES=[14,16,20,88]

def main():
    quotes=json.loads((ROOT/'data/daily-quotes.json').read_text())['quotes']
    # Use frozen compiled book names, not the currently edited source dictionaries.
    import re
    header=(ROOT/'firmware/main/generated/runtime_assets.h').read_text()
    titles=re.findall(r'\{"[^"\n]+","([^"\n]+)",kBookUnits',header)
    prompts='存储损坏，停用学习请保留数据时间无效，需校时勿评分保存失败，请重试恢复旧版：今日评分待核实词库有变更，会话已重建恢复旧版进度，请核对释义过长，暂不评分请返回'
    chars=set(prompts+TEXT+''.join(quotes)+''.join(titles)+''.join(chr(c) for c in range(32,127)))
    glyphs=[];bitmap=bytearray()
    for size in SIZES:
        alphabet='0123456789:-' if size==88 else ''.join(sorted(chars,key=ord))
        for char in alphabet:
            f=font(size,latin=ord(char)<0x300,bold=size==88)
            width=math.ceil(f.getlength(char))+4;height=size+8
            image=Image.new('1',(width,height),0);ImageDraw.Draw(image).text((2,size),char,font=f,fill=1,anchor='ls')
            glyphs.append((size,ord(char),len(bitmap),width,height,math.ceil(f.getlength(char))))
            bitmap.extend(image.tobytes())
    glyphs.sort(key=lambda g:(g[0],g[1]))
    out=ROOT/'firmware/main/generated'
    data='''// Local dashboard font subset; existing textbook assets are not regenerated.
#pragma once
#include <cstdint>
#include <cstddef>
namespace dashboard {
struct Glyph {uint16_t size;uint32_t code,offset;uint16_t width,height,advance;};
inline constexpr Glyph kGlyphs[]={
'''+',\n'.join('{%d,%d,%d,%d,%d,%d}'%g for g in glyphs)+'''\n};
inline constexpr size_t kCount=sizeof(kGlyphs)/sizeof(kGlyphs[0]);
}
'''
    (out/'dashboard_fonts.h').write_text(data)
    (out/'dashboard_fonts.bin').write_bytes(bitmap)
    print('Dashboard font bytes:',len(bitmap),'glyphs:',len(glyphs))
if __name__=='__main__':main()
