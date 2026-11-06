/*
 * eloworld - Tom Murphy VII's zero-lookahead players from "Elo World", 2019.
 *
 *   Tom Murphy VII, "Elo World, a framework for benchmarking weak chess
 *   engines", SIGBOVIK 2019.  http://tom7.org/chess/weak.pdf
 *   Source: tom7misc svn, trunk/chess/ (player.cc, player-util.cc, chess.cc)
 *   https://sourceforge.net/p/tom7misc/svn/HEAD/tree/trunk/chess/
 *   Video: https://www.youtube.com/watch?v=DpXy041BIlA
 *
 * Murphy pitted dozens of deliberately silly chess "engines" against one
 * another to calibrate an Elo scale that reaches down to the worst possible
 * play. This file recreates the ones that look no further than the move in
 * hand (plus single_player, which looks several moves ahead but pretends the
 * opponent never moves). Which one you get is the UCI option `Player`:
 *
 *   random_move      a legal move, uniformly at random
 *   first_move       the lexicographically first move by Murphy's move code
 *                    (source square, destination, promotion; rows counted
 *                    from the player's own side)
 *   alphabetical     the alphabetically first move in short algebraic
 *                    notation ("Bxf7" < "Nf3" < "O-O" < "a3"; no + or #)
 *   alphamove        the alphabetically median move, index n/2 rounded down,
 *                    after xkcd 3045 (added by Murphy in 2025)
 *   cccp             Checkmate, Check, Capture (biggest first), Push
 *                    (furthest forward, then nearest the centre), then
 *                    the best promotion piece, then Murphy's move code
 *   pacifist         avoid mating, then checking, then capturing (least
 *                    valuable capture if forced); random among the rest
 *   min_oppt_moves   leave the opponent the fewest legal replies
 *   suicide_king     minimise the Chebyshev distance between the kings
 *   reverse_starting put one's own pieces where the opponent's started
 *   huddle           minimise the summed Chebyshev distance of one's own
 *                    pieces to one's own king
 *   swarm            the same, to the opponent's king
 *   generous         maximise the material the opponent can take next move
 *                    (summed over all capturing replies)
 *   no_i_insist      as generous, but by expected gift per reply; refuses to
 *                    mate and dislikes stalemating
 *   same_color       white likes white squares, black likes black squares
 *   opposite_color   the reverse
 *   sym_mirror_y     make the board mirror-symmetric about the horizontal
 *                    axis (rank 1 against rank 8), pieces of opposite colour
 *   sym_mirror_x     the same about the vertical axis (a-file against h-file)
 *   sym_180          the same under a 180 degree rotation
 *   single_player    find the shortest mate within `Depth` moves as if the
 *                    opponent passed every turn, and play its first move;
 *                    otherwise a random move (Murphy's tournament depth: 4)
 *
 * Every "EvalResultPlayer" (pacifist down to sym_180) scores the position
 * after each legal move with a penalty, plays the lowest, and breaks ties
 * with a random number drawn per move. The rest are deterministic, except
 * random_move and single_player's fallback.
 *
 * Elo from the paper's 2019 tournament (random_move anchors nothing; the
 * scale is relative): pacifist 287, alphabetical 321, generous 343,
 * first_move 358, no_i_insist 378, huddle 400, sym_180 430, same_color 431,
 * opposite_color 432, sym_mirror_x 439, random_move 439, sym_mirror_y 443,
 * reverse_starting 446, suicide_king 447, swarm 534, cccp 554,
 * min_oppt_moves 597, single_player4 (not in the paper's table).
 *
 * Fidelity notes:
 * [quirk] Murphy's board keeps a rook that may still castle as a distinct
 *         piece type (C_ROOK). It is worth 5 like a rook, but the symmetry
 *         players count C_ROOK against ROOK as a type mismatch, and cccp's
 *         comparator treats a capture of one as "different" from a capture
 *         of the other. Reproduced: a rook on its home square with its
 *         castling right intact is a C_ROOK here too.
 * [quirk] first_move and cccp encode the promotion piece with Murphy's
 *         colour bit (8 for black), so black's promotion codes spill into
 *         the next destination square. Reproduced.
 * [quirk] cccp's move code is not row-flipped for black (Murphy's own XXX
 *         comment). Reproduced.
 * [quirk] cccp's comparator answers "not better" when two captures differ in
 *         piece but not in value (knight against bishop), so the earlier
 *         move in Murphy's generation order wins without looking at the
 *         later criteria. Reproduced, including the generation order.
 * [quirk] no_i_insist's "every reply must capture" tier can never fire,
 *         because its minimum starts at 0. Reproduced; the player is the
 *         expected-value tier plus the mate and stalemate penalties.
 * [quirk] alphamove's "rounding down" takes index n/2, which for an even
 *         count is the later of the two middle moves. Reproduced.
 * [choice] Murphy's random numbers come from an ARC4 stream seeded with the
 *          time; ours from retro.h's xorshift seeded with the time. Only
 *          the distribution matters (uniform choice, uniform tie-break).
 * [choice] single_player clears the en passant square when it pretends the
 *          same side moves again; Murphy's code leaves the double-push
 *          state set, so his searcher could consider taking its own pawn.
 *          That would never be chosen over a mate, so the sequences agree.
 * [choice] Moves are generated by retro.h but re-ordered to Murphy's
 *          generation order (squares scanned from a8 along each rank
 *          towards h1, each piece's moves in his order) wherever the order
 *          could change the choice (cccp, single_player).
 * [choice] Murphy's programs had no clock. Ours ignore the clock too; only
 *          single_player takes measurable time (a few seconds at depth 4).
 */

#define ENGINE_NAME   "Elo World 2019"
#define ENGINE_AUTHOR "Tom Murphy VII (recreated)"
#define ENG_OPTIONS
#define ENG_ROOT_ORDER
#define ENG_COMMAND
#include "retro.h"

enum { P_RANDOM, P_FIRST, P_ALPHABETICAL, P_ALPHAMOVE, P_CCCP, P_PACIFIST, P_MIN_OPPT,
       P_SUICIDE, P_REVERSE, P_HUDDLE, P_SWARM, P_GENEROUS, P_NO_I_INSIST, P_SAME,
       P_OPPOSITE, P_SYM_Y, P_SYM_X, P_SYM_180, P_SINGLE, N_PLAYERS };
static const char *player_name[N_PLAYERS] = {
  "random_move", "first_move", "alphabetical", "alphamove", "cccp", "pacifist",
  "min_oppt_moves", "suicide_king", "reverse_starting", "huddle", "swarm", "generous",
  "no_i_insist", "same_color", "opposite_color", "sym_mirror_y", "sym_mirror_x",
  "sym_180", "single_player" };
static int player = P_RANDOM, sp_depth = 4;

typedef long long i64;
#define C_ROOK 7                        /* Murphy's rook-that-may-still-castle */

/* Murphy's coordinates: row 0 is rank 8, col 0 is the a-file */
#define ROW(sq) (7 - RANK_OF(sq))
#define COL(sq) FILE_OF(sq)
static int imax(int a, int b) { return a > b ? a : b; }
static int imin(int a, int b) { return a < b ? a : b; }

/* the piece as Murphy's board would hold it: rooks still able to castle are C_ROOK */
static int mpiece(const Pos *p, int sq) {
  int x = p->pc[sq];
  if (PT(x) == ROOK) {
    if (PC(x) == WHITE && ((sq == 7 && (p->castle & 1)) || (sq == 0 && (p->castle & 2)))) return MK(WHITE, C_ROOK);
    if (PC(x) == BLACK && ((sq == 63 && (p->castle & 4)) || (sq == 56 && (p->castle & 8)))) return MK(BLACK, C_ROOK);
  }
  return x;
}

static int type_value(int t) {
  switch (t) {
  case PAWN: return 1;
  case KNIGHT: case BISHOP: return 3;
  case ROOK: case C_ROOK: return 5;
  case QUEEN: return 9;
  default: return 0;                    /* including the king */
  }
}

static int is_black_square(int sq) { return (ROW(sq) ^ COL(sq)) & 1; }

/* does the side to move have any legal move? (early exit) */
static int has_legal(const Pos *p) {
  int tmp[256], n = gen(p, tmp), i;
  for (i = 0; i < n; i++) {
    Pos c = *p; make(&c, tmp[i]);
    if (!attacked(&c, c.ksq[p->side], c.side)) return 1;
  }
  return 0;
}
static int is_mated(const Pos *p) { return in_check(p) && !has_legal(p); }

/* ------------------------------------------------------------------ */
/* Murphy's move generation order                                       */
/* ------------------------------------------------------------------ */

static int find_move(const int *mv, int n, int from, int to, int promo) {
  int i;
  for (i = 0; i < n; i++) if (M_FROM(mv[i]) == from && M_TO(mv[i]) == to && M_PROMO(mv[i]) == promo) return i;
  return -1;
}

/* re-order legal moves mv[0..n) into the order CollectLegalMoves emits them */
static int ord_out[256], ord_n;
static void ord_try(const int *mv, int n, int from, int r, int c, int promo) {
  int i;
  if (r < 0 || r > 7 || c < 0 || c > 7) return;
  i = find_move(mv, n, from, SQ(c, 7 - r), promo);
  if (i >= 0) ord_out[ord_n++] = mv[i];
}
static void murphy_order(const Pos *p, int *mv, int n) {
  int black = p->side == BLACK, srcr, srcc, i, j;
  ord_n = 0;
  for (srcr = 0; srcr < 8; srcr++) for (srcc = 0; srcc < 8; srcc++) {
    int from = SQ(srcc, 7 - srcr), x = p->pc[from], udr, udc, dc, dr;
    if (!x || PC(x) != p->side) continue;
    switch (PT(x)) {
    case PAWN: {
      int d = black ? 1 : -1;
      if (srcr == (black ? 1 : 6)) ord_try(mv, n, from, srcr + 2 * d, srcc, 0);
      for (dc = -1; dc <= 1; dc++) {
        if (srcr + d == (black ? 7 : 0)) {
          ord_try(mv, n, from, srcr + d, srcc + dc, KNIGHT);
          ord_try(mv, n, from, srcr + d, srcc + dc, BISHOP);
          ord_try(mv, n, from, srcr + d, srcc + dc, ROOK);
          ord_try(mv, n, from, srcr + d, srcc + dc, QUEEN);
        } else ord_try(mv, n, from, srcr + d, srcc + dc, 0);
      }
      break;
    }
    case KNIGHT:
      for (udr = -1; udr <= 1; udr += 2) for (udc = -1; udc <= 1; udc += 2) {
        ord_try(mv, n, from, srcr + udr, srcc + 2 * udc, 0);
        ord_try(mv, n, from, srcr + 2 * udr, srcc + udc, 0);
      }
      break;
    case ROOK: case QUEEN:
      for (dc = srcc - 1; dc >= 0; dc--) { ord_try(mv, n, from, srcr, dc, 0); if (p->pc[SQ(dc, 7 - srcr)]) break; }
      for (dc = srcc + 1; dc < 8; dc++)  { ord_try(mv, n, from, srcr, dc, 0); if (p->pc[SQ(dc, 7 - srcr)]) break; }
      for (dr = srcr - 1; dr >= 0; dr--) { ord_try(mv, n, from, dr, srcc, 0); if (p->pc[SQ(srcc, 7 - dr)]) break; }
      for (dr = srcr + 1; dr < 8; dr++)  { ord_try(mv, n, from, dr, srcc, 0); if (p->pc[SQ(srcc, 7 - dr)]) break; }
      if (PT(x) == ROOK) break;
      /* fall through */
    case BISHOP:
      for (udr = -1; udr <= 1; udr += 2) for (udc = -1; udc <= 1; udc += 2) {
        for (dr = srcr + udr, dc = srcc + udc; dr >= 0 && dc >= 0 && dr < 8 && dc < 8; dr += udr, dc += udc) {
          ord_try(mv, n, from, dr, dc, 0);
          if (p->pc[SQ(dc, 7 - dr)]) break;
        }
      }
      break;
    case KING:
      if (srcr == (black ? 0 : 7) && srcc == 4) { ord_try(mv, n, from, srcr, 2, 0); ord_try(mv, n, from, srcr, 6, 0); }
      for (dc = -1; dc <= 1; dc++) for (dr = -1; dr <= 1; dr++)
        if (dc || dr) ord_try(mv, n, from, srcr + dr, srcc + dc, 0);
      break;
    }
  }
  /* anything not matched (should never happen) keeps its place at the end */
  for (i = 0; i < n; i++) {
    for (j = 0; j < ord_n; j++) if (ord_out[j] == mv[i]) break;
    if (j == ord_n) ord_out[ord_n++] = mv[i];
  }
  memcpy(mv, ord_out, n * sizeof(int));
}

/* ------------------------------------------------------------------ */
/* short algebraic notation, Murphy's ShortMoveString                   */
/* ------------------------------------------------------------------ */

static void san(const Pos *p, int m, const int *legal, int n, char *s) {
  int from = M_FROM(m), to = M_TO(m), type = PT(p->pc[from]), i;
  const char letter[8] = " PNBRQKR";
  if (is_castle(p, m)) { strcpy(s, FILE_OF(to) < FILE_OF(from) ? "O-O-O" : "O-O"); return; }
  if (type == PAWN) {
    if (M_CAP(m)) { *s++ = 'a' + FILE_OF(from); *s++ = 'x'; }
  } else {
    int need = 0, same_row = 0, same_col = 0;
    for (i = 0; i < n; i++) {
      int f2 = M_FROM(legal[i]);
      if (f2 != from && M_TO(legal[i]) == to && M_PROMO(legal[i]) == M_PROMO(m) && PT(p->pc[f2]) == type) {
        need = 1;
        if (FILE_OF(f2) == FILE_OF(from)) same_col = 1;
        if (RANK_OF(f2) == RANK_OF(from)) same_row = 1;
      }
    }
    *s++ = letter[type];
    if (need) {
      if (same_col && same_row) { *s++ = 'a' + FILE_OF(from); *s++ = '1' + RANK_OF(from); }
      else if (same_col) *s++ = '1' + RANK_OF(from);
      else *s++ = 'a' + FILE_OF(from);
    }
    if (M_CAP(m)) *s++ = 'x';
  }
  *s++ = 'a' + FILE_OF(to); *s++ = '1' + RANK_OF(to);
  if (M_PROMO(m)) { *s++ = '='; *s++ = letter[M_PROMO(m)]; }
  *s = 0;
}

/* ------------------------------------------------------------------ */
/* position penalties (EvalResultPlayer): c is the position after the   */
/* move, so c.side is the opponent and c.side^1 the player              */
/* ------------------------------------------------------------------ */

static int knight_distance(int r1, int c1, int r2, int c2) {
  static const int kd[64] = {
    0, 3, 2, 3, 2, 3, 4, 5,  3, 2, 1, 2, 3, 4, 3, 4,  2, 1, 4, 3, 2, 3, 4, 5,  3, 2, 3, 2, 3, 4, 3, 4,
    2, 3, 2, 3, 4, 3, 4, 5,  3, 4, 3, 4, 3, 4, 5, 4,  4, 3, 4, 3, 4, 5, 4, 5,  5, 4, 5, 4, 5, 4, 5, 6 };
  return kd[abs(r2 - r1) * 8 + abs(c2 - c1)];
}

static i64 pen_pacifist(const Pos *c) {
  i64 material = 0; int sq;
  if (is_mated(c)) return 0xFFFFFFFFFFFFLL;
  for (sq = 0; sq < 64; sq++) if (c->pc[sq] && PC(c->pc[sq]) == c->side) material += type_value(PT(mpiece(c, sq)));
  return in_check(c) ? 1000000 - material : -material;
}

static i64 pen_suicide_king(const Pos *c) {
  return imax(abs(RANK_OF(c->ksq[0]) - RANK_OF(c->ksq[1])), abs(FILE_OF(c->ksq[0]) - FILE_OF(c->ksq[1])));
}

static i64 pen_reverse_starting(const Pos *c) {
  int black = c->side == WHITE, pawn_row = black ? 6 : 1, back_row = black ? 7 : 0, sq;
  i64 dist = 0;
  for (sq = 0; sq < 64; sq++) {
    int x = c->pc[sq], r = ROW(sq), col = COL(sq);
    if (!x || PC(x) != (c->side ^ 1)) continue;
    switch (PT(x)) {
    case PAWN:   dist += abs(r - pawn_row); break;
    case ROOK:   dist += abs(r - back_row) + imin(col, 7 - col); break;
    case KNIGHT: dist += imin(knight_distance(r, col, back_row, 1), knight_distance(r, col, back_row, 6)); break;
    case QUEEN:  dist += imax(abs(r - back_row), abs(col - 3)); break;
    case KING:   dist += imax(abs(r - back_row), abs(col - 4)); break;
    case BISHOP: {
      int dest_col = is_black_square(sq) ? (black ? 2 : 5) : (black ? 5 : 2);
      dist += imax(abs(r - back_row), abs(col - dest_col));
      break;
    }
    }
  }
  return dist;
}

/* summed Chebyshev distance of the player's pieces to king ksq */
static i64 pen_gather(const Pos *c, int ksq) {
  int sq, us = c->side ^ 1; i64 dist = 0;
  for (sq = 0; sq < 64; sq++) if (c->pc[sq] && PC(c->pc[sq]) == us)
    dist += imax(abs(RANK_OF(sq) - RANK_OF(ksq)), abs(FILE_OF(sq) - FILE_OF(ksq)));
  return dist;
}

/* value the opponent's reply m would capture (en passant counts 1) */
static int captured_value(const Pos *c, int m) {
  int to = M_TO(m);
  if (c->pc[to]) return type_value(PT(mpiece(c, to)));
  return M_CAP(m) ? 1 : 0;
}

static i64 pen_generous(const Pos *c) {
  int mv[256], n = gen_legal(c, mv), i; i64 dist = 0;
  for (i = 0; i < n; i++) dist -= captured_value(c, mv[i]);
  return dist;
}

static i64 pen_no_i_insist(const Pos *c) {
  int mv[256], n = gen_legal(c, mv), i, total = 0, min_capture = 0;
  for (i = 0; i < n; i++) {
    int v = captured_value(c, mv[i]);
    min_capture = imin(min_capture, v);          /* never rises above 0: Murphy's bug, kept */
    total += v;
  }
  if (n == 0) return in_check(c) ? 0xFFFFFFFFFFFFLL : 0xFFFFFFFFLL;
  if (min_capture > 0) return -min_capture;
  return (i64)(-(total * 1000000.0) / n);
}

static i64 pen_color(const Pos *c, int same) {
  int sq, black = c->side == WHITE; i64 pen = 0;
  for (sq = 0; sq < 64; sq++) if (c->pc[sq] && PC(c->pc[sq]) == (c->side ^ 1))
    if ((black == is_black_square(sq)) == same) pen--;
  return pen;
}

static i64 pen_symmetry(const Pos *c, int flip) {
  int sq; i64 pen = 0;
  for (sq = 0; sq < 64; sq++) {
    int piece = mpiece(c, sq), other = mpiece(c, sq ^ flip);
    if (!piece && !other) continue;
    if (!piece || !other) { pen += 10; continue; }
    if (PC(piece) == PC(other)) { pen += 5; continue; }
    if (PT(piece) != PT(other)) pen++;
  }
  return pen;
}

static i64 penalty(const Pos *c) {
  switch (player) {
  case P_PACIFIST:    return pen_pacifist(c);
  case P_MIN_OPPT:    return count_legal(c);
  case P_SUICIDE:     return pen_suicide_king(c);
  case P_REVERSE:     return pen_reverse_starting(c);
  case P_HUDDLE:      return pen_gather(c, c->ksq[c->side ^ 1]);
  case P_SWARM:       return pen_gather(c, c->ksq[c->side]);
  case P_GENEROUS:    return pen_generous(c);
  case P_NO_I_INSIST: return pen_no_i_insist(c);
  case P_SAME:        return pen_color(c, 1);
  case P_OPPOSITE:    return pen_color(c, 0);
  case P_SYM_Y:       return pen_symmetry(c, 56);
  case P_SYM_X:       return pen_symmetry(c, 7);
  case P_SYM_180:     return pen_symmetry(c, 63);
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* the deterministic players                                            */
/* ------------------------------------------------------------------ */

/* Murphy's move code; flip=1 counts rows from black's side (first_move's black code) */
static int move_code(const Pos *p, int m, int flip) {
  int fr = M_FROM(m), to = M_TO(m);
  int src = (flip ? RANK_OF(fr) : ROW(fr)) * 8 + COL(fr);
  int dst = (flip ? RANK_OF(to) : ROW(to)) * 8 + COL(to);
  int promo = M_PROMO(m) ? MK(p->side, M_PROMO(m)) : 0;   /* Murphy's promote_to carries the colour bit */
  return (src * 64 + dst) * 8 + promo;
}

typedef struct { int m, is_mate, is_check, captured, promo; } Labeled;

static int center_distance(int col) { return col < 4 ? 3 - col : col - 4; }

/* cccp: is a better than b? Murphy's comparator, verbatim */
static int cccp_better(const Labeled *a, const Labeled *b, int black, const Pos *p) {
  if (a->is_mate != b->is_mate) return a->is_mate;
  if (a->is_check != b->is_check) return a->is_check;
  if (a->captured != b->captured) return type_value(b->captured) < type_value(a->captured);
  if (ROW(M_TO(a->m)) != ROW(M_TO(b->m))) return black ? ROW(M_TO(b->m)) < ROW(M_TO(a->m)) : ROW(M_TO(a->m)) < ROW(M_TO(b->m));
  if (COL(M_TO(a->m)) != COL(M_TO(b->m))) {
    int acs = center_distance(COL(M_TO(a->m))), bcs = center_distance(COL(M_TO(b->m)));
    if (acs != bcs) return acs < bcs;
  }
  if (a->promo != b->promo) return type_value(PT(b->promo)) < type_value(PT(a->promo));
  return move_code(p, a->m, 0) < move_code(p, b->m, 0);
}

static int choose_cccp(const Pos *p, int *mv, int n) {
  Labeled lab[256]; int i, best = 0;
  murphy_order(p, mv, n);
  for (i = 0; i < n; i++) {
    Pos c = *p; make(&c, mv[i]);
    lab[i].m = mv[i];
    lab[i].is_check = in_check(&c);
    lab[i].is_mate = lab[i].is_check && !has_legal(&c);
    lab[i].captured = p->pc[M_TO(mv[i])] ? PT(mpiece(p, M_TO(mv[i]))) : (M_CAP(mv[i]) ? PAWN : EMPTY);
    lab[i].promo = M_PROMO(mv[i]) ? MK(p->side, M_PROMO(mv[i])) : 0;
  }
  for (i = 1; i < n; i++) if (cccp_better(&lab[i], &lab[best], p->side == BLACK, p)) best = i;
  return mv[best];
}

/* single_player: shortest mate within depth moves if the opponent never moves.
   Returns the length found (0 if none); seq[0] is the move to play. */
static int find_mate(const Pos *p, int depth, int *seq) {
  int mv[256], n = gen_legal(p, mv), i, best_len = 0, tmp[16];
  murphy_order(p, mv, n);
  for (i = 0; i < n; i++) {
    Pos c = *p; int chk, sd, len;
    make(&c, mv[i]);
    chk = in_check(&c);
    if (!has_legal(&c)) {
      if (chk) { seq[0] = mv[i]; return 1; }   /* a one-move mate: nothing can be shorter */
      continue;                                 /* stalemate is no good */
    }
    if (chk || depth <= 1) continue;            /* can't hand the move back while he is in check */
    c.side ^= 1; c.ep = -1;                     /* pretend it is our move again [choice: ep cleared] */
    sd = depth - 1;
    if (best_len) sd = imin(sd, best_len - 2);  /* only strictly shorter sequences matter */
    len = find_mate(&c, sd, tmp);
    if (len && (!best_len || len + 1 < best_len)) {
      seq[0] = mv[i]; memcpy(seq + 1, tmp, len * sizeof(int)); best_len = len + 1;
    }
  }
  return best_len;
}

/* ------------------------------------------------------------------ */
/* choosing the move                                                    */
/* ------------------------------------------------------------------ */

static i64 last_pen[256];                     /* per move, for the console command */
static char last_san[256][8];
static int last_mate[16], last_mate_len;      /* single_player's sequence */

static int choose(const Pos *p, int *mv, int n) {
  int i, best;
  switch (player) {
  case P_RANDOM:
    return mv[rng() % (uint64_t)n];
  case P_FIRST: {
    int flip = p->side == BLACK;
    for (i = 0; i < n; i++) last_pen[i] = move_code(p, mv[i], flip);
    for (best = 0, i = 1; i < n; i++) if (last_pen[i] < last_pen[best]) best = i;
    return mv[best];
  }
  case P_ALPHABETICAL: case P_ALPHAMOVE: {
    int idx[256], j;
    for (i = 0; i < n; i++) { san(p, mv[i], mv, n, last_san[i]); idx[i] = i; }
    for (i = 1; i < n; i++) {                   /* sort indices by string */
      int k = idx[i];
      for (j = i - 1; j >= 0 && strcmp(last_san[idx[j]], last_san[k]) > 0; j--) idx[j + 1] = idx[j];
      idx[j + 1] = k;
    }
    for (i = 0; i < n; i++) last_pen[idx[i]] = i + 1;
    return mv[idx[player == P_ALPHAMOVE ? n >> 1 : 0]];
  }
  case P_CCCP:
    return choose_cccp(p, mv, n);
  case P_SINGLE: {
    int seq[16];
    if ((last_mate_len = find_mate(p, sp_depth, seq))) { memcpy(last_mate, seq, last_mate_len * sizeof(int)); return seq[0]; }
    return mv[rng() % (uint64_t)n];
  }
  default: {                                    /* EvalResultPlayer: lowest penalty, random tie-break */
    uint32_t r[256];
    for (i = 0; i < n; i++) {
      Pos c = *p; make(&c, mv[i]);
      r[i] = (uint32_t)(rng() >> 32);
      last_pen[i] = penalty(&c);
    }
    for (best = 0, i = 1; i < n; i++)
      if (last_pen[i] < last_pen[best] || (last_pen[i] == last_pen[best] && r[i] < r[best])) best = i;
    return mv[best];
  }
  }
}

/* the whole player lives here: pick the move, hand the driver just that one */
int eng_root_order(const Pos *p, int *mv, int n) {
  mv[0] = choose(p, mv, n);
  return 1;
}

int eng_eval(const Pos *p) { (void)p; return 0; }

void eng_init(void) {
  cfg.alphabeta = 1;
  cfg.fixed_depth = 1;
  cfg.qdepth = 0;
  rng_state ^= (uint64_t)now_ms() * 0x9E3779B97F4A7C15ULL;
}

void eng_options(void) {
  int i;
  out("option name Player type combo default %s", player_name[player]);
  for (i = 0; i < N_PLAYERS; i++) out(" var %s", player_name[i]);
  out("\n");
  out("option name Depth type spin default 4 min 1 max 5\n");
}
int eng_setoption(const char *name, const char *value) {
  int i;
  if (!strcmp(name, "Player")) {
    for (i = 0; i < N_PLAYERS; i++) if (!strcmp(value, player_name[i])) player = i;
    return 1;
  }
  if (!strcmp(name, "Depth")) { sp_depth = atoi(value); if (sp_depth < 1) sp_depth = 1; if (sp_depth > 5) sp_depth = 5; return 1; }
  return 0;
}

/* console: "elo [player]" lists the legal moves in Murphy's order with the
   player's number for each (penalty, move code or alphabetical rank) and
   marks the choice; a player name as argument switches player first */
int eng_command(const char *cmd, const Pos *p) {
  int mv[256], ord[256], n, i, chosen;
  char *arg;
  if (strcmp(cmd, "elo")) return 0;
  while ((arg = strtok(NULL, " ")))
    for (i = 0; i < N_PLAYERS; i++) if (!strcmp(arg, player_name[i])) player = i;
  n = gen_legal(p, mv);
  if (!n) { out("no legal moves\n"); return 1; }
  memcpy(ord, mv, n * sizeof(int));
  chosen = choose(p, ord, n);                   /* last_pen[] is indexed like mv[] */
  memcpy(ord, mv, n * sizeof(int));
  murphy_order(p, ord, n);
  out("%s\n", player_name[player]);
  if (player == P_SINGLE) {
    if (last_mate_len) { char ms[8]; out("mate in %d:", last_mate_len); for (i = 0; i < last_mate_len; i++) { move_str(last_mate[i], ms); out(" %s", ms); } out("\n"); }
    else out("no mate within %d\n", sp_depth);
  }
  for (i = 0; i < n; i++) {
    char ms[8], sn[8]; int j;
    move_str(ord[i], ms); san(p, ord[i], mv, n, sn);
    out("%-6s %-7s", ms, sn);
    if (player == P_FIRST || player == P_ALPHABETICAL || player == P_ALPHAMOVE || (player >= P_PACIFIST && player <= P_SYM_180)) {
      for (j = 0; j < n && mv[j] != ord[i]; j++) ;
      out(" %lld", last_pen[j]);
    }
    out("%s\n", ord[i] == chosen ? "  <--" : "");
  }
  return 1;
}

int main(void) { return uci_main(); }
