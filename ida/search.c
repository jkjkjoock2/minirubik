/* search.c - Stage 3: IDA* written the way the RV32I version will run.
 * Hot path: no multiply, divide or modulo; the stack is walked by pointer. */
#include <stdint.h>
#ifndef SEARCH_NO_MAIN /* ida/ref.c builds solve() without stdio and main */
#include <stdio.h>
#endif
#include <string.h>

#include "tables.h"

enum { CUBIES = 7, ORIENTATIONS = 729, NONE = 0xFF };

typedef struct {
    uint16_t p, o;          /* this node */
    uint16_t cp, co;        /* child after `turn` quarter turns of `face` */
    const uint16_t *pp;     /* perm_q row of `face`, set once per face */
    const uint16_t *op;     /* ori_q row of `face` */
    uint8_t face;           /* face being tried: 0..2, 3 = all tried */
    uint8_t turn;           /* quarter turns applied to cp, co so far: 0..3 */
    uint8_t mface;          /* the move that led here: face (NONE = root) */
    uint8_t mturn;          /*                         and quarter turns 1..3 */
} frame_t;

static const uint16_t *const perm_row[3] = {perm_q[0], perm_q[1], perm_q[2]};
static const uint16_t *const ori_row[3] = {ori_q[0], ori_q[1], ori_q[2]};

static frame_t stack[12];

static int h(uint16_t p, uint16_t o)
{
    return perm_pdb[p] > ori_pdb[o] ? perm_pdb[p] : ori_pdb[o];
}

/* Returns the solution length g; the moves are stack[1..g].mface / .mturn. */
static int solve(uint16_t p0, uint16_t o0)
{
    int bound = h(p0, o0);
    for (;;) {
        frame_t *f = stack;
        int g = 0;
        f->p = p0;
        f->o = o0;
        f->face = 0;
        f->turn = 0;
        f->mface = NONE;                             /* no face is NONE */
        for (;;) {
            if (f->p == 0 && f->o == 0)
                return g;
            if (f->turn == 0) {                      /* start a new face */
                if (f->face == f->mface)             /* same-face pruning */
                    ++f->face;
                if (f->face == 3) {                  /* all faces tried: pop */
                    if (f == stack)
                        break;                       /* root exhausted */
                    --f;
                    --g;
                    continue;
                }
                f->cp = f->p;
                f->co = f->o;
                f->pp = perm_row[f->face];           /* once per face */
                f->op = ori_row[f->face];
            }
            uint8_t face = f->face;
            f->cp = f->pp[f->cp];                    /* one more quarter turn */
            f->co = f->op[f->co];
            uint8_t turn = ++f->turn;
            if (turn == 3) {                         /* R' done: next face */
                f->turn = 0;
                ++f->face;
            }
            if ((g + 1) + h(f->cp, f->co) > bound)   /* g + h > bound: prune */
                continue;
            frame_t *c = f + 1;                      /* push the child */
            c->p = f->cp;
            c->o = f->co;
            c->face = 0;
            c->turn = 0;
            c->mface = face;
            c->mturn = turn;
            f = c;
            ++g;
        }
        ++bound;
    }
}

/* Copied from solver.c: Lehmer code of the permutation, base 3 of o[0..5]. */
static int parse(const char *text, uint16_t *p, uint16_t *o)
{
    uint8_t perm[CUBIES], ori[CUBIES], used = 0, sum = 0;
    if (strlen(text) != 2 * CUBIES)
        return 0;
    for (int i = 0; i < CUBIES; ++i) {
        perm[i] = (uint8_t) (text[i] - '1');
        ori[i] = (uint8_t) (text[i + CUBIES] - '1');
        if (perm[i] >= CUBIES || ori[i] >= 3 || (used >> perm[i] & 1))
            return 0;
        used |= (uint8_t) (1U << perm[i]);
        sum = (uint8_t) (sum + ori[i]);
    }
    if (sum % 3)
        return 0;
    uint32_t pr = 0, orr = 0;
    for (int i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (int j = i + 1; j < CUBIES; ++j)
            if (perm[j] < perm[i])
                ++smaller;
        pr = pr * (uint32_t) (CUBIES - i) + smaller;
    }
    for (int i = 0; i < 6; ++i)
        orr = orr * 3U + ori[i];
    *p = (uint16_t) pr;
    *o = (uint16_t) orr;
    return 1;
}

#ifndef SEARCH_NO_MAIN
int main(int argc, char **argv)
{
    static const char *const names[3][3] = {
        {"R", "R2", "R'"}, {"B", "B2", "B'"}, {"D", "D2", "D'"}};
    uint16_t p, o;
    if (argc != 2 || !parse(argv[1], &p, &o)) {
        fprintf(stderr, "usage: %s <14-digit state>\n", argv[0]);
        return 2;
    }
    int g = solve(p, o);

    /* Check the path with the tables: it must reach solved. */
    uint16_t cp = p, co = o;
    for (int i = 1; i <= g; ++i)
        for (int t = 0; t < stack[i].mturn; ++t) {
            cp = perm_q[stack[i].mface][cp];
            co = ori_q[stack[i].mface][co];
        }
    for (int i = 1; i <= g; ++i)
        printf("%s%s", i > 1 ? " " : "", names[stack[i].mface][stack[i].mturn - 1]);
    printf("\n%d moves, %s\n", g, cp == 0 && co == 0 ? "solves" : "WRONG");
    return 0;
}
#endif
