"""Bench companion for the original, non-gasket fit-check project."""
from pathlib import Path
import json, io, zipfile
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor, white
from reportlab.lib.utils import ImageReader
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import Paragraph
from reportlab.lib.styles import ParagraphStyle

ROOT=Path(__file__).resolve().parents[1]
OLD=Path('/Users/magnustrandokken/.codex/worktrees/internal-gasket/trimix-analysator/hardware/cad/rev04/internal-seal/fit-tests/unmarked/views/parts')
DEST=ROOT/'output/pdf/Trimix_Original_Fit_Check_Guide.pdf';DEST.parent.mkdir(parents=True,exist_ok=True)
for name,file in [('Arial','Arial.ttf'),('Arial-Bold','Arial Bold.ttf')]:
    pdfmetrics.registerFont(TTFont(name,'/System/Library/Fonts/Supplemental/'+file))
NAVY=HexColor('#193840');TEAL=HexColor('#087F87');GRAY=HexColor('#53666A');PALE=HexColor('#EAF3F3');LINE=HexColor('#CAD8D8')
W,H=595.276,841.89;M=36;CW=W-2*M
c=canvas.Canvas(str(DEST),pagesize=(W,H));c.setTitle('Trimix original enclosure - fit check');c.setAuthor('Trimix design project')

def text(s,x,y,size=10,bold=False,color=NAVY):
    c.setFillColor(color);c.setFont('Arial-Bold' if bold else 'Arial',size);c.drawString(x,H-y-size,s)

def para(s,x,y,w,size=10,bold=False,color=NAVY):
    style=ParagraphStyle('p',fontName='Arial-Bold' if bold else 'Arial',fontSize=size,leading=size*1.35,textColor=color,spaceAfter=0)
    p=Paragraph(s,style);_,h=p.wrap(w,H);p.drawOn(c,x,H-y-h);return h

def line(x1,y1,x2,y2):
    c.setStrokeColor(LINE);c.setLineWidth(.65);c.line(x1,H-y1,x2,H-y2)

def image(src,x,y,w,h):
    c.drawImage(ImageReader(src),x,H-y-h,w,h,preserveAspectRatio=True,anchor='c',mask='auto')

def frame(n,title,sub):
    text('TRIMIX  /  ORIGINAL ENCLOSURE',M,25,9,True,TEAL)
    text(title,M,49,23,True)
    para(sub,M,82,CW,10,color=GRAY)
    line(M,791,W-M,791);text('H2D / 0.4 mm / Bambu PLA Basic / 100% scale',M,803,8,color=GRAY)
    c.setFont('Arial',8);c.drawRightString(W-M,H-811,f'{n} / 3')

def box(s,y):
    c.setFillColor(PALE);c.roundRect(M,H-y-57,CW,57,7,fill=1,stroke=0)
    para(s,M+13,y+11,CW-26,10)

frame(1,'Back to the original enclosure','15 October 2026  |  Dry mechanical fit checks for your screen, sensors and battery.')
image(ROOT/'views/original-assembly.png',M,111,245,200)
text('Print only plates 3 and 4',304,126,15,True)
para('You already printed the previous kit. Only the housing and rear lid have changed. All 15 other pieces match the original design and can be reused.',304,154,252,11)
para('The gasket design remains saved separately in Fusion as <b>Trimix_Enclosure_A3_InternalSeal</b>. The active original variant is <b>Trimix_Enclosure_A3_Original</b>.',304,234,252,9.5)
text('One Bambu Studio file, four plates',M,324,15,True)
rows=[('01','Small fit checks','14 pieces, including D02 PCB replica','Reuse previous prints'),('02','Sensor manifold P07','1 part; gas passages protected from support','Reuse previous print'),('03','Original housing P01','Original walls, opening and screw bosses','REPRINT'),('04','Original rear lid P02','Original locating tabs; no rear gasket lip','REPRINT')]
for i,(n,title,description,status) in enumerate(rows):
    y=358+i*60
    text(n,M,y,17,True,TEAL);text(title,M+35,y,11,True)
    text(description,M+35,y+19,9,color=GRAY)
    text(status,421,y+2,9,True,TEAL);line(M,y+48,W-M,y+48)
box('<b>Removed:</b> the six rear-gasket coupons C01-C06. D01 remains omitted: D02 already includes the PCB outline and component-height envelope. There are no added physical ID labels.',611)
para('<b>Before printing:</b> select the actual build surface in Bambu Studio; the supplied project assumes Textured PEI. Keep scale at 100%. Remove brim and accessible supports before measuring. The manifold retains its channel-support exclusions.',M,686,CW,10)
para('<b>Scope:</b> this kit checks mechanical fit. The PCB replicas are provisional mechanical allocations, not exact routed PCB models. The sensor gas-path seals remain a separate requirement; deferring the rear gasket does not remove them.',M,745,CW,9)
c.showPage()

frame(2,'Identify the small pieces','All 14 pieces are on plate 1. These existing prints can be reused; IDs below appear in the guide and Bambu object list only.')
tiles=[
('D02','Main PCB replica','Fit on P03; check holes and lid clearance.','30 x 99 mm; 1.6 mm board; 6.6 mm overall'),
('D03','USB PCB replica','Fit in P06 and clamp with P10.','16 x 13.6 x 0.6 mm'),
('P03','Electronics carrier','Test D02 seating and removal from housing.','30.4 x 100 x 2 mm'),
('P04','Lower screen retainer','Use with the real screen and housing.','7 x 10.3 x 6.4 mm'),
('P05','Upper screen retainer','Use with the real screen and housing.','7 x 9.7 x 6.4 mm'),
('P06','USB support frame','Seat D03 or the actual USB board.','25.8 x 19.5 x 12.2 mm'),
('P08','Sensor chamber lid','Test against P07 and fitted sensors.','69.5 x 47 x 10.7 mm'),
('P09','USB bezel','Check port opening and cable clearance.','20 x 11 x 4.4 mm in print orientation'),
('P10','USB retaining bridge','Clamp the board without bending it.','25.8 x 18.3 x 8.3 mm'),
('P11','AO2 adapter','Thread gently onto the real AO2 nose.','20.2 mm outside diameter; 3.8 mm tall'),
('C07','Screen edge coupon','Try against the real screen casing edge.','Local edge fit; full fit needs P01'),
('C08','Battery rail coupon','Try against the occupied battery holder.','Local contact fit; full fit needs P01/P02'),
('C09','M2 insert ladder','Use your actual inserts; compare pilots.','3.0 / 3.1 / 3.2 / 3.3 mm; 4.2 mm deep'),
('C10','M3 insert ladder','Use your actual inserts; compare pilots.','4.0 / 4.1 / 4.2 / 4.3 mm; 5.2 mm deep')]
for i,(pid,title,action,dim) in enumerate(tiles):
    col=i%2;row=i//2;x=M+col*270;y=125+row*91
    fn=pid+'-top.png' if pid in ['C09','C10'] else pid+'.png'
    image(OLD/fn,x,y+8,82,65)
    para(pid+'  <b>'+title+'</b>',x+87,y,168,9.5)
    para(action,x+87,y+28,168,9)
    para(dim,x+87,y+59,168,8,color=GRAY)
    line(x,y+86,x+253,y+86)
para('Insert ladder order is identified by 1-4 edge notches. Nominal dimensions are CAD targets, not pass/fail tolerances. Record actual measurements before sanding or drilling.',M,765,CW,8.5,color=GRAY)
c.showPage()

frame(3,'Bench checklist - original enclosure','Use your digital caliper and ruler. Record the tightest clearance and where it occurs. Leave batteries disconnected during fitting.')
box('<b>Measure consistently:</b> zero clean caliper jaws; use light pressure. Measure width at both ends and the middle. For clearance, subtract the measured component size from its measured opening. Optional feeler gauges can check an accessible gap; verify any printed gauge thickness first.',115)
text('Part / check',M+22,189,10,True);text('What to measure or try',216,189,10,True);text('Result / notes',474,189,10,True)
checks=[
('P01 housing','Measure width and length; target 85 x 180 mm. Use ruler for 180 mm if caliper range is shorter.'),
('P01 + P02 lid','Close with no gasket. Compare seam at all four corners. Check for rocking, bowing or a raised corner.'),
('Screen opening','Inside jaws across opening: target 70.1 x 117.6 mm. Measure the screen casing; record total clearance.'),
('Screen retainers P04/P05','Seat real screen. Tighten gently; retainers touch casing, with no glass pressure or screen movement.'),
('Battery + C08','Measure occupied holder width/length/height. Check insertion, wire exit and removal using the full housing.'),
('Carrier P03 + D02','Measure replica board thickness away from raised blocks: 1.6 mm. Seat all holes without bending.'),
('PCB / lid clearance','With D02 fitted, close P02 gently. Check no component block touches the lid; note any contact point.'),
('AO2 + P11','Try the thread by hand. Check nose seats and sensor can be installed/removed without forcing.'),
('Other sensors + P07','Fit your sensors one at a time. Check bodies, boards, leads and connector access; then fit all together.'),
('Manifold + P08','Check the chamber lid seats and all screws align. Leads stay clear; cartridge remains removable.'),
('USB P06/P09/P10/D03','Test board seating, bridge and cable plug. Check service release: 2.5 mm inward, then rearward.'),
('Button + cables','Install real button/nut if available. Check clearance and wire routing with rear lid closed.'),
('Inserts C09/C10','Record pilot that suits each actual insert. Inspect for bulging/cracks; then trial spare screws.'),
('Full assembly / service','Assemble all available parts, close, reopen and remove parts. Repeat five times; record binding or wear.')]
for i,(name,action) in enumerate(checks):
    y=216+i*35
    c.setStrokeColor(TEAL);c.setLineWidth(.8);c.rect(M,H-y-10,9,9,fill=0,stroke=1)
    para(name,M+22,y-1,151,9,True)
    h=para(action,216,y-1,248,8.3)
    assert h<32,(name,h)
    line(476,y+20,W-M,y+20);line(M,y+31,W-M,y+31)
text('Measurements / changes to report',M,717,11,True)
line(M,750,W-M,750);line(M,775,W-M,775)
c.save()
print(DEST)
