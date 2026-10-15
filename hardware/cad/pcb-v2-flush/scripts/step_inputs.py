"""Read named STEP assemblies into world-positioned CadQuery shapes.

No source STEP files are written. API:
    parts = load_named_step(path)
    housing = next(p['shape'] for p in parts if p['part_id']=='TMX-A3-P01')
Every p['shape'] already carries its complete parent/instance placement.
Use p['path'] as the stable unique instance key; product IDs can repeat.
"""
from pathlib import Path
import re, json, hashlib
import cadquery as cq
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TDocStd import TDocStd_Document
from OCP.TCollection import TCollection_ExtendedString, TCollection_AsciiString
from OCP.TDF import TDF_Label, TDF_LabelSequence, TDF_Tool
from OCP.TDataStd import TDataStd_Name
from OCP.XCAFDoc import XCAFDoc_DocumentTool
from OCP.TopLoc import TopLoc_Location
from OCP.IFSelect import IFSelect_RetDone
from OCP.Interface import Interface_Static
from OCP.Bnd import Bnd_Box
from OCP.BRepBndLib import BRepBndLib


def label_name(label):
    a=TDataStd_Name()
    return a.Get().ToExtString() if label.FindAttribute(TDataStd_Name.GetID_s(), a) else ''


def label_entry(label):
    s=TCollection_AsciiString();TDF_Tool.Entry_s(label,s);return s.ToCString()


def decode_step_string(text):
    text=text.replace("''", "'")
    text=re.sub(r'\\X2\\([0-9A-Fa-f]+)\\X0\\',lambda m:bytes.fromhex(m[1]).decode('utf-16-be'),text)
    text=re.sub(r'\\X4\\([0-9A-Fa-f]+)\\X0\\',lambda m:bytes.fromhex(m[1]).decode('utf-32-be'),text)
    return text.replace('\n','').replace('\r','')


def product_index(path):
    raw=Path(path).read_text(errors='replace')
    out={}
    pat=r"#(\d+)\s*=\s*PRODUCT\s*\(\s*'((?:[^']|'')*)'\s*,\s*'((?:[^']|'')*)'\s*,\s*(?:'((?:[^']|'')*)'|\$)"
    for match in re.finditer(pat,raw,re.S):
        entity,pid,name,desc=match.groups()
        record={'part_id':decode_step_string(pid),'product_name':decode_step_string(name),'description':decode_step_string(desc or ''),'step_product_entity':'#'+entity}
        out.setdefault(record['product_name'],[]).append(record)
        if record['part_id']!=record['product_name']:out.setdefault(record['part_id'],[]).append(record)
    return out


def bounds(shape):
    """Exact BREP extrema (not a triangulation), in source STEP length units/mm."""
    b=Bnd_Box();BRepBndLib.AddOptimal_s(shape.wrapped,b,False,False)
    a=list(b.Get())
    return {'min':a[:3],'max':a[3:],'size':[a[i+3]-a[i] for i in range(3)],'center':[(a[i+3]+a[i])/2 for i in range(3)]}


def load_named_step(path):
    path=Path(path).resolve();index=product_index(path)
    reader=STEPCAFControl_Reader();reader.SetNameMode(True);reader.SetColorMode(True);reader.SetLayerMode(True)
    Interface_Static.SetIVal_s('read.stepcaf.subshapes.name',1)
    Interface_Static.SetCVal_s('xstep.cascade.unit','MM')
    if reader.ReadFile(str(path))!=IFSelect_RetDone:raise RuntimeError('STEP read failed: '+str(path))
    doc=TDocStd_Document(TCollection_ExtendedString('XmXCAF'))
    if not reader.Transfer(doc):raise RuntimeError('STEP transfer failed: '+str(path))
    tool=XCAFDoc_DocumentTool.ShapeTool_s(doc.Main());roots=TDF_LabelSequence();tool.GetFreeShapes(roots)
    parts=[]
    def walk(label,parent_location,path_items):
        inst_name=label_name(label);definition=label;reference=tool.IsReference_s(label)
        world=parent_location
        if reference:
            definition=TDF_Label();tool.GetReferredShape_s(label,definition)
            world=parent_location.Multiplied(tool.GetLocation_s(label))
        def_name=label_name(definition)
        entry=label_entry(label)
        instance_name=inst_name or def_name or entry
        branch=path_items+[instance_name+' ['+entry+']']
        if tool.IsAssembly_s(definition):
            if not reference:world=parent_location.Multiplied(tool.GetLocation_s(definition))
            children=TDF_LabelSequence();tool.GetComponents_s(definition,children,False)
            for i in range(1,children.Length()+1):walk(children.Value(i),world,branch)
            return
        raw_shape=tool.GetShape_s(definition)
        if raw_shape.IsNull():return
        shape=cq.Shape.cast(raw_shape.Moved(world))
        products=index.get(def_name,index.get(inst_name,[]))
        prod=products[0] if products else {}
        tr=world.Transformation()
        rec={'path':' / '.join(branch),'name':instance_name,'definition_name':def_name,
             'part_id':prod.get('part_id',''),'product_name':prod.get('product_name',def_name),
             'description':prod.get('description',''),'step_product_entity':prod.get('step_product_entity',''),
             'label_entry':entry,'definition_label_entry':label_entry(definition),
             'world_transform_3x4':[[tr.Value(i,j) for j in range(1,5)] for i in range(1,4)],
             'bounds':bounds(shape),'volume_mm3':shape.Volume(),'solid_count':len(shape.Solids()),
             'shape':shape}
        parts.append(rec)
    for i in range(1,roots.Length()+1):walk(roots.Value(i),TopLoc_Location(),[])
    return parts


def serializable_parts(parts):
    return [{k:v for k,v in p.items() if k!='shape'} for p in parts]


def export_inventory(path,output):
    path=Path(path).resolve();parts=load_named_step(path)
    result={'source':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'part_count':len(parts),'solid_count':sum(p['solid_count'] for p in parts),
            'parts':serializable_parts(parts)}
    Path(output).write_text(json.dumps(result,indent=2)+'\n');return parts

if __name__=='__main__':
    import argparse
    ap=argparse.ArgumentParser();ap.add_argument('source');ap.add_argument('output');args=ap.parse_args()
    parts=export_inventory(args.source,args.output)
    print(f'{len(parts)} named leaf instances; {sum(p["solid_count"] for p in parts)} solids; wrote {args.output}',flush=True)
