# Append engines to Myracle engines.json.gz (edit the NEW table). Run with Myracle CLOSED.
# Probes each UCI exe for its option list; xboard entries get no options.
import gzip, json, subprocess, sys, os, time, shutil
CFG='/mnt/d/myracle/config/engines.json.gz'
OLD='/mnt/d/myracle/engines/oldies'
# name, folder, exe, protocol, comment
NEW=[
 ("Rebel 6.0 1994",        "rebel6",       "rebeluci.exe",                    "uci",    "Ed Schröder's 2012 Windows build via wb2uci adapter; book off in wb2uci.eng"),
 ("GNU Chess 4.0 1992",    "gnuchess4",    "gnuchesx.exe",                    "xboard", "4.0.pl80 built from source with protocol patches; needs whole minutes of base time"),
 ("Kaissa 1992",           "kaissa",       "kaissa_64_ja_sse4.exe",           "xboard", "PC Kaissa 1992, descendant of the 1974 world champion (Jim Ablett port)"),
 ("Chess VI 1973",         "chess6",       "pdp-11_chess6_64_ja.exe",         "xboard", "Ken Thompson's PDP-11 Chess VI, Belle's ancestor (Jim Ablett port)"),
 ("CHEKMO-II 1978",        "chekmo",       "chekmo_II_64_ja.exe",             "xboard", "John Comeau's PDP-8 program, late 1970s (Jim Ablett port)"),
 ("Microchess 1976",       "microchess",   "microchessw.exe",                 "xboard", "UNTESTED in a GUI: announces variant nocastle, mishandles castling/ep/promotion"),
 ("TSCP 1.81 1997",        "tscp",         "tscp181.exe",                     "xboard", "Tom Kerrigan's Simple Chess Program"),
 ("micro-Max 4.8 2007",    "micromax",     "umax4_8w_promo.exe",              "xboard", "H.G. Muller, ~1950 chars of C; rebuilt to print promotion piece"),
 ("Toledo Nanochess 2009", "toledo",       "toledo_nanochess_wb_fixed.exe",   "xboard", "Oscar Toledo, 1257 chars of C; fine at 60+1 and up"),
 ("Iota 2012",             "iota",         "iota.exe",                        "uci",    "1788 chars of C, never castles; 32-bit build"),
 ("Phalanx XXV 1997",      "phalanx",      "phalanx_xxv_64_ja_uci_sse4.exe",  "uci",    "Dusan Dobes, lineage 1997 (Jim Ablett UCI build)"),
 ("Pasteque 2018",         "pasteque",     "pasteque-x64_popcnt_0_0_0.exe",   "uci",    "random-initialised eval learned by self-play"),
 ("Rodent IV 2019",        "rodent",       "rodentIV.exe",                    "uci",    "set Personality (Tal, Petrosian...) for the fun"),
 ("PeSTO 2.210 2020",      "pesto",        "PeSTO.exe",                       "uci",    "piece-square tables only, on a strong search"),
 ("Random Mover 2020",     "randommover",  "chess-random-mover.exe",          "uci",    "pure random legal mover"),
 ("MinimalChess 0.6 2021", "minimalchess", "MinimalChessEngine.exe",          "uci",    "Thomas Jahn"),
 ("Fairy-Random 2021",     "fairyrandom",  "fairy-random_x86-64.exe",         "uci",    "Fairy-Stockfish as a random mover"),
 ("Crafty 11.13 1996",     "crafty1113",   "crafty1113.exe",                  "xboard", "Hyatt's Cray Blitz descendant, built from the 1996 source with protocol-2 patches (see BUILD.txt); no book"),
]
def num(s):
    try: return int(s)
    except: 
        try: return float(s)
        except: return s
def uci_options(folder, exe):
    p=subprocess.Popen(['./'+exe],cwd=folder,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
    p.stdin.write(b'uci\n'); p.stdin.flush(); time.sleep(2.5); p.stdin.write(b'quit\n'); p.stdin.flush()
    try: out,_=p.communicate(timeout=10)
    except subprocess.TimeoutExpired: p.kill(); out,_=p.communicate()
    opts=[]
    for line in out.decode('latin-1').replace('\r','').split('\n'):
        t=line.split()
        if len(t)<4 or t[0]!='option' or t[1]!='name': continue
        # tokens: option name <words> type <t> [default <d>] [min m] [max M] [var v...]
        i=t.index('type'); name=' '.join(t[2:i]); typ=t[i+1]; rest=t[i+2:]
        o={'name':name,'type':typ}
        if typ=='button': continue
        def grab(key):
            if key in rest:
                j=rest.index(key); k=j+1
                while k<len(rest) and rest[k] not in ('min','max','var','default'): k+=1
                return ' '.join(rest[j+1:k])
            return None
        d=grab('default')
        if typ=='spin':
            o['default']=num(d); o['min']=num(grab('min')); o['max']=num(grab('max'))
        elif typ=='check':
            o['default']=(d=='true')
        elif typ=='combo':
            o['default']=d
            ch=[]; j=0
            while j<len(rest):
                if rest[j]=='var':
                    k=j+1
                    while k<len(rest) and rest[k]!='var': k+=1
                    ch.append(' '.join(rest[j+1:k])); j=k
                else: j+=1
            o['choices']=ch
        else:
            o['default']=d if d is not None else ''
        o['value']=o['default']
        opts.append(o)
    return opts
with gzip.open(CFG,'rt',encoding='utf-8') as f: engines=json.load(f)
have={e['name'] for e in engines}
added=[]
for name,folder,exe,proto,comment in NEW:
    if name in have: continue
    e={'name':name,'group':'oldies','command':exe,'working_folder':'engines/oldies/'+folder,'protocol':proto,'comment':comment}
    if proto=='uci':
        o=uci_options(os.path.join(OLD,folder),exe)
        if name.startswith('Rebel'):
            for x in o:
                if x['name']=='Ponder': x['value']=False
        e['options']=o
    engines.append(e); added.append((name,len(e.get('options',[]))))
if '--dry' in sys.argv:
    print(json.dumps(engines[-len(added):],indent=1,ensure_ascii=False)[:6000]); print(added); sys.exit()
shutil.copy(CFG,CFG+'.bak-'+time.strftime('%Y%m%d-%H%M%S'))
txt=json.dumps(engines,indent='\t',ensure_ascii=False,separators=(',',' : '))
with open(CFG,'wb') as raw:
    with gzip.GzipFile(filename='engines.json',mode='wb',fileobj=raw,compresslevel=9) as g: g.write(txt.encode('utf-8'))
print('wrote',len(engines),'entries; added:',added)
