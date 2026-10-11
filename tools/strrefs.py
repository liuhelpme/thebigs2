# Find functions that reference rdata strings matching a regex (lis/addi pairs).
import re,glob,sys,collections
pat=re.compile(sys.argv[1].encode())
# usage: strrefs.py REGEX RDATA_DUMP (section dump of .rdata at 0x82000400), run from the project root
rd=open(sys.argv[2],'rb').read(); base=0x82000400
targets={}
for m in re.finditer(rb'[\x20-\x7e]{5,}\x00',rd):
    if pat.search(m.group()): targets[base+m.start()]=m.group()[:-1].decode()
pairs={}
for a,s in targets.items():
    hi=(a+0x8000)>>16; lo=a-(hi<<16)
    hi_s=hi-0x10000 if hi>=0x8000 else hi
    pairs[(hi_s,lo)]=s
func_re=re.compile(r'^DEFINE_REX_FUNC\((sub_[0-9A-F]+)\)',re.M)
lis_re=re.compile(r'// lis (r\d+),(-?\d+)')
addi_re=re.compile(r'// (?:addi|ori) r\d+,(r\d+),(-?\d+)')
hits=collections.defaultdict(set)
for f in glob.glob('generated/default/thebigs2_recomp.*.cpp'):
    s=open(f).read()
    idx=[(m.start(),m.group(1)) for m in func_re.finditer(s)]
    for i,(st,name) in enumerate(idx):
        en=idx[i+1][0] if i+1<len(idx) else len(s)
        body=s[st:en]
        his={int(v) for r,v in lis_re.findall(body)}
        for r,v in addi_re.findall(body):
            for h in his:
                if (h,int(v)) in pairs: hits[pairs[(h,int(v))]].add(name)
for s in sorted(hits): print(s,'->',' '.join(sorted(hits[s])))
