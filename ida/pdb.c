/* pdb.c - Stage 2 experiment 1:
 * build the permutation and orientation pattern databases
 * and print their distance distributions. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { CUBIES = 7, PERMUTATIONS = 5040, ORIENTATIONS = 729, MOVES = 9 };

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};


static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};


static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static state_t apply_move(state_t state, uint8_t move)
{
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for (uint8_t i = 0; i < turns; ++i)
        state = quarter_turn(state, (uint8_t) (move / 3U));
    return state;
}

static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
}

static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}


static uint16_t perm_next[MOVES][PERMUTATIONS]; /* perm rank after move m */
static uint16_t ori_next[MOVES][ORIENTATIONS];  /* ori rank after move m */
static uint8_t perm_pdb[PERMUTATIONS];          /* distance to solved */
static uint8_t ori_pdb[ORIENTATIONS];

/* Fill perm_next and ori_next for all 9 moves.
 * rank_state(&s) / ORIENTATIONS = permutation rank
 * rank_state(&s) % ORIENTATIONS = orientation rank
 * (solver.c build_table does this for 3 quarter turns; you need 9 moves) */
static void build_moves(void)
{
    state_t s, t;

    for (uint16_t r = 0; r < PERMUTATIONS; ++r) {
        unrank_state((uint32_t)r * ORIENTATIONS, &s); /* perm = r, ori = 0 */
        for (uint8_t m = 0; m < MOVES; ++m) {
            t = apply_move(s, m);
            perm_next[m][r] =
                (uint16_t)(rank_state(&t) / ORIENTATIONS);
        }
    }

    for (uint16_t r = 0; r < ORIENTATIONS; ++r) {
        unrank_state(r, &s); /* perm = 0, ori = r */
        for (uint8_t m = 0; m < MOVES; ++m) {
            t = apply_move(s, m);
            ori_next[m][r] =
                (uint16_t)(rank_state(&t) % ORIENTATIONS);
        }
    }
}

/* BFS from node 0 (solved) over n nodes; neighbours of x are next[m * n + x].
 * Write dist[x]; mark unvisited as 0xFF first.
 * Return the largest distance found. */
static int bfs(int n, const uint16_t *next, uint8_t *dist)
{
    uint16_t queue[PERMUTATIONS];      /* big enough for both tables */
    int head = 0, tail = 0, max = 0;

    memset(dist, 0xFF, (size_t) n);    /* 0xFF = not visited yet */
    dist[0] = 0;                       /* node 0 = solved */
    queue[tail++] = 0;

    while (head < tail) {
        uint16_t x = queue[head++];
        for (int m = 0; m < MOVES; ++m) {
            uint16_t y = next[m * n + x];
            if (dist[y] == 0xFF) {
                dist[y] = dist[x] + 1;
                if (dist[y] > max)
                    max = dist[y];
                queue[tail++] = y;
            }
        }
    }
    return max;
}

/* Print "d: count" for d = 0..max, and check the counts sum to n. */
static void histogram(const char *name, int n, const uint8_t *dist, int max)
{
    int total = 0;
    printf("%s (max = %d)\n", name, max);
    for (int d = 0; d <= max; ++d) {
        int count = 0;
        for (int x = 0; x < n; ++x)
            if (dist[x] == d)
                ++count;
        printf("  %2d: %5d\n", d, count);
        total += count;
    }
    printf("  total %d / %d %s\n", total, n, total == n ? "OK" : "MISSING");
}

enum { STATES = PERMUTATIONS * ORIENTATIONS };
static uint8_t true_dist[STATES];     /* exact distance, host only */
static uint32_t big_queue[STATES];

/* BFS over the whole cube; a move updates the two parts independently. */
static int full_bfs(void)
{
    uint32_t head = 0, tail = 0;
    int max = 0;

    memset(true_dist, 0xFF, STATES);
    true_dist[0] = 0;
    big_queue[tail++] = 0;

    while (head < tail) {
        uint32_t s = big_queue[head++];
        uint16_t p = (uint16_t) (s / ORIENTATIONS);
        uint16_t o = (uint16_t) (s % ORIENTATIONS);
        for (int m = 0; m < MOVES; ++m) {
            uint32_t t = (uint32_t) perm_next[m][p] * ORIENTATIONS + ori_next[m][o];
            if (true_dist[t] == 0xFF) {
                true_dist[t] = true_dist[s] + 1;
                if (true_dist[t] > max)
                    max = true_dist[t];
                big_queue[tail++] = t;
            }
        }
    }
    return max;
}

/* H1: h = max(perm, ori) never exceeds the true distance. */
static void check_h1(void)
{
    long violations = 0, ori_wins = 0, ties = 0, h11[8] = {0};
    double sum_h = 0, sum_d = 0;

    for (uint32_t s = 0; s < STATES; ++s) {
        uint8_t hp = perm_pdb[s / ORIENTATIONS];
        uint8_t ho = ori_pdb[s % ORIENTATIONS];
        uint8_t h = hp > ho ? hp : ho;
        uint8_t d = true_dist[s];

        if (h > d)
            ++violations;
        if (ho > hp)
            ++ori_wins;
        else if (ho == hp)
            ++ties;
        if (d == 11)
            ++h11[h];
        sum_h += h;
        sum_d += d;
    }
    printf("H1 violations: %ld %s\n", violations, violations ? "FAIL" : "PASS");
    printf("ori > perm: %ld, ties: %ld (of %d)\n", ori_wins, ties, STATES);
    printf("average h = %.2f, average true distance = %.2f\n",
           sum_h / STATES, sum_d / STATES);
    printf("h for the distance-11 states:\n");
    for (int h = 0; h < 8; ++h)
        printf("  h=%d: %ld\n", h, h11[h]);
}

int main(void)
{
    build_moves();

    int max_p = bfs(PERMUTATIONS, &perm_next[0][0], perm_pdb);

    int max_o = bfs(ORIENTATIONS,
                    &ori_next[0][0],
                    ori_pdb);

    histogram("permutation",
              PERMUTATIONS,
              perm_pdb,
              max_p);

    histogram("orientation",
              ORIENTATIONS,
              ori_pdb,
              max_o);

    int max_d = full_bfs();
    histogram("full cube", STATES, true_dist, max_d);
    check_h1();

    return 0;
}