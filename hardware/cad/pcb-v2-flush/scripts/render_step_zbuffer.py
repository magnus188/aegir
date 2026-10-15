"""Depth-correct orthographic rendering of actual STEP tessellation.

python render_step_zbuffer.py assembly.step out.png --view iso
python render_step_zbuffer.py assembly.step port.png --view port --crop 81 99 18 42 17 34
Pure mesh rendering: no generated or manually retouched imagery.
"""
from pathlib import Path
import argparse,re,math,json,hashlib
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from numba import njit
from step_inputs import load_named_step

_source_inventory=Path(__file__).with_name('sourceparts.json')
if not _source_inventory.exists():
    _source_inventory=Path(__file__).parent.parent/'verification/case-source-inventory.json'
_source_names={p['part_id']:' '.join(p.get(k,'') for k in ('product_name','description')) for p in json.loads(_source_inventory.read_text())['parts']} if _source_inventory.exists() else {}


def color_for(part):
    text=' '.join(str(part.get(k,'')) for k in ['name','definition_name','part_id']).lower()
    if part.get('reference')=='J901' or re.search(r' / J901 \[',part.get('path','')) or 'pcb2-j901' in text:return (182,190,200)
    for part_id,description in _source_names.items():
        if re.search(re.escape(part_id.lower())+r'(?:$|[^a-z0-9-])',text):text+=' '+description.lower();break
    if 'tmx-v2-p12' in text:text+=' bezel'
    if re.search(r'pcb2-[cruldq]\d+',text):return (70,76,83)
    if re.search(r'pcb2-j\d+',text):return (170,175,171)
    for token,color in [('pcb',(39,115,80)),('usb4105',(182,190,200)),('bezel',(57,100,127)),('housing',(120,150,173)),('rear cover',(181,198,210)),('carrier',(65,160,155)),('battery',(93,105,116)),('18650',(88,145,174)),('screw',(88,95,104)),('insert',(190,157,80)),('guition',(53,58,65)),('display',(53,58,65)),('manifold',(187,197,186)),('seal',(45,49,55)),('gasket',(45,49,55))]:
        if token in text:return color
    return (190,172,128)

@njit(cache=True)
def rasterize(triangles,screen,colors,width,height,clip_min,clip_max):
    output=np.empty((height,width,3),np.uint8);output[:,:,:]=246
    depth=np.full((height,width),-1e100)
    for i in range(len(screen)):
        a=screen[i,0];b=screen[i,1];c=screen[i,2]
        denom=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(denom)<1e-10:continue
        x0=max(0,int(math.floor(min(a[0],b[0],c[0]))));x1=min(width-1,int(math.ceil(max(a[0],b[0],c[0]))))
        y0=max(0,int(math.floor(min(a[1],b[1],c[1]))));y1=min(height-1,int(math.ceil(max(a[1],b[1],c[1]))))
        for y in range(y0,y1+1):
            py=y+.5
            for x in range(x0,x1+1):
                px=x+.5
                u=((b[1]-c[1])*(px-c[0])+(c[0]-b[0])*(py-c[1]))/denom
                v=((c[1]-a[1])*(px-c[0])+(a[0]-c[0])*(py-c[1]))/denom
                w=1-u-v
                if u<-.000001 or v<-.000001 or w<-.000001:continue
                z=u*a[2]+v*b[2]+w*c[2]
                if z<=depth[y,x]:continue
                inside=True
                for axis in range(3):
                    p=u*triangles[i,0,axis]+v*triangles[i,1,axis]+w*triangles[i,2,axis]
                    if p<clip_min[axis]-1e-7 or p>clip_max[axis]+1e-7:inside=False;break
                if not inside:continue
                depth[y,x]=z
                for k in range(3):output[y,x,k]=colors[i,k]
    return output


def render_parts(parts,output,view='iso',crop=None,title=None,hide=None,tolerance=.10,width=1400,height=1500,section=False):
    parts=[p for p in parts if not(hide and re.search(hide,' '.join(str(p.get(k,'')) for k in ['path','name','definition_name','part_id']),re.I))]
    if crop:
        lo=np.array(crop[::2],dtype=float);hi=np.array(crop[1::2],dtype=float)
        parts=[p for p in parts if all(p['bounds']['max'][i]>=lo[i] and p['bounds']['min'][i]<=hi[i] for i in range(3))]
    else:
        lo=np.min([p['bounds']['min'] for p in parts],axis=0);hi=np.max([p['bounds']['max'] for p in parts],axis=0)
    if not parts:raise ValueError('No selected geometry')
    elev,azim={'iso':(32,-55),'rear':(87,-90),'port':(12,-12),'front':(-70,-90)}[view]
    el,az=math.radians(elev),math.radians(azim)
    toward=np.array([math.cos(el)*math.cos(az),math.cos(el)*math.sin(az),math.sin(el)])
    right=np.array([-math.sin(az),math.cos(az),0]);up=np.cross(toward,right)
    basis=np.array([right,up,toward]);center=(hi+lo)/2
    corners=np.array([[x,y,z] for x in [lo[0],hi[0]] for y in [lo[1],hi[1]] for z in [lo[2],hi[2]]])
    projected=(corners-center)@basis.T
    extent=np.ptp(projected[:,:2],axis=0);scale=min((width-100)/extent[0],(height-160)/extent[1])
    projected_center=(projected[:,:2].min(0)+projected[:,:2].max(0))/2
    alltri=[];allcolors=[]
    light=np.array([.35,-.5,.79]);light/=np.linalg.norm(light)
    for part in parts:
        verts,faces=part['shape'].tessellate(tolerance,.10)
        if not len(faces):continue
        triangles=np.array([v.toTuple() for v in verts])[np.array(faces,dtype=int)]
        normals=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]);normals/=np.maximum(np.linalg.norm(normals,axis=1)[:,None],1e-15)
        illumination=.62+.38*np.maximum(normals@light,0)
        colors=np.asarray(color_for(part),dtype=float)[None,:]*illumination[:,None]
        alltri.append(triangles);allcolors.append(colors.astype(np.uint8))
    triangles=np.concatenate(alltri);colors=np.concatenate(allcolors)
    screen=(triangles-center)@basis.T
    screen[:,:,0]=(screen[:,:,0]-projected_center[0])*scale+width/2
    screen[:,:,1]=-(screen[:,:,1]-projected_center[1])*scale+height/2+15
    clip_min=lo if section else np.full(3,-1e100)
    clip_max=hi if section else np.full(3,1e100)
    pixels=rasterize(triangles,screen,colors,width,height,clip_min,clip_max)
    image=Image.fromarray(pixels)
    if title:
        draw=ImageDraw.Draw(image)
        draw.rectangle((0,0,width,70),fill=(246,246,246))
        try:font=ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',26)
        except OSError:font=ImageFont.load_default()
        draw.text((width/2,28),title,font=font,fill=(35,44,51),anchor='mt')
    Path(output).parent.mkdir(parents=True,exist_ok=True);image.save(output)
    return {'output':str(output),'leaf_instances':len(parts),'triangles':len(triangles),'view':view,'bounds':[lo.tolist(),hi.tolist()]}

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('source');ap.add_argument('output');ap.add_argument('--view',choices=['iso','rear','port','front'],default='iso');ap.add_argument('--crop',type=float,nargs=6);ap.add_argument('--section',action='store_true',help='Clip actual geometry at crop bounds; without this crop sets viewport only.');ap.add_argument('--title');ap.add_argument('--hide');ap.add_argument('--width',type=int,default=1400);ap.add_argument('--height',type=int,default=1500);args=ap.parse_args()
    source=Path(args.source).resolve()
    source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
    parts=load_named_step(source)
    if hashlib.sha256(source.read_bytes()).hexdigest()!=source_hash:
        raise RuntimeError('Source STEP changed while importing; repeat once export is finished.')
    report=render_parts(parts,args.output,args.view,args.crop,args.title,args.hide,width=args.width,height=args.height,section=args.section)
    report.update(source=str(source),sha256=source_hash,hidden=args.hide,section=args.section,method='Actual STEP BREP tessellation; orthographic per-pixel depth buffer; illustrative colors')
    Path(args.output).with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report),flush=True)
