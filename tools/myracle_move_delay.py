#!/usr/bin/env python3
"""Slow engines down in Myracle so games can be watched.  Run with Myracle CLOSED.

    python3 tools/myracle_move_delay.py MS [name-prefix ...]

Two mechanisms, chosen per entry:

  * Our own engines (engines/retro, and the Searchless wrappers) have a
    MoveDelay option: they never answer sooner than MS ms after "go".  The
    option is added to every such entry's stored option list (default 0, so
    Myracle does not complain that defaults changed) and set to MS for the
    entries whose name starts with one of the prefixes.

  * Third-party engines get the slow.exe proxy (tools/slow.c): it is copied
    into the engine's folder with a slow.txt naming the real command and the
    delay, and the entry's command is pointed at slow.exe.  An entry already
    on slow.exe just gets its slow.txt delay rewritten.

Prefixes are matched case-insensitively against the start of the entry name
("Elo World", "SOMA", "Fairy", "Random Mover", "Searchless").  MS 0 restores
instant replies (option 0 / proxy delay 0; the proxy stays in place).

Example:  python3 tools/myracle_move_delay.py 2000 "Elo World" SOMA Searchless Fairy "Random Mover"
"""
import gzip, json, os, re, shutil, sys, time

MYRACLE = '/mnt/d/myracle'
CFG = MYRACLE + '/config/engines.json.gz'
SLOW_EXE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'slow.exe')
OURS = ('engines/retro', 'engines/oldies/searchless')

if len(sys.argv) < 2 or not sys.argv[1].lstrip('-').isdigit():
    sys.exit(__doc__)
ms = int(sys.argv[1]); prefixes = [p.lower() for p in sys.argv[2:]]

raw = gzip.open(CFG, 'rt', encoding='utf-8').read()
raw = re.sub(r'\\x([0-9a-fA-F]{2})', lambda m: chr(int(m.group(1), 16)), raw)
eng = json.loads(raw)

def wanted(e): return any(e['name'].lower().startswith(p) for p in prefixes)
def ours(e): return e.get('working_folder', '').replace('\\', '/').rstrip('/') in OURS

changed = 0
for e in eng:
    opts = e.setdefault('options', [])
    if ours(e):
        md = next((o for o in opts if o['name'] == 'MoveDelay'), None)
        if md is None:
            md = {'name': 'MoveDelay', 'type': 'spin', 'default': 0, 'min': 0, 'max': 60000, 'value': 0}
            i = next((k for k, o in enumerate(opts) if o['name'] == 'LogFile'), len(opts))
            opts.insert(i, md); changed += 1
            print('option added  ', e['name'])
        if wanted(e) and md['value'] != ms:
            md['value'] = ms; changed += 1
            print(f'MoveDelay={ms:<5}', e['name'])
    elif wanted(e):
        folder = os.path.join(MYRACLE, e['working_folder'].replace('\\', '/'))
        if not os.path.isdir(folder):
            print('no folder for ', e['name'], folder); continue
        real = e['command']
        if real.lower() == 'slow.exe':   # already wrapped: keep the command from slow.txt
            with open(os.path.join(folder, 'slow.txt')) as f: real = f.readline().strip()
        else:
            e['command'] = 'slow.exe'; changed += 1
        if not os.path.exists(os.path.join(folder, 'slow.exe')):
            shutil.copy(SLOW_EXE, os.path.join(folder, 'slow.exe'))
        with open(os.path.join(folder, 'slow.txt'), 'w', newline='\r\n') as f:
            f.write(f'{real}\n{ms}\n')
        print(f'slow.exe {ms:<5}  ', e['name'], '->', real)

if not changed:
    print('nothing to change'); sys.exit()
shutil.copy(CFG, CFG + '.bak-' + time.strftime('%Y%m%d-%H%M%S'))
txt = json.dumps(eng, indent='\t', ensure_ascii=True, separators=(',', ' : '))
with open(CFG, 'wb') as f:
    with gzip.GzipFile(filename='engines.json', mode='wb', fileobj=f, compresslevel=9) as g:
        g.write(txt.encode())
print('written', len(eng), 'entries,', changed, 'changes')
