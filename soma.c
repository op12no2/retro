/*
 * soma - John Maynard Smith's "Smith One-Move Analyzer", 1961.
 *
 *   J. Maynard Smith and D. Michie, "Machines that play games",
 *   New Scientist 12 (260), 9 November 1961, pp. 367-369.
 *
 * A one-ply analyser: every legal move is scored by the value of the
 * resulting position and the best is played, ties decided "by tossing a
 * coin". The value, for the side that has just moved (call it White):
 *
 *   (i)   material, White minus Black, P 10, Kt and B 30, R 50, Q 90, K 1000
 *   (ii)  square control: for each White piece, the squares it attacks
 *         (whether occupied or not) at 1 each, 2 for the four centre
 *         squares, 3 for a square next to the Black king. Thus 1.e4 scores
 *         +8 and so do 1.e3, 1.Nf3 and 1.Nc3.
 *   (iii) swap-off values: for each piece en prise, S is what its owner
 *         would lose "if both players behaved efficiently" (never below 0).
 *         Black pieces en prise: one with S > 0 gives White +5 "for the
 *         initiative"; two or more give the second highest S plus 5 for
 *         every other, since Black will move the piece with the highest S.
 *         White pieces en prise: subtract the highest S plus 5 for every
 *         other. The article's own example (king and rook en prise, S 1000
 *         and 50, scores +55) fixes the arithmetic: second highest + 5 x
 *         (n - 1).
 *
 * The article also mentions, without numbers, rules that encourage castling,
 * discourage leaving pieces where a pawn advance can attack them, and allow
 * for pinned pieces.
 *
 * [choice] castling +5; a piece standing where an enemy pawn could attack it
 *          after one push, -1; pinned pieces are not treated specially.
 * [choice] "squares to which that piece might legally move" is taken as the
 *          squares the piece attacks, pins ignored, pawns by their capture
 *          squares; this reproduces the article's +8 for the opening moves.
 * [choice] S is a static exchange evaluation, least valuable attacker first,
 *          stopping when a recapture does not pay.
 * [choice] Option Depth lets it look further ahead than one move, which the
 *          original never did.
 *
 * SOMA's one recorded game, against Michie's Machiavelli by hand over a
 * weekend in 1961, was drawn on move 29 with "combatants exhausted".
 */

#define ENGINE_NAME   "SOMA 1961"
#define ENGINE_AUTHOR "John Maynard Smith (recreated)"
#define ENG_OPTIONS
#define ENG_ROOT_ORDER
#define ENG_COMMAND
#include "retro.h"

static const int val[7] = { 0, 10, 30, 30, 50, 90, 1000 };
static int opt_castle = 5, opt_pawn_threat = 1;

static int square_value(int sq, int enemy_king) {
  int df = abs(FILE_OF(sq) - FILE_OF(enemy_king)), dr = abs(RANK_OF(sq) - RANK_OF(enemy_king));
  if ((df | dr) && df <= 1 && dr <= 1) return 3;
  if (sq == SQ(3,3) || sq == SQ(4,3) || sq == SQ(3,4) || sq == SQ(4,4)) return 2;
  return 1;
}

/* squares attacked by the piece on sq, summed by square_value */
static int piece_control(const Pos *p, int sq, int enemy_king) {
  int x = p->pc[sq], us = PC(x), s120 = mailbox64[sq], i, t, s = 0;
  switch (PT(x)) {
  case PAWN:
    for (i = -1; i <= 1; i += 2) { t = mailbox[s120 + (us == WHITE ? 10 : -10) + i]; if (t >= 0) s += square_value(t, enemy_king); }
    break;
  case KNIGHT:
    for (i = 0; i < 8; i++) { t = mailbox[s120 + knight_d[i]]; if (t >= 0) s += square_value(t, enemy_king); }
    break;
  case KING:
    for (i = 0; i < 8; i++) { t = mailbox[s120 + king_d[i]]; if (t >= 0) s += square_value(t, enemy_king); }
    break;
  default: {
    int nd = PT(x) == QUEEN ? 8 : 4;
    for (i = 0; i < nd; i++) {
      int d, y;
      if (PT(x) == ROOK) d = rook_d[i]; else if (PT(x) == BISHOP) d = bishop_d[i]; else d = i < 4 ? bishop_d[i] : rook_d[i - 4];
      y = s120 + d;
      while ((t = mailbox[y]) >= 0) { s += square_value(t, enemy_king); if (p->pc[t]) break; y += d; }
    }
  }
  }
  return s;
}

/* can an enemy pawn attack sq after a single advance? */
static int pawn_threat(const Pos *p, int sq, int us) {
  int them = us ^ 1, s120 = mailbox64[sq], i;
  for (i = -1; i <= 1; i += 2) {
    /* enemy pawn two ranks away on an adjacent file, with the square it would advance to empty */
    int from = mailbox[s120 + (us == WHITE ? 20 : -20) + i], step = mailbox[s120 + (us == WHITE ? 10 : -10) + i];
    if (from >= 0 && step >= 0 && p->pc[from] == MK(them, PAWN) && !p->pc[step]) return 1;
  }
  return 0;
}

/* SOMA's value of position p for the side `us` that has just moved (p->side is the opponent) */
static int soma_value(const Pos *p, int us, int castled) {
  int them = us ^ 1, sq, mat = 0, ctl = 0, v = 0;
  int own_s[32], no = 0, their_s[32], nt = 0, i, j;
  for (sq = 0; sq < 64; sq++) {
    int x = p->pc[sq], s;
    if (!x) continue;
    if (PC(x) == us) {
      mat += val[PT(x)];
      ctl += piece_control(p, sq, p->ksq[them]);
      if (PT(x) != PAWN && PT(x) != KING && pawn_threat(p, sq, us)) v -= opt_pawn_threat;
    } else mat -= val[PT(x)];
    s = see_en_prise(p, sq, val);
    if (s > 0) { if (PC(x) == us) own_s[no++] = s; else their_s[nt++] = s; }
  }
  /* sort descending */
  for (i = 1; i < no; i++) { int w = own_s[i]; for (j = i - 1; j >= 0 && own_s[j] < w; j--) own_s[j + 1] = own_s[j]; own_s[j + 1] = w; }
  for (i = 1; i < nt; i++) { int w = their_s[i]; for (j = i - 1; j >= 0 && their_s[j] < w; j--) their_s[j + 1] = their_s[j]; their_s[j + 1] = w; }
  if (nt == 1) v += 5;
  else if (nt >= 2) v += their_s[1] + 5 * (nt - 1);
  if (no >= 1) v -= own_s[0] + 5 * (no - 1);
  if (castled) v += opt_castle;
  return mat + ctl + v;
}

/* the search wants the side to move's view: minus the value for the side that just moved */
int eng_eval(const Pos *p) { return -soma_value(p, p->side ^ 1, 0); }

/* castling credit belongs to the move, so score root moves ourselves and
   shuffle them randomly first: the search keeps the first of equal moves,
   which is SOMA's coin toss */
static int root_val[256];
int eng_root_order(const Pos *p, int *mv, int n) {
  int i, j, sc[256];
  for (i = n - 1; i > 0; i--) { j = (int)(rng() % (uint64_t)(i + 1)); int t = mv[i]; mv[i] = mv[j]; mv[j] = t; }
  for (i = 0; i < n; i++) { Pos c = *p; make(&c, mv[i]); sc[i] = soma_value(&c, p->side, is_castle(p, mv[i])); }
  for (i = 1; i < n; i++) {
    int m = mv[i], s = sc[i];
    for (j = i - 1; j >= 0 && sc[j] < s; j--) { mv[j + 1] = mv[j]; sc[j + 1] = sc[j]; }
    mv[j + 1] = m; sc[j + 1] = s;
  }
  memcpy(root_val, sc, n * sizeof(int));
  return n;
}

void eng_init(void) {
  cfg.alphabeta = 1;
  cfg.fixed_depth = 1;
  cfg.qdepth = 0;
  rng_state ^= (uint64_t)now_ms() * 0x9E3779B97F4A7C15ULL;
}

void eng_options(void) {
  out("option name Depth type spin default 1 min 1 max 8\n");
  out("option name CastlingBonus type spin default 5 min 0 max 100\n");
  out("option name PawnThreatPenalty type spin default 1 min 0 max 100\n");
}
int eng_setoption(const char *name, const char *value) {
  if (!strcmp(name, "Depth")) { cfg.fixed_depth = atoi(value); return 1; }
  if (!strcmp(name, "CastlingBonus")) { opt_castle = atoi(value); return 1; }
  if (!strcmp(name, "PawnThreatPenalty")) { opt_pawn_threat = atoi(value); return 1; }
  return 0;
}

/* console: "soma" lists each root move with its increase in value */
int eng_command(const char *cmd, const Pos *p) {
  if (strcmp(cmd, "soma")) return 0;
  int mv[256], n = gen_legal(p, mv), i, base = soma_value(p, p->side, 0); char ms[8];
  n = eng_root_order(p, mv, n);
  for (i = 0; i < n && i < 8; i++) { move_str(mv[i], ms); out("%s %+d\n", ms, root_val[i] - base); }
  return 1;
}

int main(void) { return uci_main(); }
