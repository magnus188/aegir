#!/usr/bin/env python3
"""Fault cases for the cross-domain contract checker and raw-log extraction."""
import csv
import importlib.util
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def module(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'scripts'/f'{name}.py')
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
contract=module('verify_system_contract')
raw=module('extract_characterization')

class ReviewTools(unittest.TestCase):
    def test_contract_and_pin_mutations(self):
        self.assertEqual(contract.audit()['failed'],0)
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'contract.h'; original=(ROOT/'main/hardware_contract.h').read_text()
            for before,after in [('kSda{28,21}','kSda{29,21}'),('kPowerKill{33,8}','kPowerKill{33,7}'),
                                 ('kOxygenAddress=0x40','kOxygenAddress=0x41'),('kUnusedOxygenSelector{52,7}','kUnusedOxygenSelector{52,15}'),
                                 ('kUsbPermission{50,11}','kUsbPermission{52,7}'),('kUsbCcAddress=0x47','kUsbCcAddress=0x67'),
                                 ('kUsbBc12Address=0x5f','kUsbBc12Address=0x5e'),('constexpr HostPin kSda{28,21};','')]:
                with self.subTest(mutation=before):
                    path.write_text(original.replace(before,after));self.assertGreater(contract.audit(contract=path)['failed'],0)
    def test_disconnected_latch_feedback_rejected(self):
        original=ROOT.parents[1]/'hardware/system-review/electrical/analyzer-netlist.xml'
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'netlist.xml'
            for ref,pin in [('U112','3'),('Q112','1'),('Q112','2'),('Q112','3'),('R107','1')]:
                with self.subTest(ref=ref,pin=pin):
                    tree=ET.parse(original)
                    matches=[(n,p) for n in tree.findall('./nets/net') for p in n.findall('node')
                             if p.get('ref')==ref and p.get('pin')==pin]
                    self.assertEqual(len(matches),1)
                    matches[0][0].remove(matches[0][1]);tree.write(path)
                    self.assertGreater(contract.audit(netlist=path)['failed'],0)
    def test_logs_preserve_faults_identity_and_individual_time(self):
        values=['4294967290','1','2','45','6','-345','-0.0001','8','nan','1.65','22.1','50','1.01','1','4','8','0','1']
        later=values.copy();later[0]='12';later[1]='2';later[6]='nan';later[17]='2'
        with tempfile.TemporaryDirectory() as temp:
            src=Path(temp)/'serial.log';dest=Path(temp)/'capture.csv'
            src.write_text('startup\nI (51) GAS_RAW: CSV1,'+','.join(values)+'\nI (80) GAS_RAW: CSV1,'+','.join(later)+'\n')
            self.assertEqual(raw.extract(src,dest),2)
            with dest.open() as f: rows=list(csv.reader(f))
            self.assertEqual(rows,[raw.FIELDS,values,later])
    def test_incomplete_or_nonnumeric_logs_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            src=Path(temp)/'bad.log';dest=Path(temp)/'capture.csv'
            src.write_text('CSV1,1,2,3\n')
            with self.assertRaises(ValueError):raw.extract(src,dest)
            src.write_text('CSV1,'+','.join(['wrong']*18)+'\n')
            with self.assertRaises(ValueError):raw.extract(src,dest)

if __name__=='__main__':unittest.main(verbosity=2)
