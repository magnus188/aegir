"""Refine coarse raster contact candidates using a local 0.05 mm grid."""
import numpy as np
from PIL import Image,ImageFilter,ImageDraw
import prepare_original_prints as O
I=O.Q.I

def island_bounds(mask,contact):
    a=np.asarray(mask,dtype=bool);p=np.asarray(contact,dtype=bool)
    parent=[];area=[];supported=[];bounds=[];previous=[]
    def find(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    for y,row in enumerate(a):
        t=np.diff(np.r_[False,row,False].astype(np.int8));starts=np.where(t==1)[0];ends=np.where(t==-1)[0]-1
        current=[]
        for left,right in zip(starts,ends):
            i=len(parent);parent.append(i);area.append(int(right-left+1));supported.append(bool(p[y,left:right+1].any()));bounds.append([int(left),y,int(right),y])
            for pl,pr,pi in previous:
                if pr<left-1:continue
                if pl>right+1:break
                r=find(pi);s=find(i)
                if r!=s:
                    parent[s]=r;area[r]+=area[s];supported[r]|=supported[s]
                    bounds[r]=[min(bounds[r][0],bounds[s][0]),min(bounds[r][1],bounds[s][1]),max(bounds[r][2],bounds[s][2]),max(bounds[r][3],bounds[s][3])]
            current.append((left,right,i))
        previous=current
    return [(area[i],bounds[i]) for i in range(len(parent)) if find(i)==i and not supported[i]]

def contact_at(layers,layer,lo,hi,pitch):
    size=np.maximum(2,np.ceil((hi-lo)/pitch).astype(int)+1)
    old=Image.new('1',tuple(size))
    for l in layers:
        if l['index']>=layer['index']:break
        if l['z']>=layer['z']-.5-1e-5:
            old=Image.fromarray(np.asarray(old)|np.asarray(I.raster_layer(l,lo,hi,pitch)))
    return old

def refine_candidates(code,inspection,folder):
    if not inspection['island_candidate_layers']:return []
    layers,_=I.parse_gcode(code);lo,hi=I.get_bounds(layers);rows=[]
    for candidate in inspection['island_candidate_layers']:
        layer=next(l for l in layers if l['index']==candidate['layer'])
        old=contact_at(layers,layer,lo,hi,.25)
        islands=island_bounds(I.raster_layer(layer,lo,hi,.25),old.filter(ImageFilter.MaxFilter(3)))
        for k,(area,b) in enumerate(islands):
            if area*.25*.25<.25:continue
            left=lo+np.array(b[:2])*.25-2;right=lo+np.array(b[2:])*.25+2
            prior=contact_at(layers,layer,left,right,.05)
            mask=I.raster_layer(layer,left,right,.05)
            fine=I.unsupported_components(mask,prior.filter(ImageFilter.MaxFilter(3)))
            unresolved=[a for a in fine if a*.05*.05>=.01]
            p=np.asarray(prior);m=np.asarray(mask);rgb=np.full((*p.shape,3),255,dtype=np.uint8)
            rgb[p]=[225,155,71];rgb[m]=[33,127,139];rgb[m&p]=[40,85,81]
            im=Image.fromarray(rgb).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            im.resize((im.width*2,im.height*2)).save(folder/f'refined-contact-z{layer["z"]}-{k}.png')
            rows.append({'z_mm':layer['z'],'coarse_area_mm2':area*.25*.25,'local_bounds_mm':[left.tolist(),right.tolist()],'grid_mm':.05,'contact_margin_mm':.05,'unresolved_islands':len(unresolved),'largest_unresolved_area_mm2':max(unresolved,default=0)*.05*.05})
    return rows
