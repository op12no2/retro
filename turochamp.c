/*
 * turochamp - Alan Turing and David Champernowne's "paper machine", 1948.
 *
 * Source: A. M. Turing, "Chess", in B. V. Bowden (ed.), Faster Than Thought,
 * Pitman 1953, pp. 288-295. Everything below follows that text; where it is
 * silent the choice is marked [choice].
 *
 * Material: P 1, Kt 3, B 3.5, R 5, Q 10, checkmate 1000. The value of a
 * position is the RATIO W/B of the two sides' totals, "better than W - B as
 * the latter does not encourage exchanges when one has the advantage".
 *
 * Search: every move for White and every reply for Black is "considerable".
 * Beyond that only: a recapture after a capture; the capture of an undefended
 * piece; the capture of a higher-valued piece by a lower one; a move giving
 * checkmate. A position with no considerable moves is "dead" and is valued by
 * the ratio. In a position where some moves are considerable the value is the
 * best of (a) the considerable continuations and (b) the position itself,
 * "the latter alternative to be omitted if all moves are considerable".
 *
 * Choice of move: "the move chosen must have the greatest possible value,
 * and, consistent with this, the greatest possible position-play value".
 * Position-play is computed only for the position after the machine's own
 * candidate move, counting only the machine's pieces plus two terms for the
 * enemy king (mate threat 1.0, check 0.5). See position_play() for the
 * itemised rules (a) to (i).
 *
 * [choice] A bare king has material 0, which would make the ratio infinite;
 *          it is counted as half a pawn. The search works on 1000*ln(W/B),
 *          which orders positions exactly as the ratio does and negates
 *          cleanly between the sides; UCI "score cp" shows that number.
 * [choice] Alpha-beta is used. With a lexicographic root tie-break and
 *          root moves pre-sorted by position-play it returns exactly the
 *          move the plain minimax would.
 * [choice] Turing's sqrt table is used as printed (one decimal place).
 * [note]   Replaying the 1952 Turing-Glennie game, the position-play
 *          increases printed by Turing are reproduced exactly for moves
 *          1-9 and 18; moves 10, 14, 15 and 21 differ by 0.2 to 0.7, which
 *          is in line with Champernowne's remark that the hand arithmetic
 *          was "a bit slapdash". The engine's choices deviate from the game
 *          where the 2004 ChessBase reconstruction also deviated.
 * [choice] Option FollowChecks adds checks to the considerable moves, as
 *          Champernowne recalled they did in practice. Off by default: the
 *          1953 text does not list them.
 */

#define ENGINE_NAME   "Turochamp 1948"
#define ENGINE_AUTHOR "Alan Turing and David Champernowne (recreated)"
#define ENG_OPTIONS
#define ENG_ROOT_ORDER
#define ENG_CONSIDER
#define ENG_COMMAND
#include "retro.h"

static const double val[7] = { 0, 1.0, 3.0, 3.5, 5.0, 10.0, 0 };
static int opt_checks = 0;

/* Turing's square-root table, one decimal place */
static double tsqrt(int n) {
  static const double t[14] = { 0, 1, 1.4, 1.7, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.3, 3.5, 3.6 };
  if (n < 0) n = 0;
  return n < 14 ? t[n] : floor(sqrt((double)n) * 10 + 0.5) / 10;
}

static double material(const Pos *p, int side) {
  int sq; double m = 0;
  for (sq = 0; sq < 64; sq++) if (p->pc[sq] && PC(p->pc[sq]) == side) m += val[PT(p->pc[sq])];
  return m > 0 ? m : 0.5;
}

/* 1000 * ln(W/B) from the side to move's point of view */
int eng_eval(const Pos *p) {
  return (int)floor(1000.0 * log(material(p, p->side) / material(p, p->side ^ 1)) + 0.5);
}

/* number of pieces of side `by` attacking sq, pawns only / non-pawns only */
static int pawn_attackers(const Pos *p, int sq, int by) {
  int s120 = mailbox64[sq], t, n = 0;
  if (by == WHITE) {
    t = mailbox[s120 - 11]; if (t >= 0 && p->pc[t] == MK(WHITE, PAWN)) n++;
    t = mailbox[s120 - 9];  if (t >= 0 && p->pc[t] == MK(WHITE, PAWN)) n++;
  } else {
    t = mailbox[s120 + 11]; if (t >= 0 && p->pc[t] == MK(BLACK, PAWN)) n++;
    t = mailbox[s120 + 9];  if (t >= 0 && p->pc[t] == MK(BLACK, PAWN)) n++;
  }
  return n;
}

/* pseudo-legal moves of a queen standing on sq, captures counting double (rule d) */
static int queen_reach(const Pos *p, int sq, int us) {
  int s120 = mailbox64[sq], i, n = 0;
  for (i = 0; i < 8; i++) {
    int d = i < 4 ? bishop_d[i] : rook_d[i - 4], x = s120 + d, t;
    while ((t = mailbox[x]) >= 0) {
      if (p->pc[t]) { if (PC(p->pc[t]) != us) n += 2; break; }
      n++; x += d;
    }
  }
  return n;
}

/* position-play value of position p (opponent to move) for the side `us`
   that has just moved; `castled` is 1 if the move just made was castling */
static double position_play(const Pos *p, int us, int castled) {
  Pos q = *p; q.side = us; q.ep = -1;      /* as if it were our move again */
  int mv[256], n = gen_legal(&q, mv), i, sq, them = us ^ 1;
  int moves[64] = {0};
  double v = 0;
  int can_castle_now = 0, mate_threat = 0;

  for (i = 0; i < n; i++) {
    int from = M_FROM(mv[i]);
    if (is_castle(&q, mv[i])) { can_castle_now = 1; continue; }     /* (c): "moves other than castling" */
    moves[from] += M_CAP(mv[i]) ? 2 : 1;                           /* (a): a capture counts as two moves */
    if (!mate_threat) {                                             /* (h): threat of checkmate */
      Pos c = q; make(&c, mv[i]);
      if (in_check(&c) && count_legal(&c) == 0) mate_threat = 1;
    }
  }
  for (sq = 0; sq < 64; sq++) {
    int x = p->pc[sq];
    if (!x || PC(x) != us) continue;
    switch (PT(x)) {
    case QUEEN: case ROOK: case BISHOP: case KNIGHT: {
      int def = attackers(p, sq, us);
      v += tsqrt(moves[sq]);                                        /* (a) */
      if (PT(x) != QUEEN) { if (def >= 1) v += 1.0; if (def >= 2) v += 0.5; }   /* (b) */
      break;
    }
    case KING: {
      Pos k = *p;
      v += tsqrt(moves[sq]);                                        /* (c) */
      k.pc[sq] = MK(us, QUEEN);
      v -= tsqrt(queen_reach(&k, sq, us));                          /* (d) vulnerability */
      /* (e) castling: 1.0 while it has not been lost by K or R moves, a
         further 1.0 if it could be played next move, another 1.0 for
         playing it. Turing's own table (18.O-O-O = 3.2) shows a castled
         king keeps all three points, so castling counts 3.0 */
      if (castled) v += 3.0;
      else {
        if (p->castle & (us == WHITE ? 3 : 12)) v += 1.0;
        if (can_castle_now) v += 1.0;
      }
      break;
    }
    case PAWN: {
      int r = us == WHITE ? RANK_OF(sq) - 1 : 6 - RANK_OF(sq);
      v += 0.2 * r;                                                 /* (f) */
      if (attackers(p, sq, us) - pawn_attackers(p, sq, us) > 0) v += 0.3;   /* (g) */
      break;
    }
    }
  }
  if (mate_threat) v += 1.0;                                        /* (h) */
  if (attacked(p, p->ksq[them], us)) v += 0.5;                      /* (i) check */
  return v;
}

/* root moves in order of position-play value, best first: the search then
   picks the first move among those with the greatest material value */
static double root_pp[256]; static int root_mv[256], root_n;
int eng_root_order(const Pos *p, int *mv, int n) {
  double sc[256]; int i, j;
  for (i = 0; i < n; i++) { Pos c = *p; make(&c, mv[i]); sc[i] = position_play(&c, p->side, is_castle(p, mv[i])); }
  for (i = 1; i < n; i++) {
    int m = mv[i]; double s = sc[i];
    for (j = i - 1; j >= 0 && sc[j] < s; j--) { mv[j + 1] = mv[j]; sc[j + 1] = sc[j]; }
    mv[j + 1] = m; sc[j + 1] = s;
  }
  memcpy(root_mv, mv, n * sizeof(int)); memcpy(root_pp, sc, n * sizeof(double)); root_n = n;
  return n;
}

/* Turing's considerable moves beyond the first two plies */
int eng_consider(const Pos *p, int m, int ply, int prev) {
  int to = M_TO(m), cap = M_CAP(m), mover = PT(p->pc[M_FROM(m)]);
  (void)ply;
  if (cap) {
    if (M_CAP(prev) && M_TO(prev) == to) return 1;                 /* recapture */
    if (attackers(p, to, p->side ^ 1) == 0) return 1;              /* undefended piece */
    if (val[cap] > val[mover]) return 1;                           /* higher value by lower */
  }
  {
    Pos c = *p; make(&c, m);
    if (in_check(&c)) {
      if (count_legal(&c) == 0) return 1;                          /* checkmate */
      if (opt_checks) return 1;
    }
  }
  return 0;
}

void eng_init(void) {
  cfg.alphabeta = 1;
  cfg.fixed_depth = 2;     /* "white's next move and black's reply" */
  cfg.qdepth = 16;
  cfg.must_move = 1;       /* no stand pat when all moves are considerable */
}

void eng_options(void) {
  out("option name Depth type spin default 2 min 1 max 10\n");
  out("option name FollowChecks type check default false\n");
}
int eng_setoption(const char *name, const char *value) {
  if (!strcmp(name, "Depth")) { cfg.fixed_depth = atoi(value); return 1; }
  if (!strcmp(name, "FollowChecks")) { opt_checks = !strcmp(value, "true"); return 1; }
  return 0;
}

/* console: "pp" lists the position-play value of every root move */
int eng_command(const char *cmd, const Pos *p) {
  if (strcmp(cmd, "pp")) return 0;
  int mv[256], n = gen_legal(p, mv), i; char ms[8];
  double base = position_play(p, p->side, 0);   /* value of the present position for the mover */
  n = eng_root_order(p, mv, n);
  out("present %.1f\n", base);
  for (i = 0; i < n && i < 8; i++) { move_str(mv[i], ms); out("%s %.1f (%+.1f)\n", ms, root_pp[i], root_pp[i] - base); }
  return 1;
}

int main(void) { return uci_main(); }
