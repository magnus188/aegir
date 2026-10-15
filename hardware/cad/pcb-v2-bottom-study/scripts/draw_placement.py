"""Dimensioned placement-study illustration; never a routed PCB or print model."""
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle

OUT=Path(__file__).resolve().parents[1]/'views'
fig,ax=plt.subplots(figsize=(10,10),dpi=180)
fig.patch.set_facecolor('#f6f7f7');ax.set_facecolor('#f6f7f7')
case=FancyBboxPatch((0,0),85,180,boxstyle='round,pad=0,rounding_size=8',
                  facecolor='#dce3e8',edgecolor='#536674',linewidth=2)
ax.add_patch(case)
ax.add_patch(FancyBboxPatch((2.4,2.4),80.2,175.2,boxstyle='round,pad=0,rounding_size=5.6',
             facecolor='#f7f9fa',edgecolor='#a8b4bd',linewidth=1))
# Existing manifold and battery occupied-space footprints, in unchanged coordinates.
ax.add_patch(Rectangle((3,130),79,47,facecolor='#e3e6e9',edgecolor='#8796a0',lw=1))
ax.text(42.5,154,'Gassdel\nuendret',ha='center',va='center',color='#4d5f6b',fontsize=11)
ax.add_patch(Rectangle((5.4,27),42,80.4,facecolor='#e7d6ac',edgecolor='#aa915a',lw=1.5))
for x in (7.1,27.5):
 ax.add_patch(FancyBboxPatch((x,32),18.2,69,boxstyle='round,pad=0,rounding_size=5',
              facecolor='#d6c080',edgecolor='#bba060',linewidth=.7))
ax.text(26.4,67,'Samme\nbatteriholder',ha='center',va='center',fontsize=11,color='#433921')
# Exact core outline translated -18.9 Y, joined to the proposed rotated USB tab.
# The added bridge shape is a placement envelope, not a reviewed copper layout.
outline=[(34.01509,1.3),(51.5,1.3),(51.5,6.3),(56.2,6.3),(56.2,2.1),
 (80.4,2.1),(80.4,84.1),(59.3,84.1),(59.3,101.1),(50.4,101.1),
 (50.4,21),(34.01509,21)]
ax.add_patch(Polygon(outline,closed=True,facecolor='#42876d',edgecolor='#1d5e45',lw=1.5))
# Current GCT body bounds rigidly rotated around its mouth; Z is not depicted.
ax.add_patch(Rectangle((42.5-4.47,1.3),8.94,7.78,facecolor='#e8a558',edgecolor='#a46825',lw=1))
ax.add_patch(Rectangle((41.26,11.8),2.48,1,facecolor='#273833',lw=0))
ax.add_patch(Rectangle((46.4,10.3),1.6,.8,facecolor='#273833',lw=0))
ax.annotate('USB-C i bunnen\nSvakt innfelt',xy=(42.5,1.3),xytext=(10,-25),
            fontsize=12,color='#6f461d',ha='left',arrowprops=dict(arrowstyle='->',color='#a46825',lw=1.4))
ax.annotate('Ett sammenhengende kort\nca. 46,4 × 99,8 mm',xy=(68,51),xytext=(104,58),
            fontsize=12,color='#1d5e45',ha='left',va='center',
            arrowprops=dict(arrowstyle='-',color='#1d5e45',lw=1.3))
ax.annotate('Hoveddelen flyttes\n18,9 mm nedover',xy=(56,100),xytext=(104,105),
            fontsize=11,color='#344f43',ha='left',va='center',
            arrowprops=dict(arrowstyle='-',color='#344f43',lw=1.2))
ax.annotate('85 mm',xy=(0,189),xytext=(42.5,189),ha='center',va='center',fontsize=12,
            arrowprops=dict(arrowstyle='->',color='#536674',lw=1.2))
ax.annotate('',xy=(85,189),xytext=(50,189),arrowprops=dict(arrowstyle='->',color='#536674',lw=1.2))
ax.annotate('',xy=(-9,0),xytext=(-9,180),arrowprops=dict(arrowstyle='<->',color='#536674',lw=1.2))
ax.text(-13,90,'180 mm',rotation=90,ha='center',va='center',fontsize=11,color='#536674')
ax.text(104,160,'SETT FRA BAKSIDEN\nLokket er ikke vist',fontsize=10,color='#657583',ha='left',va='center')
ax.text(104,24,'Kortet heves også 3,5 mm.\nNye kortfester og USB-støtte\nmå utformes i plastkassen.',fontsize=11,color='#536674',ha='left',va='center',linespacing=1.5)
ax.set_xlim(-20,177);ax.set_ylim(-36,196);ax.set_aspect('equal');ax.axis('off')
fig.suptitle('85 mm bredde · USB-C i bunnen',x=.5,y=.97,fontsize=21,color='#273b48')
fig.text(.5,.925,'Plasseringsstudie – kortlayout og nye fester gjenstår',ha='center',fontsize=12,color='#657583')
fig.text(.5,.04,'Målene er fra CAD-modellene. Dette er ikke en produksjons- eller utskriftsfil.',
         ha='center',fontsize=10,color='#657583')
fig.subplots_adjust(top=.90,bottom=.08,left=.03,right=.99)
OUT.mkdir(parents=True,exist_ok=True)
for ext in ('png','svg'):fig.savefig(OUT/f'usb-i-bunnen-studie.{ext}',facecolor=fig.get_facecolor())
