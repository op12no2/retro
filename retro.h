/*
 * retro.h - shared scaffolding for faithful recreations of early chess
 * programmes as UCI engines. Board, move generation, make/unmake, hashing,
 * perft, UCI loop, clock management and a configurable minimax / alpha-beta
 * driver live here. Each engine is one .c file that includes this header
 * and defines:
 *
 *   ENGINE_NAME, ENGINE_AUTHOR          strings for "id"
 *   int  eng_eval(const Pos *p)         static evaluation, side to move's view, centipawns
 *   void eng_init(void)                 called once at start-up (set cfg defaults)
 *
 * and optionally, after #defining ENG_OPTIONS / ENG_SELECT / ENG_ROOT_ORDER /
 * ENG_CONSIDER / ENG_EXTEND before the include:
 *
 *   void eng_options(void)              print extra "option name ..." lines
 *   int  eng_setoption(const char *name, const char *value)   return 1 if handled
 *   int  eng_select(const Pos *p, int *mv, int n, int ply, int depth, int prev)
 *                                       prune/order moves at an interior node, return new count
 *   int  eng_root_order(const Pos *p, int *mv, int n)
 *                                       order root moves before the first iteration
 *   int  eng_consider(const Pos *p, int m, int ply, int prev)
 *                                       is move m worth following past the horizon? (quiescence)
 *   int  eng_extend(const Pos *child, int m, int prev)
 *                                       1 if the move should not use up a ply
 *
 * cfg (struct Config) holds the search rules an engine can tune.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
static long long now_ms(void) { return (long long)GetTickCount64(); }
#else
#include <time.h>
static long long now_ms(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif

/* ------------------------------------------------------------------ */
/* output, optionally mirrored to a log file                           */
/* ------------------------------------------------------------------ */

static FILE *log_file;
static pthread_mutex_t log_mx = PTHREAD_MUTEX_INITIALIZER;
static void out(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
  if (log_file) {
    pthread_mutex_lock(&log_mx);
    fputs("> ", log_file);
    va_start(ap, fmt); vfprintf(log_file, fmt, ap); va_end(ap);
    fflush(log_file);
    pthread_mutex_unlock(&log_mx);
  }
}
static void log_in(const char *line) {
  if (log_file) { pthread_mutex_lock(&log_mx); fprintf(log_file, "< %s\n", line); fflush(log_file); pthread_mutex_unlock(&log_mx); }
}

/* ------------------------------------------------------------------ */
/* board                                                               */
/* ------------------------------------------------------------------ */

enum { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum { WHITE, BLACK };

#define PT(p)    ((p) & 7)
#define PC(p)    ((p) >> 3)
#define MK(c, t) (((c) << 3) | (t))
#define FILE_OF(s) ((s) & 7)
#define RANK_OF(s) ((s) >> 3)
#define SQ(f, r)   ((f) + 8 * (r))

typedef struct {
  unsigned char pc[64];
  int side, castle, ep, fifty;
  int ksq[2];
} Pos;

static const int mailbox[120] = {
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1, 0, 1, 2, 3, 4, 5, 6, 7,-1, -1, 8, 9,10,11,12,13,14,15,-1,
  -1,16,17,18,19,20,21,22,23,-1, -1,24,25,26,27,28,29,30,31,-1,
  -1,32,33,34,35,36,37,38,39,-1, -1,40,41,42,43,44,45,46,47,-1,
  -1,48,49,50,51,52,53,54,55,-1, -1,56,57,58,59,60,61,62,63,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};
static const int mailbox64[64] = {
  21,22,23,24,25,26,27,28, 31,32,33,34,35,36,37,38, 41,42,43,44,45,46,47,48, 51,52,53,54,55,56,57,58,
  61,62,63,64,65,66,67,68, 71,72,73,74,75,76,77,78, 81,82,83,84,85,86,87,88, 91,92,93,94,95,96,97,98
};
static const int knight_d[8] = { -21,-19,-12,-8,8,12,19,21 };
static const int king_d[8]   = { -11,-10,-9,-1,1,9,10,11 };
static const int bishop_d[4] = { -11,-9,9,11 };
static const int rook_d[4]   = { -10,-1,1,10 };

static int castle_mask[64];

/* move: from | to<<6 | promo<<12 | captured<<15 */
#define M_FROM(m)  ((m) & 63)
#define M_TO(m)    (((m) >> 6) & 63)
#define M_PROMO(m) (((m) >> 12) & 7)
#define M_CAP(m)   (((m) >> 15) & 7)
#define MOVE(f, t, p, c) ((f) | ((t) << 6) | ((p) << 12) | ((c) << 15))

static uint64_t zob[2][7][64], zob_side, zob_castle[16], zob_ep[64];
static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
static uint64_t rng(void) {
  rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
  return rng_state;
}

static void init_tables(void) {
  int i, c, t;
  for (i = 0; i < 64; i++) castle_mask[i] = 15;
  castle_mask[0]  &= ~2; castle_mask[7]  &= ~1; castle_mask[4]  &= ~3;
  castle_mask[56] &= ~8; castle_mask[63] &= ~4; castle_mask[60] &= ~12;
  for (c = 0; c < 2; c++) for (t = 0; t < 7; t++) for (i = 0; i < 64; i++) zob[c][t][i] = rng();
  zob_side = rng();
  for (i = 0; i < 16; i++) zob_castle[i] = rng();
  for (i = 0; i < 64; i++) zob_ep[i] = rng();
}

static uint64_t hash_pos(const Pos *p) {
  uint64_t h = 0; int i;
  for (i = 0; i < 64; i++) if (p->pc[i]) h ^= zob[PC(p->pc[i])][PT(p->pc[i])][i];
  if (p->side) h ^= zob_side;
  h ^= zob_castle[p->castle];
  if (p->ep >= 0) h ^= zob_ep[p->ep];
  return h;
}

/* is square sq attacked by side `by` */
static int attacked(const Pos *p, int sq, int by) {
  int s120 = mailbox64[sq], i, t;
  if (by == WHITE) {
    t = mailbox[s120 - 11]; if (t >= 0 && p->pc[t] == MK(WHITE, PAWN)) return 1;
    t = mailbox[s120 - 9];  if (t >= 0 && p->pc[t] == MK(WHITE, PAWN)) return 1;
  } else {
    t = mailbox[s120 + 11]; if (t >= 0 && p->pc[t] == MK(BLACK, PAWN)) return 1;
    t = mailbox[s120 + 9];  if (t >= 0 && p->pc[t] == MK(BLACK, PAWN)) return 1;
  }
  for (i = 0; i < 8; i++) {
    t = mailbox[s120 + knight_d[i]]; if (t >= 0 && p->pc[t] == MK(by, KNIGHT)) return 1;
    t = mailbox[s120 + king_d[i]];   if (t >= 0 && p->pc[t] == MK(by, KING)) return 1;
  }
  for (i = 0; i < 4; i++) {
    int d = bishop_d[i], x = s120 + d;
    while ((t = mailbox[x]) >= 0) {
      if (p->pc[t]) { if (PC(p->pc[t]) == by && (PT(p->pc[t]) == BISHOP || PT(p->pc[t]) == QUEEN)) return 1; break; }
      x += d;
    }
    d = rook_d[i]; x = s120 + d;
    while ((t = mailbox[x]) >= 0) {
      if (p->pc[t]) { if (PC(p->pc[t]) == by && (PT(p->pc[t]) == ROOK || PT(p->pc[t]) == QUEEN)) return 1; break; }
      x += d;
    }
  }
  return 0;
}

/* number of pieces of side `by` attacking sq (used by several evaluations) */
static inline int attackers(const Pos *p, int sq, int by) {
  int s120 = mailbox64[sq], i, t, n = 0;
  if (by == WHITE) {
    t = mailbox[s120 - 11]; if (t >= 0 && p->pc[t] == MK(WHITE, PAWN)) n++;
    t = mailbox[s120 - 9];  if (t >= 0 && p->pc[t] == MK(WHITE, PAWN)) n++;
  } else {
    t = mailbox[s120 + 11]; if (t >= 0 && p->pc[t] == MK(BLACK, PAWN)) n++;
    t = mailbox[s120 + 9];  if (t >= 0 && p->pc[t] == MK(BLACK, PAWN)) n++;
  }
  for (i = 0; i < 8; i++) {
    t = mailbox[s120 + knight_d[i]]; if (t >= 0 && p->pc[t] == MK(by, KNIGHT)) n++;
    t = mailbox[s120 + king_d[i]];   if (t >= 0 && p->pc[t] == MK(by, KING)) n++;
  }
  for (i = 0; i < 4; i++) {
    int d = bishop_d[i], x = s120 + d;
    while ((t = mailbox[x]) >= 0) {
      if (p->pc[t]) { if (PC(p->pc[t]) == by && (PT(p->pc[t]) == BISHOP || PT(p->pc[t]) == QUEEN)) n++; break; }
      x += d;
    }
    d = rook_d[i]; x = s120 + d;
    while ((t = mailbox[x]) >= 0) {
      if (p->pc[t]) { if (PC(p->pc[t]) == by && (PT(p->pc[t]) == ROOK || PT(p->pc[t]) == QUEEN)) n++; break; }
      x += d;
    }
  }
  return n;
}

static int in_check(const Pos *p) { return attacked(p, p->ksq[p->side], p->side ^ 1); }

static void make(Pos *p, int m) {
  int from = M_FROM(m), to = M_TO(m), promo = M_PROMO(m);
  int us = p->side, piece = p->pc[from], new_ep = -1;
  p->fifty++;
  if (p->pc[to] || PT(piece) == PAWN) p->fifty = 0;
  if (PT(piece) == PAWN) {
    if (to == p->ep) p->pc[us == WHITE ? to - 8 : to + 8] = EMPTY;
    if (abs(to - from) == 16) new_ep = (from + to) / 2;
    if (promo) piece = MK(us, promo);
  } else if (PT(piece) == KING) {
    p->ksq[us] = to;
    if (to - from == 2)  { p->pc[to - 1] = p->pc[to + 1]; p->pc[to + 1] = EMPTY; }
    if (from - to == 2)  { p->pc[to + 1] = p->pc[to - 2]; p->pc[to - 2] = EMPTY; }
  }
  p->castle &= castle_mask[from] & castle_mask[to];
  p->pc[to] = piece; p->pc[from] = EMPTY;
  p->ep = new_ep; p->side ^= 1;
}

static inline int is_castle(const Pos *p, int m) {
  return PT(p->pc[M_FROM(m)]) == KING && abs(M_TO(m) - M_FROM(m)) == 2;
}

/* pseudo-legal generation; castling is fully checked here */
static int gen(const Pos *p, int *mv) {
  int n = 0, sq, us = p->side, them = us ^ 1;
  int fwd = us == WHITE ? 8 : -8;
  int start_rank = us == WHITE ? 1 : 6, promo_rank = us == WHITE ? 6 : 1;
  for (sq = 0; sq < 64; sq++) {
    int piece = p->pc[sq], t, i, s120;
    if (!piece || PC(piece) != us) continue;
    s120 = mailbox64[sq];
    switch (PT(piece)) {
    case PAWN: {
      int to = sq + fwd;
      if (!p->pc[to]) {
        if (RANK_OF(sq) == promo_rank) {
          mv[n++] = MOVE(sq, to, QUEEN, 0); mv[n++] = MOVE(sq, to, ROOK, 0);
          mv[n++] = MOVE(sq, to, BISHOP, 0); mv[n++] = MOVE(sq, to, KNIGHT, 0);
        } else {
          mv[n++] = MOVE(sq, to, 0, 0);
          if (RANK_OF(sq) == start_rank && !p->pc[to + fwd]) mv[n++] = MOVE(sq, to + fwd, 0, 0);
        }
      }
      for (i = -1; i <= 1; i += 2) {
        t = mailbox[s120 + (us == WHITE ? 10 : -10) + i];
        if (t < 0) continue;
        if (p->pc[t] && PC(p->pc[t]) == them) {
          if (RANK_OF(sq) == promo_rank) {
            mv[n++] = MOVE(sq, t, QUEEN, PT(p->pc[t]));  mv[n++] = MOVE(sq, t, ROOK, PT(p->pc[t]));
            mv[n++] = MOVE(sq, t, BISHOP, PT(p->pc[t])); mv[n++] = MOVE(sq, t, KNIGHT, PT(p->pc[t]));
          } else mv[n++] = MOVE(sq, t, 0, PT(p->pc[t]));
        } else if (t == p->ep) mv[n++] = MOVE(sq, t, 0, PAWN);
      }
      break;
    }
    case KNIGHT:
      for (i = 0; i < 8; i++) {
        t = mailbox[s120 + knight_d[i]];
        if (t >= 0 && (!p->pc[t] || PC(p->pc[t]) == them)) mv[n++] = MOVE(sq, t, 0, PT(p->pc[t]));
      }
      break;
    case KING:
      for (i = 0; i < 8; i++) {
        t = mailbox[s120 + king_d[i]];
        if (t >= 0 && (!p->pc[t] || PC(p->pc[t]) == them)) mv[n++] = MOVE(sq, t, 0, PT(p->pc[t]));
      }
      break;
    default: {
      int nd = PT(piece) == QUEEN ? 8 : 4;
      for (i = 0; i < nd; i++) {
        int d, x;
        if (PT(piece) == ROOK) d = rook_d[i];
        else if (PT(piece) == BISHOP) d = bishop_d[i];
        else d = i < 4 ? bishop_d[i] : rook_d[i - 4];
        x = s120 + d;
        while ((t = mailbox[x]) >= 0) {
          if (p->pc[t]) { if (PC(p->pc[t]) == them) mv[n++] = MOVE(sq, t, 0, PT(p->pc[t])); break; }
          mv[n++] = MOVE(sq, t, 0, 0);
          x += d;
        }
      }
    }
    }
  }
  {
    int k = us == WHITE ? 4 : 60;
    int kside = us == WHITE ? 1 : 4, qside = us == WHITE ? 2 : 8;
    if ((p->castle & (kside | qside)) && !attacked(p, k, them)) {
      if ((p->castle & kside) && !p->pc[k + 1] && !p->pc[k + 2] &&
          !attacked(p, k + 1, them) && !attacked(p, k + 2, them)) mv[n++] = MOVE(k, k + 2, 0, 0);
      if ((p->castle & qside) && !p->pc[k - 1] && !p->pc[k - 2] && !p->pc[k - 3] &&
          !attacked(p, k - 1, them) && !attacked(p, k - 2, them)) mv[n++] = MOVE(k, k - 2, 0, 0);
    }
  }
  return n;
}

static int gen_legal(const Pos *p, int *out) {
  int tmp[256], n = gen(p, tmp), i, k = 0;
  for (i = 0; i < n; i++) {
    Pos c = *p; make(&c, tmp[i]);
    if (!attacked(&c, c.ksq[p->side], c.side)) out[k++] = tmp[i];
  }
  return k;
}

static inline int count_legal(const Pos *p) { int tmp[256]; return gen_legal(p, tmp); }

/* legal moves for the side NOT on move, as if it were its turn (mobility) */
static inline int count_legal_other(const Pos *p) {
  Pos q = *p; q.side ^= 1; q.ep = -1;
  return count_legal(&q);
}

static inline int gives_check(const Pos *p, int m) { Pos c = *p; make(&c, m); return in_check(&c); }

/* ------------------------------------------------------------------ */
/* static exchange evaluation                                          */
/* ------------------------------------------------------------------ */

/* values of the pieces of side `by` that attack sq, ascending (x-rays ignored) */
static inline int see_collect(const Pos *p, int sq, int by, int *list, const int *v) {
  int s120 = mailbox64[sq], i, t, n = 0, j;
  if (by == WHITE) {
    t = mailbox[s120 - 11]; if (t >= 0 && p->pc[t] == MK(WHITE, PAWN)) list[n++] = v[PAWN];
    t = mailbox[s120 - 9];  if (t >= 0 && p->pc[t] == MK(WHITE, PAWN)) list[n++] = v[PAWN];
  } else {
    t = mailbox[s120 + 11]; if (t >= 0 && p->pc[t] == MK(BLACK, PAWN)) list[n++] = v[PAWN];
    t = mailbox[s120 + 9];  if (t >= 0 && p->pc[t] == MK(BLACK, PAWN)) list[n++] = v[PAWN];
  }
  for (i = 0; i < 8; i++) {
    t = mailbox[s120 + knight_d[i]]; if (t >= 0 && p->pc[t] == MK(by, KNIGHT)) list[n++] = v[KNIGHT];
    t = mailbox[s120 + king_d[i]];   if (t >= 0 && p->pc[t] == MK(by, KING)) list[n++] = v[KING];
  }
  for (i = 0; i < 4; i++) {
    int d = bishop_d[i], x = s120 + d;
    while ((t = mailbox[x]) >= 0) {
      if (p->pc[t]) { if (PC(p->pc[t]) == by && (PT(p->pc[t]) == BISHOP || PT(p->pc[t]) == QUEEN)) list[n++] = v[PT(p->pc[t])]; break; }
      x += d;
    }
    d = rook_d[i]; x = s120 + d;
    while ((t = mailbox[x]) >= 0) {
      if (p->pc[t]) { if (PC(p->pc[t]) == by && (PT(p->pc[t]) == ROOK || PT(p->pc[t]) == QUEEN)) list[n++] = v[PT(p->pc[t])]; break; }
      x += d;
    }
  }
  for (i = 1; i < n; i++) { int w = list[i]; for (j = i - 1; j >= 0 && list[j] > w; j--) list[j + 1] = list[j]; list[j + 1] = w; }
  return n;
}

/* gain for side `by` from capturing the enemy piece on sq with a piece worth
   first_val, both sides then recapturing with their least valuable piece and
   stopping when it no longer pays */
static inline int see_capture(const Pos *p, int sq, int by, int first_val, const int *v) {
  int att[16], def[16], na, nd, gain[40], d = 0, i, cur, ai = 0, di = 0, turn = 1;
  na = see_collect(p, sq, by, att, v);
  nd = see_collect(p, sq, by ^ 1, def, v);
  for (i = 0; i < na; i++) if (att[i] == first_val) { memmove(&att[i], &att[i + 1], (na - i - 1) * sizeof(int)); na--; break; }
  gain[0] = v[PT(p->pc[sq])];
  cur = first_val;
  while (d < 38) {
    d++;
    gain[d] = cur - gain[d - 1];
    if (turn) { if (di >= nd) break; cur = def[di++]; }
    else      { if (ai >= na) break; cur = att[ai++]; }
    turn ^= 1;
  }
  while (--d) if (-gain[d] < gain[d - 1]) gain[d - 1] = -gain[d];
  return gain[0];
}

/* what the owner of the piece on sq stands to lose if the enemy starts
   capturing it with its least valuable attacker; 0 if nothing or not attacked */
static inline int see_en_prise(const Pos *p, int sq, const int *v) {
  int owner = PC(p->pc[sq]), a[16], n = see_collect(p, sq, owner ^ 1, a, v), g;
  if (!n) return 0;
  g = see_capture(p, sq, owner ^ 1, a[0], v);
  return g > 0 ? g : 0;
}

/* ------------------------------------------------------------------ */
/* engine hooks and search configuration                               */
/* ------------------------------------------------------------------ */

#define MAXPLY 128
#define INF    1000000
#define MATE   500000

typedef struct {
  int alphabeta;      /* 1: alpha-beta cutoffs, 0: plain minimax */
  int qdepth;         /* max plies past the horizon (0: evaluate at the horizon) */
  int fixed_depth;    /* >0: always search exactly this depth, ignore the clock */
  int max_ext;        /* max extensions per line */
  int draw_score;     /* value of repetition / 50-move draw */
  int report_pv;      /* print pv lines */
  int must_move;      /* 1: no stand pat when every legal move is "considerable" (Turing) */
} Config;
static Config cfg = { 1, 8, 0, 0, 0, 1, 0 };

int  eng_eval(const Pos *p);
void eng_init(void);
void eng_options(void);
int  eng_setoption(const char *name, const char *value);
int  eng_select(const Pos *p, int *mv, int n, int ply, int depth, int prev);
int  eng_root_order(const Pos *p, int *mv, int n);
int  eng_consider(const Pos *p, int m, int ply, int prev);
int  eng_extend(const Pos *child, int m, int prev);
int  eng_command(const char *cmd, const Pos *p);   /* extra console commands, return 1 if handled */
int  eng_cutoff(const Pos *p, int ply, int depth);  /* 1: treat this interior node as a leaf */

/* defaults: an engine defines ENG_OPTIONS, ENG_SELECT, ENG_ROOT_ORDER,
   ENG_CONSIDER or ENG_EXTEND before including this header when it
   provides its own version of the hook */
#ifndef ENG_OPTIONS
void eng_options(void) {}
int  eng_setoption(const char *name, const char *value) { (void)name; (void)value; return 0; }
#endif
#ifndef ENG_SELECT
int eng_select(const Pos *p, int *mv, int n, int ply, int depth, int prev) { (void)p; (void)mv; (void)ply; (void)depth; (void)prev; return n; }
#endif
#ifndef ENG_ROOT_ORDER
int eng_root_order(const Pos *p, int *mv, int n) { (void)p; (void)mv; return n; }
#endif
#ifndef ENG_CONSIDER
int eng_consider(const Pos *p, int m, int ply, int prev) { (void)p; (void)ply; (void)prev; return M_CAP(m) || M_PROMO(m); }
#endif
#ifndef ENG_EXTEND
int eng_extend(const Pos *child, int m, int prev) { (void)child; (void)m; (void)prev; return 0; }
#endif
#ifndef ENG_COMMAND
int eng_command(const char *cmd, const Pos *p) { (void)cmd; (void)p; return 0; }
#endif
#ifndef ENG_CUTOFF
int eng_cutoff(const Pos *p, int ply, int depth) { (void)p; (void)ply; (void)depth; return 0; }
#endif

/* ------------------------------------------------------------------ */
/* search driver                                                       */
/* ------------------------------------------------------------------ */

static volatile int stop_flag;
static long long nodes, t_start, t_hard;
static int seldepth, time_limited;
static long long node_limit;
static int pv[MAXPLY][MAXPLY], pv_len[MAXPLY];
static uint64_t path[MAXPLY];
static uint64_t game_hist[1024]; static int game_hist_n;
/* the game so far, for engines that keep state across moves: the position
   the last "position" command started from and the moves played since */
static Pos game_start; static int game_moves[1024], game_moves_n;
static int game_ply;   /* half-moves played from the start position of the game */

static void check_time(void) {
  if ((nodes & 1023) == 0) {
    if (time_limited && now_ms() >= t_hard) stop_flag = 1;
    if (node_limit && nodes >= node_limit) stop_flag = 1;
  }
}

static int is_repetition(const Pos *p, int ply) {
  uint64_t h = hash_pos(p); int i;
  path[ply] = h;
  for (i = ply - 2; i >= 0; i -= 2) if (path[i] == h) return 1;
  for (i = game_hist_n - 2; i >= 0; i -= 2) if (game_hist[i] == h) return 1;
  return 0;
}

static void set_pv(int ply, int m) {
  pv[ply][0] = m; memcpy(&pv[ply][1], pv[ply + 1], pv_len[ply + 1] * sizeof(int));
  pv_len[ply] = pv_len[ply + 1] + 1;
}

/* beyond the horizon: follow only moves the engine considers worth following */
static int qsearch(const Pos *p, int ply, int alpha, int beta, int qd, int prev) {
  int mv[256], n, i, best, stand;
  nodes++; check_time();
  if (ply > seldepth) seldepth = ply;
  pv_len[ply] = 0;
  if (stop_flag) return 0;
  n = gen_legal(p, mv);
  if (n == 0) return in_check(p) ? -MATE + ply : cfg.draw_score;
  if (p->fifty >= 100) return cfg.draw_score;
  stand = eng_eval(p);
  if (qd <= 0 || ply >= MAXPLY - 1) return stand;
  best = stand;
  if (cfg.must_move) {
    int k = 0;
    for (i = 0; i < n; i++) k += eng_consider(p, mv[i], ply, prev);
    if (k == n) best = -INF;
  }
  if (cfg.alphabeta && best > alpha) alpha = best;
  if (cfg.alphabeta && alpha >= beta) return best;
  for (i = 0; i < n; i++) {
    int m = mv[i], s; Pos c;
    if (!eng_consider(p, m, ply, prev)) continue;
    c = *p; make(&c, m);
    s = -qsearch(&c, ply + 1, -beta, -alpha, qd - 1, m);
    if (stop_flag) return 0;
    if (s > best) {
      best = s; set_pv(ply, m);
      if (cfg.alphabeta) { if (s > alpha) alpha = s; if (alpha >= beta) break; }
    }
  }
  return best;
}

static int search(const Pos *p, int depth, int ply, int alpha, int beta, int ext, int prev) {
  int mv[256], n, i, best = -INF;
  nodes++; check_time();
  if (ply > seldepth) seldepth = ply;
  pv_len[ply] = 0;
  if (stop_flag) return 0;
  if (is_repetition(p, ply)) return cfg.draw_score;
  if (p->fifty >= 100) return cfg.draw_score;
  n = gen_legal(p, mv);
  if (n == 0) return in_check(p) ? -MATE + ply : cfg.draw_score;
  if (depth <= 0 || ply >= MAXPLY - 1 || (ply > 0 && eng_cutoff(p, ply, depth)))
    return qsearch(p, ply, alpha, beta, cfg.qdepth, prev);
  n = eng_select(p, mv, n, ply, depth, prev);
  for (i = 0; i < n; i++) {
    int m = mv[i], s, nd = depth - 1, ne = ext;
    Pos c = *p; make(&c, m);
    if (ext < cfg.max_ext && eng_extend(&c, m, prev)) { nd = depth; ne = ext + 1; }
    s = -search(&c, nd, ply + 1, -beta, -alpha, ne, m);
    if (stop_flag) return 0;
    if (s > best) {
      best = s; set_pv(ply, m);
      if (cfg.alphabeta) { if (s > alpha) alpha = s; if (alpha >= beta) break; }
    }
  }
  return best;
}

/* ------------------------------------------------------------------ */
/* uci                                                                 */
/* ------------------------------------------------------------------ */

static Pos root;
static pthread_t search_thread;
static int searching;
typedef struct { int depth; long long nodes, movetime, wtime, btime, winc, binc; int movestogo, infinite; } Limits;
static Limits lim;

static const char *sq_name(int s) {
  static char buf[8][3]; static int k;
  char *b = buf[k++ & 7];
  b[0] = 'a' + FILE_OF(s); b[1] = '1' + RANK_OF(s); b[2] = 0;
  return b;
}
static void move_str(int m, char *s) {
  sprintf(s, "%s%s", sq_name(M_FROM(m)), sq_name(M_TO(m)));
  if (M_PROMO(m)) { const char *pr = " pnbrqk"; s[4] = pr[M_PROMO(m)]; s[5] = 0; }
}
static int parse_move(const Pos *p, const char *s) {
  int mv[256], n = gen_legal(p, mv), i; char t[8];
  for (i = 0; i < n; i++) { move_str(mv[i], t); if (!strcmp(t, s)) return mv[i]; }
  return 0;
}

static void set_fen(Pos *p, const char *fen) {
  int sq = 56;
  const char *s = fen;
  memset(p, 0, sizeof *p);
  p->ep = -1;
  while (*s && *s != ' ') {
    if (*s == '/') sq -= 16;
    else if (isdigit((unsigned char)*s)) sq += *s - '0';
    else {
      int c = isupper((unsigned char)*s) ? WHITE : BLACK, t = 0;
      switch (tolower((unsigned char)*s)) {
      case 'p': t = PAWN; break; case 'n': t = KNIGHT; break; case 'b': t = BISHOP; break;
      case 'r': t = ROOK; break; case 'q': t = QUEEN; break; case 'k': t = KING; p->ksq[c] = sq; break;
      }
      if (sq >= 0 && sq < 64) p->pc[sq] = MK(c, t);
      sq++;
    }
    s++;
  }
  while (*s == ' ') s++;
  p->side = (*s == 'b') ? BLACK : WHITE;
  while (*s && *s != ' ') s++;
  while (*s == ' ') s++;
  while (*s && *s != ' ') {
    if (*s == 'K') p->castle |= 1;
    if (*s == 'Q') p->castle |= 2;
    if (*s == 'k') p->castle |= 4;
    if (*s == 'q') p->castle |= 8;
    s++;
  }
  while (*s == ' ') s++;
  if (*s && *s != '-' && *s != ' ') p->ep = (s[0] - 'a') + 8 * (s[1] - '1');
  while (*s && *s != ' ') s++;
  while (*s == ' ') s++;
  if (*s) p->fifty = atoi(s);
  if (p->pc[4] != MK(WHITE, KING)) p->castle &= ~3;
  if (p->pc[60] != MK(BLACK, KING)) p->castle &= ~12;
  if (p->pc[7] != MK(WHITE, ROOK)) p->castle &= ~1;
  if (p->pc[0] != MK(WHITE, ROOK)) p->castle &= ~2;
  if (p->pc[63] != MK(BLACK, ROOK)) p->castle &= ~4;
  if (p->pc[56] != MK(BLACK, ROOK)) p->castle &= ~8;
}

static const char *START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static void print_info(int depth, int score, long long elapsed) {
  char ms[8]; int i;
  long long nps = elapsed > 0 ? nodes * 1000 / elapsed : 0;
  out("info depth %d seldepth %d ", depth, seldepth);
  if (score > MATE - MAXPLY) out("score mate %d ", (MATE - score + 1) / 2);
  else if (score < -MATE + MAXPLY) out("score mate %d ", -(MATE + score + 1) / 2);
  else out("score cp %d ", score);
  out("nodes %lld nps %lld time %lld", nodes, nps, elapsed);
  if (cfg.report_pv) {
    out(" pv");
    for (i = 0; i < pv_len[0]; i++) { move_str(pv[0][i], ms); out(" %s", ms); }
  }
  out("\n");
}

static void *search_main(void *arg) {
  int mv[256], n, i, depth, best_move = 0, max_depth;
  int ply0_scores[256];
  long long t_soft = 0;
  char ms[8];
  (void)arg;

  nodes = 0; seldepth = 0;
  t_start = now_ms();
  time_limited = 0; node_limit = lim.nodes;
  max_depth = lim.depth ? lim.depth : MAXPLY - 2;
  if (cfg.fixed_depth && !lim.depth && !lim.infinite) max_depth = cfg.fixed_depth;

  if (lim.movetime) { time_limited = 1; t_soft = t_hard = lim.movetime - 20; }
  else if (lim.wtime || lim.btime) {
    long long t = root.side == WHITE ? lim.wtime : lim.btime;
    long long inc = root.side == WHITE ? lim.winc : lim.binc;
    int mtg = lim.movestogo ? lim.movestogo : 30;
    time_limited = 1;
    t_soft = t / mtg + inc * 3 / 4;
    t_hard = t / 15 + inc / 2;
    if (t_hard < t_soft) t_hard = t_soft;
    if (t_hard > t - 100) t_hard = t - 100;
    if (t_soft > t_hard) t_soft = t_hard;
    if (t_hard < 5) t_hard = 5;
    if (t_soft < 1) t_soft = 1;
  }
  if (time_limited) { t_soft += t_start; t_hard += t_start; }

  n = gen_legal(&root, mv);
  if (n == 0) { out("bestmove 0000\n"); searching = 0; return NULL; }
  path[0] = hash_pos(&root);
  n = eng_select(&root, mv, n, 0, max_depth, 0);
  n = eng_root_order(&root, mv, n);
  best_move = mv[0];
  for (i = 0; i < n; i++) ply0_scores[i] = 0;

  for (depth = 1; depth <= max_depth; depth++) {
    int alpha = -INF, beta = INF, best = -INF, best_i = -1, j;
    int iter_pv[MAXPLY], iter_pv_len = 0, iter_seldepth;
    long long elapsed;

    /* keep root moves in order of the previous iteration's scores (stable,
       so an engine's own root ordering survives as the tie-break) */
    if (depth > 1) for (i = 1; i < n; i++) {
      int m = mv[i], s = ply0_scores[i];
      for (j = i - 1; j >= 0 && ply0_scores[j] < s; j--) { mv[j + 1] = mv[j]; ply0_scores[j + 1] = ply0_scores[j]; }
      mv[j + 1] = m; ply0_scores[j + 1] = s;
    }
    seldepth = 0;
    for (i = 0; i < n; i++) {
      Pos c = root; int s;
      make(&c, mv[i]);
      s = -search(&c, depth - 1, 1, -beta, -alpha, 0, mv[i]);
      if (stop_flag) break;
      ply0_scores[i] = s;
      if (s > best) {
        best = s; best_i = i;
        iter_pv[0] = mv[i]; memcpy(&iter_pv[1], pv[1], pv_len[1] * sizeof(int));
        iter_pv_len = pv_len[1] + 1;
        if (cfg.alphabeta && s > alpha) alpha = s;
      }
    }
    iter_seldepth = seldepth;
    if (stop_flag) {
      if (!cfg.alphabeta && best_i > 0 && i > 0) {
        best_move = mv[best_i];
        memcpy(pv[0], iter_pv, iter_pv_len * sizeof(int)); pv_len[0] = iter_pv_len;
        print_info(depth, best, now_ms() - t_start);
      }
      break;
    }
    best_move = mv[best_i];
    memcpy(pv[0], iter_pv, iter_pv_len * sizeof(int)); pv_len[0] = iter_pv_len;
    seldepth = iter_seldepth;
    elapsed = now_ms() - t_start;
    print_info(depth, best, elapsed);
    if ((best > MATE - MAXPLY || best < -MATE + MAXPLY) && !lim.infinite) break;
    if (n == 1 && !lim.infinite && !lim.depth) break;
    if (time_limited && !lim.infinite && now_ms() >= t_soft) break;
  }
  while (lim.infinite && !stop_flag) {
#ifdef _WIN32
    Sleep(5);
#else
    struct timespec ts = {0, 5000000}; nanosleep(&ts, NULL);
#endif
  }
  move_str(best_move, ms);
  out("bestmove %s\n", ms);
  searching = 0;
  return NULL;
}

static void stop_search(void) {
  if (searching) { stop_flag = 1; pthread_join(search_thread, NULL); searching = 0; }
}

static long long perft(const Pos *p, int depth) {
  int mv[256], n, i; long long c = 0;
  if (depth == 0) return 1;
  n = gen_legal(p, mv);
  if (depth == 1) return n;
  for (i = 0; i < n; i++) { Pos q = *p; make(&q, mv[i]); c += perft(&q, depth - 1); }
  return c;
}

static void print_board(const Pos *p) {
  int r, f;
  for (r = 7; r >= 0; r--) {
    for (f = 0; f < 8; f++) {
      int x = p->pc[r * 8 + f];
      const char *s = " PNBRQK  pnbrqk";
      putchar(x ? s[(PC(x) ? 8 : 0) + PT(x)] : '.');
    }
    putchar('\n');
  }
  out("%s to move, castle %d, ep %d, fifty %d, eval %d\n", p->side ? "black" : "white", p->castle, p->ep, p->fifty, eng_eval(p));
}

static int uci_main(void) {
  char line[1 << 16], *tok;
  setvbuf(stdout, NULL, _IONBF, 0);
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  init_tables();
  eng_init();
  set_fen(&root, START_FEN);
  game_hist_n = 1; game_hist[0] = hash_pos(&root);
  game_start = root; game_moves_n = 0;

  while (fgets(line, sizeof line, stdin)) {
    line[strcspn(line, "\r\n")] = 0;
    log_in(line);
    tok = strtok(line, " ");
    if (!tok) continue;
    if (!strcmp(tok, "uci")) {
      out("id name %s\n", ENGINE_NAME);
      out("id author %s\n", ENGINE_AUTHOR);
      eng_options();
      out("option name LogFile type string default <empty>\n");
      out("uciok\n");
    } else if (!strcmp(tok, "isready")) {
      out("readyok\n");
    } else if (!strcmp(tok, "stop")) {
      stop_search();
    } else if (!strcmp(tok, "quit")) {
      stop_search(); break;
    } else if (!strcmp(tok, "setoption")) {
      char *name = NULL, *value = NULL;
      stop_search();
      while ((tok = strtok(NULL, " "))) {
        if (!strcmp(tok, "name")) name = strtok(NULL, " ");
        else if (!strcmp(tok, "value")) value = strtok(NULL, " ");
      }
      if (name && value) {
        if (!strcmp(name, "LogFile")) {
          if (log_file) { fclose(log_file); log_file = NULL; }
          if (strcmp(value, "<empty>") && strcmp(value, "")) log_file = fopen(value, "a");
        } else eng_setoption(name, value);
      }
    } else if (!strcmp(tok, "ucinewgame")) {
      stop_search();
      set_fen(&root, START_FEN);
      game_hist_n = 1; game_hist[0] = hash_pos(&root); game_ply = 0;
      game_start = root; game_moves_n = 0;
    } else if (!strcmp(tok, "position")) {
      stop_search();
      tok = strtok(NULL, " ");
      if (tok && !strcmp(tok, "fen")) {
        char fen[256] = ""; int k = 0;
        while ((tok = strtok(NULL, " ")) && strcmp(tok, "moves") && k < 6) { strcat(fen, tok); strcat(fen, " "); k++; }
        set_fen(&root, fen);
        if (tok && strcmp(tok, "moves")) tok = strtok(NULL, " ");
      } else {
        set_fen(&root, START_FEN);
        tok = strtok(NULL, " ");
      }
      game_hist_n = 0; game_hist[game_hist_n++] = hash_pos(&root);
      game_ply = 0;
      game_start = root; game_moves_n = 0;
      if (tok && !strcmp(tok, "moves")) {
        while ((tok = strtok(NULL, " "))) {
          int m = parse_move(&root, tok);
          if (!m) break;
          if (game_moves_n < 1024) game_moves[game_moves_n++] = m;
          make(&root, m); game_ply++;
          if (root.fifty == 0) game_hist_n = 0;
          if (game_hist_n < 1024) game_hist[game_hist_n++] = hash_pos(&root);
        }
      }
    } else if (!strcmp(tok, "go")) {
      stop_search();
      memset(&lim, 0, sizeof lim);
      while ((tok = strtok(NULL, " "))) {
        char *v;
        if (!strcmp(tok, "infinite")) { lim.infinite = 1; continue; }
        if (!strcmp(tok, "ponder")) continue;
        v = strtok(NULL, " ");
        if (!v) break;
        if (!strcmp(tok, "depth")) lim.depth = atoi(v);
        else if (!strcmp(tok, "nodes")) lim.nodes = atoll(v);
        else if (!strcmp(tok, "movetime")) lim.movetime = atoll(v);
        else if (!strcmp(tok, "wtime")) lim.wtime = atoll(v);
        else if (!strcmp(tok, "btime")) lim.btime = atoll(v);
        else if (!strcmp(tok, "winc")) lim.winc = atoll(v);
        else if (!strcmp(tok, "binc")) lim.binc = atoll(v);
        else if (!strcmp(tok, "movestogo")) lim.movestogo = atoi(v);
      }
      stop_flag = 0; searching = 1;
      pthread_create(&search_thread, NULL, search_main, NULL);
    } else if (!strcmp(tok, "perft")) {
      int d = (tok = strtok(NULL, " ")) ? atoi(tok) : 4;
      long long t0 = now_ms(), c = perft(&root, d);
      out("perft %d = %lld (%lld ms)\n", d, c, now_ms() - t0);
    } else if (!strcmp(tok, "eval")) {
      out("eval %d\n", eng_eval(&root));
    } else if (!strcmp(tok, "d")) {
      print_board(&root);
    } else if (eng_command(tok, &root)) {
      /* handled by the engine */
    }
  }
  return 0;
}
