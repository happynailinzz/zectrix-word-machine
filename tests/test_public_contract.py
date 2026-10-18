import importlib.util
import json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('audit',ROOT/'tools/audit_registered_vocab.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
class PublicTests(unittest.TestCase):
    def test_registry_contract(self):
        report=audit.audit()
        self.assertEqual(report['errors'],[])
        self.assertEqual(report['global_unique_ids'],5794)
    def test_seven_datasets_are_included(self):
        books=json.loads((ROOT/'data/books.json').read_text())['books']
        self.assertEqual(len([b for b in books if b['source']]),7)
        self.assertIsNone(next(b for b in books if b['deck_id']=='pep-g9-lower')['source'])
    def test_no_external_project_dependency(self):
        config=(ROOT/'firmware/CMakeLists.txt').read_text()
        self.assertNotIn('note4-demo',config)
        self.assertFalse(any(p.is_symlink() for p in (ROOT/'firmware/components').rglob('*')))
