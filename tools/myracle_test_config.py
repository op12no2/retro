# Launch every oldies entry from Myracle engines.json.gz through cutechess exactly as configured (smoke test).
import gzip, json, subprocess, re, sys, os
CC='/mnt/d/myracle/tools/cutechess-win64/cutechess-cli.exe'
eng=json.load(gzip.open('/mnt/d/myracle/config/engines.json.gz','rt',encoding='utf-8'))
new=[e for e in eng if e['group']=='oldies' and e['name'] not in ('Sargon I 1978','Psion 2.13 1985','N.E.G. 1.3')]
opp=next(e for e in eng if e['name']=='Random Mover 2020')
def args(e, tag):
    a=['-engine','name='+tag,'cmd='+e['command'],'dir=D:\\myracle\\'+e['working_folder'].replace('/','\\'),'proto='+e['protocol']]
    for o in e.get('options',[]):
        if o['type'] in ('spin','check','combo') and o['name']!='Protocol':
            v=o['value']; v='true' if v is True else 'false' if v is False else str(v)
            a.append('option.%s=%s'%(o['name'],v))
    return a
os.chdir('/mnt/d/myracle/tournaments')
for e in new:
    tag=re.sub(r'[^A-Za-z0-9]','',e['name'])
    cmd=[CC]+args(e,tag)+args(opp,'Random')+['-each','tc=20+0.5','timemargin=3000','-games','1','-concurrency','1']
    try: r=subprocess.run(cmd,capture_output=True,text=True,timeout=400); out=r.stdout+r.stderr
    except subprocess.TimeoutExpired: out='TIMEOUT'
    out=out.replace('\r','')
    fin=[l for l in out.split('\n') if l.startswith('Finished game')]
    warn=[l for l in out.split('\n') if 'Warning' in l or 'rror' in l or 'Cannot' in l or 'illegal' in l.lower()]
    print('%-24s %-7s %s' % (e['name'], e['protocol'], fin[0][fin[0].find(':')+1:].strip() if fin else out.strip().split('\n')[-1][:100]))
    for w in warn[:3]: print('      ', w[:140])
    sys.stdout.flush()
