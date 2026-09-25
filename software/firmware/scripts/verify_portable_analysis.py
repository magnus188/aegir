#!/usr/bin/env python3
"""Run clang's static analyzer over portable review production units.

This deliberately does not stand in for ESP-IDF peripheral or physical tests.
Outputs include exact commands and a source/header manifest for reproduction.
"""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/build/static-analysis'
UNITS = [
    'sensors/acquisition_engine', 'sensors/ads122c04', 'sensors/power_monitor',
    'sensors/environment_monitor', 'sensors/ze07_co', 'sensors/co_qualification', 'sensors/usb_source_monitor',
    'sensors/bc12_monitor', 'sensors/usb_input_policy',
    'services/gas_calibration_core', 'services/gas_calibration_journal',
    'services/blob_journal', 'services/oxygen_selection_core', 'services/ota_core',
    'services/storage_service', 'services/maintenance_service',
    'services/charge_profile', 'services/charge_standby_policy',
    'services/sd_log_core', 'services/sd_log_files', 'services/sd_log_format', 'services/sd_log_service',
]

def digest(path):
    return {'path': str(path.relative_to(ROOT)),
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    results = []
    for unit in UNITS:
        source = ROOT / ('main/' + unit + '.cpp')
        report = OUT / (source.stem + '.plist')
        command = ['clang++', '--analyze', '-std=c++17', '-Wall', '-Wextra',
                   '-I', 'main', '-I', 'simulator/stubs', '-I', 'tests/third_party/cjson',
                   str(source.relative_to(ROOT)), '-o', str(report.relative_to(ROOT))]
        run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        results.append({**digest(source), 'command': command, 'exit_code': run.returncode,
                        'diagnostics': run.stdout + run.stderr,
                        'report': digest(report) if report.exists() else None})
    headers = sorted({p for folder in ['main', 'simulator/stubs', 'tests/third_party/cjson']
                      for p in (ROOT / folder).rglob('*.h')})
    result = {
        'tool': subprocess.check_output(['clang++', '--version'], text=True).splitlines()[0],
        'scope': f'{len(UNITS)} portable production units; ESP peripheral adapters excluded',
        'results': results, 'headers': [digest(p) for p in headers],
        'status': 'passed digitally' if all(r['exit_code'] == 0 and not r['diagnostics']
                                          for r in results) else 'correction required',
    }
    (OUT / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    for row in results:
        if row['exit_code'] or row['diagnostics']:
            print(row['path'], row['exit_code'], row['diagnostics'])
    print(f"Static analysis: {result['status']} ({len(results)} units)")
    return int(result['status'] != 'passed digitally')

if __name__ == '__main__':
    raise SystemExit(main())
