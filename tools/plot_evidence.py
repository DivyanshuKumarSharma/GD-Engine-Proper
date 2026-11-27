"""Regenerate portfolio figures from captured CSVs; requires matplotlib."""
import csv
import json
import math
from pathlib import Path
import statistics
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root=Path(__file__).resolve().parents[1]/'docs/evidence'
plt.rcParams.update({'figure.facecolor':'#101820','axes.facecolor':'#101820','text.color':'#edf2f7',
    'axes.labelcolor':'#edf2f7','xtick.color':'#cbd5e1','ytick.color':'#cbd5e1','axes.edgecolor':'#64748b','font.size':10})
def rows(name):
    with (root/name).open(encoding='utf-8-sig') as stream: return list(csv.DictReader(stream))
data=rows('windows-algorithms.csv')
fig,axes=plt.subplots(1,2,figsize=(11,4.4),layout='constrained')
for ax,count in zip(axes,[1000,10000]):
    selected=[next(r for r in data if r['algorithm']==kind and int(r['size'])==count) for kind in ['brute_128_rays','bvh_128_rays','bvh_build']]
    values=[float(r['mean_us'])/1000 for r in selected]
    bars=ax.bar(['Brute force\n128 rays','BVH\n128 rays','BVH build\n(one rebuild)'],values,color=['#fbbf24','#22d3ee','#a78bfa'])
    ax.bar_label(bars,fmt='%.3f ms',padding=4,color='#edf2f7'); ax.set_ylim(0,max(values)*1.22)
    ax.set_title(f'{count:,} static AABBs'); ax.set_ylabel('Mean CPU wall time (ms)')
fig.suptitle('Spatial-query benchmark | Release, fixed seed, 100 samples\nBuild cost is separate; these are not engine frame times',fontsize=12)
fig.savefig(root/'algorithm-comparison.png',dpi=160); plt.close(fig)

summary={}
for label,name in [('Main-thread physics','windows-main-frames.csv'),('Worker physics + wait','windows-worker-frames.csv')]:
    samples=[float(r['frame_ms']) for r in rows(name)]
    # Same fixed warmup rule for both runs; include measured transitions afterward.
    samples=samples[20:]
    ordered=sorted(samples)
    summary[label]={'samples':len(samples),'mean_ms':statistics.mean(samples),'p99_ms':ordered[math.ceil(.99*len(ordered))-1],'max_ms':max(samples)}
fig,ax=plt.subplots(figsize=(9,4.6),layout='constrained')
for i,(label,s) in enumerate(summary.items()):
    bars=ax.bar([i*3,i*3+1],[s['mean_ms'],s['p99_ms']],color=['#22d3ee','#a78bfa'],label=label)
    ax.bar_label(bars,fmt='%.2f ms',padding=4,color='#edf2f7')
ax.set_xticks([0,1,3,4],['Main / mean','Main / p99','Worker / mean','Worker / p99'])
ax.set_ylabel('CPU frame wall time, includes presentation (ms)')
ax.set_ylim(0,max(max(s['p99_ms'],s['mean_ms']) for s in summary.values())*1.25)
ax.set_title('Same editor workflow, physics scheduling comparison\n20 warmup frames discarded; exploratory run, not a speedup claim')
fig.savefig(root/'frame-comparison.png',dpi=160); plt.close(fig)
(root/'frame-summary.json').write_text(json.dumps(summary,indent=2))

data=rows('windows-netcode.csv'); fig,axes=plt.subplots(2,1,figsize=(10,6),sharex=True,layout='constrained')
seconds=[int(r['tick'])/60 for r in data]
for key,label,colour in [('predicted_x','Client predicted','#22d3ee'),('server_x','Server authoritative','#fbbf24'),('interpolated_x','Interpolated remote','#a78bfa')]:
    axes[0].plot(seconds,[float(r[key]) for r in data],label=label,color=colour,linewidth=1.2)
axes[0].set_ylabel('Position X (m)'); axes[0].legend(facecolor='#182632',labelcolor='white')
axes[0].set_title('Real loopback UDP | 75 ms each way, 20 ms jitter, 5% loss\nVirtual 60 Hz timeline; prediction reconciles after acknowledgements')
axes[1].plot(seconds,[int(r['pending']) for r in data],color='#34d399'); axes[1].set_ylabel('Unacknowledged inputs'); axes[1].set_xlabel('Simulated seconds')
fig.savefig(root/'network-comparison.png',dpi=160); plt.close(fig)
print(json.dumps(summary,indent=2))
