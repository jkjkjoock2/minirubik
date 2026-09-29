# Animate a 4x4 sprite across a Ripes LED matrix.
# Instantiate an "LED Matrix" device in the I/O tab before assembling,
# otherwise the LED_MATRIX_0_* symbols are undefined.

        .equ  BLUE,   0x0000FF
        .equ  INIT_X, 10
        .equ  INIT_Y, 10
        .equ  STEP_X, 2                 # columns moved per animation step
        .equ  STEPS,  5
        .equ  DELAY,  8192

        .data
# 4x4 sprite, row-major, one 0x00RRGGBB word per pixel
face:
        .word 0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00   # forehead
        .word 0x00FFFF00, 0x00000000, 0x00000000, 0x00FFFF00   # eyes
        .word 0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00   # cheeks
        .word 0x00FFFF00, 0x00FF0000, 0x00FF0000, 0x00FFFF00   # mouth
blank:
        .word BLUE, BLUE, BLUE, BLUE
        .word BLUE, BLUE, BLUE, BLUE
        .word BLUE, BLUE, BLUE, BLUE
        .word BLUE, BLUE, BLUE, BLUE

        .text
        .globl main

# Register allocation:
#   s0  frame buffer base        s1  row stride in bytes
#   s2  first address past the frame buffer
#   s3  address of the sprite's top-left pixel
#   s4  remaining animation steps
main:
        li    s0, LED_MATRIX_0_BASE
        li    t0, LED_MATRIX_0_SIZE
        add   s2, s0, t0
        li    t0, LED_MATRIX_0_WIDTH
        slli  s1, t0, 2                 # stride = WIDTH * 4 bytes

        # Paint every pixel blue.
        li    t1, BLUE
        mv    t0, s0
clear:
        sw    t1, 0(t0)
        addi  t0, t0, 4
        bltu  t0, s2, clear             # unsigned: 0xF0000000 is negative as signed

        # addr(x, y) = base + (y * WIDTH + x) * 4, evaluated once without mul.
        mv    s3, s0
        li    t0, INIT_Y
        beqz  t0, at_row                # INIT_Y = 0 must not enter the loop
skip_row:
        add   s3, s3, s1
        addi  t0, t0, -1
        bnez  t0, skip_row
at_row:
        addi  s3, s3, INIT_X * 4

        li    s4, STEPS
        la    a0, face
        jal   blit
        beqz  s4, done                  # STEPS = 0 must not enter the loop
animate:
        jal   delay
        la    a0, blank
        jal   blit                      # erase at the current position
        addi  s3, s3, STEP_X * 4        # right STEP_X columns
        add   s3, s3, s1                # down 1 row
        la    a0, face
        jal   blit
        addi  s4, s4, -1
        bnez  s4, animate
done:
        li    a7, 10                    # Exit
        ecall

# blit(a0 = address of a 4x4 block of words) copies that block into the matrix
# at the position held in s3. Leaf function, so it needs no stack frame.
# Reads s1 and s3, writes t0-t4, and consumes a0 by walking it to the end
# of the block.
blit:
        li    t0, 4                     # rows remaining
        mv    t1, s3                    # destination row pointer
blit_row:
        li    t2, 4                     # columns remaining
        mv    t3, t1
blit_col:
        lw    t4, 0(a0)
        sw    t4, 0(t3)
        addi  a0, a0, 4
        addi  t3, t3, 4
        addi  t2, t2, -1
        bnez  t2, blit_col
        add   t1, t1, s1                # advance one display row
        addi  t0, t0, -1
        bnez  t0, blit_row
        ret

# Busy-wait so the animation is visible at simulated clock speeds.
delay:
        li    t0, DELAY
delay_loop:
        addi  t0, t0, -1
        bnez  t0, delay_loop
        ret
