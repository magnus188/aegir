"""Render physical STEP geometry, not a concept image. No design edits."""
from pathlib import Path
import argparse,hashlib,json
from step_inputs import load_named_step
from render_step_zbuffer import render_parts

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('source');ap.add_argument('output_directory')
    args=ap.parse_args()
    source=Path(args.source).resolve();out=Path(args.output_directory);out.mkdir(parents=True,exist_ok=True)
    digest=hashlib.sha256(source.read_bytes()).hexdigest();parts=load_named_step(source)
    if hashlib.sha256(source.read_bytes()).hexdigest()!=digest:raise RuntimeError('STEP changed during import')
    views=[
      ('pcb2-case-open.png',dict(view='iso',hide='TMX-A3-P02',title='PCB 2.0 – faktisk CAD-geometri, baklokket skjult')),
      ('pcb2-case-closed.png',dict(view='iso',title='PCB 2.0 – faktisk CAD-geometri, samlet kasse')),
      ('pcb2-usb-port.png',dict(view='port',crop=[80,100,18,42,16,35],title='USB-C – faktisk CAD-geometri, nærbilde',width=1500,height=1100)),
    ]
    reports=[]
    for filename,settings in views:
        report=render_parts(parts,out/filename,**settings)
        report.update(source=str(source),sha256=digest,settings=settings,method='Actual STEP BREP tessellation, orthographic per-pixel depth buffer; illustrative colors')
        (out/filename).with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report),flush=True);reports.append(report)
    (out/'render-provenance.json').write_text(json.dumps(reports,indent=2)+'\n')
