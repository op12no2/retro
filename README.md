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
| beal | 1994 | Don Beal, Martin Smith | alpha-beta over a purely random evaluation |
| eloworld | 2019 | Tom Murphy VII | the zero-lookahead players of "Elo World": random, alphabetical, cccp, pacifist, suicide king and friends |

Binaries are on the Releases page. Build from source with `./build.sh`
(needs gcc and `x86_64-w64-mingw32-gcc`), or `./build.sh tech` for one
engine. Every engine honours `go wtime/btime/winc/binc`, `movetime`,
`depth`, `nodes`, `infinite` and `stop`, reports depth, score, nodes, nps
and principal variation, and has a `LogFile` option that mirrors the UCI
traffic to a file. Console commands `perft N`, `eval` and `d` work in all of
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

### beal (1994)

Beal and Smith ("Random Evaluations in Chess", *ICCA Journal* 17(1),
1994; see the [Chess Programming Wiki](https://www.chessprogramming.org/Don_Beal))
replaced the evaluation with a random number and found the program still
played recognisable chess. The side with more legal moves
gets more random draws to choose the maximum from, so the search implicitly
rewards mobility and avoids losing pieces. Nothing in this engine knows
anything about chess beyond the rules. Options `Range`, `Quiescence`.

### eloworld (2019)

Tom Murphy VII's ["Elo World, a framework for benchmarking weak chess
engines"](http://tom7.org/chess/weak.pdf) (SIGBOVIK 2019, with a
[video](https://www.youtube.com/watch?v=DpXy041BIlA)) played dozens of
deliberately silly engines against one another to extend the Elo scale down
to the worst possible play. This engine recreates his zero-lookahead
players from the source in his
[svn repository](https://sourceforge.net/p/tom7misc/svn/HEAD/tree/trunk/chess/)
(player.cc, player-util.cc, chess.cc). The UCI option `Player` chooses one:
`random_move`, `first_move` (lowest move code), `alphabetical` (first in
short algebraic notation, so 1.Na3: capitals sort before pawn moves),
`alphamove` (the alphabetical median, after xkcd 3045), `cccp` (Checkmate,
Check, Capture, Push), `pacifist`, `min_oppt_moves`, `suicide_king`,
`reverse_starting`, `huddle`, `swarm`, `generous`, `no_i_insist`,
`same_color`, `opposite_color`, `sym_mirror_y`, `sym_mirror_x`, `sym_180`
and `single_player`, which looks `Depth` moves ahead (default 4) as if the
opponent never moved and plays the first move of the shortest mate it
finds, otherwise a random move. The source header describes each rule and
every quirk kept from Murphy's code: his castling-rook piece type, the
colour bit in his promotion codes, a comparator that calls a knight capture
and a bishop capture "different but neither better", and a "forced gift"
tier in no_i_insist that can never fire. Moves are generated in his order
wherever that order decides a tie. Paper Elo, for the tournament
programme: pacifist 287, alphabetical 321, generous 343, first_move 358,
no_i_insist 378, huddle 400, sym_180 430, same_color 431, opposite_color
432, sym_mirror_x 439, random_move 439, sym_mirror_y 443, reverse_starting
446, suicide_king 447, swarm 534, cccp 554, min_oppt_moves 597. Console
command `elo [player]` lists the legal moves in Murphy's order with each
one's penalty, code or alphabetical rank and marks the choice.

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
| beal | Beal and Smith, "Random Evaluations in Chess", ICCA Journal 17(1), 1994 | [CPW](https://www.chessprogramming.org/Don_Beal) |
| eloworld | Murphy, [*Elo World*](http://tom7.org/chess/weak.pdf), SIGBOVIK 2019; [source](https://sourceforge.net/p/tom7misc/svn/HEAD/tree/trunk/chess/) | [video](https://www.youtube.com/watch?v=DpXy041BIlA), [CPW](https://www.chessprogramming.org/Tom_Murphy_VII) |

## Credits

Written by Claude (Anthropic) for op12no2's "Retro Fun" engine
tournament, November 2026, from the primary sources named in each file.
The original programmes belong to their authors; these are recreations and
say so in their UCI `id author` strings.
