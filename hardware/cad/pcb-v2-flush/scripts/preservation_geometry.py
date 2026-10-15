"""Independent BRep geometry signatures for read-only STEP roundtrip review.

Analytic surfaces/curves and B-spline control geometry are compared in world
coordinates, plus trimmed edge samples and vertices. No solids are healed,
transformed or modified. Degenerate topology differences are reported separately.
"""
import math,json,hashlib
from collections import Counter
from OCP.BRepAdaptor import BRepAdaptor_Surface,BRepAdaptor_Curve
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps
from step_inputs import bounds
ROUND=6

def quant(values):return tuple(round(float(v),ROUND) for v in values)
def pt(p):return quant((p.X(),p.Y(),p.Z()))
def direction(v):
    values=[v.X(),v.Y(),v.Z()]
    for q in values:
        if abs(q)>1e-10:
            if q<0:values=[-x for x in values]
            break
    return quant(values)
def axis(a):
    d=direction(a.Direction());p=pt(a.Location());dot=sum(x*y for x,y in zip(p,d))
    return d,quant(x-dot*y for x,y in zip(p,d))
def bspline_curve(s):
    pol=tuple((pt(s.Pole(i)),round(s.Weight(i),ROUND)) for i in range(1,s.NbPoles()+1))
    knots=tuple((round(s.Knot(i),ROUND),s.Multiplicity(i))for i in range(1,s.NbKnots()+1))
    return s.Degree(),s.IsPeriodic(),pol,knots

def surface(face):
    a=BRepAdaptor_Surface(face.wrapped,True);t=face.geomType()
    if t=='PLANE':
        q=a.Plane();d=direction(q.Axis().Direction());p=pt(q.Location());v=(d,round(sum(x*y for x,y in zip(p,d)),ROUND))
    elif t=='CYLINDER':q=a.Cylinder();v=(axis(q.Axis()),round(q.Radius(),ROUND))
    elif t=='SPHERE':q=a.Sphere();v=(pt(q.Location()),round(q.Radius(),ROUND))
    elif t=='CONE':q=a.Cone();v=(pt(q.Apex()),direction(q.Axis().Direction()),round(abs(q.SemiAngle()),ROUND))
    elif t=='TORUS':q=a.Torus();v=(pt(q.Location()),direction(q.Axis().Direction()),round(q.MajorRadius(),ROUND),round(q.MinorRadius(),ROUND))
    elif t=='BSPLINE':
        q=a.BSpline();v=(q.UDegree(),q.VDegree(),q.IsUPeriodic(),q.IsVPeriodic(),
            tuple(tuple((pt(q.Pole(i,j)),round(q.Weight(i,j),ROUND))for j in range(1,q.NbVPoles()+1))for i in range(1,q.NbUPoles()+1)),
            tuple((round(q.UKnot(i),ROUND),q.UMultiplicity(i))for i in range(1,q.NbUKnots()+1)),
            tuple((round(q.VKnot(i),ROUND),q.VMultiplicity(i))for i in range(1,q.NbVKnots()+1)))
    else:raise RuntimeError('Unsupported preservation surface '+t)
    return t,v

def edge(e):
    a=BRepAdaptor_Curve(e.wrapped);t=e.geomType()
    if t=='LINE':v=axis(a.Line().Position())
    elif t=='CIRCLE':q=a.Circle();v=(pt(q.Location()),direction(q.Axis().Direction()),round(q.Radius(),ROUND))
    elif t=='ELLIPSE':q=a.Ellipse();v=(pt(q.Location()),direction(q.Axis().Direction()),direction(q.XAxis().Direction()),round(q.MajorRadius(),ROUND),round(q.MinorRadius(),ROUND))
    elif t=='BSPLINE':v=bspline_curve(a.BSpline())
    else:raise RuntimeError('Unsupported preservation curve '+t)
    # Order-independent samples preserve the actual trim, not only infinite support.
    samples=tuple(sorted(quant(e.positionAt(u).toTuple())for u in (0,.125,.25,.375,.5,.625,.75,.875,1)))
    return t,v,round(e.Length(),ROUND),samples

def measures(s):
    vp=GProp_GProps();ap=GProp_GProps()
    ve=BRepGProp.VolumeProperties_s(s.wrapped,vp,1e-10,False,False)
    ae=BRepGProp.SurfaceProperties_s(s.wrapped,ap,1e-10,False)
    return {'volume_mm3':vp.Mass(),'area_mm2':ap.Mass(),'requested_relative_integration_epsilon':1e-10,
            'reported_volume_error':ve,'reported_area_error':ae}

def compare(a,b):
    sa=[surface(f)for f in a.Faces()];sb=[surface(f)for f in b.Faces()]
    ea=[edge(e)for e in a.Edges()];eb=[edge(e)for e in b.Edges()]
    va=sorted(set(quant(v.toTuple())for v in a.Vertices()));vb=sorted(set(quant(v.toTuple())for v in b.Vertices()))
    ba=bounds(a);bb=bounds(b)
    same_surfaces=set(sa)==set(sb);same_edges=Counter(ea)==Counter(eb)
    ma,mb=measures(a),measures(b)
    row={'source_valid':a.isValid(),'candidate_valid':b.isValid(),'coordinate_rounding_mm':10**-ROUND,
         'world_bounds_max_delta_mm':max(abs(x-y)for key in ('min','max')for x,y in zip(ba[key],bb[key])),
         'unique_analytic_and_bspline_surface_geometry_matches':same_surfaces,
         'trimmed_edge_geometry_multiset_matches':same_edges,'unique_vertex_coordinates_match':va==vb,
         'source_topology':{'solids':len(a.Solids()),'faces':len(a.Faces()),'edges':len(a.Edges()),'vertices':len(a.Vertices())},
         'candidate_topology':{'solids':len(b.Solids()),'faces':len(b.Faces()),'edges':len(b.Edges()),'vertices':len(b.Vertices())},
         'source_measures':ma,'candidate_measures':mb,
         'volume_difference_mm3':abs(ma['volume_mm3']-mb['volume_mm3']),
         'area_difference_mm2':abs(ma['area_mm2']-mb['area_mm2']),
         'unmatched_source_surfaces':len(set(sa)-set(sb)),'unmatched_candidate_surfaces':len(set(sb)-set(sa)),
         'unmatched_source_edges':sum((Counter(ea)-Counter(eb)).values()),'unmatched_candidate_edges':sum((Counter(eb)-Counter(ea)).values())}
    row['strict_signature_match']=all([a.isValid(),b.isValid(),same_surfaces,same_edges,va==vb,row['world_bounds_max_delta_mm']<1e-6])
    row['topology_identical']=row['source_topology']==row['candidate_topology']
    return row


def sampled_boundary_distance(a,b):
    """Finite boundary sample screen, explicitly not a Hausdorff-distance proof."""
    from OCP.BRepExtrema import BRepExtrema_DistShapeShape
    from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeVertex
    from OCP.gp import gp_Pnt
    def distance(point,shape):
        query=BRepExtrema_DistShapeShape(BRepBuilderAPI_MakeVertex(gp_Pnt(*point)).Vertex(),shape.wrapped)
        query.Perform()
        if not query.IsDone():raise RuntimeError('Boundary distance did not converge')
        return query.Value()
    directions=[]
    for s,t in ((a,b),(b,a)):
        vertices=[v.toTuple() for v in s.Vertices()]
        points=[e.positionAt(u).toTuple() for e in s.Edges() for u in (0,.2,.4,.6,.8,1)]
        directions.append({'vertex_sample_count':len(vertices),'edge_sample_count':len(points),
            'maximum_vertex_to_other_brep_mm':max(distance(p,t) for p in vertices),
            'maximum_edge_sample_to_other_brep_mm':max(distance(p,t) for p in points)})
    maximum=max(d[k] for d in directions for k in ('maximum_vertex_to_other_brep_mm','maximum_edge_sample_to_other_brep_mm'))
    return {'source_to_candidate':directions[0],'candidate_to_source':directions[1],
            'maximum_sampled_distance_mm':maximum,'acceptance_mm':.001,'within_acceptance':maximum<=.001,
            'limitation':'Vertices and six arc-length samples per edge, both directions. This does not prove a continuous maximum surface deviation or exact BRep identity.'}
