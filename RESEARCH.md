# Retro Fun research notes (night of 5-6 Nov 2026)

Two questions: which early programmes are specified well enough to recreate
faithfully, and which existing engines of any era are quirky enough to be
worth a slot. Sections 1 and 3 are complete; sections 2 and 4 are filled in
below as the remaining research came back.

## 1. Built tonight (in this directory, Windows exes ready)

| engine | fidelity | notes |
|---|---|---|
| turochamp.exe | high | from Turing's 1953 text; reproduces his printed arithmetic |
| tech.exe | high | from Gillogly's thesis tables; endgame king terms approximated |
| bernstein.exe | good | all eight decision questions and the tree are specified; the material weight and some definitions are not |
| soma.exe | high | the New Scientist text gives every number except two minor rules |
| kotok.exe | fair | every eval weight and the plausible widths are in the thesis; the FORTRAN listing exists as an image-only scan for a future exact port |
| beal.exe | exact | the experiment is fully specified by one sentence |
| abramson.exe | high | Abramson's thesis gives the definition, the sampler pseudocode and the convergence schedule; the rules for ending a random chess game are his only silence |
| uct.exe | high | Kocsis and Szepesvári's algorithm and Ramanujan et al.'s chess settings (random playouts, -1/0/+1, exploration 0.4, 10,000 episodes); expansion and final-move details are standard choices the papers leave unsaid |
| eloworld.exe | exact | Murphy's player.cc, fate-player.cc and numeric-player.cc transcribed rule by rule, including his move order, tie-breaks and a live sampling bug; replays the paper's cccp game move for move (added 6 Nov 2026) |
| elofish.exe | exact | the 24 engine-backed Elo World entrants (Stockfish levels, stockfish1m, worstfish, 15 blends, Topple) driving Stockfish 10 and Topple 0.3.5 as children, Murphy's protocol (FEN reset, counters 10 10) reproduced (added 6 Nov 2026) |

Plus shannon.exe in ../shannon (v1.2).

## 2. Specifications of the 1950s programmes

Primary papers were obtained and read page by page (WJCC 1958, Scientific
American and Chess Review 1958 for Bernstein; Kotok's 1962 thesis scan;
Newell, Shaw and Simon in IBM J. R&D 1958; Stein and Ulam in Chess Review
1957). Copies are in the session scratch directory.

- **Bernstein 1957** (IBM 704): fully specified at the level of the eight questions, the seven-move table, the four-ply plain minimax, the four evaluation terms and piece values P1 N3 B3 R5 Q9, with the opening candidate set and two published games to test against. Unpublished: the material multiplier, the exact definition of a controlled square, move ordering within a question. Built tonight.
- **Kotok-McCarthy 1962** (IBM 7090, FORTRAN): plausible widths 4 3 2 2 1 1 1 1, alpha-beta, depth 8 or a stable position, and every evaluation weight are in the thesis, as are two 1962 game fragments and the four 1966-67 ITEP match games. The full source listing is in Appendix 1 of the thesis as a scan without text (about 100 pages); transcribing it would give an exact port. Built tonight from the description.
- **NSS 1958** (JOHNNIAC, IPL-IV): a goal-directed program with lexicographic vector values and "first acceptable move" choice. It never played a game and only two of the planned goals (material balance, centre control) were ever coded. A faithful recreation is not possible; anything beyond those two goals would be invented. Not built.
- **Los Alamos MANIAC I 1956**: 6x6 chess without bishops, two-move full-width search, material plus mobility with a pawn worth eight moves. Cannot play standard chess; Fairy-Stockfish has a "losalamos" variant if a 6x6 bracket is ever wanted. Not built.
- **Turochamp 1948** and **Tech 1971**: see section 1; both were built from primary sources (Turing's 1953 chapter, Gillogly's 1978 thesis).
- **SOMA 1961** (Maynard Smith, New Scientist): the scoring rules are quoted in full on the Chess Programming Wiki, including the worked example that fixes the swap-off arithmetic. Two rules (castling encouragement, pawn-advance threats) have no numbers. The famous 1961 hand-played game was SOMA against Michie's Machiavelli, drawn, not against Turochamp, which never played anyone but Glennie. Built tonight.
- **Machiavelli 1948** (Michie and Wylie): no rules survive beyond the remark that SOMA is similar to it. Not recreatable.

## 3. Existing quirky engines worth downloading

Verified by a research pass on 5 Nov 2026. "WB" means WinBoard protocol:
Arena and cutechess-cli speak it natively, Myracle may need Wb2Uci or
PolyGlot in between.

### Ranked for watchability

1. **Sargon 1978** (already in the field). Bill Forster's machine translation of the Z80 listing. UCI exe. https://github.com/billforsternz/retro-sargon/releases
2. **MAME dedicated computers as UCI**: Franz Huber's CB-Emu / MessChess (https://fhub.jimdofree.com/) and Sandro Ronco's mame-chessengine Lua plugin (https://github.com/sronco/mame-chessengine). Real Mephisto, Fidelity Chess Challenger, Novag and Saitek firmware. ROMs needed. No PV output; timing in engine-vs-engine play is the author's stated caveat.
3. **PyTuroChamp family**: Martin Doege's Python collection with Turochamp, Bernstein 1957, SOMA 1961, El Ajedrecista and a random mover, UCI via ptc_xboard.py (https://github.com/mdoege/PyTuroChamp); nimTUROCHAMP is a native binary (https://github.com/mdoege/nimTUROCHAMP). Useful as a second opinion against our C recreations.
4. **Elo World** (Tom Murphy VII, SIGBOVIK 2019, http://tom7.org/chess/weak.pdf): the zero-lookahead players are now `eloworld.exe` in this repo, one UCI option `Player` for all twenty-eight, including the six fate-based players (data table embedded), equalizer and the post-paper binary_pi / binary_e. The engine-backed entrants (stockfish0..20, stockfish1m, worstfish, the 15 blends, topple10k/1m) are `elofish.exe`, which drives Stockfish 10 and Topple 0.3.5. Not recreated: the `rational_pi` / `rational_e` players (arithmetic coding over big rationals), the letter and blind players (trained networks; blind's net.val is not in the repository), and chessmaster.nes_lv1/lv2 (NES emulation). With both exes the paper's 51-player field is 46 strong. **Anti-engines**: WorstFish logic (https://github.com/VisibleReality/lichess-worstfish, 50 lines over Stockfish MultiPV, needs a local UCI wrapper), BadFish (https://github.com/SocksPls/BadFish), Trollfish the draw-seeker (https://github.com/EthanJamesLew/Trollfish), worse-chess UCI adapter that plays the N-th worst line of any engine (https://github.com/ecrucru/worse-chess).
5. **DeepMind searchless_chess** (Ruoss et al. 2024, https://github.com/google-deepmind/searchless_chess): the 9M/136M/270M action-value transformers, no search at all. Installed 6 Nov 2026 as `oldies/searchless` (Windows launcher exes that run the JAX model inside WSL on the GPU; README.txt there). **Maia 1100 via lc0** with `go nodes 1`: pure human-move prediction at a target rating, so games look like club chess (https://github.com/CSSLab/maia-chess).
6. **Pastèque 0.0.0**: randomly initialised evaluation learned only from self-play, CCRL 362 (https://github.com/vshcherbyna/pasteque).
7. **POS 1.20**: 65 competing move-selection algorithms chosen by a genetic algorithm, native UCI, about 450 Elo, occasionally loses to a random mover (https://komputilo.nl/chess/pos/).
8. **Evaluation ablation bracket**: MinimalChess 0.2 material-only and 0.3 PST-only (https://github.com/lithander/MinimalChessEngine/releases), PeSTO piece-square-tables-only on a strong search (https://rofchade.nl/?page_id=7), Rune with per-term eval switches including capture-only movegen (https://github.com/Code-Sploit/Rune).
9. **TeXCCChess**: an LLM-written engine in pure TeX that searches at pdflatex compile time, about 1280 Elo, UCI via wrapper (https://github.com/acherm/agentic-chessengine-latex-TeXCCChess).
10. **Toledo Nanochess** (1257 characters of C, WB exe, https://nanochess.org/chess3.html) versus **micro-Max 4.8** (about 1950 characters, WB exe, https://home.hccnet.nl/h.g.muller/max.zip): a documented grudge match.

### Weird searches

- Rocinante 2.0 (Berliner's PB* best-first plus greedy MCTS, UCI, 1518) https://www.chessprogramming.org/Rocinante (Windows build unverified)
- Scorpio 2.8.5 with `montecarlo 1`: pre-neural MCTS with alpha-beta rollouts, WB https://github.com/dshawul/Scorpio/releases/tag/2.8.5
- N.E.G. 1.2 (H.G. Muller): zero look-ahead, moves chosen from static attack and defence counts, WB exe https://home.hccnet.nl/h.g.muller/NEG1_2.exe
- random_eval_bot: another Beal demonstrator, C++ UCI, build required https://github.com/koedem/random_eval_bot
- Genetic Chess (evolved eval genome, UCI and XBoard) https://github.com/MarkZH/Genetic_Chess
- micro-Max at `sd 1` or `sd 2` for a genuine one- or two-ply engine

### Weird implementations

- Sunfish, 111 lines of Python, native UCI https://github.com/thomasahle/sunfish
- TSCP 1.81, the didactic ancestor, WB exe http://www.tckerrigan.com/Chess/TSCP/
- PSChess in PostScript, runs on a network printer, needs a stdin shim https://github.com/nst/PSChess
- CoronaChess in Scheme (UCI) https://github.com/pmon/coronachess; PerlChess (UCI) https://github.com/greg-kennedy/PerlChess; Luciole in Lua (UCI) https://codeberg.org/rchastain/luciole; Barbarossa in Haskell (UCI, 2500) https://github.com/nionita/Barbarossa
- ChessBrainVB: 3100 Elo in Visual Basic 6, UCI exe https://github.com/RZulu54/ChessBrainVB
- IOCCC entries by Toledo 2005 and Paxson 1992, console only, adapter needed
- Rexigon, a "vibe-coded" WB engine, exe in repo https://github.com/silentshark-uk/rexigon

### Historic code running today

- Kaissa 1992 PC version, UCI port 2026 (descendant of the 1974 champion) https://github.com/jasper-sinclair/Kaissa
- CHEKMO-II (1970s PDP-8) under SIMH with a Python UCI adapter, Windows untested https://github.com/okalachev/chekmo
- UltraTalk drives the 1985 MSX UltraChess under openMSX https://github.com/folkertvanheusden/UltraTalk
- Microchess 1976 in Bill Forster's C port, console only, no UCI wrapper https://www.benlo.com/microchess/ForsterMicrochessC.zip
- Chess 0.5 (Frey and Atkin 1978) Python UCI by "Dirk" on the PicoChess group https://groups.google.com/g/picochess/c/7AhOnYxKoB4
- Mephisto and Rebel dedicated machines as UCI by Ed Schröder, site was down when checked https://rebel13.nl/dedicated/dedicated%20as%20uci.html

### Random movers and Elo anchors

chess-random-mover (UCI exe) https://github.com/ivankrylatskoe/chess-random-mover; Fairy-Random (UCI exe) https://github.com/Fulmene/Fairy-Random; Minic `Level 0` is a random mover and `Level 100` is 3660, one exe for the whole ladder https://github.com/tryingsomestuff/Minic; Iota 1.0, 1788 characters, never castles, CCRL 881 https://github.com/DanielWhite94/iota; Monchester, "strictly scholastic", CCRL 570 https://github.com/unserializable/monchester; EasyPeasy, weakest on CCRL at 196, bundled with Mayura ChessBoard http://www.mayura.com/chess/

Not found anywhere: conspiracy-number or proof-number engines for orthodox chess, UCI wrappers for Cyrus, Colossus, Chessmaster 2000 or Psion beyond what you already run.

## 4. Historic programmes that run today

Verified 5 Nov 2026. rebel13.nl was down all day (HTTP 526); its contents
were confirmed from Wayback captures.

### Ready to run (Windows binary exists)

1. **MessChess / CB-Emu Pro** (Franz Huber, https://fhub.jimdofree.com/, download via the linked cloud folder). One exe, `MessChess.exe <machine>`, speaks UCI and WinBoard, and covers about 500 dedicated computers: Fidelity Chess Challenger 1 to 10 and Elite, Mephisto MM I to V, Glasgow, Amsterdam, Dallas, Roma, Risc, Novag Constellation and Super Expert, Boris, SciSys Mark V, Saitek Sparc, Tasc King. Caveat: the machine plays at its own internal level and ignores the GUI clock, so pick a level that fits 12+8. No search info. ROMs bundled, legally grey. Alternative with your own ROMs: Sandro Ronco's mame-chessengine Lua plugin https://github.com/sronco/mame-chessengine (last touched for MAME 0.231).
2. **GNU Chess 4.0.pl77 Win32** (1992), WinBoard protocol, bundled as gnuchess.exe inside the old WinBoard installer https://ftp.gnu.org/old-gnu/xboard/winboard/winboard-4_2_7b.exe. Zero effort, authentic.
3. **Kaissa, 1992 PC version** of the 1974 world champion lineage. Jim Ablett WinBoard exe https://github.com/jimablett/Kaissa, UCI fork https://github.com/jasper-sinclair/Kaissa.
4. **Ken Thompson's PDP-11 Chess VI (1973)**, Belle's ancestor, Jim Ablett WinBoard exe https://github.com/jimablett/Chess-VI-Belle-11-1-73-ported-to-modern-systems-PDP-11-Chess- (about 200 nps, use a depth limit).
5. **CHEKMO-II** (John Comeau, PDP-8, 1970s), Jim Ablett WinBoard exe https://github.com/jimablett/Chekmo-II.
6. **Cray Blitz 49h (1990)**, Intel-Fortran build by Jim Ablett, WinBoard and UCI wrapper, links in https://talkchess.com/viewtopic.php?t=83496. Caveats: fixed seconds per move, restart the engine after every game, separate White and Black configs.
7. **Microchess 1976** as a WinBoard engine, Andre Adrian's wrapper of Bill Forster's C port: http://www.andreadrian.de/schach/microchessw.exe. Mishandles castling, en passant and promotion, so expect illegal-move losses; ignores the clock; about 550 Elo.
8. **Mephisto Rebel 5.0, MM IV, MM V, Amsterdam, Dallas, Roma as UCI** (Ed Schröder's messtiny, with Richard Lang's permission): https://rebel13.nl/dedicated/dedicated%20as%20uci.html when the site returns. Also native Windows Gideon Pro 1993, Rebel 6.0 1994 and Rebel Century as WinBoard and UCI from https://rebel13.nl/windows/rebel%27s%20with%20uci.html. Your Psion 1985 is one of Lang's 68000 programs, so these are its siblings.
9. Slightly late for "retro" but ready WinBoard exes: TSCP 1.81 (1997), Faile 1.4.4, Phalanx XXII, MiniMAX (the 1984 Steinwender and Donninger book program). **Crafty 11.13 (1996)**, Hyatt's Cray Blitz descendant and the version of Crafty's first WMCCC, is done: every release from 8.11 (1995) survives verbatim at https://github.com/lazydroid/crafty-chess (one commit per version; nothing between 11.13 and 15.17), and 11.13 builds with mingw from its own NT_i386 target plus a few protocol-2 patches (`oldies/crafty1113/BUILD.txt`). 8.22 (1995) is the stretch goal: it pre-dates the `xboard` command.

### Real code that needs a bridge

- **Chess 4.6 and 4.9** (Slate and Atkin): the actual CDC binaries run under the DtCyber emulator with NOS 2.8.7 (https://github.com/kej715/DtCyber, `node install-product chess49`), or live at http://www.nostalgiccomputing.org/. Terminal only, so a telnet-scraping bridge would be needed. The complete Chess 4.6 COMPASS source listing is at the Computer History Museum.
- **Mac Hack VI**: see section 5.
- **Chess 0.5** (Atkin and Frey, BYTE 1978): Pascal source at https://www.moorecad.com/standardpascal/Chess05.pas, GNU Pascal Windows console exe http://www.andreadrian.de/schach/chess05gnu.exe with descriptive-notation input, no protocol, node limit only. A small adapter would do it.
- **Duchess** (Duke, PL/I) runs on Hercules with MVS 3.8J, 3270 terminal only.
- **GNU Chess 1.2 (1987)** builds with MinGW from https://github.com/neozeed/gnuchess-87 but predates xboard, console only.

### DOS commercial programs

Sargon III, IV and V, Chessmaster 2000, 2100 and 3000, Cyrus, Colossus X, ChessGenius 3 (official freeware), HIARCS 1.0 (official free), Rebel 6 to 12 and Decade all run in DOSBox but none speak UCI or WinBoard, and no DOSBox-to-UCI scraper exists. The only historical path is Donninger's Auto232 serial protocol through com0com and a 20-year-old adapter. Realistic only as a hobby project; the emulated dedicated machines above are the easier way to get Rebel, Gideon and Genius into the field.

### Lost

Nuchess, Coko, Chaos, Ribbit, Ostrich, Bebe, J.Biit, the Dartmouth and Daly programs: descriptions only, no code anywhere. Kotok-McCarthy survives only as a thesis. Belle was hardware.

## 5. Mac Hack VI (Greenblatt, 1967-80), from earlier in the day

No recreation from the assembler exists. The 1980 MIDAS source (ocm.470, 10,600 lines plus 8,100 in ocaux.224) survives in https://github.com/PDP-10/its/tree/master/src/chprog and runs under ITS emulation. A Python "simulation" written by an AI from the paper was posted to the PicoChess group in July 2025. Recommended route if wanted: a user-mode PDP-10 interpreter in C with the assembled OCM image embedded, ITS teletype calls stubbed and a UCI shim speaking descriptive notation. Several sessions of work; the ITS build is the risky step.

## 6. Round-robin of the recreations, 10s+0.1, Windows exes under fastchess

42 games, all terminated normally (no time losses, no illegal moves).

| engine | points | W-D-L |
|---|---|---|
| Tech 1971 | 12/12 | 12-0-0 |
| Turochamp 1948 | 10/12 | 10-0-2 |
| Bernstein 1957 | 7/12 | 6-2-4 |
| Shannon 1950 Type B (Width 4) | 6.5/12 | 6-1-5 |
| SOMA 1961 | 4.5/12 | 4-1-7 |
| Beal 1994 | 1/12 | 0-2-10 |
| Kotok-McCarthy 1962 | 1/12 | 0-2-10 |

Tech is the only one with real search depth and wins everything, as
Gillogly would have predicted. Turochamp's two plies plus capture
extension beat the four-ply Bernstein and the Type B Shannon; Turing's
position-play is better chess knowledge than it looks. Kotok's low finish
reflects the "stable position" cut, which makes it effectively a two-ply
engine over four candidate moves; the original was weak too (it lost 3-1
to ITEP) but this recreation is from the thesis description and deserves a
pass over the FORTRAN listing before being trusted.
