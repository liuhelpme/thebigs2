# Summarise tools/perf/sample.sh output: idle vs busy, inclusive and self time.
#   python3 tools/perf/prof.py <samples.txt> [inclusive rows]
import sys, re, collections
IDLE=re.compile(r'futex|nanosleep|clock_nanosleep|pthread_cond|__syscall_cancel|epoll')
inc=collections.Counter(); self_=collections.Counter(); n=0; idle=collections.Counter(); par=collections.Counter()
for l in open(sys.argv[1]):
    p=l.rstrip('\n').split('|',2)
    if len(p)<3: continue
    s=[re.sub(r'\(.*','',x).replace('rex::graphics::','').replace('vulkan::','vk::') for x in p[2].split(' < ')]
    named=[f for f in s if not f.startswith('0x') and not f.startswith('__') and 'pthread' not in f and 'lll_' not in f]
    if IDLE.search(s[0]):
        idle[' < '.join(named[:2])]+=1; continue
    n+=1; self_[named[0] if named else s[0]]+=1
    if len(named)>1: par[' < '.join(named[:2])]+=1
    seen=set()
    for f in named:
        if f not in seen: inc[f]+=1; seen.add(f)
print('busy', n, 'idle', sum(idle.values()))
for k,v in idle.most_common(4): print('  idle %3d %s'%(v,k[:150]))
print('-- inclusive')
for f,c in inc.most_common(int(sys.argv[2]) if len(sys.argv)>2 else 30)[6:]: print('  %3d %4.0f%%  %s'%(c,100*c/n,f[:100]))
print('-- self < parent')
for f,c in par.most_common(20): print('  %3d %4.0f%%  %s'%(c,100*c/n,f[:140]))
