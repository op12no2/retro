/*
 * uct - Upper Confidence bounds applied to Trees, 2006, as tried on chess.
 *
 *   L. Kocsis and C. Szepesvári, "Bandit based Monte-Carlo Planning",
 *   ECML 2006, LNCS 4212, pp. 282-293.
 *   R. Ramanujan, A. Sabharwal and B. Selman, "On Adversarial Search
 *   Spaces and Sampling-Based Planning", ICAPS 2010, pp. 242-245, and
 *   "Understanding Sampling Style Adversarial Search Methods", UAI 2010.
 *   O. Arenz, "Monte Carlo Chess", B.Sc. thesis, TU Darmstadt 2012 (MCC),
 *   not consulted: the repository refuses downloads.
 *
 * Kocsis and Szepesvári's rollout-based planner (their Fig. 1): repeat
 * episodes from the root until the time is up; in each episode walk down
 * the tree choosing at every node the action with the best upper
 * confidence bound, its mean reward plus a bias 2 Cp sqrt(ln t / s), t the
 * node's visits and s the action's (their eqs. 1-2, Cp = 1/sqrt 2 for
 * rewards in [0, 1]); play one random game from the first position that
 * is not yet in the tree, and add its result to every node on the way
 * back. Finally play "the action with the highest average observed
 * long-term reward".
 *
 * Ramanujan, Sabharwal and Selman ran exactly this on chess: uniformly
 * random playouts, rewards -1, 0, +1 with a draw 0, and "the exploration
 * bias parameter for UCT is fixed at a value of 0.4, for this was
 * empirically observed to produce the best performance". With 10,000
 * episodes it beat a two-ply minimax with random rollouts at the leaves
 * 74% of the time but could not see the "shallow traps", five- and
 * seven-move losing lines, that chess is full of, which is their
 * explanation of why UCT conquered Go and not chess. Those are the
 * defaults here: Exploration 40 (hundredths, on the [-1, 1] reward scale)
 * and 10,000 episodes when there is no clock.
 *
 * Console: "uct" runs 2000 episodes from the position and lists the root
 * moves with visits and mean reward.
 *
 * [choice] one new node per episode, the standard expansion; a node's
 *          children are all created when it is first expanded and an
 *          unvisited child, chosen at random, is tried before the bound
 *          chooses among visited ones. The papers do not say.
 * [choice] random games end by mate, stalemate, the fifty-move rule, a
 *          dead position (bare kings, or king and one minor against king)
 *          or ClipPlies plies, the last three as draws; promotions are
 *          among the uniformly random choices. Inside the tree the
 *          fifty-move rule ends a line as a draw, and a root move that
 *          repeats a position for the third time is a draw by rule.
 * [choice] the tree is discarded between moves; no transpositions.
 * [choice] the tree holds at most two million nodes; when full, the walk
 *          plays out from the frontier instead of expanding.
 * [choice] ties on mean reward go to the first move generated.
 */

#define ENGINE_NAME   "UCT 2006"
#define ENGINE_AUTHOR "Kocsis and Szepesvari (recreated)"
#define ENG_OPTIONS
#define ENG_ROOT_ORDER
#define ENG_COMMAND
#include <math.h>
#include "retro.h"

typedef struct {
  int move, first, nchild;   /* nchild -1: not yet expanded */
  int visits; double sum;    /* reward for the player who moved into this node */
} Node;

#define POOL_MAX 2000000
static Node *pool; static int pool_n;
static int opt_c = 40, opt_clip = 300, opt_episodes = 10000;
static int root_rep[256];    /* root children that repeat a position for the third time */
static long long episodes; static int longest, deepest;

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

/* uniformly random game from p: +1 win, 0 draw, -1 loss for the side to move at p */
static int random_game(Pos p, int depth) {
  int mv[256], n, ply, r = 0;
  for (ply = 0; ; ply++) {
    if (p.fifty >= 100 || ply >= opt_clip || dead(&p)) break;
    n = gen_legal(&p, mv);
    if (n == 0) { if (in_check(&p)) r = (ply & 1) ? 1 : -1; break; }
    make(&p, mv[rng() % (uint64_t)n]);
    nodes++;
  }
  if (depth + ply > longest) longest = depth + ply;
  return r;
}

static int expand(int ni, const Pos *p) {
  int mv[256], n = gen_legal(p, mv), i;
  if (pool_n + n > POOL_MAX) return 0;
  pool[ni].first = pool_n; pool[ni].nchild = n;
  for (i = 0; i < n; i++) {
    Node *c = &pool[pool_n++];
    c->move = mv[i]; c->first = -1; c->nchild = -1; c->visits = 0; c->sum = 0.0;
  }
  return 1;
}

static void episode(const Pos *root) {
  Pos p = *root;
  int path[MAXPLY], depth = 0, ni = 0, r = 0, d;
  double c = opt_c / 100.0;
  path[0] = 0;
  for (;;) {
    Node *nd = &pool[ni];
    int i, k, unvisited = 0, pick = -1;
    double bestv = -1e9;
    if (depth > 0 && (p.fifty >= 100 || (depth == 1 && root_rep[ni - pool[0].first]))) { r = 0; break; }
    if (nd->nchild < 0) {
      if (nd->visits == 0 && ni != 0) { r = random_game(p, depth); break; }   /* first visit: play out */
      if (!expand(ni, &p)) { r = random_game(p, depth); break; }               /* tree full */
    }
    if (nd->nchild == 0) { r = in_check(&p) ? -1 : 0; break; }                /* mated or stalemated */
    for (i = 0; i < nd->nchild; i++) if (pool[nd->first + i].visits == 0) unvisited++;
    if (unvisited) {
      k = (int)(rng() % (uint64_t)unvisited);
      for (i = 0; i < nd->nchild; i++) if (pool[nd->first + i].visits == 0 && k-- == 0) { pick = i; break; }
    } else {
      double lt = log((double)nd->visits);
      for (i = 0; i < nd->nchild; i++) {
        Node *ch = &pool[nd->first + i];
        double v = ch->sum / ch->visits + c * sqrt(lt / ch->visits);
        if (v > bestv) { bestv = v; pick = i; }
      }
    }
    ni = nd->first + pick;
    make(&p, pool[ni].move);
    if (++depth >= MAXPLY - 1) { r = random_game(p, depth); path[depth] = ni; break; }
    path[depth] = ni;
  }
  if (depth > deepest) deepest = depth;
  /* r is for the side to move at the last position; the node holding that
     position was entered by the other side */
  for (d = depth, r = -r; d >= 0; d--, r = -r) { pool[path[d]].visits++; pool[path[d]].sum += r; }
  episodes++;
}

static int mean_cp(double m) {
  double w = (m + 1.0) / 2.0;
  if (w < 0.002) w = 0.002;
  if (w > 0.998) w = 0.998;
  return (int)(400.0 * log10(w / (1.0 - w)));
}

static int best_child(int ni) {   /* highest mean reward among visited children */
  int i, best = -1; double bm = -1e9;
  for (i = 0; i < pool[ni].nchild; i++) {
    Node *ch = &pool[pool[ni].first + i];
    if (ch->visits && ch->sum / ch->visits > bm) { bm = ch->sum / ch->visits; best = i; }
  }
  return best;
}

static void report(void) {
  char ms[8]; int ni = 0, b, len = 0;
  long long el = now_ms() - t_start;
  b = best_child(0);
  if (b < 0) return;
  out("info depth %d seldepth %d score cp %d nodes %lld nps %lld time %lld pv",
      deepest, longest, mean_cp(pool[pool[0].first + b].sum / pool[pool[0].first + b].visits), episodes, el > 0 ? episodes * 1000 / el : 0, el);
  while (pool[ni].nchild > 0 && (b = best_child(ni)) >= 0 && len < 16) {
    ni = pool[ni].first + b; move_str(pool[ni].move, ms); out(" %s", ms); len++;
  }
  out("\n");
}

static void search_root(const Pos *p) {
  long long budget = lim.nodes ? lim.nodes : (time_limited ? 0 : opt_episodes), next_report = 500;
  int i, j, tmp[256];
  pool_n = 1; pool[0].move = 0; pool[0].first = -1; pool[0].nchild = -1; pool[0].visits = 0; pool[0].sum = 0.0;
  episodes = 0; longest = 0; deepest = 0;
  expand(0, p);
  for (i = 0; i < pool[0].nchild; i++) {        /* draws by repetition at the root */
    Pos c = *p; int reps = 0; uint64_t h;
    make(&c, pool[pool[0].first + i].move); h = hash_pos(&c);
    for (j = 0; j < game_hist_n; j++) if (game_hist[j] == h) reps++;
    root_rep[i] = reps >= 2;
    (void)tmp;
  }
  while (!stop_flag) {
    if (time_limited && now_ms() >= t_soft) break;
    if (budget && episodes >= budget) break;
    episode(p);
    if (episodes >= next_report) { report(); next_report *= 2; }
  }
  report();
}

int eng_root_order(const Pos *p, int *mv, int n) {
  int b, i;
  (void)n;
  search_root(p);
  b = best_child(0);
  if (b >= 0) {
    int m = pool[pool[0].first + b].move;
    for (i = 0; i < n; i++) if (mv[i] == m) { mv[i] = mv[0]; mv[0] = m; break; }
  }
  return 1;
}

int eng_eval(const Pos *p) { (void)p; return 0; }

void eng_init(void) {
  cfg.alphabeta = 1;
  cfg.fixed_depth = 1;
  cfg.qdepth = 0;
  pool = malloc(sizeof(Node) * POOL_MAX);
  rng_state ^= (uint64_t)now_ms() * 0x9E3779B97F4A7C15ULL;
}

void eng_options(void) {
  out("option name Exploration type spin default 40 min 0 max 1000\n");
  out("option name Episodes type spin default 10000 min 1 max 100000000\n");
  out("option name ClipPlies type spin default 300 min 20 max 10000\n");
}
int eng_setoption(const char *name, const char *value) {
  if (!strcmp(name, "Exploration")) { opt_c = atoi(value); return 1; }
  if (!strcmp(name, "Episodes")) { opt_episodes = atoi(value); return 1; }
  if (!strcmp(name, "ClipPlies")) { opt_clip = atoi(value); return 1; }
  return 0;
}

/* console: "uct" runs 2000 episodes and lists the root moves */
int eng_command(const char *cmd, const Pos *p) {
  int i; char ms[8];
  if (strcmp(cmd, "uct")) return 0;
  time_limited = 0; memset(&lim, 0, sizeof lim); lim.nodes = 2000; stop_flag = 0; t_start = now_ms(); nodes = 0;
  search_root(p);
  for (i = 0; i < pool[0].nchild; i++) {
    Node *ch = &pool[pool[0].first + i];
    move_str(ch->move, ms);
    out("%s visits %5d mean %+.3f%s\n", ms, ch->visits, ch->visits ? ch->sum / ch->visits : 0.0, root_rep[i] ? " (repetition)" : "");
  }
  out("%lld episodes, %d tree nodes, deepest %d, longest %d plies, %lld positions, %lld ms\n", episodes, pool_n, deepest, longest, nodes, now_ms() - t_start);
  return 1;
}

int main(void) { return uci_main(); }
