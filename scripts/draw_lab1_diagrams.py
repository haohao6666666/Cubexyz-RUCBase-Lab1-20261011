"""Draw implementation diagrams from the Lab1 page layout and pin rules."""
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, Polygon
from matplotlib.font_manager import FontProperties

OUT=Path(__file__).resolve().parents[1]/'evidence/20261011'
font_path=Path('/System/Library/Fonts/Supplemental/Songti.ttc')
FONT=FontProperties(fname=str(font_path)) if font_path.exists() else FontProperties(family='Noto Sans CJK SC')
BLUE='#244A69'; GREEN='#327669'; GRAY='#4C5359'
def text(ax,x,y,s,size=12,**kw):
    ax.text(x,y,s,fontproperties=FONT,fontsize=size,color=GRAY,ha='center',va='center',**kw)
def box(ax,x,y,w,h,label,color=BLUE,size=12):
    ax.add_patch(FancyBboxPatch((x,y),w,h,boxstyle='round,pad=0.03,rounding_size=0.06',facecolor='white',edgecolor=color,linewidth=1.4))
    text(ax,x+w/2,y+h/2,label,size)
def arrow(ax,a,b,color=BLUE):
    ax.annotate('',xy=b,xytext=a,arrowprops={'arrowstyle':'->','lw':1.4,'color':color})
def save(fig,name):
    fig.savefig(OUT/(name+'.png'),dpi=250,bbox_inches='tight',facecolor='white')
    fig.savefig(OUT/(name+'.svg'),bbox_inches='tight',facecolor='white');plt.close(fig)

fig,ax=plt.subplots(figsize=(9,3.8));ax.set(xlim=(0,10),ylim=(0,4));ax.axis('off')
text(ax,5,3.7,'记录页布局：R = 64 字节，N = 63 个槽',16)
# Diagram widths are deliberately expanded for small headers; offsets are exact.
regions=[(0.35,1.1,'公共页头\n4 B','0'),(1.45,1.65,'记录页头\n8 B','4'),(3.1,1.3,'位图\n8 B','12'),(4.4,4.3,'63 个记录槽\n4032 B','20'),(8.7,.95,'余量\n44 B','4052')]
for x,w,label,offset in regions:
    ax.add_patch(Rectangle((x,2.2),w,.8,facecolor='white',edgecolor=BLUE,linewidth=1.3));text(ax,x+w/2,2.6,label,11);text(ax,x,2.03,offset,10)
text(ax,9.65,2.03,'4096',10)
text(ax,5,1.63,'槽地址 = 页起始地址 + 20 + slot_no × 64',12)
for i,bit in enumerate([1,0,1,1,0,0,1,0]):
    x=2.15+i*.63;ax.add_patch(Rectangle((x,.83),.55,.48,facecolor='white',edgecolor=GREEN,linewidth=1.2));text(ax,x+.275,1.07,str(bit),12);text(ax,x+.275,.64,str(i),10)
text(ax,5,.25,'位为 1 才能读取或扫描；位为 0 时旧字节不构成有效记录。',12)
save(fig,'record-layout')

fig,ax=plt.subplots(figsize=(9,4.5));ax.set(xlim=(0,10),ylim=(0,5));ax.axis('off')
text(ax,5,4.7,'缓冲帧状态与页面引用的配对',16)
box(ax,.45,2.7,2,.95,'空闲帧\nfree_list_',BLUE)
box(ax,3.95,2.7,2.3,.95,'固定帧\npin_count > 0',BLUE)
box(ax,7.6,2.7,2,.95,'可替换帧\npin_count = 0',GREEN)
arrow(ax,(2.45,3.18),(3.95,3.18));text(ax,3.2,3.52,'fetch / new',11)
arrow(ax,(6.25,3.18),(7.6,3.18));text(ax,6.92,3.88,'最后一次 unpin',11)
arrow(ax,(7.6,2.53),(6.25,2.53));text(ax,6.92,2.22,'再次 fetch',11)
box(ax,4.05,.5,2.1,.9,'LRU 选出候选\n旧脏页先写回',GREEN,11)
arrow(ax,(8.6,2.7),(8.6,.95));arrow(ax,(8.6,.95),(6.15,.95));text(ax,8.0,.57,'victim',11)
arrow(ax,(5.1,1.4),(5.1,2.7));text(ax,3.5,1.99,'安装目标页\n固定一次',11)
text(ax,2.65,.65,'删除零引用页\n返回空闲表',11)
ax.plot([7.75,7.2,7.2,1.45],[2.7,1.9,.18,.18],color=BLUE,lw=1.3)
arrow(ax,(1.45,.18),(1.45,2.7))
text(ax,5,4.24,'dirty 独立于固定状态；flush 成功后清脏，不改变 pin_count。',11)
save(fig,'frame-states')

def decision(ax,x,y,w,h,label):
    ax.add_patch(Polygon([(x+w/2,y+h),(x+w,y+h/2),(x+w/2,y),(x,y+h/2)],closed=True,facecolor='white',edgecolor=BLUE,linewidth=1.4))
    text(ax,x+w/2,y+h/2,label,11)

fig,ax=plt.subplots(figsize=(9,6.4));ax.set(xlim=(0,10),ylim=(0,8));ax.axis('off')
text(ax,5,7.7,'fetch_page：先读取，再提交帧转换',16)
box(ax,3.6,6.7,2.8,.55,'输入 PageId，持有缓冲池锁',size=11)
decision(ax,3.85,5.55,2.3,.8,'页表命中？');arrow(ax,(5,6.7),(5,6.35))
box(ax,7.1,5.65,2.5,.65,'退出 LRU，引用加一\n返回已有页面',size=11);arrow(ax,(6.15,5.95),(7.1,5.95));text(ax,6.6,6.14,'是',10)
decision(ax,3.85,4.25,2.3,.8,'有可用帧？');arrow(ax,(5,5.55),(5,5.05));text(ax,5.27,5.31,'否',10)
box(ax,.35,4.32,2.7,.65,'全部固定：返回 nullptr',size=11);arrow(ax,(3.85,4.65),(3.05,4.65));text(ax,3.43,4.84,'否',10)
box(ax,3.55,3.15,2.9,.65,'目标页读入临时数组',size=11);arrow(ax,(5,4.25),(5,3.8));text(ax,5.27,4.03,'是',10)
box(ax,3.55,2.02,2.9,.65,'旧页为脏则先写回',size=11);arrow(ax,(5,3.15),(5,2.67))
box(ax,7.15,2.55,2.45,1.1,'读写抛出异常\n归还空闲帧或 LRU 候选\n重新抛出',GREEN,10)
ax.plot([6.45,6.78,6.78],[3.48,3.48,3.1],ls='--',lw=1.3,color=GREEN)
ax.plot([6.45,6.78,6.78],[2.34,2.34,3.1],ls='--',lw=1.3,color=GREEN)
arrow(ax,(6.78,3.1),(7.15,3.1),GREEN)
box(ax,3.55,.92,2.9,.65,'移除旧映射，安装新页\npin_count = 1',size=11);arrow(ax,(5,2.02),(5,1.57))
box(ax,3.95,.05,2.1,.47,'返回目标页面',size=11);arrow(ax,(5,.92),(5,.52))
text(ax,1.6,2.6,'选帧顺序\n空闲表优先\n否则 LRU 队尾',11)
save(fig,'fetch-flow')

fig,ax=plt.subplots(figsize=(9,6.4));ax.set(xlim=(0,10),ylim=(0,8));ax.axis('off')
text(ax,5,7.7,'insert_record：位图与非满页链表同步更新',16)
box(ax,3.5,6.75,3,.55,'检查输入，创建或取得非满页',size=11)
decision(ax,3.7,5.55,2.6,.85,'非满页链表为空？');arrow(ax,(5,6.75),(5,6.4))
box(ax,.35,5.53,2.65,.88,'创建新页\n初始化页头和位图\n成功后更新文件头',size=10)
arrow(ax,(3.7,5.98),(3,5.98));text(ax,3.36,6.18,'是',10)
box(ax,7.05,5.61,2.6,.7,'取得链表头页面\n接管一次固定引用',size=11)
arrow(ax,(6.3,5.98),(7.05,5.98));text(ax,6.67,6.18,'否',10)
box(ax,3.5,4.2,3,.8,'寻找首个空槽\n复制记录，置位，记录数加一',size=11)
ax.plot([1.68,1.68,5],[5.53,5.14,5.14],color=BLUE,lw=1.3)
ax.plot([8.35,8.35,5],[5.61,5.14,5.14],color=BLUE,lw=1.3);arrow(ax,(5,5.14),(5,5.0))
decision(ax,3.7,2.86,2.6,.86,'插入后页面已满？');arrow(ax,(5,4.2),(5,3.72))
box(ax,7.05,2.85,2.6,.85,'链表头移到后继\n本页 next = RM_NO_PAGE',size=10)
arrow(ax,(6.3,3.29),(7.05,3.29));text(ax,6.67,3.49,'是',10)
box(ax,3.5,1.45,3,.8,'标记修改，返回实际 Rid\n作用域结束统一 unpin',size=11)
arrow(ax,(5,2.86),(5,2.25));text(ax,5.25,2.55,'否',10)
ax.plot([8.35,8.35,6.5],[2.85,1.85,1.85],color=BLUE,lw=1.3);arrow(ax,(6.8,1.85),(6.5,1.85))
text(ax,5,.57,'分配失败时不推进文件头；写操作释放时保留 dirty 标记。',12)
save(fig,'insert-flow')
