        .text
main:
        li t0, 1000000     # 1. 把計數器 t0 設成 N（先用 1000000）

loop:
        addi t0,t0,-1         # 2. 計數器 t0 減 1

        bnez  t0, loop     # 3. t0 不是 0 就跳回 loop

        li    a7, 10       # 4. 把 a7 設成 10（代表結束程式）

        ecall          # 5. 呼叫 ecall
