#!/usr/bin/env python3
"""Scan the publishable set, ignoring generated/local artifacts."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
SKIP={'.git','.local','.venv','output','build','generated','__pycache__','managed_components'}
PATTERNS=[r'/Users/[^/\s]+/',r'[A-Za-z]:\\Users\\',r'(?i)gh[pousr]_[A-Za-z0-9]{20,}',r'(?i)github_pat_[A-Za-z0-9_]{20,}',r'(?i)sk-[A-Za-z0-9]{20,}']
errors=[];count=0
for p in ROOT.rglob('*'):
 if not p.is_file() or any(part in SKIP for part in p.relative_to(ROOT).parts):continue
 count+=1
 if p.resolve()==Path(__file__).resolve():continue
 if p.is_symlink():errors.append((str(p.relative_to(ROOT)),'symlink'))
 if p.suffix.lower() in {'.pdf','.xlsx','.xls','.docx','.bin','.log'}:errors.append((str(p.relative_to(ROOT)),'private/source/binary artifact'))
 try:text=p.read_text()
 except UnicodeDecodeError:continue
 for pattern in PATTERNS:
  if re.search(pattern,text):errors.append((str(p.relative_to(ROOT)),'sensitive pattern'))
print('Files scanned:',count,'findings:',errors)
raise SystemExit(bool(errors))
