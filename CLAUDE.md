# CLAUDE.md

Notes for Claude working in this repository. Read before changing anything.

## What this is

Faithful recreations of early chess programmes as UCI engines, one C file
each on top of `retro.h`. They are played in the user's "Retro Fun"
tournament on Windows (Myracle GUI, 12 min + 8 s, adjudication off because
watching engines fail to mate is the point). The event values quirkiness
and historical honesty over strength. Never make an engine stronger at the
cost of fidelity.

## Where things live on the user's machine

- This repo: `~/retro` (WSL). Sessions start here.
- Myracle's engine folders, on the Windows drive:
  `/mnt/d/myracle/engines/retro/` holds our exes; copy them there
  after a rebuild (`cp *.exe /mnt/d/myracle/engines/retro/`). It also
  holds `stockfish_10_x64.exe` and `Topple_v0.3.5_x86_64.exe` for elofish.
  `/mnt/d/myracle/engines/oldies/` holds Sargon, Psion and the downloaded
  or rebuilt historic and quirky engines, one folder each, with
  `oldies/README.md` recording protocol, provenance and quirks.
- Myracle's debug log and PGNs: `/mnt/d/myracle/tournaments/`.
- Windows cutechess-cli for testing third-party exes:
  `/mnt/d/myracle/tools/cutechess-win64/`.
- Myracle drives both UCI and WinBoard engines, so WinBoard-only engines
  need no adapter.

## Layout

- `retro.h`: board, move generation, make, attack detection, static exchange
  evaluation, Zobrist hashing, UCI loop, clock management, and a
  configurable minimax / alpha-beta driver with quiescence. Engines include
  it and define hooks. Change it carefully: every engine shares it, and a
  change in search semantics can silently alter a recreation.
- `<engine>.c`: one per programme. Header comment names the primary sources
  and lists every judgement call as `[choice]`. Keep that discipline.
  `eloworld_data.h` is the one data header (Murphy's fate table and the
  hex digits of pi and e), generated, not hand-edited. `elofish.c` is the
  one engine that spawns other programs (Stockfish 10, Topple 0.3.5 beside
  the exe; sources in README); keep those child exes out of git.
- `build.sh [engine...]`: builds `<engine>` (Linux) and `<engine>.exe`
  (static Windows, mingw-w64). Binaries are git-ignored and shipped as
  GitHub releases.
- `README.md`: for people. `RESEARCH.md`: survey of what else could be
  recreated and what already exists elsewhere.

## Hooks

An engine must define `ENGINE_NAME`, `ENGINE_AUTHOR`, `eng_eval`
(side-to-move view, centipawn-ish integers) and `eng_init` (set `cfg`).
Optional hooks are enabled by `#define`-ing the matching macro before the
include: `ENG_OPTIONS` (eng_options / eng_setoption), `ENG_SELECT` (prune
or order moves at a node, also called at the root), `ENG_ROOT_ORDER`
(root order before the first iteration; the driver keeps the first of
equal-scoring moves, so this is how lexicographic tie-breaks are done),
`ENG_CONSIDER` (which moves to follow past the horizon), `ENG_EXTEND`,
`ENG_CUTOFF` (treat an interior node as a leaf), `ENG_COMMAND` (console
debug commands). `game_start` and `game_moves[]` hold the position the
last `position` command began from and the moves since, for engines
whose choice depends on the game so far. A no-search engine (eloworld) does all its choosing in
`eng_root_order`, puts the move in `mv[0]` and returns 1: the driver then
plays that move without letting its own mate and stalemate scoring
interfere. `cfg` fields: `alphabeta`, `qdepth`, `fixed_depth`,
`max_ext`, `draw_score`, `must_move` (Turing's rule: no stand pat when
every move is considerable). An engine that searches on its own inside
`eng_root_order` (abramson, uct: Monte Carlo, not minimax) reads the
driver's clock: `time_limited`, `t_soft` (stop starting new work), `t_hard`,
`t_start`, `lim` (`nodes`, `depth`, `infinite`) and `stop_flag`, which it
must poll; when it returns 1 from more than one move the driver suppresses
its own info line so the engine's stands.

## Testing an engine

1. `./build.sh name`, then `printf 'position fen ...\nperft 4\nquit\n' | ./name`
   against known perft counts if move generation was touched.
2. Check the engine's own numbers against the paper's worked examples with
   its console command (`pp`, `table`, `soma`, `pos`, `plaus`). The source
   comment says what the paper's numbers are.
3. A short fastchess match with the Windows exe, which runs under WSL:
   `~/fastchess/fastchess -engine cmd=./name.exe name=X -engine cmd=./tech.exe name=Tech -each tc=10+0.1 proto=uci -rounds 1 -games 2 -repeat -concurrency 2 -pgnout file=/tmp/x.pgn`
   and confirm every game ends by mate, draw or resignation, never by
   illegal move or time loss. Delete the `config.json` fastchess leaves.
4. When piping `quit` after `go`, sleep first; otherwise the search is
   stopped at once.

## Testing third-party Windows engines

Linux fastchess is UCI-only and reads Windows CRLF output as illegal moves,
so it is only good for our own engines (which force binary stdout). For
downloaded exes use the Windows cutechess-cli from a directory under
`/mnt/d` so relative paths resolve:
`cutechess-cli.exe -engine cmd=x.exe dir=folder proto=xboard|uci name=X -engine cmd=tech.exe dir=. proto=uci name=Tech -each tc=60+1 timemargin=3000 -rounds 1 -games 2 -repeat -concurrency 2 -pgnout t.pgn`.
Downloaded exes need `chmod +x` before WSL will launch them. To slow a
third-party engine down for watching, `tools/slow.c` (`slow.exe`, build
line in its header) is a stdio proxy: put it in the engine's folder with a
`slow.txt` (line 1 the real command, line 2 the delay in ms), point the
entry's command at `slow.exe` and keep the protocol; it holds `bestmove`,
`move` and protocol-1 move lines until the delay has passed since the GUI's
last line and passes everything else straight through. Probe first
with `printf 'uci\n'` or `printf 'xboard\nprotover 2\n'` (sleep before
`quit`). Use 60+1 rather than 10s+0.1: old engines lose on time at bullet.
Keep concurrency at 2 on this machine. Lessons so far: micro-Max omitted
the promotion piece (fixed in `oldies/micromax`, source alongside); Iota
needs a 32-bit build; GNU Chess 4 needed a handful of protocol patches
(see `oldies/gnuchess4/BUILD.txt`); Microchess's xboard layer had no
force mode and replied to every move it was fed, so Myracle's book moves
desynchronised its board (rewritten, `oldies/microchess/BUILD.txt`). Myracle, unlike cutechess, requires
an xboard engine to answer `protover 2` with a `feature ... done=1` line:
without one it waits 20 s and logs "failed to start engine" (seen with
micro-Max 4.8 and TSCP 1.81, both patched to reply). So a protocol-1 exe
that passes cutechess can still fail in Myracle; probe with `protover 2`.

## Conventions

- Commits are authored by Claude: `git -c user.name="Claude Fable 5.1" -c user.email="noreply@anthropic.com" commit`, ending with the
  `Co-Authored-By` trailer the harness asks for. The user owns the repo.
- Releases carry all exes: `gh release create vX.Y *.exe` with a short note.
- Windows stdout must stay in binary mode (done in `uci_main`), or
  Linux-side GUIs read `bestmove e2e4\r` as an illegal move.
- Option names are part of the user's tournament config: do not rename
  them. `retro.h` gives every engine `MoveDelay` (ms; the reply is held
  until that long after `go`, `stop` releases it) and `LogFile`; the
  delay exists so the search-free engines can be watched at a human pace
  (Myracle has no setting for this). Adding a common option means every
  stored Myracle entry needs it (`tools/myracle_move_delay.py` adds
  `MoveDelay`), or Myracle refuses to start the engine. Shannon's are `Type`, `AlphaBeta`, `QuiescenceDepth`, `Width`,
  `Extensions`; the user runs Type B, Width 4, QuiescenceDepth 4,
  Extensions 3.

## Known gaps and next steps

- Kotok is built from the thesis description; its FORTRAN listing exists
  as an image-only scan (thesis Appendix 1). Transcribing it would make the
  recreation exact. The original opened 1.e4; ours opens 1.Nc3.
- Tech's endgame king terms and bare-king play are approximations marked
  `[approx]`.
- Mac Hack VI (Greenblatt): the 1980 MIDAS source survives in the PDP-10/its
  repository. The honest route is a user-mode PDP-10 interpreter in C with
  the assembled image embedded and a UCI shim speaking descriptive
  notation; see RESEARCH.md section 5.
- Not recreatable: NSS 1958 (never played, two goals coded), Machiavelli
  1948 (no rules survive), Los Alamos 1956 (6x6 board).

## Myracle's engine list

Myracle stores its engines in `/mnt/d/myracle/config/engines.json.gz`: a
gzip of a plain JSON array (name, group, command, working_folder relative
to `/mnt/d/myracle`, protocol, optional comment, options[] with
name/type/default/min/max or choices/value). Myracle rewrites it on exit, so
only edit it while Myracle is closed, and back it up first. Scripts in
`tools/`: `myracle_add_engines.py` (bulk add, probes UCI options),
`myracle_test_config.py` (launch every entry through cutechess as
configured), `myracle_fix_defaults.py` (refresh stored defaults), `myracle_move_delay.py MS
[name-prefix ...]` (adds the `MoveDelay` option to our entries and sets it,
or wraps third-party entries in `slow.exe`).

Gotchas: if an exe is rebuilt with different option defaults, Myracle
compares the stored `default` fields with what the engine reports and
refuses to start it ("defaults have changed") until the entry is refreshed.
So never copy a rebuild whose option list changed into `engines/retro`
while a tournament is running: the entries cannot be refreshed until Myracle
closes, and the engines would fail when their games came up (nearly
happened 7 Nov 2026 with MoveDelay; the v1.3 exes were put back). Stage such
builds and swap them in together with the config refresh.
Keep comments ASCII: Myracle's own writer escapes non-ASCII as `\xHH`,
which is not valid JSON. The user names engines "Name version year"
(e.g. "Rebel 6.0 1994") and references them by name in `.tour` files, so
don't rename existing entries.
