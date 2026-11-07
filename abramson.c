/*
 * abramson - Bruce Abramson's expected-outcome sampler, 1987.
 *
 *   B. Abramson, "The Expected-Outcome Model of Two-Player Games", PhD
 *   thesis, Columbia University, 1987 (report CUCS-315-87), published as
 *   a book by Pitman / Morgan Kaufmann in 1991. Also B. Abramson,
 *   "Expected-Outcome: A General Model of Static Evaluation", IEEE Trans.
 *   PAMI 12(2), 1990, pp. 182-193, and B. Abramson and R. Korf, "A Model
 *   of Two-Player Evaluation Functions", AAAI-87.
 *
 * "The expected-outcome value of a game-tree node, G, is given by a
 * player's expected payoff over an infinite number of random completions
 * of a game beginning at G" (thesis p. 30): WIN 1, LOSS 0, and for chess a
 * DRAW is half a win, "Chess1" on p. 90. A mean can be estimated by
 * sampling, and appendix B gives the sampler: play random legal moves to
 * the end of the game, tally wins, losses and draws, and keep doubling the
 * sample from 16 until two successive estimates, projected as wins out of
 * 1024 games, agree within a tolerance. The stronger of his two Othello
 * samplers (section 5.2.2 and footnote 11) ran passes of 16, 32, 64, 128,
 * 256, 512 and 1024 games with tolerances of 0, 16, 24, 20, 18 and 9
 * between successive passes, and used no lookahead at all: the board after
 * each legal move was sampled and the best estimate played. It beat the
 * expert weighted-squares evaluator 48-2 at 8x8 Othello, taking one to ten
 * minutes a move in 1987. This is that player, applied to chess, the first
 * random-playout evaluation and the ancestor of Monte Carlo tree search.
 *
 * Abramson's own chess work used the same sampler (random games played to
 * the end, draws counted, pawns always promoted to queens; pp. 88-96) to
 * learn material values by regression: P 4.0, R 4.8, N 2.5, B 3.3, Q 9.6,
 * the pawn inflated because a sampled pawn is a pawn-and-a-queen. He did
 * not report a game of chess played by the sampler itself, remarking that
 * chess samplers were "orders of magnitude slower than Othello samplers".
 * This engine plays the games he could not afford.
 *
 * Console: "eo" samples the root moves with the current settings and lists
 * each move's estimate, wins out of 1024 random games.
 *
 * [choice] the sampler visits the root moves round-robin, one pass at a
 *          time, so that when the clock cuts the protocol short every move
 *          has had the same number of games; a move whose estimate has
 *          converged by Abramson's test is left alone.
 * [choice] MaxSamples (default 1024, Abramson's cap) ends the protocol;
 *          0 keeps doubling for as long as the clock allows. At three
 *          thousand random games a second his stopping rule is satisfied
 *          within a second or two, so Converge=false switches it off and
 *          lets every move be sampled until MaxSamples or the clock: that
 *          is the setting for a tournament where the sampler should spend
 *          its time, and the only way it improves with more of it.
 * [choice] a random game ends by mate, stalemate, the fifty-move rule, a
 *          dead position (bare kings, or king and one minor against king)
 *          or ClipPlies plies, the last three as draws. Abramson records no
 *          rule for ending a random chess game beyond "to the end".
 * [choice] a root move that repeats a position for the third time is a
 *          draw by rule and scores 512 without sampling.
 * [choice] ties between estimates go to the first move generated.
 * [choice] IgnoreDraws uses his "Chess2" formulation, wins over decisive
 *          games only, which learned nearly the same piece values.
 */

#define ENGINE_NAME   "Abramson 1987 Expected-Outcome"
#define ENGINE_AUTHOR "Bruce Abramson (recreated)"
#define ENG_OPTIONS
#define ENG_ROOT_ORDER
#define ENG_COMMAND
#include <math.h>
#include "retro.h"

static int opt_max = 1024, opt_clip = 300, opt_ignore_draws = 0, opt_converge = 1;
/* tolerated change, in wins out of 1024, between passes 16->32, 32->64, 64->128, 128->256, 256->512, 512->1024 */
static const int tolerance[6] = { 0, 16, 24, 20, 18, 9 };

static long long games;       /* random games played this search */
static int longest;           /* longest of them, in plies */

/* bare kings, or a lone minor piece: nobody can win */
static int dead(const Pos *p) {
  int sq, extra = 0, minor = 0;
  for (sq = 0; sq < 64; sq++) {
    int t = PT(p->pc[sq]);
    if (!t || t == KING) continue;
    extra++;
    if (t == KNIGHT || t == BISHOP) minor++;
  }
  return extra == 0 || (extra == 1 && minor == 1);
}

/* legal moves with pawns promoting to queens only, as Abramson's sampler assumed */
static int sampler_moves(const Pos *p, int *mv) {
  int n = gen_legal(p, mv), i, k = 0;
  for (i = 0; i < n; i++) {
    int pr = M_PROMO(mv[i]);
    if (pr && pr != QUEEN) continue;
    mv[k++] = mv[i];
  }
  return k;
}

/* one random game from p: 2 win, 1 draw, 0 loss for the side to move at p */
static int random_game(Pos p) {
  int mv[256], n, ply, r = 1;
  for (ply = 0; ; ply++) {
    if (p.fifty >= 100 || ply >= opt_clip || dead(&p)) break;
    n = sampler_moves(&p, mv);
    if (n == 0) { if (in_check(&p)) r = (ply & 1) ? 2 : 0; break; }
    make(&p, mv[rng() % (uint64_t)n]);
    nodes++;
  }
  if (ply > longest) longest = ply;
  games++;
  return r;
}

/* Abramson's estimate: wins projected over 1024 games */
static int project(int w, int d, int l) {
  if (opt_ignore_draws) return w + l ? (int)(1024LL * w / (w + l)) : 512;
  return (int)(1024LL * (2 * w + d) / (2 * (w + d + l)));
}

static int out_of_time(void) {
  if (stop_flag) return 1;
  if (time_limited && now_ms() >= t_soft) return 1;
  if (lim.nodes && games >= lim.nodes) return 1;
  return 0;
}

static int eo_cp(int est) {
  double eo = est / 1024.0;
  if (eo < 0.002) eo = 0.002;
  if (eo > 0.998) eo = 0.998;
  return (int)(400.0 * log10(eo / (1.0 - eo)));
}

/* per root move, kept for the console command */
static int r_est[256], r_games[256], r_conv[256];

int eng_root_order(const Pos *p, int *mv, int n) {
  Pos child[256];
  int w[256], d[256], l[256], i, pass, size, best = 0, max_pass = lim.depth ? lim.depth : 64;
  char ms[8];
  games = 0; longest = 0;
  if (n == 1) return 1;                             /* a forced move needs no games */
  for (i = 0; i < n; i++) {
    int tmp[256], j, reps = 0; uint64_t h;
    child[i] = *p; make(&child[i], mv[i]);
    w[i] = d[i] = l[i] = r_games[i] = 0; r_conv[i] = 0; r_est[i] = -1;
    if (gen_legal(&child[i], tmp) == 0) {          /* mate or stalemate: exact */
      r_est[i] = in_check(&child[i]) ? 1024 : 512; r_conv[i] = 1;
      continue;
    }
    h = hash_pos(&child[i]);
    for (j = 0; j < game_hist_n; j++) if (game_hist[j] == h) reps++;
    if (reps >= 2 || child[i].fifty >= 100) { r_est[i] = 512; r_conv[i] = 1; }
  }
  for (pass = 0, size = 16; pass < max_pass; pass++, size *= 2) {
    int any = 0;
    for (i = 0; i < n; i++) {
      int e, tol;
      if (r_conv[i]) continue;
      any = 1;
      while (r_games[i] < size) {
        int r = 2 - random_game(child[i]);           /* our view: the child is the opponent's move */
        if (r == 2) w[i]++; else if (r == 1) d[i]++; else l[i]++;
        r_games[i]++;
        if (out_of_time()) goto done;
      }
      e = project(w[i], d[i], l[i]);
      tol = tolerance[pass - 1 < 5 ? (pass > 0 ? pass - 1 : 0) : 5];
      if (opt_converge && pass > 0 && abs(e - r_est[i]) <= tol) r_conv[i] = 1;
      r_est[i] = e;
    }
    if (!any) break;
    for (best = 0, i = 1; i < n; i++) if (r_est[i] > r_est[best]) best = i;
    {
      long long el = now_ms() - t_start;
      move_str(mv[best], ms);
      out("info depth %d seldepth %d score cp %d nodes %lld nps %lld time %lld pv %s\n",
          pass + 1, longest, eo_cp(r_est[best]), games, el > 0 ? games * 1000 / el : 0, el, ms);
    }
    if (opt_max && size >= opt_max) break;
    if (!opt_max && !time_limited && !lim.nodes && size >= 1024) break;   /* no clock to run on: Abramson's cap */
  }
done:
  for (best = 0, i = 1; i < n; i++) if (r_est[i] > r_est[best]) best = i;
  {   /* final line, so the reported pv is the move played even when the clock cut a pass short */
    long long el = now_ms() - t_start;
    move_str(mv[best], ms);
    out("info depth %d seldepth %d score cp %d nodes %lld nps %lld time %lld pv %s\n",
        pass + 1, longest, eo_cp(r_est[best]), games, el > 0 ? games * 1000 / el : 0, el, ms);
  }
  { int t = mv[0]; mv[0] = mv[best]; mv[best] = t; }
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
  out("option name MaxSamples type spin default 1024 min 0 max 1048576\n");
  out("option name ClipPlies type spin default 300 min 20 max 10000\n");
  out("option name IgnoreDraws type check default false\n");
  out("option name Converge type check default true\n");
}
int eng_setoption(const char *name, const char *value) {
  if (!strcmp(name, "MaxSamples")) { opt_max = atoi(value); return 1; }
  if (!strcmp(name, "ClipPlies")) { opt_clip = atoi(value); return 1; }
  if (!strcmp(name, "IgnoreDraws")) { opt_ignore_draws = !strcmp(value, "true"); return 1; }
  if (!strcmp(name, "Converge")) { opt_converge = !strcmp(value, "true"); return 1; }
  return 0;
}

/* console: "eo" samples every root move to Abramson's cap and lists the estimates */
int eng_command(const char *cmd, const Pos *p) {
  int mv[256], n, i, order[256]; char ms[8];
  if (strcmp(cmd, "eo")) return 0;
  n = gen_legal(p, mv);
  memcpy(order, mv, n * sizeof(int));
  time_limited = 0; memset(&lim, 0, sizeof lim); stop_flag = 0; t_start = now_ms(); nodes = 0;
  eng_root_order(p, order, n);
  for (i = 0; i < n; i++) { move_str(mv[i], ms); out("%s %4d/1024 after %4d games%s\n", ms, r_est[i], r_games[i], r_conv[i] ? " (converged)" : ""); }
  out("%lld games, %lld positions, longest %d plies, %lld ms\n", games, nodes, longest, now_ms() - t_start);
  return 1;
}

int main(void) { return uci_main(); }
