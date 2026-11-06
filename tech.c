/*
 * tech - James Gillogly's Technology Chess Program, Carnegie Mellon, 1971-72.
 *
 *   J. J. Gillogly, "The Technology Chess Program", Artificial Intelligence 3 (1972)
 *   J. J. Gillogly, "Performance Analysis of the Technology Chess Program",
 *                   PhD thesis, CMU-CS-78-189 (1978)
 *
 * A deliberately knowledge-free program, built to measure how far raw search
 * ("technology") alone gets you. Full-width alpha-beta to a fixed depth with
 * iterative deepening, leaves scored by MATERIAL ONLY (P 100, N 330, B 330,
 * R 500, Q 900), all capture chains followed to quiescence with the "null
 * move" (stand pat) allowed, captures ordered most-valuable-victim first.
 * No forward pruning, no check extensions, no transposition scores.
 *
 * Its only chess knowledge is a positional pre-sort of the ROOT moves, from
 * heuristics by Hans Berliner (thesis section 2.2). The search accepts a
 * later move only if it is strictly better on material, so "the first of
 * the materially best moves will be accepted" and the positional score acts
 * purely as a tie-break. The pre-sort has phases: opening (first 8 moves),
 * middle game, and endgames (one side under 1950 of non-king material).
 *
 * Where the thesis scan is unreadable the Tech II source (Baisley, MIT) was
 * used to fill numbers; a few endgame terms are approximated and marked.
 *
 * Not reproduced: the 3-ply limit in the first 8 moves (a time-saving
 * measure of 1971), the killer heuristic (Gillogly found it useless), the
 * 70-position opening book, aspiration windows, pondering.
 */

#define ENGINE_NAME   "Tech 1971"
#define ENGINE_AUTHOR "James J. Gillogly (recreated)"
#define ENG_OPTIONS
#define ENG_SELECT
#define ENG_ROOT_ORDER
#define ENG_COMMAND
#include "retro.h"

static const int val[7] = { 0, 100, 330, 330, 500, 900, 0 };

static int nonking_material(const Pos *p, int side) {
  int sq, m = 0;
  for (sq = 0; sq < 64; sq++) if (p->pc[sq] && PC(p->pc[sq]) == side && PT(p->pc[sq]) != KING) m += val[PT(p->pc[sq])];
  return m;
}
static int count_type(const Pos *p, int side, int t) {
  int sq, n = 0;
  for (sq = 0; sq < 64; sq++) if (p->pc[sq] == MK(side, t)) n++;
  return n;
}
static int is_endgame(const Pos *p) { return nonking_material(p, WHITE) < 1950 || nonking_material(p, BLACK) < 1950; }

/* material only; in the endgame a pawn is worth 120, or 190 if we have two or fewer */
int eng_eval(const Pos *p) {
  int sq, s = 0, eg = is_endgame(p);
  int pv[2] = { 100, 100 };
  if (eg) {
    pv[WHITE] = count_type(p, WHITE, PAWN) <= 2 ? 190 : 120;
    pv[BLACK] = count_type(p, BLACK, PAWN) <= 2 ? 190 : 120;
  }
  for (sq = 0; sq < 64; sq++) if (p->pc[sq]) {
    int c = PC(p->pc[sq]), t = PT(p->pc[sq]);
    int v = t == PAWN ? pv[c] : val[t];
    s += c == p->side ? v : -v;
  }
  return s;
}

/* ---- root positional analysis (thesis 2.2) ---- */

/* Fig 2.1 centre control */
static const int cca[64] = {
  0,1,2,3,3,2,1,0,
  1,3,4,5,5,4,3,1,
  2,4,6,7,7,6,4,2,
  3,5,7,8,8,7,5,3,
  3,5,7,8,8,7,5,3,
  2,4,6,7,7,6,4,2,
  1,3,4,5,5,4,3,1,
  0,1,2,3,3,2,1,0
};
static int cheb(int a, int b) {
  int df = abs(FILE_OF(a) - FILE_OF(b)), dr = abs(RANK_OF(a) - RANK_OF(b));
  return df > dr ? df : dr;
}
/* Fig 2.2: opponent's king field, by distance */
static int king_field(int sq, int ksq) {
  switch (cheb(sq, ksq)) { case 1: return 10; case 2: return 8; case 3: return 2; default: return 0; }
}
/* Fig 2.5: endgame mask to close in on the enemy king */
static int king_field_eg(int sq, int ksq) {
  switch (cheb(sq, ksq)) { case 0: case 1: case 2: return 10; case 3: return 5; default: return 4; }
}
/* relative square helpers for black */
static int rel(int sq, int us) { return us == WHITE ? sq : (sq ^ 56); }

static int pawn_on(const Pos *p, int side, int sq) { return p->pc[sq] == MK(side, PAWN); }
static int isolated_pawns(const Pos *p, int us) {
  int files[8] = {0}, sq, f, n = 0;
  for (sq = 0; sq < 64; sq++) if (p->pc[sq] == MK(us, PAWN)) files[FILE_OF(sq)]++;
  for (f = 0; f < 8; f++) if (files[f] && (f == 0 || !files[f - 1]) && (f == 7 || !files[f + 1])) n += files[f];
  return n;
}
static int is_passed(const Pos *p, int sq, int side) {
  int f = FILE_OF(sq), r, df;
  for (r = RANK_OF(sq) + (side == WHITE ? 1 : -1); r >= 0 && r < 8; r += side == WHITE ? 1 : -1)
    for (df = -1; df <= 1; df++)
      if (f + df >= 0 && f + df < 8 && p->pc[SQ(f + df, r)] == MK(side ^ 1, PAWN)) return 0;
  return 1;
}
static int file_opposed(const Pos *p, int sq, int side) {
  int f = FILE_OF(sq), r;
  for (r = RANK_OF(sq) + (side == WHITE ? 1 : -1); r >= 0 && r < 8; r += side == WHITE ? 1 : -1)
    if (p->pc[SQ(f, r)] == MK(side ^ 1, PAWN)) return 1;
  return 0;
}
static int mobility_after(const Pos *after, int us) {
  Pos q = *after; q.side = us; q.ep = -1;
  int tmp[256]; return gen(&q, tmp);
}

/* positional score of move m for side `us` in position p (before the move) */
static int tech_score(const Pos *p, int m) {
  int us = p->side, them = us ^ 1;
  int from = M_FROM(m), to = M_TO(m), piece = PT(p->pc[from]), cap = M_CAP(m);
  int rfrom = rel(from, us), rto = rel(to, us);
  int fullmove = game_ply / 2 + 1;
  int mat_us = nonking_material(p, us), mat_them = nonking_material(p, them);
  int endgame = mat_us < 1950 || mat_them < 1950;
  int opening = !endgame && fullmove <= 8;
  int s = 0;
  Pos after = *p; make(&after, m);
  int oppk = p->ksq[them];

  if (!endgame) {
    static const int prio_open[7] = { 0, 1, 4, 3, 2, 1, -1 };
    static const int prio_mid[7]  = { 0, 3, 4, 3, 2, 1, -1 };
    const int *prio = opening ? prio_open : prio_mid;
    int centre_pawn = cap == PAWN && (to == SQ(3,3) || to == SQ(4,3) || to == SQ(3,4) || to == SQ(4,4));

    s += (cca[to] - cca[from]) * prio[piece];
    if (piece == PAWN) {
      if (rfrom == SQ(4,1) && rto == SQ(4,3)) s += 30;        /* P-K4 */
      if (rfrom == SQ(4,2) && rto == SQ(4,3)) s += 2;         /* P K3-K4 */
      if (rfrom == SQ(3,1) && rto == SQ(3,3)) s += 20;        /* P-Q4 */
      if (rfrom == SQ(3,2) && rto == SQ(3,3)) s += 2;         /* P Q3-Q4 */
      if (FILE_OF(from) <= 1 || FILE_OF(from) >= 6) s -= opening ? 10 : 5;   /* wing pawn advance */
      if (cap) {
        double cf = fabs(FILE_OF(from) - 3.5), ct = fabs(FILE_OF(to) - 3.5);
        s += ct < cf ? 5 : -5;                                 /* capture toward / away from centre */
        if (isolated_pawns(&after, us) >= 2) s -= 10;
      }
    }
    if (is_castle(p, m)) s += to > from ? 30 : 10;             /* O-O, O-O-O */
    if (piece == KNIGHT && (rto == SQ(0,2) || rto == SQ(7,2))) s -= 15;      /* N-R3 */
    if (piece != PAWN && (rto == SQ(4,2) || rto == SQ(3,2)) && pawn_on(p, us, rto == SQ(4,2) ? rel(SQ(4,1), us) : rel(SQ(3,1), us)))
      s -= 50;                                                 /* piece to K3/Q3 blocking a pawn */
    if (piece != PAWN && FILE_OF(from) >= 4) s += 2;           /* piece moving from king side */
    if (centre_pawn) s += attackers(p, to, them) ? -15 : 50;   /* capture of a centre pawn */
    if (!opening) {
      s += mobility_after(&after, us);
      s += (king_field(to, oppk) - king_field(from, oppk)) * prio[piece];
      if (mat_us > mat_them && cap && cap != PAWN) s += 10;
      /* moving a piece that blocks our own c- or f-pawn */
      if (piece != PAWN && (rfrom == SQ(2,2) || rfrom == SQ(5,2)) && pawn_on(p, us, rel(rfrom - 8, us))) s += 5;
    }
  } else {
    static const int prio_eg[7] = { 0, 0, 4, 3, 1, 1, 4 };
    static const int adv_opposed[8]   = { 0, 0, 0, 2, 1, 3, 4, 6 };     /* ranks 3..8; last two [approx] */
    static const int adv_unopposed[8] = { 0, 0, 0, 3, 5, 10, 13, 23 };  /* rank 8 (promotion) is 80 */
    int pawns_only = mat_us == count_type(p, us, PAWN) * 100 && mat_them == count_type(p, them, PAWN) * 100;
    int bare_king = mat_them == 0;

    if (piece == PAWN) {
      int r = RANK_OF(rto) + 1;
      if (M_PROMO(m)) s += 80;
      else s += file_opposed(p, from, us) ? adv_opposed[r - 1] : adv_unopposed[r - 1];
      { int f = FILE_OF(to), rr, n = 0; for (rr = 0; rr < 8; rr++) if (after.pc[SQ(f, rr)] == MK(us, PAWN)) n++; if (n > 1) s -= 10; }
    } else {
      s += (cca[to] - cca[from]) * prio_eg[piece];
    }
    if (piece == ROOK) {
      /* rook behind a passed pawn, either colour */
      int f = FILE_OF(to), sq;
      for (sq = 0; sq < 64; sq++)
        if (FILE_OF(sq) == f && PT(after.pc[sq]) == PAWN && is_passed(&after, sq, PC(after.pc[sq]))) {
          int behind = PC(after.pc[sq]) == WHITE ? RANK_OF(to) < RANK_OF(sq) : RANK_OF(to) > RANK_OF(sq);
          if (behind) s += 15;
        }
    }
    if (piece == KING) {
      if (pawns_only) {
        /* [approx] Fig 2.3: king towards passed pawns */
        int sq, best_before = 99, best_after = 99;
        for (sq = 0; sq < 64; sq++) if (PT(p->pc[sq]) == PAWN && is_passed(p, sq, PC(p->pc[sq]))) {
          if (cheb(from, sq) < best_before) best_before = cheb(from, sq);
          if (cheb(to, sq) < best_after) best_after = cheb(to, sq);
        }
        if (best_after < best_before) s += 6;
        if (best_after == 1) s += 12;
      } else {
        s += (king_field_eg(to, oppk) - king_field_eg(from, oppk)) * 2;
      }
    }
    if (!pawns_only) s += mobility_after(&after, us);
    if (bare_king) {
      /* [approx] pieces v bare king: Gillogly ran a special 2-ply search
         scoring -32 x enemy king centrality; here each move is scored by
         how far it restricts the enemy king and brings our king closer */
      Pos q = after; q.side = them; q.ep = -1;
      int tmp[256], k = 0, i, n = gen_legal(&q, tmp);
      for (i = 0; i < n; i++) if (PT(q.pc[M_FROM(tmp[i])]) == KING) k += cca[M_TO(tmp[i])];
      s -= 4 * k;
      if (piece == KING) s += 16 * (cheb(from, oppk) - cheb(to, oppk));
    }
  }
  return s;
}

static int root_sc[256];
int eng_root_order(const Pos *p, int *mv, int n) {
  int sc[256], i, j;
  for (i = 0; i < n; i++) sc[i] = tech_score(p, mv[i]);
  for (i = 1; i < n; i++) {
    int m = mv[i], s = sc[i];
    for (j = i - 1; j >= 0 && sc[j] < s; j--) { mv[j + 1] = mv[j]; sc[j + 1] = sc[j]; }
    mv[j + 1] = m; sc[j + 1] = s;
  }
  memcpy(root_sc, sc, n * sizeof(int));
  return n;
}

/* interior nodes: captures of the most valuable pieces first */
int eng_select(const Pos *p, int *mv, int n, int ply, int depth, int prev) {
  int i, j;
  (void)p; (void)ply; (void)depth; (void)prev;
  for (i = 1; i < n; i++) {
    int m = mv[i], k = val[M_CAP(m)];
    for (j = i - 1; j >= 0 && val[M_CAP(mv[j])] < k; j--) mv[j + 1] = mv[j];
    mv[j + 1] = m;
  }
  return n;
}

void eng_init(void) { cfg.alphabeta = 1; cfg.qdepth = 16; cfg.fixed_depth = 0; }

void eng_options(void) { out("option name Depth type spin default 0 min 0 max 20\n"); }
int eng_setoption(const char *name, const char *value) {
  if (!strcmp(name, "Depth")) { cfg.fixed_depth = atoi(value); return 1; }
  return 0;
}

/* console: "pos" lists the positional pre-sort of the root moves */
int eng_command(const char *cmd, const Pos *p) {
  if (strcmp(cmd, "pos")) return 0;
  int mv[256], n = gen_legal(p, mv), i; char ms[8];
  n = eng_root_order(p, mv, n);
  for (i = 0; i < n && i < 10; i++) { move_str(mv[i], ms); out("%s %d\n", ms, root_sc[i]); }
  return 1;
}

int main(void) { return uci_main(); }
