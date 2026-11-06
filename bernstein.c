/*
 * bernstein - Alex Bernstein's IBM 704 chess program, 1957.
 *
 *   Bernstein, Roberts, Arbuckle, Belsky, "A chess playing program for the
 *     IBM 704", Proc. Western Joint Computer Conference, 1958
 *   Bernstein and Roberts, "Computer v. Chess-Player", Scientific American,
 *     June 1958
 *   Bernstein, "A chess playing program for the I.B.M. 704", Chess Review,
 *     July 1958
 *   Scans of all three are linked from
 *   https://www.chessprogramming.org/The_Bernstein_Chess_Program
 *
 * The first complete chess program. At every node a "plausible move table"
 * of at most SEVEN moves is filled by asking eight questions in order,
 * stopping when the table is full:
 *
 *   1. Is the king in check?
 *   2. (a) Can material be gained? (b) Can material be lost?
 *      (c) Can material be exchanged?
 *   3. Is castling possible?   (if so, castle: no other moves are added)
 *   4. Can minor pieces be developed?
 *   5. Can key squares be occupied?  (squares controlled by diagonally
 *      connected pawns)
 *   6. Can open files be occupied?
 *   7. Can any pawns be moved?       (P-K4, P-K3, P-Q4 first)
 *   8. Can any piece be moved?
 *
 * The tree is 7 wide and 4 half-moves deep, plain minimax, 2,800 positions,
 * eight minutes on the 704. The evaluation gives each side a score from
 * mobility, area control, king defence and material, "criteria 1, 2 and 3
 * are added together while 4 is multiplied by a large factor", and the
 * position is valued by "the ratio of its own and opponent's scores".
 * Piece values P 1, N 3, B 3, R 5, Q 9.
 *
 * [choice] The "large factor" was never published; 20 is used (option
 *          MaterialFactor). A square is "completely controlled" when one
 *          side attacks it and the other does not. Mobility counts legal
 *          moves. King defence counts the squares next to the king that
 *          its own side attacks. "Safety" in 2(b) means a square the
 *          opponent does not attack.
 * [choice] Material gain/loss/exchange are judged by a static exchange
 *          evaluation with the least valuable attacker first. Bernstein's
 *          tables recorded attackers and defenders per square, so this is
 *          close to what the 704 did.
 * [choice] Moves inside a question come in the piece order of Bernstein's
 *          Table I: K, Q, B, N, R, P. The opening then yields exactly the
 *          seven moves he lists: four knight moves, P-K4, P-K3, P-Q4.
 * [choice] Option Conservative reproduces the "2-minute mode": only moves
 *          that do not lower the static score are followed further, or the
 *          best two if none do.
 */

#define ENGINE_NAME   "Bernstein 1957"
#define ENGINE_AUTHOR "Alex Bernstein, M. de V. Roberts, T. Arbuckle, M. Belsky (recreated)"
#define ENG_OPTIONS
#define ENG_SELECT
#define ENG_COMMAND
#include "retro.h"

static const int val[7] = { 0, 1, 3, 3, 5, 9, 100 };
static int opt_factor = 20, opt_conservative = 0, opt_table = 7;

/* ---- evaluation ---- */

static int side_score(const Pos *p, int us) {
  int sq, mob, area = 0, kdef = 0, mat = 0, them = us ^ 1, k = p->ksq[us], i;
  Pos q = *p; q.side = us; q.ep = -1;
  mob = count_legal(&q);
  for (sq = 0; sq < 64; sq++) {
    int a = attacked(p, sq, us), b = attacked(p, sq, them);
    if (a && !b) area++;
    if (p->pc[sq] && PC(p->pc[sq]) == us && PT(p->pc[sq]) != KING) mat += val[PT(p->pc[sq])];
  }
  for (i = 0; i < 8; i++) {
    int t = mailbox[mailbox64[k] + king_d[i]];
    if (t >= 0 && attacked(p, t, us)) kdef++;
  }
  return mob + area + kdef + opt_factor * mat;
}

/* 1000 * ln(ratio) so that the ordering is Bernstein's and the sign flips cleanly */
int eng_eval(const Pos *p) {
  double a = side_score(p, p->side), b = side_score(p, p->side ^ 1);
  if (a < 1) a = 1;
  if (b < 1) b = 1;
  return (int)floor(1000.0 * log(a / b) + 0.5);
}

/* ---- the plausible move generator ---- */

static int piece_order(int t) {
  switch (t) { case KING: return 0; case QUEEN: return 1; case BISHOP: return 2; case KNIGHT: return 3; case ROOK: return 4; default: return 5; }
}

static int tab[256], ntab;
static int in_table(int m) { int i; for (i = 0; i < ntab; i++) if (tab[i] == m) return 1; return 0; }
static void add(int m) { if (ntab < opt_table && !in_table(m)) tab[ntab++] = m; }

/* add all moves of mv[] satisfying pred, in Table I piece order */
#define ADD_IF(cond) do { int o_; for (o_ = 0; o_ < 6 && ntab < opt_table; o_++) for (i = 0; i < n; i++) { int m = mv[i]; if (piece_order(PT(p->pc[M_FROM(m)])) == o_ && (cond)) add(m); } } while (0)

static int file_open(const Pos *p, int f) { int r; for (r = 0; r < 8; r++) if (PT(p->pc[SQ(f, r)]) == PAWN) return 0; return 1; }

static int key_square(const Pos *p, int sq, int us) {
  /* attacked by one of our pawns that is diagonally connected to another of our pawns */
  int s120 = mailbox64[sq], i;
  for (i = -1; i <= 1; i += 2) {
    int t = mailbox[s120 + (us == WHITE ? -10 : 10) + i];      /* pawn that would attack sq */
    if (t >= 0 && p->pc[t] == MK(us, PAWN)) {
      int t120 = mailbox64[t], j, connected = 0;
      for (j = -1; j <= 1; j += 2) {
        int u = mailbox[t120 + 10 + j]; if (u >= 0 && p->pc[u] == MK(us, PAWN)) connected = 1;
        u = mailbox[t120 - 10 + j];     if (u >= 0 && p->pc[u] == MK(us, PAWN)) connected = 1;
      }
      if (connected) return 1;
    }
  }
  return 0;
}

static int plausible(const Pos *p, int *mv, int n) {
  int i, us = p->side, them = us ^ 1;
  ntab = 0;

  /* 1. king in check: king moves, interpositions, captures of the checker: every legal move */
  if (in_check(p)) { ADD_IF(1); goto done; }

  /* 2a. gain material */
  ADD_IF(M_CAP(mv[i]) && see_capture(p, M_TO(mv[i]), us, val[PT(p->pc[M_FROM(mv[i])])], val) > 0);
  /* 2b. lose material: move attacked pieces to safety */
  {
    int sq;
    for (sq = 0; sq < 64 && ntab < opt_table; sq++) {
      int x = p->pc[sq];
      if (!x || PC(x) != us || PT(x) == KING) continue;
      if (see_en_prise(p, sq, val) <= 0) continue;         /* not attacked, or adequately defended */
      for (i = 0; i < n; i++) if (M_FROM(mv[i]) == sq && !attacked(p, M_TO(mv[i]), them)) add(mv[i]);
    }
  }
  /* 2c. exchanges */
  ADD_IF(M_CAP(mv[i]) && see_capture(p, M_TO(mv[i]), us, val[PT(p->pc[M_FROM(mv[i])])], val) == 0);
  if (ntab >= opt_table) goto done;

  /* 3. castling: if possible, castle and consider nothing else */
  for (i = 0; i < n; i++) if (is_castle(p, mv[i])) { add(mv[i]); goto done; }

  /* 4. develop minor pieces */
  ADD_IF((PT(p->pc[M_FROM(mv[i])]) == KNIGHT || PT(p->pc[M_FROM(mv[i])]) == BISHOP) &&
         RANK_OF(M_FROM(mv[i])) == (us == WHITE ? 0 : 7) && !M_CAP(mv[i]));
  /* 5. occupy key squares */
  ADD_IF(PT(p->pc[M_FROM(mv[i])]) != PAWN && !M_CAP(mv[i]) && key_square(p, M_TO(mv[i]), us));
  /* 6. occupy open files */
  ADD_IF((PT(p->pc[M_FROM(mv[i])]) == ROOK || PT(p->pc[M_FROM(mv[i])]) == QUEEN) && !M_CAP(mv[i]) &&
         file_open(p, FILE_OF(M_TO(mv[i]))) && FILE_OF(M_TO(mv[i])) != FILE_OF(M_FROM(mv[i])));
  /* 7. pawn moves, P-K4 P-K3 P-Q4 first */
  {
    int pri[3][2] = { { SQ(4,1), SQ(4,3) }, { SQ(4,1), SQ(4,2) }, { SQ(3,1), SQ(3,3) } }, k;
    for (k = 0; k < 3; k++) for (i = 0; i < n; i++) {
      int f = M_FROM(mv[i]), t = M_TO(mv[i]);
      if (us == BLACK) { f ^= 56; t ^= 56; }
      if (PT(p->pc[M_FROM(mv[i])]) == PAWN && f == pri[k][0] && t == pri[k][1]) add(mv[i]);
    }
    ADD_IF(PT(p->pc[M_FROM(mv[i])]) == PAWN);
  }
  /* 8. any piece */
  ADD_IF(1);
done:
  memcpy(mv, tab, ntab * sizeof(int));
  return ntab;
}

int eng_select(const Pos *p, int *mv, int n, int ply, int depth, int prev) {
  (void)ply; (void)depth; (void)prev;
  n = plausible(p, mv, n);
  if (opt_conservative && n > 2) {
    /* 2-minute mode: keep moves that do not lower the score, else the best two */
    int sc[256], keep[256], k = 0, i, j, here = eng_eval(p);
    for (i = 0; i < n; i++) { Pos c = *p; make(&c, mv[i]); sc[i] = -eng_eval(&c); }
    for (i = 0; i < n; i++) if (sc[i] >= here) keep[k++] = mv[i];
    if (k == 0) {
      for (i = 1; i < n; i++) { int m = mv[i], s = sc[i]; for (j = i - 1; j >= 0 && sc[j] < s; j--) { mv[j + 1] = mv[j]; sc[j + 1] = sc[j]; } mv[j + 1] = m; sc[j + 1] = s; }
      k = 2; memcpy(keep, mv, 2 * sizeof(int));
    }
    memcpy(mv, keep, k * sizeof(int)); n = k;
  }
  return n;
}

void eng_init(void) {
  cfg.alphabeta = 0;      /* Bernstein declined McCarthy's suggestion of alpha-beta */
  cfg.fixed_depth = 4;
  cfg.qdepth = 0;         /* no quiescence: the 704 evaluated at four half-moves */
}

void eng_options(void) {
  out("option name Depth type spin default 4 min 1 max 12\n");
  out("option name TableSize type spin default 7 min 1 max 50\n");
  out("option name MaterialFactor type spin default 20 min 1 max 1000\n");
  out("option name AlphaBeta type check default false\n");
  out("option name Conservative type check default false\n");
}
int eng_setoption(const char *name, const char *value) {
  if (!strcmp(name, "Depth")) { cfg.fixed_depth = atoi(value); return 1; }
  if (!strcmp(name, "TableSize")) { opt_table = atoi(value); if (opt_table > 250) opt_table = 250; return 1; }
  if (!strcmp(name, "MaterialFactor")) { opt_factor = atoi(value); return 1; }
  if (!strcmp(name, "AlphaBeta")) { cfg.alphabeta = !strcmp(value, "true"); return 1; }
  if (!strcmp(name, "Conservative")) { opt_conservative = !strcmp(value, "true"); return 1; }
  return 0;
}

/* console: "table" shows the plausible move table for the position */
int eng_command(const char *cmd, const Pos *p) {
  if (strcmp(cmd, "table")) return 0;
  int mv[256], n = gen_legal(p, mv), i; char ms[8];
  n = plausible(p, mv, n);
  for (i = 0; i < n; i++) { move_str(mv[i], ms); out("%s ", ms); }
  out("\n");
  return 1;
}

int main(void) { return uci_main(); }
