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

typedef struct {
    uint16_t p, o;     /* state at this depth */
    uint8_t next;      /* next move to try (0..9) */
    uint8_t last;      /* move that led here, 9 = none (root) */
} frame_t;

static long nodes;     /* how many states the search pushed */

static int h_of(uint16_t p, uint16_t o)
{
    return perm_pdb[p] > ori_pdb[o] ? perm_pdb[p] : ori_pdb[o];
}

/* Returns the optimal length; path[0..len-1] gets the moves. */
static int ida_star(uint32_t start, uint8_t *path)
{
    frame_t st[12];
    uint16_t p0 = (uint16_t) (start / ORIENTATIONS);
    uint16_t o0 = (uint16_t) (start % ORIENTATIONS);
    int bound = h_of(p0, o0);

    for (;;) {
        int top = 0;
        st[0] = (frame_t) {p0, o0, 0, 9};
        while (top >= 0) {
            frame_t *f = &st[top];
            if (f->p == 0 && f->o == 0)
                return top;                    /* solved at depth top */
            if (f->next == MOVES) {            /* all moves tried here */
                --top;                         /* pop */
                continue;
            }
            uint8_t m = f->next++;
            if ((f->last != 9) && (m / 3 == f->last / 3)) /* same face as last move */
                continue;
            uint16_t np = perm_next[m][f->p];
            uint16_t no = ori_next[m][f->o];
            if ((top + 1) + h_of(np, no) > bound) /* g + h > bound: prune */
                continue;
            ++nodes;
            path[top] = m;
            st[top + 1] = (frame_t) {np, no, 0, m};
            ++top;                             /* push */
        }
        ++bound;                               /* no solution within bound */
    }
}

/* Apply path with the tables and check it reaches solved. */
static int path_solves(uint32_t start, const uint8_t *path, int len)
{
    uint16_t p = (uint16_t) (start / ORIENTATIONS);
    uint16_t o = (uint16_t) (start % ORIENTATIONS);
    for (int i = 0; i < len; ++i) {
        p = perm_next[path[i]][p];
        o = ori_next[path[i]][o];
    }
    return p == 0 && o == 0;
}

/* Rank of a 14-digit state string such as "21345671111111". */
static uint32_t rank_of_string(const char *text)
{
    state_t s;
    for (int i = 0; i < CUBIES; ++i) {
        s.p[i] = (uint8_t) (text[i] - '1');
        s.o[i] = (uint8_t) (text[i + CUBIES] - '1');
    }
    return rank_state(&s);
}

/* Run IDA* on every distance-11 state and report node counts. */
static void measure_ida(void)
{
    long worst = 0, total = 0, count = 0, bad = 0;
    long by_h_count[8] = {0}, by_h_max[8] = {0}, by_h_sum[8] = {0};
    uint32_t worst_state = 0;
    uint8_t path[11];

    for (uint32_t s = 0; s < STATES; ++s) {
        if (true_dist[s] != 11)
            continue;
        nodes = 0;
        int len = ida_star(s, path);
        if (len != 11 || !path_solves(s, path, len))
            ++bad;
        int h = h_of((uint16_t) (s / ORIENTATIONS), (uint16_t) (s % ORIENTATIONS));
        ++by_h_count[h];
        by_h_sum[h] += nodes;
        if (nodes > by_h_max[h])
            by_h_max[h] = nodes;
        if (nodes > worst) {
            worst = nodes;
            worst_state = s;
        }
        total += nodes;
        ++count;
    }
    printf("IDA* on %ld distance-11 states: %ld wrong\n", count, bad);
    printf("nodes: max %ld, average %.0f\n", worst, (double) total / count);
    printf("worst state rank %u, its h = %d\n", worst_state,
           h_of((uint16_t) (worst_state / ORIENTATIONS),
                (uint16_t) (worst_state % ORIENTATIONS)));
    printf("by root h:   count   average       max\n");
    for (int h = 0; h < 8; ++h)
        if (by_h_count[h])
            printf("  h=%d:    %6ld  %8.0f  %8ld\n", h, by_h_count[h],
                   (double) by_h_sum[h] / by_h_count[h], by_h_max[h]);

    uint32_t sample = rank_of_string("21345671111111");
    nodes = 0;
    int len = ida_star(sample, path);
    printf("sample 21345671111111: length %d, nodes %ld, %s\n", len, nodes,
           path_solves(sample, path, len) ? "solves" : "WRONG");
}

static uint16_t perm_q[3][PERMUTATIONS];
static uint16_t ori_q[3][ORIENTATIONS];

/* The quarter turns R, B and D are moves 0, 3 and 6. */
static void build_quarter(void)
{
    for (int f = 0; f < 3; ++f) {
        memcpy(perm_q[f], perm_next[f * 3], sizeof perm_q[f]);
        memcpy(ori_q[f],  ori_next[f * 3],  sizeof ori_q[f]);
    }
}

/* Each value 0..n-1 appears exactly once in the row. */
static int is_permutation(const uint16_t *row, int n)
{
    uint8_t seen[PERMUTATIONS] = {0};
    for (int i = 0; i < n; ++i) {
        if (row[i] >= n || seen[row[i]])
            return 0;
        seen[row[i]] = 1;
    }
    return 1;
}

/* Four quarter turns give back every entry. */
static int fourth_power_is_identity(const uint16_t *row, int n)
{
    for (int i = 0; i < n; ++i)
        if (row[row[row[row[i]]]] != i)
            return 0;
    return 1;
}

/* No 0xFF, solved entry is 0, largest value equals max. */
static int pdb_ok(const uint8_t *pdb, int n, int max)
{
    int largest = 0;
    for (int i = 0; i < n; ++i) {
        if (pdb[i] == 0xFF)
            return 0;
        if (pdb[i] > largest)
            largest = pdb[i];
    }
    return pdb[0] == 0 && largest == max;
}

static int check_h2(void)
{
    int ok = 1;
    for (int f = 0; f < 3; ++f) {
        ok &= is_permutation(perm_q[f], PERMUTATIONS);
        ok &= fourth_power_is_identity(perm_q[f], PERMUTATIONS);
        ok &= is_permutation(ori_q[f], ORIENTATIONS);
        ok &= fourth_power_is_identity(ori_q[f], ORIENTATIONS);
    }
    ok &= pdb_ok(perm_pdb, PERMUTATIONS, 7);
    ok &= pdb_ok(ori_pdb, ORIENTATIONS, 6);
    printf("H2: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Print a [3][n] table as {{row 0}, {row 1}, {row 2}}. */
static void emit_rows(FILE *out, const char *name, const uint16_t *v, int n)
{
    fprintf(out, "static const uint16_t %s[3][%d] = {", name, n);
    for (int f = 0; f < 3; ++f) {
        fprintf(out, "%s{", f ? ", " : "");
        for (int i = 0; i < n; ++i)
            fprintf(out, "%s%u", i ? ", " : "", v[f * n + i]);
        fprintf(out, "}");
    }
    fprintf(out, "};\n");
}

static int emit_tables(const char *path)
{
    FILE *out = fopen(path, "w");
    if (!out)
        return 0;
    fprintf(out, "/* Generated by ida/pdb.c. Do not edit. */\n#include <stdint.h>\n\n");

    fprintf(out, "static const uint8_t perm_pdb[%d] = {", PERMUTATIONS);
    for (int i = 0; i < PERMUTATIONS; ++i)
        fprintf(out, "%s%u", i ? ", " : "", perm_pdb[i]);
    fprintf(out, "};\n");

    fprintf(out, "static const uint8_t ori_pdb[%d] = {", ORIENTATIONS);
    for (int i = 0; i < ORIENTATIONS; ++i)
        fprintf(out, "%s%u", i ? ", " : "", ori_pdb[i]);
    fprintf(out, "};\n");

    emit_rows(out, "perm_q", &perm_q[0][0], PERMUTATIONS);
    emit_rows(out, "ori_q", &ori_q[0][0], ORIENTATIONS);

    return fclose(out) == 0;
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
    build_quarter();
    if (check_h2() && emit_tables("ida/tables.h"))
        printf("wrote ida/tables.h\n");
    measure_ida();

    return 0;
}