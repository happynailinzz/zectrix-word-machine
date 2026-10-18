#!/usr/bin/env python3
"""Validate the actual registry consumed by firmware; report quality separately."""
import hashlib,json,re
from collections import Counter
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def audit(root=ROOT):
    books=json.loads((root/'data/books.json').read_text())['books']
    report={'errors':[],'books':[],'global_unique_ids':0,'warnings':[]};ids={}
    def error(book,word,detail):report['errors'].append({'deck':book,'id':word,'detail':detail})
    for b in books:
        if not b['source']:continue
        path=root/b['source'];d=json.loads(path.read_text());words=d['words']
        unit_ids=b.get('unit_ids',[])
        if len(b['units'])!=len(unit_ids)+1 or len(set(unit_ids))!=len(unit_ids) or len(unit_ids)>16:error(b['deck_id'],None,'invalid unit definitions')
        if len(words)!=d['word_count']:error(b['deck_id'],None,'word_count mismatch')
        seen=set();counts=Counter();missing=Counter();flags=Counter()
        for w in words:
            wid=w.get('word_id');lemma=w.get('lemma');gloss=w.get('gloss')
            if not isinstance(wid,str) or not 0<len(wid.encode())<=63:error(b['deck_id'],wid,'ID must be 1..63 UTF8 bytes')
            if wid in seen:error(b['deck_id'],wid,'duplicate within book')
            seen.add(wid)
            for field in ('lemma','gloss'):
                if not isinstance(w.get(field),str) or not w[field].strip():error(b['deck_id'],wid,'blank '+field)
                elif any(ord(c)<32 for c in w[field]):error(b['deck_id'],wid,'control character '+field)
            if isinstance(lemma,str) and re.search('[\u4e00-\u9fff]',lemma):error(b['deck_id'],wid,'Chinese text in English headword')
            identity=tuple(w.get(k) for k in ('lemma','ipa','pos','gloss'))
            if wid in ids and ids[wid]!=identity:error(b['deck_id'],wid,'conflicting shared content')
            ids[wid]=identity
            if not w.get('units'):error(b['deck_id'],wid,'missing unit membership')
            for u in w.get('units',[]):
                if u not in unit_ids:error(b['deck_id'],wid,'unknown unit '+u)
                counts[u]+=1
            pages=w.get('source_pages')
            if not isinstance(pages,list) or any(type(p)!=int or not 1<=p<=65535 for p in pages):error(b['deck_id'],wid,'invalid source_pages')
            if not pages:missing['source_page']+=1
            for field in ('ipa','pos'):
                if not w.get(field):missing[field]+=1
            if isinstance(gloss,str) and '待补' in gloss:missing['translation_pending']+=1
            flags.update(w.get('review_flags',[]))
        source=d.get('source_manifest')
        if source and not (root/source).is_file():error(b['deck_id'],None,'source manifest missing')
        report['books'].append({'deck':b['deck_id'],'file':b['source'],'records':len(words),'units':dict(counts),'missing':dict(missing),'review_flags':dict(flags),
            'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'quality_status':'user_supplied_draft_not_linguistically_certified'})
    report['global_unique_ids']=len(ids)
    lost=root/'research/pep-xlsx-2026/lost-entries-report.json'
    if lost.exists():report['reported_missing_source_entries']=len(json.loads(lost.read_text())['items'])
    report['engineering_status']='pass' if not report['errors'] else 'fail'
    report['quality_note']='Engineering compatibility is not a vocabulary-completeness or translation/IPA correctness certificate.'
    return report

def main():
    report=audit();(ROOT/'output').mkdir(exist_ok=True);out=ROOT/'output/vocabulary-audit.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'status':report['engineering_status'],'errors':report['errors'],'global_ids':report['global_unique_ids'],
      'books':[(b['deck'],b['records'],b['missing']) for b in report['books']]},ensure_ascii=False))
    raise SystemExit(bool(report['errors']))
if __name__=='__main__':main()
