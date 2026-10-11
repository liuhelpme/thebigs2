import re,glob,collections,json,sys
# usage: run from the project root after codegen; writes callgraph.json
func_re=re.compile(r'^DEFINE_REX_FUNC\((sub_[0-9A-F]+)\)',re.M)
call_re=re.compile(r'\b(sub_[0-9A-F]{8}|__imp__[A-Za-z]\w+|__savegprlr_\d+|__restgprlr_\d+)\(ctx, base\)')
callees={}; sizes={}
for f in glob.glob('generated/default/thebigs2_recomp.*.cpp'):
    s=open(f).read()
    idx=[(m.start(),m.group(1)) for m in func_re.finditer(s)]
    for i,(st,name) in enumerate(idx):
        en=idx[i+1][0] if i+1<len(idx) else len(s)
        body=s[st:en]
        callees[name]=sorted(set(c for c in call_re.findall(body) if not c.startswith('__savegpr') and not c.startswith('__restgpr')))
        sizes[name]=body.count('\n\t// ')
json.dump({'callees':callees,'sizes':sizes},open('callgraph.json','w'))
print(len(callees))
