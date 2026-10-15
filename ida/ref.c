/* ref.c - the gcc reference build for Stage 4: the same algorithm as
 * search.c (parse, solve, replay check, print), freestanding for Ripes.
 * Build:
 *   riscv-none-elf-gcc -O2 -march=rv32i -mabi=ilp32 -nostartfiles \
 *       -I ida -o ida/ref.elf ida/ref.c
 * Run:  Ripes --mode cli -t elf --src ida/ref.elf --proc RV32_ISS --iret
 * Exit codes as in rubik.s: 0 solved and verified, 1 check failed, 2 invalid.
 */
#define SEARCH_NO_MAIN
#include "search.c"

static const char state[] = "21345671111111";

static void ecall_putchar(int c)
{
    register int a0 __asm__("a0") = c;
    register int a7 __asm__("a7") = 11;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7));
}

static int run(void)
{
    static const char names[] = "RBD";
    uint16_t p, o;
    if (!parse(state, &p, &o))
        return 2;
    int g = solve(p, o);

    uint16_t cp = p, co = o;                 /* T5: replay the path */
    for (int i = 1; i <= g; ++i)
        for (int t = 0; t < stack[i].mturn; ++t) {
            cp = perm_q[stack[i].mface][cp];
            co = ori_q[stack[i].mface][co];
        }
    if (cp != 0 || co != 0)
        return 1;

    for (int i = 1; i <= g; ++i) {           /* print, as rubik.s does */
        if (i > 1)
            ecall_putchar(' ');
        ecall_putchar(names[stack[i].mface]);
        if (stack[i].mturn == 2)
            ecall_putchar('2');
        else if (stack[i].mturn == 3)
            ecall_putchar('\'');
    }
    ecall_putchar('\n');
    return 0;
}

void _start(void)
{
    register int a0 __asm__("a0") = run();
    register int a7 __asm__("a7") = 93;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7));
    for (;;)
        ;
}
