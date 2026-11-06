/*
 * elofish - the engine-backed entrants of Tom Murphy VII's "Elo World", 2019.
 *
 *   Tom Murphy VII, "Elo World, a framework for benchmarking weak chess
 *   engines", SIGBOVIK 2019.  http://tom7.org/chess/weak.pdf
 *   Source: tom7misc svn, trunk/chess/ (stockfish.cc, stockfish-player.cc,
 *   uci-player.cc, player-util.h BlendRandom, tournament.cc)
 *   https://sourceforge.net/p/tom7misc/svn/HEAD/tree/trunk/chess/
 *
 * The companion of eloworld.c: these players call a real engine, which must
 * sit beside this exe (options StockfishPath, TopplePath). Murphy built his
 * Stockfish from source in early 2019; Stockfish 10 (November 2018) is the
 * matching release and the one our BUILD notes describe. Topple is 0.3.5,
 * the version his code names. The UCI option `Player` chooses:
 *
 *   stockfish0 stockfish5 stockfish10 stockfish15 stockfish20
 *                    Stockfish with "Skill Level" N, asked for a move with a
 *                    bare "go": no time, no depth, no node limit. Stockfish
 *                    treats that as time management with zero time on the
 *                    clock and answers after a few milliseconds, which is why
 *                    the paper says these move "basically instantaneously".
 *   stockfish1m      Skill Level 20, "go nodes 1000000" (about a second)
 *   worstfish        for every legal move, ask Stockfish (Skill Level
 *                    `WorstfishLevel`, bare go) to evaluate the position it
 *                    leaves, from the opponent's side, and play the move the
 *                    opponent likes best; ties broken at random
 *   stockfish1m_rNNN  (NNN in 64 128 256 512 1024 2048 4096 8192 16384 32768
 *                    49152 57344 61440 63488 64512) before each move draw a
 *                    16-bit random number; below NNN play a uniformly random
 *                    legal move, otherwise stockfish1m's move
 *   topple10k topple1m
 *                    Topple with "go nodes 10000" / "go nodes 1000000"
 *                    (Topple_v0.3.5_x86_64.exe; Murphy ran the znver1 build)
 *
 * Murphy's engines are stateless by construction: before every move the
 * engine gets "position fen <fen>" with the halfmove and fullmove counters
 * both set to 10 and no move history, so it never knows it is repeating
 * positions (the paper notes the draws that causes). No ucinewgame, no hash
 * clearing, default Hash and Threads, no books or tablebases. Reproduced.
 *
 * Paper Elo: worstfish 189, stockfish1m_r64512 463, r63488 482, r61440 521,
 * r57344 601, r49152 752, r32768 976, r16384 1278, stockfish0 1336,
 * stockfish5 1645, r8192 1690, topple10k 1772, stockfish10 1915,
 * stockfish15 1953, r4096 2020, stockfish20 2139, topple1m 2219, r2048 2262,
 * r1024 2426, r512 2522, r256 2582, r128 2609, r64 2638, stockfish1m 2644.
 *
 * [quirk] The paper says worstfish uses Stockfish "configured as stockfish0";
 *         the code, in the February 2019 revision as now, constructs it with
 *         level 20. The code is followed; `WorstfishLevel` lets you pick.
 * [quirk] worstfish's penalty for a mate score: "mate 0" (the opponent is
 *         mated) is the worst outcome; "mate N" for the opponent is the best;
 *         "mate -N" (we would mate) is bad in proportion. Copied as written.
 * [choice] Murphy's code aborts if the engine answers with an illegal move or
 *          "(none)"; we play a random legal move instead and say so.
 * [choice] The 16-bit random draws come from retro.h's xorshift seeded with
 *          the time; Murphy used ARC4 seeded with the time.
 * [choice] A time control is ignored, as in Murphy's tournament. stockfish1m
 *          and topple1m take about a second a move on a modern machine,
 *          worstfish about thirty quick Stockfish calls.
 */

#define ENGINE_NAME   "Elo World Engines 2019"
#define ENGINE_AUTHOR "Tom Murphy VII (recreated); Stockfish and Topple by their authors"
#define ENG_OPTIONS
#define ENG_ROOT_ORDER
#define ENG_COMMAND
#include "retro.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

/* ------------------------------------------------------------------ */
/* a child engine on pipes                                              */
/* ------------------------------------------------------------------ */

typedef struct {
#ifdef _WIN32
  HANDLE in, out, proc;
#else
  int in, out; pid_t pid;
#endif
  char buf[4096]; int blen, bpos;
  int alive;
} Sub;

static int sub_start(Sub *s, const char *exe) {
  memset(s, 0, sizeof *s);
#ifdef _WIN32
  {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE cin_r, cin_w, cout_r, cout_w;
    STARTUPINFOA si; PROCESS_INFORMATION pi; char cmd[1024];
    if (!CreatePipe(&cin_r, &cin_w, &sa, 0) || !CreatePipe(&cout_r, &cout_w, &sa, 0)) return 0;
    SetHandleInformation(cin_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(cout_r, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES; si.hStdInput = cin_r; si.hStdOutput = cout_w; si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    snprintf(cmd, sizeof cmd, "\"%s\"", exe);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return 0;
    CloseHandle(cin_r); CloseHandle(cout_w); CloseHandle(pi.hThread);
    s->in = cout_r; s->out = cin_w; s->proc = pi.hProcess;
  }
#else
  {
    int a[2], b[2];
    if (pipe(a) || pipe(b)) return 0;
    s->pid = fork();
    if (s->pid < 0) return 0;
    if (s->pid == 0) {
      dup2(a[0], 0); dup2(b[1], 1); close(a[0]); close(a[1]); close(b[0]); close(b[1]);
      execl(exe, exe, (char *)NULL); _exit(127);
    }
    close(a[0]); close(b[1]); s->out = a[1]; s->in = b[0];
  }
#endif
  s->alive = 1;
  return 1;
}

static int sub_write(Sub *s, const char *str) {
  size_t n = strlen(str);
#ifdef _WIN32
  DWORD w; return WriteFile(s->out, str, (DWORD)n, &w, NULL) && w == n;
#else
  return write(s->out, str, n) == (ssize_t)n;
#endif
}

static int sub_readline(Sub *s, char *line, int max) {
  int n = 0;
  for (;;) {
    if (s->bpos >= s->blen) {
#ifdef _WIN32
      DWORD r; if (!ReadFile(s->in, s->buf, sizeof s->buf, &r, NULL) || r == 0) { s->alive = 0; return 0; }
      s->blen = (int)r;
#else
      ssize_t r = read(s->in, s->buf, sizeof s->buf); if (r <= 0) { s->alive = 0; return 0; }
      s->blen = (int)r;
#endif
      s->bpos = 0;
    }
    while (s->bpos < s->blen) {
      char c = s->buf[s->bpos++];
      if (c == '\n') { line[n] = 0; return 1; }
      if (c != '\r' && n < max - 1) line[n++] = c;
    }
  }
}

static void sub_stop(Sub *s) {
  if (!s->alive) return;
  sub_write(s, "quit\n");
#ifdef _WIN32
  WaitForSingleObject(s->proc, 2000); TerminateProcess(s->proc, 0);
  CloseHandle(s->proc); CloseHandle(s->in); CloseHandle(s->out);
#else
  { int st; usleep(100000); kill(s->pid, SIGTERM); waitpid(s->pid, &st, 0); close(s->in); close(s->out); }
#endif
  s->alive = 0;
}

/* ------------------------------------------------------------------ */
/* the players                                                          */
/* ------------------------------------------------------------------ */

enum { P_SF0, P_SF5, P_SF10, P_SF15, P_SF20, P_SF1M, P_WORST,
       P_R64, P_R128, P_R256, P_R512, P_R1024, P_R2048, P_R4096, P_R8192, P_R16384, P_R32768,
       P_R49152, P_R57344, P_R61440, P_R63488, P_R64512, P_TOPPLE10K, P_TOPPLE1M, N_PLAYERS };
static const char *player_name[N_PLAYERS] = {
  "stockfish0", "stockfish5", "stockfish10", "stockfish15", "stockfish20", "stockfish1m", "worstfish",
  "stockfish1m_r64", "stockfish1m_r128", "stockfish1m_r256", "stockfish1m_r512", "stockfish1m_r1024",
  "stockfish1m_r2048", "stockfish1m_r4096", "stockfish1m_r8192", "stockfish1m_r16384", "stockfish1m_r32768",
  "stockfish1m_r49152", "stockfish1m_r57344", "stockfish1m_r61440", "stockfish1m_r63488", "stockfish1m_r64512",
  "topple10k", "topple1m" };
static const int blend_thresh[15] = { 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 49152, 57344, 61440, 63488, 64512 };
static const int sf_level[5] = { 0, 5, 10, 15, 20 };

static int player = P_SF0, worst_level = 20;
static char sf_path[512] = "stockfish_10_x64.exe", topple_path[512] = "Topple_v0.3.5_x86_64.exe";
static Sub child; static int child_player = -1;
static long long calls, call_ms;

/* FEN as Murphy's ToFEN(10, 10) writes it */
static void pos_fen(const Pos *p, char *s) {
  int r, f, e = 0;
  for (r = 7; r >= 0; r--) {
    for (f = 0; f < 8; f++) {
      int x = p->pc[SQ(f, r)];
      if (!x) { e++; continue; }
      if (e) { *s++ = '0' + e; e = 0; }
      *s++ = PC(x) == WHITE ? "?PNBRQK"[PT(x)] : "?pnbrqk"[PT(x)];
    }
    if (e) { *s++ = '0' + e; e = 0; }
    if (r) *s++ = '/';
  }
  *s++ = ' '; *s++ = p->side == WHITE ? 'w' : 'b'; *s++ = ' ';
  if (!p->castle) *s++ = '-';
  if (p->castle & 1) *s++ = 'K';
  if (p->castle & 2) *s++ = 'Q';
  if (p->castle & 4) *s++ = 'k';
  if (p->castle & 8) *s++ = 'q';
  *s++ = ' ';
  if (p->ep >= 0) { *s++ = 'a' + FILE_OF(p->ep); *s++ = '1' + RANK_OF(p->ep); } else *s++ = '-';
  strcpy(s, " 10 10");
}

static int uses_topple(int pl) { return pl == P_TOPPLE10K || pl == P_TOPPLE1M; }
static int skill_of(int pl) {
  if (pl <= P_SF20) return sf_level[pl];
  if (pl == P_WORST) return worst_level;
  return 20;
}

/* start (or restart) the child for the current player, as Murphy's InitEngine does */
static int child_ready(void) {
  char line[1024], cmd[128];
  if (child.alive && child_player == player) return 1;
  if (child.alive) sub_stop(&child);
  if (!sub_start(&child, uses_topple(player) ? topple_path : sf_path)) {
    out("info string cannot start %s\n", uses_topple(player) ? topple_path : sf_path); return 0;
  }
  sub_write(&child, "uci\n");
  while (sub_readline(&child, line, sizeof line)) if (!strcmp(line, "uciok")) break;
  if (!child.alive) return 0;
  if (!uses_topple(player)) { snprintf(cmd, sizeof cmd, "setoption name Skill Level value %d\n", skill_of(player)); sub_write(&child, cmd); }
  child_player = player;
  return 1;
}

/* Murphy's GetMove: position fen + go, read to bestmove, keep the last info line's score */
static int engine_move(const Pos *p, const char *go, char *move, int *is_mate, int *score) {
  char fen[128], cmd[256], line[2048], info[2048] = "";
  long long t0 = now_ms();
  if (!child_ready()) return 0;
  pos_fen(p, fen);
  snprintf(cmd, sizeof cmd, "position fen %s\ngo%s%s\n", fen, *go ? " " : "", go);
  sub_write(&child, cmd);
  move[0] = 0;
  while (sub_readline(&child, line, sizeof line)) {
    if (!strncmp(line, "info", 4)) strcpy(info, line);
    if (!strncmp(line, "bestmove", 8)) { sscanf(line + 8, " %15s", move); break; }
  }
  calls++; call_ms += now_ms() - t0;
  if (is_mate) {
    char *t = strstr(info, " score ");
    *is_mate = 0; *score = 0;
    if (t) { char typ[8]; int v; if (sscanf(t + 7, "%7s %d", typ, &v) == 2) { *is_mate = !strcmp(typ, "mate"); *score = v; } }
  }
  return child.alive && move[0] != 0;
}

static int random_legal(const Pos *p, int *mv, int n) { (void)p; return mv[rng() % (uint64_t)n]; }

static int legal_from_string(const Pos *p, const char *s, int *mv, int n) {
  int m = parse_move(p, s), i;
  for (i = 0; i < n; i++) if (mv[i] == m) return m;
  return 0;
}

static int choose(const Pos *p, int *mv, int n) {
  char ms[16]; int m, dummy1, dummy2;
  const char *go = "";
  if (player >= P_R64 && player <= P_R64512) {
    if ((int)(rng() & 0xFFFF) < blend_thresh[player - P_R64]) { out("info string random move\n"); return random_legal(p, mv, n); }
    go = "nodes 1000000";
  } else if (player == P_SF1M) go = "nodes 1000000";
  else if (player == P_TOPPLE10K) go = "nodes 10000";
  else if (player == P_TOPPLE1M) go = "nodes 1000000";
  if (player == P_WORST) {
    /* EvalResultPlayer over Stockfish's opinion of each resulting position */
    long long best_pen = 0; uint32_t best_r = 0; int best = -1, i;
    for (i = 0; i < n; i++) {
      Pos c = *p; long long pen; int is_mate, score; uint32_t r = (uint32_t)(rng() >> 32);
      make(&c, mv[i]);
      if (!engine_move(&c, "", ms, &is_mate, &score)) return random_legal(p, mv, n);
      if (is_mate) {
        if (score == 0) pen = 0xFFFFFFFFFFFFLL;
        else if (score > 0) pen = -0xFFFFFFFFFFFFLL + score;
        else pen = 0xFFFFFFFFFFFFLL + score;
      } else pen = -(long long)score;
      if (best < 0 || pen < best_pen || (pen == best_pen && r < best_r)) { best = i; best_pen = pen; best_r = r; }
    }
    out("info string worstfish penalty %lld\n", best_pen);
    return mv[best];
  }
  if (!engine_move(p, go, ms, &dummy1, &dummy2)) { out("info string engine failed, random move\n"); return random_legal(p, mv, n); }
  m = legal_from_string(p, ms, mv, n);
  if (!m) { out("info string engine answered %s, random move\n", ms); return random_legal(p, mv, n); }
  if (dummy1) out("info string engine score mate %d\n", dummy2); else out("info string engine score cp %d\n", dummy2);
  return m;
}

int eng_root_order(const Pos *p, int *mv, int n) { mv[0] = choose(p, mv, n); return 1; }
int eng_eval(const Pos *p) { (void)p; return 0; }

void eng_init(void) {
  cfg.alphabeta = 1; cfg.fixed_depth = 1; cfg.qdepth = 0;
  rng_state ^= (uint64_t)now_ms() * 0x9E3779B97F4A7C15ULL;
#ifndef _WIN32
  strcpy(sf_path, "./stockfish10-linux"); strcpy(topple_path, "./topple");
#endif
}

void eng_options(void) {
  int i;
  out("option name Player type combo default %s", player_name[player]);
  for (i = 0; i < N_PLAYERS; i++) out(" var %s", player_name[i]);
  out("\n");
  out("option name StockfishPath type string default %s\n", sf_path);
  out("option name TopplePath type string default %s\n", topple_path);
  out("option name WorstfishLevel type spin default 20 min 0 max 20\n");
}
int eng_setoption(const char *name, const char *value) {
  int i;
  if (!strcmp(name, "Player")) { for (i = 0; i < N_PLAYERS; i++) if (!strcmp(value, player_name[i])) player = i; return 1; }
  if (!strcmp(name, "StockfishPath")) { snprintf(sf_path, sizeof sf_path, "%s", value); if (child.alive) sub_stop(&child); return 1; }
  if (!strcmp(name, "TopplePath")) { snprintf(topple_path, sizeof topple_path, "%s", value); if (child.alive) sub_stop(&child); return 1; }
  if (!strcmp(name, "WorstfishLevel")) { worst_level = atoi(value); if (child.alive) sub_stop(&child); return 1; }
  return 0;
}

/* console: "fish" shows the FEN sent, the child's answer and timing */
int eng_command(const char *cmd, const Pos *p) {
  char fen[128], ms[16]; int is_mate, score;
  if (strcmp(cmd, "fish")) return 0;
  pos_fen(p, fen);
  out("%s: position fen %s\n", player_name[player], fen);
  if (engine_move(p, player == P_SF1M ? "nodes 1000000" : "", ms, &is_mate, &score))
    out("bestmove %s score %s %d (%lld calls, %lld ms total)\n", ms, is_mate ? "mate" : "cp", score, calls, call_ms);
  else out("no answer\n");
  return 1;
}

int main(void) { int r = uci_main(); if (child.alive) sub_stop(&child); return r; }
