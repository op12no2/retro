/*
 * shannon - Claude E. Shannon, "Programming a Computer for Playing Chess",
 * Philosophical Magazine, Ser. 7, Vol. 41, No. 314, March 1950.
 * https://vision.unipv.it/IA1/ProgrammingaComputerforPlayingChess.pdf
 *
 * Evaluation, Shannon's f(P) in centipawns:
 *
 *   900 Q + 500 R + 300 (B+N) + 100 P
 *   - 50 (doubled + backward + isolated pawns)
 *   + 10 (mobility, counted as legal moves)
 *
 * Type A: full-width minimax to a fixed depth, continuing through captures
 *         until the position is quiescent.
 * Type B: a "plausible move generator" keeps only forcing moves (captures,
 *         checks, promotions, castling, moves of attacked pieces) plus the
 *         best few quiet moves by static evaluation, and forcing lines are
 *         searched deeper than quiet ones.
 *
 * Anachronisms, all optional or unavoidable: iterative deepening so that
 * x+y clocks can be honoured, alpha-beta (option, off by default: Shannon's
 * search was plain minimax), stand pat in the quiescence search (the paper
 * does not say whether an exchange at the horizon is compulsory, and
 * treating it as compulsory plays very badly), recognition of repetition
 * and fifty-move draws.
 *
 * [choice] Backward pawn: not isolated, no friendly pawn on an adjacent file
 *          level with or behind it, and the stop square controlled by an
 *          enemy pawn. Shannon defines none of his pawn terms.
 * [choice] Type B heuristics are an interpretation; Shannon gave principles,
 *          not a specification.
 */

#define ENGINE_NAME   "Shannon 1950"
#define ENGINE_AUTHOR "Claude E. Shannon (recreated)"
#define ENG_OPTIONS
#define ENG_SELECT
#define ENG_EXTEND
#include "retro.h"

static int opt_type_b = 0, opt_width = 7, opt_ext = 3;
static const int piece_val[7] = { 0, 100, 300, 300, 500, 900, 0 };

static int pawn_penalties(const Pos *p, int us) {
  int them = us ^ 1, sq, f, pen = 0;
  int files[2][8] = {{0}};
  for (sq = 0; sq < 64; sq++) if (PT(p->pc[sq]) == PAWN) files[PC(p->pc[sq])][FILE_OF(sq)]++;
  for (f = 0; f < 8; f++) if (files[us][f] > 1) pen += files[us][f] - 1;          /* doubled */
  for (sq = 0; sq < 64; sq++) {
    int r, support, back, s120, t;
    if (p->pc[sq] != MK(us, PAWN)) continue;
    f = FILE_OF(sq); r = RANK_OF(sq);
    if ((f == 0 || !files[us][f - 1]) && (f == 7 || !files[us][f + 1])) { pen++; continue; }   /* isolated */
    support = 0;
    for (t = 0; t < 64 && !support; t++)
      if (p->pc[t] == MK(us, PAWN) && abs(FILE_OF(t) - f) == 1 && (us == WHITE ? RANK_OF(t) <= r : RANK_OF(t) >= r)) support = 1;
    if (support) continue;
    s120 = mailbox64[sq] + (us == WHITE ? 10 : -10);
    back = 0;
    t = mailbox[s120 + (us == WHITE ? 10 : -10) - 1]; if (t >= 0 && p->pc[t] == MK(them, PAWN)) back = 1;
    t = mailbox[s120 + (us == WHITE ? 10 : -10) + 1]; if (t >= 0 && p->pc[t] == MK(them, PAWN)) back = 1;
    pen += back;                                                                    /* backward */
  }
  return pen;
}

int eng_eval(const Pos *p) {
  int mat[2] = {0, 0}, sq, us = p->side, them = us ^ 1;
  for (sq = 0; sq < 64; sq++)
    if (p->pc[sq] && PT(p->pc[sq]) != KING) mat[PC(p->pc[sq])] += piece_val[PT(p->pc[sq])];
  return (mat[us] - mat[them])
       - 50 * (pawn_penalties(p, us) - pawn_penalties(p, them))
       + 10 * (count_legal(p) - count_legal_other(p));
}

/* Type B plausible move generator: forcing moves plus the best `Width` quiet moves */
int eng_select(const Pos *p, int *mv, int n, int ply, int depth, int prev) {
  int quiet[256], qscore[256], out[256], nq = 0, k = 0, i, j, them = p->side ^ 1;
  (void)ply; (void)depth; (void)prev;
  if (!opt_type_b || in_check(p)) return n;
  for (i = 0; i < n; i++) {
    int m = mv[i], f = 0;
    if (M_CAP(m) || M_PROMO(m)) f = 1;
    else if (is_castle(p, m)) f = 1;
    else if (attacked(p, M_FROM(m), them)) f = 1;        /* piece in danger */
    else if (gives_check(p, m)) f = 1;
    if (f) out[k++] = m;
    else { Pos c = *p; make(&c, m); quiet[nq] = m; qscore[nq++] = -eng_eval(&c); }
  }
  for (i = 0; i < nq && i < opt_width; i++) {
    int bi = i, t;
    for (j = i + 1; j < nq; j++) if (qscore[j] > qscore[bi]) bi = j;
    t = quiet[i]; quiet[i] = quiet[bi]; quiet[bi] = t; t = qscore[i]; qscore[i] = qscore[bi]; qscore[bi] = t;
    out[k++] = quiet[i];
  }
  memcpy(mv, out, k * sizeof(int));
  return k;
}

/* Type B: a check, or a recapture on the square of the last capture, does not use up a ply */
int eng_extend(const Pos *child, int m, int prev) {
  if (!opt_type_b) return 0;
  return in_check(child) || (M_CAP(m) && M_CAP(prev) && M_TO(m) == M_TO(prev));
}

void eng_init(void) { cfg.alphabeta = 0; cfg.qdepth = 4; cfg.max_ext = opt_ext; }

void eng_options(void) {
  out("option name Type type combo default A var A var B\n");
  out("option name AlphaBeta type check default false\n");
  out("option name QuiescenceDepth type spin default 4 min 0 max 32\n");
  out("option name Width type spin default 7 min 1 max 50\n");
  out("option name Extensions type spin default 3 min 0 max 16\n");
}
int eng_setoption(const char *name, const char *value) {
  if (!strcmp(name, "Type")) { opt_type_b = toupper((unsigned char)value[0]) == 'B'; return 1; }
  if (!strcmp(name, "AlphaBeta")) { cfg.alphabeta = !strcmp(value, "true"); return 1; }
  if (!strcmp(name, "QuiescenceDepth")) { cfg.qdepth = atoi(value); return 1; }
  if (!strcmp(name, "Width")) { opt_width = atoi(value); return 1; }
  if (!strcmp(name, "Extensions")) { opt_ext = atoi(value); cfg.max_ext = opt_ext; return 1; }
  return 0;
}

int main(void) { return uci_main(); }
