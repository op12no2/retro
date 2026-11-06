/*
 * kotok - the Kotok-McCarthy program, MIT 1959-62.
 *
 *   Alan Kotok, "A Chess Playing Program for the IBM 7090 Computer",
 *   B.S. thesis, MIT, June 1962 (= AI Memo 41). https://dspace.mit.edu/handle/1721.1/17406 With Elwyn Berlekamp,
 *   Michael Lieberman, Charles Niessen, Robert Wagner; supervisor John
 *   McCarthy. Later improved by McCarthy at Stanford and beaten 3-1 by the
 *   ITEP program (Kaissa's ancestor) in the 1966-67 telegraph match.
 *
 * Search: alpha-beta (one of its first uses in chess), a Shannon type B
 * tree in which the moves at each ply are ordered by a one-ply evaluation
 * and only the best few are kept. From the thesis: "The number of moves
 * presented is a function of increasing ply. Current values in order of
 * increasing ply are: 4 3 2 2 1 1 1 1 0 0." The search ends at that depth
 * or earlier "at a stable position".
 *
 * Evaluation, weights as given in the thesis (unit: 1/60 pawn):
 *   material   P 1, N 3, B 3, R 5, Q 9 (x 60); when a side is at least 4
 *              points ahead the multipliers are adjusted to favour trading
 *   centre     control of the 16 centre squares, each worth 1/60 pawn,
 *              values seen from the mover's side:   8 8 4 4
 *                                                   4 8 8 4
 *                                                   2 4 4 2
 *                                                   1 1 1 1
 *              faded out between move 20 and move 30
 *   development 1 point per pawn, 3 or 4 per piece, weighted 1/15 pawn,
 *              faded out likewise
 *   pawns      open file +8, isolated -1, backward -5, doubled -3,
 *              passed +10, weighted 1/20 pawn
 *
 * [choice] "Controlling" a centre square means attacking it. "Stable" means
 *          the side to move has no capture that wins material. A developed
 *          piece is a knight or bishop off its home rank (3) or a castled
 *          king (4); a developed pawn is one off its start square (1).
 *          "Open file +8" is counted for a rook or queen on a file without
 *          pawns. The trade adjustment multiplies the material difference by
 *          1 + (material gone from the board) / (starting material).
 * [choice] The thesis's Appendix 1 contains the full FORTRAN/FAP listing,
 *          scanned without a text layer. This is a recreation from the
 *          thesis's description, not a transcription of the listing.
 */

#define ENGINE_NAME   "Kotok-McCarthy 1962"
#define ENGINE_AUTHOR "Alan Kotok, John McCarthy et al. (recreated)"
#define ENG_OPTIONS
#define ENG_SELECT
#define ENG_CUTOFF
#define ENG_COMMAND
#include "retro.h"

static const int val[7] = { 0, 1, 3, 3, 5, 9, 0 };
static int width[12] = { 4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0 };

/* centre table, rows from the mover's side: rank 3 (nearest) is the bottom row */
static const int centre_rows[4][4] = { { 8,8,4,4 }, { 4,8,8,4 }, { 2,4,4,2 }, { 1,1,1,1 } };
static int centre_value(int sq, int us) {
  int f = FILE_OF(sq), r = us == WHITE ? RANK_OF(sq) : 7 - RANK_OF(sq);
  if (f < 2 || f > 5 || r < 2 || r > 5) return 0;
  return centre_rows[5 - r][f - 2];
}

static double fade(void) {
  int move = game_ply / 2 + 1;
  if (move <= 20) return 1.0;
  if (move >= 30) return 0.0;
  return (30 - move) / 10.0;
}

static int side_terms(const Pos *p, int us, int *mat) {
  int sq, s = 0, m = 0, f, them = us ^ 1;
  int files[8] = {0}, efiles[8] = {0};
  for (sq = 0; sq < 64; sq++) {
    int x = p->pc[sq];
    if (PT(x) == PAWN) { if (PC(x) == us) files[FILE_OF(sq)]++; else efiles[FILE_OF(sq)]++; }
  }
  for (sq = 0; sq < 64; sq++) {
    int x = p->pc[sq], t = PT(x);
    if (!x || PC(x) != us) continue;
    if (t != KING) m += val[t];
    switch (t) {
    case PAWN: {
      int r = RANK_OF(sq), start = us == WHITE ? 1 : 6, pawn_pts = 0;
      f = FILE_OF(sq);
      if (r != start) s += 4;                                  /* development 1 x (1/15 pawn) */
      if ((f == 0 || !files[f - 1]) && (f == 7 || !files[f + 1])) pawn_pts -= 1;   /* isolated */
      if (files[f] > 1) pawn_pts -= 3;                         /* doubled, per extra pawn roughly */
      {
        int passed = 1, rr, df;
        for (rr = r + (us == WHITE ? 1 : -1); rr >= 0 && rr < 8; rr += us == WHITE ? 1 : -1)
          for (df = -1; df <= 1; df++) if (f + df >= 0 && f + df < 8 && p->pc[SQ(f + df, rr)] == MK(them, PAWN)) passed = 0;
        if (passed) pawn_pts += 10;
      }
      {
        /* backward: no friendly pawn beside or behind on adjacent files and the stop square is attacked by an enemy pawn */
        int support = 0, t2, stop = sq + (us == WHITE ? 8 : -8);
        for (t2 = 0; t2 < 64; t2++) if (p->pc[t2] == MK(us, PAWN) && abs(FILE_OF(t2) - f) == 1 && (us == WHITE ? RANK_OF(t2) <= r : RANK_OF(t2) >= r)) support = 1;
        if (!support && stop >= 0 && stop < 64 && attacked(p, stop, them) && !attacked(p, stop, us)) pawn_pts -= 5;
      }
      s += 3 * pawn_pts;                                       /* 1/20 pawn per point */
      break;
    }
    case KNIGHT: case BISHOP:
      if (RANK_OF(sq) != (us == WHITE ? 0 : 7)) s += 12;       /* development 3 points */
      break;
    case ROOK: case QUEEN:
      if (!files[FILE_OF(sq)] && !efiles[FILE_OF(sq)]) s += 24; /* open file +8 x 3 */
      break;
    case KING:
      if ((us == WHITE && (sq == 6 || sq == 2)) || (us == BLACK && (sq == 62 || sq == 58))) s += 16;   /* castled: 4 points */
      break;
    }
  }
  /* centre control */
  for (sq = 0; sq < 64; sq++) { int v = centre_value(sq, us); if (v && attacked(p, sq, us)) s += v; }
  *mat = m;
  return s;
}

int eng_eval(const Pos *p) {
  int mu, mt, su = side_terms(p, p->side, &mu), st = side_terms(p, p->side ^ 1, &mt);
  double material = 60.0 * (mu - mt);
  if (abs(mu - mt) >= 4) material *= 1.0 + (78.0 - (mu + mt)) / 78.0;   /* favour trading when well ahead */
  return (int)floor(material + fade() * (su - st) + 0.5);
}

/* keep the best `width[ply]` moves by one-ply evaluation */
int eng_select(const Pos *p, int *mv, int n, int ply, int depth, int prev) {
  int sc[256], i, j, w = ply < 12 ? width[ply] : 0;
  (void)depth; (void)prev;
  if (w < 1) w = 1;
  for (i = 0; i < n; i++) { Pos c = *p; make(&c, mv[i]); sc[i] = -eng_eval(&c); }
  for (i = 1; i < n; i++) {
    int m = mv[i], s = sc[i];
    for (j = i - 1; j >= 0 && sc[j] < s; j--) { mv[j + 1] = mv[j]; sc[j + 1] = sc[j]; }
    mv[j + 1] = m; sc[j + 1] = s;
  }
  return n < w ? n : w;
}

/* "stable position": nobody can win material by a capture */
int eng_cutoff(const Pos *p, int ply, int depth) {
  int mv[256], n = gen_legal(p, mv), i;
  (void)depth;
  if (ply < 2) return 0;
  for (i = 0; i < n; i++) if (M_CAP(mv[i]) && (val[M_CAP(mv[i])] > val[PT(p->pc[M_FROM(mv[i])])] || !attacked(p, M_TO(mv[i]), p->side ^ 1))) return 0;
  return 1;
}

void eng_init(void) { cfg.alphabeta = 1; cfg.fixed_depth = 8; cfg.qdepth = 0; }

void eng_options(void) {
  out("option name Depth type spin default 8 min 1 max 12\n");
  out("option name Widths type string default 4,3,2,2,1,1,1,1\n");
}
int eng_setoption(const char *name, const char *value) {
  if (!strcmp(name, "Depth")) { cfg.fixed_depth = atoi(value); return 1; }
  if (!strcmp(name, "Widths")) {
    int i = 0; const char *s = value;
    while (*s && i < 12) { width[i++] = atoi(s); while (*s && *s != ',') s++; if (*s == ',') s++; }
    while (i < 12) width[i++] = 1;
    return 1;
  }
  return 0;
}

int eng_command(const char *cmd, const Pos *p) {
  if (strcmp(cmd, "plaus")) return 0;
  int mv[256], n = gen_legal(p, mv), i; char ms[8];
  n = eng_select(p, mv, n, 0, 0, 0);
  for (i = 0; i < n; i++) { move_str(mv[i], ms); out("%s ", ms); }
  out("\n");
  return 1;
}

int main(void) { return uci_main(); }
