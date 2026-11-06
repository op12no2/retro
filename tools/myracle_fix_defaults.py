# Refresh stored option defaults in Myracle engines.json.gz after an exe rebuild.
# Edit the engine-name test and the option changes below; run with Myracle CLOSED.
import gzip,json,re,shutil,time,unicodedata
CFG='/mnt/d/myracle/config/engines.json.gz'
raw=gzip.open(CFG,'rt',encoding='utf-8').read()
raw=re.sub(r'\\x([0-9a-fA-F]{2})', lambda m: chr(int(m.group(1),16)), raw)
eng=json.loads(raw.encode('latin-1','ignore').decode('utf-8','ignore')) if '\\x' in raw else json.loads(raw)
for e in eng:
    if 'comment' in e:
        e['comment']=unicodedata.normalize('NFKD',e['comment']).encode('ascii','ignore').decode()
    if e['name'].startswith('Shannon'):
        names={o['name'] for o in e['options']}
        for o in e['options']:
            if o['name']=='QuiescenceDepth':
                o['default']=4
                if o['value']==8: o['value']=4
        if 'LogFile' not in names:
            e['options'].append({'name':'LogFile','type':'string','default':'<empty>','value':'<empty>'})
        print(e['name'],[(o['name'],o['default'],o['value']) for o in e['options']])
shutil.copy(CFG,CFG+'.bak-'+time.strftime('%Y%m%d-%H%M%S'))
txt=json.dumps(eng,indent='\t',ensure_ascii=True,separators=(',',' : '))
with open(CFG,'wb') as f:
    with gzip.GzipFile(filename='engines.json',mode='wb',fileobj=f,compresslevel=9) as g: g.write(txt.encode())
print('written',len(eng))
