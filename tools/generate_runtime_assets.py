#!/usr/bin/env python3
"""Generate compact glyphs and text metadata for the offline device UI."""
import hashlib
import json
import math
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from PIL import Image,ImageDraw
from app.render import font
ROOT=Path(__file__).resolve().parents[1]
UI='人教新版七上学习草稿全部单元选择继续学习开始今日统计错词回练我的收藏当前暂无到期任务或已达新词额度看答案暂跳返回菜单忘了记住模糊收藏取消已保存关闭学习长按确认返回上键下键新词评分次数到期遗忘今日未学过渡第一二三四五六七单元设置每日个收藏管理移出错词本本词需回练非本轮到期先复习课本文本核对中剩余当前已暂停进度保存失败请重试时间无效需校时勿评分时钟回拨沿用最近学习时间存储损坏停用学习请保留数据待导入词库有变更会话已重建恢复旧版待核实英文词条中文释义第页按上下键选择空下一页上一页' 
UNITS=['全部单元','Starter 1','Starter 2','Starter 3']+[f'Unit {i}' for i in range(1,8)]

def main():
    # Read registered sources only. Never rewrite user dictionaries while building.
    from tools.audit_registered_vocab import audit
    validation=audit()
    if validation['errors']:raise ValueError('词库校验失败：'+json.dumps(validation['errors'][:5],ensure_ascii=False))
    (ROOT/'output').mkdir(exist_ok=True)
    registry=json.loads((ROOT/'data/books.json').read_text())['books']
    words=[];word_index={};book_members=[];grade_ready={}
    for book in registry:
        members=[]
        if book['source']:
            source=json.loads((ROOT/book['source']).read_text())
            if not book['units'] or len(book['units'])>17:raise ValueError('每册支持最多16个分区')
            unit_ids=book.get('unit_ids') or ['starter-1','starter-2','starter-3']+[f'unit-{i}' for i in range(1,8)]
            if len({w['word_id'] for w in source['words']})!=len(source['words']):raise ValueError('同册存在重复词条ID')
            for word in source['words']:
                wid=word['word_id']
                ready=bool(book.get('study_ready',True) and word.get('study_ready',True) and word.get('gloss') and '待补' not in word['gloss'])
                grade_ready[wid]=grade_ready.get(wid,True) and ready
                if wid in word_index:
                    previous=words[word_index[wid]]
                    if any(previous.get(k)!=word.get(k) for k in ('lemma','ipa','pos','gloss')):
                        raise ValueError('同ID词条内容冲突，需拆分义项或人工合并：'+wid)
                else:word_index[wid]=len(words);words.append(word)
                mask=sum(1<<unit_ids.index(u) for u in set(word['units']))
                members.append((word_index[wid],mask,min(word.get('source_pages') or [0])))
        book_members.append(members)
    if not words:raise ValueError('至少安装一本非空词书')
    # Add all printable ASCII so runtime statistics, dates and diagnostics work.
    quotes=json.loads((ROOT/'data/daily-quotes.json').read_text())['quotes']
    assert len(quotes)==50 and len(set(quotes))==50
    chars=set('缺页'+''.join(quotes)+UI+''.join(UNITS)+''.join(chr(i) for i in range(32,127)))
    for book in registry:chars.update(book['title']+''.join(book['units']))
    for word in words:
        for field in ('lemma','ipa','pos','gloss'):chars.update(word.get(field) or '')
    chars.update((ROOT/'firmware/main/runtime_ui.cc').read_text())
    chars.update((ROOT/'firmware/main/app_main.cc').read_text())
    chars.discard('\n');chars.discard('\r')
    chars.discard('\t')
    output=ROOT/'firmware/main/generated';output.mkdir(parents=True,exist_ok=True)
    bitmaps=bytearray();glyphs=[]
    large_chars=set(''.join(w['lemma'] for w in words)+''.join(chr(c) for c in range(32,127))+''.join(b['title']+''.join(b['units']) for b in registry)+'今日统计暂无到期任务词库待导入')
    for size in (16,20,28,36):
        for char in sorted(chars,key=ord):
            if size>=28 and char not in large_chars:
                glyphs.append((ord(char),0,0,0,0));continue
            f=font(size,latin=ord(char)<0x300)
            advance=math.ceil(f.getlength(char));height=size+8;width=advance+4
            image=Image.new('1',(width,height),0)
            ImageDraw.Draw(image).text((2,size),char,font=f,fill=1,anchor='ls')
            # bitmap 1=ink, fixed row stride
            offset=len(bitmaps);bitmaps.extend(image.tobytes())
            glyphs.append((ord(char),offset,width,height,advance))
    # Table grouped by size, then Unicode codepoint; binary search in device renderer.
    glyph_text=',\n'.join('{%d,%d,%d,%d,%d}'%g for g in glyphs)
    def literal(value):return json.dumps(value or '',ensure_ascii=False)
    word_lines=[]
    mapping=['starter-1','starter-2','starter-3']+[f'unit-{i}' for i in range(1,8)]
    for word in words:
        mask=1  # Per-book unit membership is emitted separately below.
        gradable=grade_ready[word['word_id']]
        word_lines.append('{'+','.join(literal(word.get(f)) for f in ('word_id','lemma','ipa','pos','gloss'))+f",{mask},{min(word.get('source_pages') or [0])},{str(gradable).lower()}"+'}')
    signature=int.from_bytes(hashlib.sha256(json.dumps([words,registry,book_members],sort_keys=True,ensure_ascii=False).encode()).digest()[:4],'little')
    header='''// Generated locally from user-supplied textbook. Text remains an unverified draft.
#pragma once
#include <cstddef>
#include <cstdint>
namespace assets {
struct Glyph {uint32_t code,offset; uint16_t width,height,advance;};
struct Word {const char *id,*lemma,*ipa,*pos,*gloss; uint16_t units,page; bool gradable;};
'''
    header+=f'inline constexpr size_t kGlyphsPerSize={len(chars)};\ninline constexpr uint32_t kContentSignature={signature}U;\n'
    header+='inline constexpr Glyph kGlyphs[]={\n'+glyph_text+'\n};\n'
    header+='inline constexpr Word kWords[]={\n'+',\n'.join(word_lines)+'\n};\n'
    header+=f'inline constexpr size_t kCount={len(words)};\n'
    header+='static_assert(kCount<=10000, "Snapshot model supports at most 10000 IDs");\n'
    header+='inline constexpr const char* kUnits[]={'+','.join(literal(u) for u in UNITS)+'};\n}\n'
    header=header[:-2]  # reopen the generated namespace
    header+='inline constexpr const char* kQuotes[]={'+','.join(literal(q) for q in quotes)+'};\n'
    header+='struct Membership {uint32_t index; uint16_t units,page;};\n'
    for i,(book,members) in enumerate(zip(registry,book_members)):
        labels=book['units'] or ['全部单元']
        header+=f'inline constexpr const char* kBookUnits{i}[]={{'+','.join(literal(u) for u in labels)+'};\n'
        header+=f'inline constexpr Membership kMembers{i}[]={{'+(','.join('{%d,%d,%d}'%m for m in members) or '{0,0,0}')+'};\n'
    header+='struct Book {const char *id,*title; const char* const* units; size_t unit_count; const Membership* members; size_t member_count;};\n'
    header+='inline constexpr Book kBooks[]={\n'
    for i,(book,members) in enumerate(zip(registry,book_members)):
        header+='{'+literal(book['deck_id'])+','+literal(book['title'])+f',kBookUnits{i},{len(book["units"]) or 1},kMembers{i},{len(members)}'+'},\n'
    header+='};\ninline constexpr size_t kBookCount=sizeof(kBooks)/sizeof(kBooks[0]);\n}\n'
    (output/'runtime_assets.h').write_text(header)
    (output/'glyphs.bin').write_bytes(bitmaps)
    assert len({w['word_id'] for w in words})==len(words)
    assert max(len(w['word_id'].encode()) for w in words)<64
    report={'source_mutation_policy':'read-only registry build','books':len(registry),'installed_books':sum(bool(m) for m in book_members),'words':len(words),'glyphs_per_size':len(chars),'glyph_bytes':len(bitmaps),'content_signature':signature,
            'id_policy':'Existing distinct word IDs retained; explicit corrections tracked; old ambiguous IDs require migration review.',
            'ambiguous_legacy_ids':['pep_en_every_day','pep_en_dining','pep_en_william','pep_en_florence'],
            'font_policy':'Local system fonts for development only; distribution/font audit pending.'}
    (ROOT/'output/runtime-assets.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False))
if __name__=='__main__':main()
