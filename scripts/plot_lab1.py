from pathlib import Path
import csv,json,statistics,sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parents[1]
ev=Path(sys.argv[1]) if len(sys.argv)>1 else root/'evidence/20261011'
rows=list(csv.DictReader((ev/'performance.csv').open()));assert len(rows)==105
metrics={}
for r in rows:
 k=(r['kind'],r['operation'],int(r['pool_frames']),int(r['size']))
 metrics.setdefault(k,[]).append(r)
summary=[]
for (kind,op,pool,size),rs in metrics.items():
 assert len(rs)==5 and {int(x['repetition']) for x in rs}==set(range(1,6))
 assert len({x['checksum'] for x in rs})==1
 xs=[float(x['ms']) for x in rs];assert min(xs)>0
 summary.append(dict(kind=kind,operation=op,pool_frames=pool,size=size,median=statistics.median(xs),minimum=min(xs),maximum=max(xs),checksum=rs[0]['checksum']))
(ev/'summary.json').write_text(json.dumps(summary,indent=2))
s={(r['kind'],r['operation'],r['pool_frames'],r['size']):r for r in summary}
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':11,'axes.spines.top':False,'axes.spines.right':False})
colors=['#213d60','#16817c','#af6133']
fig,ax=plt.subplots(figsize=(7.5,3.8),layout='constrained')
for op,color in zip(['sequential','uniform','hotspot'],colors):
 ms=[s['page',op,n,100000] for n in [32,128,512]];med=[r['median'] for r in ms]
 ax.errorbar([0,1,2],med,yerr=[[r['median']-r['minimum'] for r in ms],[r['maximum']-r['median'] for r in ms]],marker='o',capsize=4,lw=1.6,color=color,label=op)
ax.set_xticks([0,1,2],['32','128','512']);ax.set_xlabel('Buffer pool frames (4 KiB per frame)');ax.set_ylabel('Time per 100,000 accesses (ms)');ax.grid(axis='y',alpha=.2);ax.legend(frameon=False)
fig.savefig(ev/'page-performance.png',dpi=220);fig.savefig(ev/'page-performance.svg');plt.close(fig)
fig,ax=plt.subplots(figsize=(7.5,3.8),layout='constrained')
for op,color in zip(['insert','scan','update','delete'],colors+['#696b70']):
 ms=[s['record',op,128,n] for n in [10000,50000,100000]];med=[r['median'] for r in ms]
 ax.errorbar([1,5,10],med,yerr=[[r['median']-r['minimum'] for r in ms],[r['maximum']-r['median'] for r in ms]],marker='o',capsize=4,lw=1.6,color=color,label=op)
ax.set_xticks([1,5,10]);ax.set_xlabel('Record count (10,000 records)');ax.set_ylabel('Time (ms)');ax.grid(axis='y',alpha=.2);ax.legend(frameon=False,ncol=4)
fig.savefig(ev/'record-performance.png',dpi=220);fig.savefig(ev/'record-performance.svg');plt.close(fig)
print('Validated 105 measurements; 21 groups; 5 repetitions each.')
