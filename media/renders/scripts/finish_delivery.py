"""Optimize README PNGs and verify render delivery and hardware preservation."""
from pathlib import Path
import hashlib
import json
import re

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'media/renders'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    expected = {'portrait': (2400, 3000), 'exploded': (3200, 2400)}
    images = {}
    for name, size in expected.items():
        master = OUT / 'masters' / f'aegir-{name}.png'
        with Image.open(master) as im:
            im.load()
            if im.size != size:
                raise RuntimeError(f'{name}: unexpected master size {im.size}')
            target = OUT / f'aegir-{name}.png'
            im.convert('RGB').resize((size[0]//2, size[1]//2), Image.Resampling.LANCZOS).save(target, optimize=True)
            images[name] = {'master': str(master.relative_to(ROOT)), 'master_resolution': size,
                            'master_sha256': sha(master), 'readme': str(target.relative_to(ROOT)),
                            'readme_resolution': [size[0]//2, size[1]//2],
                            'readme_sha256': sha(target), 'readme_bytes': target.stat().st_size}
    baseline = json.loads((OUT / 'verification/source-baseline.json').read_text())
    changed = [p for p, digest in baseline['files'].items()
               if not (ROOT/p).exists() or sha(ROOT/p) != digest]
    if changed:
        raise RuntimeError('Hardware sources changed: '+', '.join(changed))
    text = (ROOT / 'README.md').read_text()
    local_images = re.findall(r'<img[^>]+src="([^"]+)"', text)
    missing = [p for p in local_images if not p.startswith('http') and not (ROOT/p).is_file()]
    if missing:
        raise RuntimeError('Missing README images: '+', '.join(missing))
    if 'src="media/aegir-device-concept-render.png"' in text:
        raise RuntimeError('Old concept image still embedded')
    if 'src="hardware/cad/pcb-v2-bottom/views/usb-bottom-open.png"' in text:
        raise RuntimeError('Old CAD image still embedded')
    assert (ROOT/'media/aegir-device-concept-render.png').exists()
    assert (ROOT/'hardware/cad/pcb-v2-bottom/views/usb-bottom-open.png').exists()
    geometry = json.loads((OUT/'verification/geometry.json').read_text())
    assert geometry['population']['total_component_visuals_with_reconstructions'] == 148
    assert not geometry['population']['dnp_exported']
    payload = {
        'images': images,
        'editable_scenes': {p.name: {'bytes': p.stat().st_size, 'sha256': sha(p)}
                            for p in sorted(OUT.glob('*.blend'))},
        'hardware_sources_checked': len(baseline['files']),
        'hardware_sources_unchanged': True,
        'readme_local_image_links_valid': True,
        'historical_images_retained': True,
        'native_pcb_component_models': 142,
        'supplemental_package_models': 6,
        'fitted_component_visuals': 148,
        'dnp_component_visuals': 0,
        'script_hashes': {p.name: sha(p) for p in sorted((OUT/'scripts').glob('*.py'))},
        'reference_asset_hashes': {str(p.relative_to(OUT)): sha(p) for p in sorted((OUT/'references').rglob('*')) if p.is_file()},
        'render_stage_omissions': ['Provisional RCY connector/mate envelopes; no verified purchased mate'],
        'visual_review': 'Portrait and exploded draft inspected for camera framing, source shape, screen orientation, readable demo indication, PCB visibility and material separation. Final masters inspected separately.',
    }
    if len(payload['editable_scenes']) != 2:
        raise RuntimeError('Expected two packed Blender scenes')
    (OUT/'verification/delivery.json').write_text(json.dumps(payload, indent=2)+'\n')
    print(json.dumps(payload, indent=2))


if __name__ == '__main__':
    main()
