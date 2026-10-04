.text
main:
    li t0, 1000        # 1. 計數器 t0 = N = 1000
    li t1, 0x10000000     # 2. 位址暫存器 t1 = 0x10000000

loop:
    sw t0, 0(t1)          # 3. 把 t0 的值存到 t1 指的位址
    addi t1, t1, 4        # 4. 位址 t1 加 4
    addi t0, t0, -1       # 5. 計數器 t0 減 1
    bne t0, x0, loop      # 6. t0 不是 0 就跳回 loop
    li a7, 10             # 7. a7 = 10
    ecall                 # 8. 結束程式
