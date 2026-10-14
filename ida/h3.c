/* h3.c - gate H3: run solve() on every state, compare with an exact BFS. */
#define main search_main          /* reuse search.c without its main */
#include "search.c"
#undef main
#include <time.h>

enum { STATES = 5040 * ORIENTATIONS };
static uint8_t dist[STATES];
static uint32_t queue[STATES];

/* Exact distances, built from the quarter-turn tables by chaining. */
static void bfs(void)
{
    uint32_t head = 0, tail = 0;
    memset(dist, 0xFF, sizeof dist);
    dist[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        uint32_t s = queue[head++];
        for (int f = 0; f < 3; ++f) {
            uint16_t p = s / ORIENTATIONS, o = s % ORIENTATIONS;
            for (int t = 0; t < 3; ++t) {       /* R, R2, R' */
                p = perm_q[f][p];
                o = ori_q[f][o];
                uint32_t n = (uint32_t) p * ORIENTATIONS + o;
                if (dist[n] == 0xFF) {
                    dist[n] = dist[s] + 1;
                    queue[tail++] = n;
                }
            }
        }
    }
}

/* Replay stack[1..g] with the tables; it must reach solved. */
static int path_ok(uint16_t p, uint16_t o, int g)
{
    for (int i = 1; i <= g; ++i)
        for (int t = 0; t < stack[i].mturn; ++t) {
            p = perm_q[stack[i].mface][p];
            o = ori_q[stack[i].mface][o];
        }
    return p == 0 && o == 0;
}

int main(void)
{
    bfs();
    long wrong_len = 0, wrong_path = 0;
    clock_t start = clock();
    for (uint32_t s = 0; s < STATES; ++s) {
        uint16_t p = s / ORIENTATIONS, o = s % ORIENTATIONS;
        int g = solve(p, o);
        if (g != dist[s])                       /* length must be exact */
            ++wrong_len;
        if (!path_ok(p, o, g))
            ++wrong_path;
        if ((s & 0x3FFFF) == 0)                 /* progress every 262,144 */
            fprintf(stderr, "%u / %d\n", s, STATES);
    }
    double secs = (double) (clock() - start) / CLOCKS_PER_SEC;
    printf("H3: %ld wrong length, %ld wrong path, %.1f s, %s\n",
           wrong_len, wrong_path, secs,
           wrong_len || wrong_path ? "FAIL" : "PASS");
    return 0;
}
