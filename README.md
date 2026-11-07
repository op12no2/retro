# retro

Faithful recreations of the earliest chess programmes, 1948 to 1971, as
UCI engines, plus two later experiments that belong with them in spirit.
Each engine is one C file written from the original paper or thesis, built
as a single static Windows or Linux executable with no dependencies.

| engine | year | authors | what it is |
|---|---|---|---|
| turochamp | 1948 | Alan Turing, David Champernowne | the "paper machine", never run on a computer in Turing's lifetime |
| shannon | 1950 | Claude E. Shannon | the paper that founded computer chess; Type A and Type B strategies |
| bernstein | 1957 | Alex Bernstein, M. de V. Roberts, T. Arbuckle, M. Belsky | the first complete chess program, IBM 704 |
| soma | 1961 | John Maynard Smith | the Smith One-Move Analyzer, designed by an evolutionary biologist |
| kotok | 1962 | Alan Kotok, John McCarthy and the MIT chess group | one of the first programs with alpha-beta; lost 3-1 to ITEP in 1967 |
| tech | 1971 | James Gillogly | the "technology" program: all search, no knowledge |
| abramson | 1987 | Bruce Abramson | the expected-outcome sampler: random games to the end decide the move, the first random-playout evaluation |
| beal | 1994 | Don Beal, Martin Smith | alpha-beta over a purely random evaluation |
| uct | 2006 | Levente Kocsis, Csaba Szepesvári | Monte Carlo tree search with the UCB1 bandit rule and random playouts, as Ramanujan, Sabharwal and Selman ran it on chess |
| eloworld | 2019 | Tom Murphy VII | the zero-lookahead players of "Elo World" (24 from the paper, 4 from his repository): random, alphabetical, cccp, pacifist, suicide king, the fate-based players and friends |
| elofish | 2019 | Tom Murphy VII | the engine-backed Elo World entrants: Stockfish 10 at skill levels 0 to 20, stockfish1m, worstfish, the fifteen random blends and Topple; drives the real engines, which sit beside it |

Binaries are on the Releases page. Build from source with `./build.sh`
(needs gcc and `x86_64-w64-mingw32-gcc`), or `./build.sh tech` for one
engine. Every engine honours `go wtime/btime/winc/binc`, `movetime`,
`depth`, `nodes`, `infinite` and `stop`, reports depth, score, nodes, nps
and principal variation, and has a `LogFile` option that mirrors the UCI
traffic to a file and a `MoveDelay` option (milliseconds) that holds the
reply so the search-free ones can be watched at a human pace. Console commands `perft N`, `eval` and `d` work in all of
them, and most have one more that shows the engine's own reasoning.

## How faithful?

Each source file begins with a comment naming its sources and listing every
judgement call as `[choice]`. The rule was: what the paper says, the engine
does; where the paper is silent, choose the simplest reading and say so.
Where a recreation could be checked against the original's own numbers, it
was:

- **Turochamp** replays Turing's 1952 game against Alick Glennie and
  reproduces the position-play increases Turing printed in 1953 for moves
  1 to 9 and 18 exactly. Four later values differ by 0.2 to 0.7, which fits
  Champernowne's recollection that the hand arithmetic was "a bit
  slapdash". The gap at 18.O-O-O was exactly 2.0 until the castling rule was
  read Turing's way: a castled king keeps all three castling points.
- **Bernstein**'s plausible move table from the start position is the seven
  moves he listed in Chess Review: four knight moves, P-K4, P-K3, P-Q4.
- **SOMA** gives the article's opening moves +8 each and, in the position
  from its 1961 game, finds the Bh2+ sacrifice the authors say it would have
  played. The article's own counting rules also give 1.d4 eight points,
  which the authors missed.
- **Tech**'s root heuristics are the thesis's tables to the point: centre
  control array, piece priorities per phase, P-K4 +30, O-O +30, N-R3 -15,
  piece blocking a centre pawn -50, and so on.
- **Elo World** replays the paper's Figure 6 game, cccp against suicide_king,
  and picks every one of cccp's seven moves (1.d4, 2.Bxg5, 3.Bxe7, 4.d5,
  5.dxc6+, 6.Qd6+, 7.e4#); in the Figure 4 game each of swarm's six moves
  up to 6.Qh5# has the lowest penalty on the board, as it must since swarm
  breaks ties at random. single_player finds the Scholar's mate from the
  start position (e4, Qf3, Bc4, Qxf7, with Black passing), as Murphy's did.
- **Kotok-McCarthy** uses every weight the thesis gives, but is built from
  the thesis's description rather than its FORTRAN listing, which survives
  only as an image scan. It opens 1.Nc3 where the original opened 1.e4, so
  treat it as provisional until someone transcribes the listing.

A round-robin at 10s+0.1 among the seven, 42 games, all finished normally:

| engine | points |
|---|---|
| Tech 1971 | 12/12 |
| Turochamp 1948 | 10/12 |
| Bernstein 1957 | 7/12 |
| Shannon 1950 Type B | 6.5/12 |
| SOMA 1961 | 4.5/12 |
| Beal 1994 | 1/12 |
| Kotok-McCarthy 1962 | 1/12 |

Tech is the only one with real search depth and wins everything, which is
Gillogly's thesis in one line. That Turing's two plies beat Bernstein's four
says his position-play was better chess knowledge than it looks.

## The engines

### turochamp (1948)

Turing and Champernowne worked out the rules in 1948 and Turing described
them in the chess section of his chapter in Bowden's
[*Faster Than Thought*](https://archive.org/details/fasterthanthough00bvbo)
(1953, pp. 288-295). Material is P 1, Kt 3,
B 3.5, R 5, Q 10, valued as the ratio W/B, "better than W - B as the latter
does not encourage exchanges when one has the advantage". Every move and
every reply is "considerable"; beyond that only recaptures, captures of
undefended pieces, captures of a higher-valued piece by a lower one, and
mates are followed. A position with none of those is "dead" and evaluated.
Among materially equal moves the one with the greatest position-play wins,
computed for the machine's own pieces only: square roots of mobility,
credits for defended pieces, the king's vulnerability measured as if it were
a queen, castling points, pawn advance and defence, 1.0 for threatening mate
and 0.5 for check.

The 1952 game against Glennie took Turing half an hour a move by hand and
was lost in 29 moves; Turing's own footnotes read "Most inappropriate
moves", "Heads in the sand!" and "Fiddling while Rome burns!". The engine
plays 1.e3 rather than Turing's 1.e4, as the 2004 ChessBase reconstruction
also did: by his own rules e3 scores 4.4 to e4's 4.2, and Turing's asterisk
claiming e4 was best is a slip. Console command `pp` lists the position-play
of every root move. Options `Depth` (2) and `FollowChecks` (off; the 1953
text does not list checks, though Champernowne remembered following them).

### shannon (1950)

Shannon's [paper](https://vision.unipv.it/IA1/ProgrammingaComputerforPlayingChess.pdf)
(also as [plain text](https://www.pi.infn.it/~carosi/chess/shannon.txt))
proposed the evaluation everyone quotes, 900 Q + 500 R +
300 (B+N) + 100 P, minus 50 per doubled, backward or isolated pawn, plus 10
per legal move of mobility, and two strategies. Type A searches every move
to a fixed depth, continuing through exchanges until the position is
quiescent. Type B uses a "plausible move generator" to look only at forcing
and promising moves, deeper along forcing lines. Alpha-beta came six years
later, so by default this engine is plain minimax; `AlphaBeta=true` shows
what the same evaluation does with it. `Type` selects A or B; `Width`,
`Extensions` and `QuiescenceDepth` tune Type B. Without alpha-beta the
capture search explodes in open positions, which is why the quiescence cap
defaults to 4. Shannon's mobility term, incidentally, is enough to box in a
bare king, as he pointed out himself.

### bernstein (1957)

The first program to play complete games of chess, on an IBM 704 at eight
minutes a move, described in three 1958 articles: the
[WJCC paper](https://d1yx3ys82bpsa0.cloudfront.net/chess/a-chess-playing-program-for-the-ibm-704.bernstein-roberts-arbuckle-belsky.062303011.pdf),
[Scientific American](https://d1yx3ys82bpsa0.cloudfront.net/chess/computer-v-chessplayer.bernstein-roberts.scientific-american.june-1958.062303059.pdf)
and [Chess Review](https://d1yx3ys82bpsa0.cloudfront.net/chess/chess-review.a-chess-playing-program-for-the-ibm-704.bernstein.july-1958.062303058.pdf). At every node it fills a plausible move table of at most
seven moves by asking eight questions in order: is the king in check? can
material be gained, saved or exchanged? can we castle (if so, castle and
look no further)? develop a minor piece? occupy a key square? an open file?
move a pawn (P-K4, P-K3, P-Q4 first)? move anything? The tree is seven wide
and four half-moves deep, 2,800 positions, plain minimax. McCarthy
suggested alpha-beta to Bernstein, who "was unconvinced". Each side's score
is mobility plus controlled squares plus defended squares round its king
plus material times "a large factor" that was never published (option
`MaterialFactor`, default 20), and the position is valued by the ratio of
the two scores. Bernstein's own diagnosis, that the program moves attacked
pieces away rather than defending them, shows in its games. Console
command `table`. Options `Depth`, `TableSize`, `MaterialFactor`,
`AlphaBeta`, `Conservative` (his two-minute mode).

### soma (1961)

John Maynard Smith, the evolutionary biologist, designed SOMA and
published it with Donald Michie in New Scientist (9 November 1961); the
whole SOMA section is reproduced on the
[Chess Programming Wiki](https://www.chessprogramming.org/SOMA). It looks one move ahead:
every legal move is scored by the resulting position and the best played,
ties "by tossing a coin". Material P 10, Kt and B 30, R 50, Q 90, K 1000;
one point for every square a piece attacks, two for the centre, three next
to the enemy king; and swap-off values for pieces en prise, combined so
that one enemy piece en prise is worth +5 "for the initiative", two or more
are worth the second highest plus 5 for every other, since the opponent
will save the most valuable. Checks and forks fall out of the king counting
as a piece en prise worth 1000. Its one recorded game was against Michie's
Machiavelli, played by hand over a weekend with Maynard Smith's teenage son
as referee, and drawn on move 29 with "combatants exhausted". Console
command `soma`. Options `Depth`, `CastlingBonus`, `PawnThreatPenalty`.

### kotok (1962)

Alan Kotok's [MIT bachelor's thesis](https://dspace.mit.edu/handle/1721.1/17406)
(1962, also AI Memo 41), supervised by John McCarthy, describes
a Shannon type B program for the IBM 7090: at each ply the moves are
ordered by a one-ply evaluation and only the best few kept, "4 3 2 2 1 1 1 1"
by ply, with alpha-beta, ending at depth 8 or at a "stable position". The
evaluation weights are the thesis's: material in units of 1/60 pawn, centre
control of 16 squares with a printed value table faded out between moves
20 and 30, development at 1/15 pawn, pawn structure at 1/20 pawn. McCarthy
later improved it at Stanford and played the ITEP program by telegraph in
1966-67, losing 3-1; ITEP became Kaissa. Console command `plaus`. Options
`Depth`, `Widths`.

### tech (1971)

Gillogly built Tech at Carnegie Mellon to measure how far raw search alone
gets you, described in
[*Artificial Intelligence* 3 (1972)](https://doi.org/10.1016/0004-3702(72)90045-8)
and in full in his 1978 PhD thesis
[*Performance Analysis of the Technology Chess Program*](http://web.archive.org/web/2019id_/http://reports-archive.adm.cs.cmu.edu/anon/anon/usr/ftp/scan/CMU-CS-77-gillogly.pdf)
(CMU-CS-78-189): full-width alpha-beta, leaves scored by material only, every
capture chain followed to quiescence, no positional knowledge in the tree.
The one concession is a positional pre-sort of the root moves from
heuristics by Hans Berliner, and because the search accepts a later move
only when strictly better on material, positional merit acts purely as a
tie-break. On a 1971 PDP-10 it managed about five plies and was rated USCF
1243; Gillogly measured about 80 rating points per doubling of time and
called it the technology curve. Here it reaches eight to ten plies. Console
command `pos`. Option `Depth`.

### abramson (1987)

Bruce Abramson's PhD thesis (*The Expected-Outcome Model of Two-Player
Games*, Columbia 1987, Morgan Kaufmann 1991; the short version is
"Expected-Outcome: A General Model of Static Evaluation", *IEEE PAMI*
1990) proposes that the right value of a position is the expected result
of the game under random play from it, win 1, loss 0, draw a half, and
estimates it the obvious way: play random games to the end and count. His
appendix B sampler starts with 16 games and doubles until two successive
estimates, projected as wins out of 1024 games, agree within a tolerance
(0, 16, 24, 20, 18 and 9 for the passes up to 1024). With no lookahead at
all, sampling the board after each legal move, it beat the expert Othello
evaluator 48-2 in 1987 at one to ten minutes a move; for chess he used the
same sampler to learn piece values from random games (P 4.0, R 4.8, N 2.5,
B 3.3, Q 9.6, the pawn carrying its promotion) but found it too slow to
play. This engine plays it: Abramson's protocol, round-robin over the root
moves so an interrupted search is fair to all of them, pawns promoting to
queens as his sampler did. At three thousand random games a second the
stopping rule is satisfied in a second or two; `Converge=false` with
`MaxSamples=0` samples until the clock instead, which is how it improves
with time. `eo` on the console prints every move's estimate. Monte Carlo
tree search descends from this.

### beal (1994)

Beal and Smith ("Random Evaluations in Chess", *ICCA Journal* 17(1),
1994; see the [Chess Programming Wiki](https://www.chessprogramming.org/Don_Beal))
replaced the evaluation with a random number and found the program still
played recognisable chess. The side with more legal moves
gets more random draws to choose the maximum from, so the search implicitly
rewards mobility and avoids losing pieces. Nothing in this engine knows
anything about chess beyond the rules. Options `Range`, `Quiescence`.

### uct (2006)

Kocsis and Szepesvári's "Bandit based Monte-Carlo Planning" (ECML 2006)
turned random playouts into a tree search by treating every node as a
multi-armed bandit: descend choosing the child with the best upper
confidence bound, mean reward plus 2 C<sub>p</sub> sqrt(ln t / s), play one
random game from the first position outside the tree, back the result up,
repeat until time is up, then play the move with the highest average
reward. The recreation follows their Figure 1 and the chess settings of
Ramanujan, Sabharwal and Selman (ICAPS 2010, UAI 2010), who ran UCT with
uniformly random playouts, rewards -1, 0, +1 and an exploration constant
of 0.4 found best empirically, to show why it failed at chess: with
10,000 episodes it beat a two-ply random-rollout minimax 74% of the time
but could not see the five- and seven-move "shallow traps" that chess is
full of. Options `Exploration` (hundredths), `Episodes` (the budget when
there is no clock, 10,000 as in the paper) and `ClipPlies`; one new node
per episode, children tried in random order before the bound takes over,
the tree discarded between moves, mate and stalemate exact. `uct` on the
console runs 2000 episodes and lists the root moves with visits and means.

### eloworld (2019)

Tom Murphy VII's ["Elo World, a framework for benchmarking weak chess
engines"](http://tom7.org/chess/weak.pdf) (SIGBOVIK 2019, with a
[video](https://www.youtube.com/watch?v=DpXy041BIlA)) played dozens of
deliberately silly engines against one another to extend the Elo scale down
to the worst possible play. This engine recreates his zero-lookahead
players from the source in his
[svn repository](https://sourceforge.net/p/tom7misc/svn/HEAD/tree/trunk/chess/)
(player.cc, fate-player.cc, numeric-player.cc, chess.cc). Twenty-four of
them have Elo rows in the paper, single_player is described there without
one, and alphamove, binary_pi and binary_e were added to his repository
later. The UCI option `Player` chooses one of the twenty-eight: `random_move`, `first_move` (lowest
move code), `alphabetical` (first in short algebraic notation, so 1.Na3:
capitals sort before pawn moves), `alphamove` (the alphabetical median,
after xkcd 3045), `cccp` (Checkmate, Check, Capture, Push), `pacifist`,
`min_oppt_moves`, `suicide_king`, `reverse_starting`, `huddle`, `swarm`,
`generous`, `no_i_insist`, `same_color`, `opposite_color`, `sym_mirror_y`,
`sym_mirror_x`, `sym_180`, `single_player` (looks `Depth` moves ahead,
default 4, as if the opponent never moved and plays the first move of the
shortest mate it finds, otherwise a random move), the six fate-based
players `safe`, `dangerous`, `popular`, `rare`, `survivalist` and
`fatalist` (which steer each piece by the probability, over every lichess
game to November 2018, of it ending the game alive or captured on a given
square; the table is `eloworld_data.h`), `equalizer` (move the piece
moved least to the square visited least) and `binary_pi` / `binary_e`
(index the alphabetically sorted moves with the next bits of the
constant). The source header describes each rule and every quirk kept from
Murphy's code: his castling-rook piece type, the colour bit in his
promotion codes, a comparator that calls a knight capture and a bishop
capture "different but neither better", a "forced gift" tier in
no_i_insist that can never fire, and a shadowed variable in the fate
players that turns safe, dangerous, popular and rare from weighted-random
samplers into "the first move in generation order that is not a worst
move". That last one was in the code the paper's tournament ran, and the
paper's Elo for those four (349 to 361) sits next to first_move rather
than next to the random movers, so it is reproduced. Moves are generated
in his order wherever that order decides a tie. Paper Elo: pacifist 287,
alphabetical 321, generous 343, popular 349, dangerous 350, safe 351,
first_move 358, rare 361, no_i_insist 378, huddle 400, sym_180 430,
same_color 431, opposite_color 432, sym_mirror_x 439, survivalist 439,
random_move 439, fatalist 440, sym_mirror_y 443, reverse_starting 446,
suicide_king 447, equalizer 504, swarm 534, cccp 554, min_oppt_moves 597.
Console command `elo [player]` lists the legal moves in Murphy's order
with each one's penalty, score, code or alphabetical rank and marks the
choice.

### elofish (2019)

The rest of the Elo World field: the entrants that call a real engine. The
UCI option `Player` picks `stockfish0`, `stockfish5`, `stockfish10`,
`stockfish15`, `stockfish20` (Stockfish at that Skill Level, asked for a
move with a bare `go`, which Stockfish treats as time management with no
time and answers in milliseconds, hence the paper's "basically
instantaneously"), `stockfish1m` (Skill Level 20, a million nodes),
`worstfish` (asks Stockfish how much the opponent likes the position after
each legal move and plays the one it likes best), the fifteen
`stockfish1m_rNNN` blends (a 16-bit random number below NNN means a random
move, otherwise stockfish1m's; NNN runs 64, 128, ... 32768, 49152, 57344,
61440, 63488, 64512) and `topple10k` / `topple1m` (Topple with a node
budget). As in Murphy's code the engine is reset before every move with a
FEN whose move counters read 10 and 10 and no history, so it cannot see
repetitions, and no book, tablebase or hash clearing is used. The paper
calls worstfish "configured as stockfish0" but the code, then and now, uses
level 20; the code is followed and `WorstfishLevel` lets you choose. The
exe needs `stockfish_10_x64.exe` and `Topple_v0.3.5_x86_64.exe` beside it
(options `StockfishPath`, `TopplePath`); Stockfish 10 is built from the
`sf_10` tag of the official repository with
`make build ARCH=x86-64 COMP=mingw CXX=x86_64-w64-mingw32-g++`, Topple
0.3.5 is Vincent Tang's release binary (Murphy ran the znver1 build on his
Threadripper). Paper Elo: worstfish 189, r64512 463, r63488 482, r61440
521, r57344 601, r49152 752, r32768 976, r16384 1278, stockfish0 1336,
stockfish5 1645, r8192 1690, topple10k 1772, stockfish10 1915, stockfish15
1953, r4096 2020, stockfish20 2139, topple1m 2219, r2048 2262, r1024 2426,
r512 2522, r256 2582, r128 2609, r64 2638, stockfish1m 2644. Not
recreated from the paper: chessmaster.nes_lv1 and lv2 (an emulated NES
Chessmaster) and the three blind players (their network file is not in the
repository). Console command `fish` shows the FEN sent and the engine's
answer.

## Related reading

`RESEARCH.md` holds the survey behind this collection: which other early
programmes could be recreated and which cannot (NSS never played a game;
no rules of Machiavelli survive; Los Alamos was 6x6 chess), where the
surviving code of Mac Hack VI, Chess 4.6, Cray Blitz and Tech II lives,
which historic programs already run on Windows today, and a list of
genuinely odd modern engines for a tournament that values character over
strength.

## Sources

| engine | primary source | more |
|---|---|---|
| turochamp | Turing, "Chess", in Bowden (ed.), [*Faster Than Thought*](https://archive.org/details/fasterthanthough00bvbo), 1953 | [CPW](https://www.chessprogramming.org/Turochamp) |
| shannon | Shannon, [*Programming a Computer for Playing Chess*](https://vision.unipv.it/IA1/ProgrammingaComputerforPlayingChess.pdf), Phil. Mag. 1950 | [CPW](https://www.chessprogramming.org/Claude_Shannon) |
| bernstein | Bernstein et al., [WJCC 1958](https://d1yx3ys82bpsa0.cloudfront.net/chess/a-chess-playing-program-for-the-ibm-704.bernstein-roberts-arbuckle-belsky.062303011.pdf); Bernstein and Roberts, [Sci. Am. June 1958](https://d1yx3ys82bpsa0.cloudfront.net/chess/computer-v-chessplayer.bernstein-roberts.scientific-american.june-1958.062303059.pdf); Bernstein, [Chess Review July 1958](https://d1yx3ys82bpsa0.cloudfront.net/chess/chess-review.a-chess-playing-program-for-the-ibm-704.bernstein.july-1958.062303058.pdf) | [CPW](https://www.chessprogramming.org/The_Bernstein_Chess_Program) |
| soma | Maynard Smith and Michie, "Machines that play games", New Scientist, 9 Nov 1961 | [CPW](https://www.chessprogramming.org/SOMA) (text reproduced) |
| kotok | Kotok, [*A Chess Playing Program for the IBM 7090 Computer*](https://dspace.mit.edu/handle/1721.1/17406), MIT 1962 | [CPW](https://www.chessprogramming.org/Kotok-McCarthy-Program), [ITEP match](https://www.chessprogramming.org/Stanford-ITEP_Match) |
| tech | Gillogly, [AI 3 (1972)](https://doi.org/10.1016/0004-3702(72)90045-8); [PhD thesis 1978](http://web.archive.org/web/2019id_/http://reports-archive.adm.cs.cmu.edu/anon/anon/usr/ftp/scan/CMU-CS-77-gillogly.pdf) | [CPW](https://www.chessprogramming.org/Tech) |
| abramson | Abramson, [*The Expected-Outcome Model of Two-Player Games*](https://academiccommons.columbia.edu/doi/10.7916/D8GH9S2C), Columbia CUCS-315-87, 1987; "Expected-Outcome: A General Model of Static Evaluation", IEEE PAMI 12(2), 1990 | [CPW](https://www.chessprogramming.org/Bruce_Abramson) |
| beal | Beal and Smith, "Random Evaluations in Chess", ICCA Journal 17(1), 1994 | [CPW](https://www.chessprogramming.org/Don_Beal) |
| uct | Kocsis and Szepesvári, "Bandit based Monte-Carlo Planning", ECML 2006; Ramanujan, Sabharwal and Selman, [ICAPS 2010](https://ojs.aaai.org/index.php/ICAPS/article/view/13437) and [UAI 2010](https://arxiv.org/abs/1203.4011) | [CPW](https://www.chessprogramming.org/UCT) |
| eloworld | Murphy, [*Elo World*](http://tom7.org/chess/weak.pdf), SIGBOVIK 2019; [source](https://sourceforge.net/p/tom7misc/svn/HEAD/tree/trunk/chess/) | [video](https://www.youtube.com/watch?v=DpXy041BIlA), [CPW](https://www.chessprogramming.org/Tom_Murphy_VII) |
| elofish | as eloworld (stockfish.cc, stockfish-player.cc, uci-player.cc); [Stockfish 10](https://github.com/official-stockfish/Stockfish/tree/sf_10), [Topple 0.3.5](https://github.com/konsolas/ToppleChess/releases/tag/v0.3.5) | |

## Credits

Written by Claude (Anthropic) for op12no2's "Retro Fun" engine
tournament, November 2026, from the primary sources named in each file.
The original programmes belong to their authors; these are recreations and
say so in their UCI `id author` strings.
