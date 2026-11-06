/*
 * beal - Don Beal and Martin Smith's random-evaluation experiment,
 * "Random evaluations in chess", ICCA Journal 17(1), 1994.
 *
 * Full-width alpha-beta search in which every leaf receives a RANDOM
 * score. Mate and stalemate are still recognised. The surprising result,
 * which this engine lets you watch, is that it plays recognisable chess:
 * a position with many legal replies gives the side to move more random
 * draws to pick the maximum from, so the search implicitly rewards
 * mobility and punishes being boxed in or losing pieces.
 *
 * The only knowledge in this program is the rules of chess.
 */

#define ENGINE_NAME   "Beal 1994 Random Evaluation"
#define ENGINE_AUTHOR "Don Beal and Martin Smith (recreated)"
#define ENG_OPTIONS
#include "retro.h"

static int opt_range = 1000;
static uint64_t seed;

int eng_eval(const Pos *p) {
  (void)p;
  return (int)(rng() % (2 * (uint64_t)opt_range + 1)) - opt_range;
}

void eng_init(void) {
  cfg.alphabeta = 1;
  cfg.qdepth = 0;            /* no quiescence: Beal and Smith searched to a fixed depth */
  cfg.fixed_depth = 0;
  seed = (uint64_t)now_ms();
  rng_state ^= seed * 0x9E3779B97F4A7C15ULL;
}

void eng_options(void) {
  out("option name Range type spin default 1000 min 1 max 100000\n");
  out("option name Quiescence type check default false\n");
}

int eng_setoption(const char *name, const char *value) {
  if (!strcmp(name, "Range")) { opt_range = atoi(value); return 1; }
  if (!strcmp(name, "Quiescence")) { cfg.qdepth = strcmp(value, "true") ? 0 : 8; return 1; }
  return 0;
}

int main(void) { return uci_main(); }
