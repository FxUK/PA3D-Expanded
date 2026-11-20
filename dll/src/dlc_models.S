# PAIso: model slots beyond the base game's 871 (sprite ids 871..16383: gates, floodlights, benches, gym gear ...).
#
# The game keeps one model pointer per base-game sprite in a table at renderer+0x50 (871 entries) and refuses
# or mis-reads any higher sprite id.  All six places that touch that table are redirected here: ids 0..870 use the
# game's own table exactly as before, ids 871..16383 use paiso_tbl[], a table of our own.  (Bounds are checked
# unsigned in two steps so a negative / huge id keeps the game's original behaviour.)
#
# Each stub replaces one instruction (5-6 bytes, see InstallDlcModels in main.cpp) and jumps back to *_cont.

    .data
    .globl paiso_K_cont, paiso_K_skip, paiso_J_ok, paiso_J_have, paiso_J_fail, paiso_L_cont, paiso_B1_cont, paiso_B2_cont, paiso_C1_cont, paiso_C2_cont, paiso_C3_cont
paiso_K_cont:  .quad 0
paiso_K_skip:  .quad 0
paiso_J_ok:    .quad 0
paiso_J_have:  .quad 0
paiso_J_fail:  .quad 0
paiso_L_cont:  .quad 0
paiso_B1_cont: .quad 0
paiso_B2_cont: .quad 0
paiso_C1_cont: .quad 0
paiso_C2_cont: .quad 0
paiso_C3_cont: .quad 0
paiso_tmp:     .quad 0
    .globl paiso_regs
    .globl paiso_tree_note
paiso_tree_note: .long 0          # set by paiso_note while a tree is being drawn
    .globl paiso_bare_id
paiso_bare_id: .long 0            # the sprite slot bare trees are drawn with (set by paiso_def_to_id)
    .balign 16                     # 1.9.102: C++ copies this block with aligned SSE moves (a crash at load when it sat off a 16-byte boundary)
paiso_regs:    .fill 16, 8, 0      # registers at the draw-path stubs (diagnostic: finding the object instance)
    .globl paiso_path
paiso_path:    .long 0            # which draw path called paiso_note: 1 = B1, 2 = C2 (each is checked on its own before early culling)

    .text

# ---- L: loader store  (mov [r15 + rdi*8 + 0x50], rax) ------------------------------------------
    .globl paiso_stubL
paiso_stubL:
    cmpq $0x366, %rdi
    jbe 1f
    cmpq $0x3fff, %rdi
    jbe 2f
1:  movq %rax, 0x50(%r15,%rdi,8)
    jmp *paiso_L_cont(%rip)
2:  pushq %rcx
    leaq paiso_tbl(%rip), %rcx
    movq %rax, (%rcx,%rdi,8)
    popq %rcx
    jmp *paiso_L_cont(%rip)

# ---- B1: draw path 1 test  (cmp qword [r12 + rax*8 + 0x50], 0) ----------------------------------
    .globl paiso_stubB1
paiso_stubB1:
    movl $1, paiso_path(%rip)
    movq %rax, paiso_regs+0(%rip)
    movq %rbx, paiso_regs+8(%rip)
    movq %rcx, paiso_regs+16(%rip)
    movq %rdx, paiso_regs+24(%rip)
    movq %rsi, paiso_regs+32(%rip)
    movq %rdi, paiso_regs+40(%rip)
    movq %rbp, paiso_regs+48(%rip)
    movq %r8, paiso_regs+56(%rip)
    movq %r9, paiso_regs+64(%rip)
    movq %r10, paiso_regs+72(%rip)
    movq %r11, paiso_regs+80(%rip)
    movq %r12, paiso_regs+88(%rip)
    movq %r13, paiso_regs+96(%rip)
    movq %r14, paiso_regs+104(%rip)
    movq %r15, paiso_regs+112(%rip)
    leaq 8(%rsp), %rax
    movq %rax, paiso_regs+120(%rip)
    movq paiso_regs+0(%rip), %rax
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    movq %rax, %rdx            # 2nd argument: the sprite index the game holds for this type
    pushq %rbx                 # aligned call: the game enters these stubs with the stack at either alignment, and C++
    movq %rsp, %rbx            # code using SSE (std::string / std::set) crashed on a misaligned one (movaps, 7 Nov 2026)
    andq $-16, %rsp
    subq $0x20, %rsp
    call paiso_note
    movq %rbx, %rsp
    popq %rbx
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    popq %rax
    # Types whose sprite lives in a later bank carry a bank-local index (the bench: 43), not a model-table id: resolve those by name first
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %rbx                 # aligned call: the game enters these stubs with the stack at either alignment, and C++
    movq %rsp, %rbx            # code using SSE (std::string / std::set) crashed on a misaligned one (movaps, 7 Nov 2026)
    andq $-16, %rsp
    subq $0x20, %rsp
    call paiso_def_to_id
    movq %rbx, %rsp
    popq %rbx
    movl %eax, paiso_tmp(%rip)
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    popq %rax
    pushq %rdx
    movslq paiso_tmp(%rip), %rdx
    testq %rdx, %rdx
    jle 6f
    movq %rdx, %rax            # resolved bank-2 id replaces the bank-local index
    movq %rdx, %rbx
    popq %rdx
    jmp 9f
6:  cmpq $-3, %rdx            # -3 = off screen (paiso_note's early cull, 1.9.83): draw nothing for this object
    jne 8f
    xorl %edx, %edx
    cmpq $0, %rdx              # ZF = 1: "no model", exactly as for an object without one
    popq %rdx
    jmp *paiso_B1_cont(%rip)
8:  cmpq $-2, %rdx            # -2 = the accountant: sprite index 0 is a real model
    popq %rdx
    jne 7f
    xorl %eax, %eax
    xorl %ebx, %ebx
    jmp 1f
7:  testq %rax, %rax
    jnz 9f
    pushq %rdx                 # still 0 = no sprite: draw nothing
    xorl %edx, %edx
    cmpq $0, %rdx
    popq %rdx
    jmp *paiso_B1_cont(%rip)
9:  cmpq $0x366, %rax
    jbe 1f
    cmpq $0x3fff, %rax
    jbe 2f
1:  cmpq $0, 0x50(%r12,%rax,8)
    jmp *paiso_B1_cont(%rip)
2:  pushq %rdx
    leaq paiso_tbl(%rip), %rdx
    movq (%rdx,%rax,8), %rdx
    testq %rdx, %rdx
    popq %rdx
    jnz 5f
    pushq %rax                 # no model for this sprite id: tell the log which object type / id it was (diagnostic)
    pushq %rcx
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    movq %rax, %rdx
    pushq %rbx                 # aligned call: the game enters these stubs with the stack at either alignment, and C++
    movq %rsp, %rbx            # code using SSE (std::string / std::set) crashed on a misaligned one (movaps, 7 Nov 2026)
    andq $-16, %rsp
    subq $0x20, %rsp
    call paiso_miss
    movq %rbx, %rsp
    popq %rbx
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    popq %rax
5:  pushq %rdx
    leaq paiso_tbl(%rip), %rdx
    cmpq $0, (%rdx,%rax,8)
    popq %rdx
    jmp *paiso_B1_cont(%rip)

# ---- B2: draw path 1 load  (mov r12, [r12 + rbx*8 + 0x50]) --------------------------------------
    .globl paiso_stubB2
paiso_stubB2:
    # a bare tree reaches this load with sprite index 0 (the accountant) even after paiso_def_to_id redirected it: load the bare-tree model
    testq %rbx, %rbx
    jnz 3f
    cmpl $0, paiso_tree_note(%rip)
    je 3f
    cmpl $0x366, paiso_bare_id(%rip)
    jbe 3f
    pushq %rax
    movslq paiso_bare_id(%rip), %rax
    leaq paiso_tbl(%rip), %r12
    movq (%r12,%rax,8), %r12
    popq %rax
    jmp *paiso_B2_cont(%rip)
3:  cmpq $0x366, %rbx
    jbe 1f
    cmpq $0x3fff, %rbx
    jbe 2f
1:  movq 0x50(%r12,%rbx,8), %r12
    jmp *paiso_B2_cont(%rip)
2:  leaq paiso_tbl(%rip), %r12
    movq (%r12,%rbx,8), %r12
    jmp *paiso_B2_cont(%rip)

# ---- C1: draw path 2, attached-item test  (cmp qword [r15 + r11*8 + 0x50], 0) -------------------
    .globl paiso_stubC1
paiso_stubC1:
    testq %r11, %r11            # attached sprite 0 on a tree = the accountant model (bare trees showed as a man in a suit): no attached model
    jnz 0f
    cmpl $0, paiso_tree_note(%rip)
    je 0f
    cmpq %r11, %r11             # sets ZF, as "cmp [model], 0" does for a missing model
    jmp *paiso_C1_cont(%rip)
0:  cmpq $0x366, %r11
    jbe 1f
    cmpq $0x3fff, %r11
    jbe 2f
1:  cmpq $0, 0x50(%r15,%r11,8)
    jmp *paiso_C1_cont(%rip)
2:  pushq %rax
    leaq paiso_tbl(%rip), %rax
    cmpq $0, (%rax,%r11,8)
    popq %rax
    jmp *paiso_C1_cont(%rip)

# ---- C2: draw path 2 test  (cmp qword [rax + r15*8 + 0x50], 0) ----------------------------------
    .globl paiso_stubC2
paiso_stubC2:
    # same name-first resolution as B1: a type from a later sprite bank carries a bank-local index in r15; swap in its real model id (rcx = the definition)
    # tell the DLC which object type this draw belongs to (wall-snap type, person, door), exactly as path 1 does
    movl $2, paiso_path(%rip)
    movq %rax, paiso_regs+0(%rip)
    movq %rbx, paiso_regs+8(%rip)
    movq %rcx, paiso_regs+16(%rip)
    movq %rdx, paiso_regs+24(%rip)
    movq %rsi, paiso_regs+32(%rip)
    movq %rdi, paiso_regs+40(%rip)
    movq %rbp, paiso_regs+48(%rip)
    movq %r8, paiso_regs+56(%rip)
    movq %r9, paiso_regs+64(%rip)
    movq %r10, paiso_regs+72(%rip)
    movq %r11, paiso_regs+80(%rip)
    movq %r12, paiso_regs+88(%rip)
    movq %r13, paiso_regs+96(%rip)
    movq %r14, paiso_regs+104(%rip)
    movq %r15, paiso_regs+112(%rip)
    leaq 8(%rsp), %rax
    movq %rax, paiso_regs+120(%rip)
    movq paiso_regs+0(%rip), %rax
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    movq %r15, %rdx
    pushq %rbx                 # aligned call: the game enters these stubs with the stack at either alignment, and C++
    movq %rsp, %rbx            # code using SSE (std::string / std::set) crashed on a misaligned one (movaps, 7 Nov 2026)
    andq $-16, %rsp
    subq $0x20, %rsp
    call paiso_note
    movq %rbx, %rsp
    popq %rbx
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    popq %rax
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %rbx                 # aligned call: the game enters these stubs with the stack at either alignment, and C++
    movq %rsp, %rbx            # code using SSE (std::string / std::set) crashed on a misaligned one (movaps, 7 Nov 2026)
    andq $-16, %rsp
    subq $0x20, %rsp
    call paiso_def_to_id
    movq %rbx, %rsp
    popq %rbx
    movl %eax, paiso_tmp(%rip)
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    popq %rax
    pushq %rdx
    movslq paiso_tmp(%rip), %rdx
    testq %rdx, %rdx
    jle 6f
    movq %rdx, %r15
    popq %rdx
    jmp 3f
6:  cmpq $-3, %rdx            # -3 = off screen (early cull): draw nothing for this object
    jne 8f
    xorl %edx, %edx
    cmpq $0, %rdx
    popq %rdx
    jmp *paiso_C2_cont(%rip)
8:  cmpq $-2, %rdx            # the accountant keeps slot 0
    popq %rdx
    je 3f
    testq %r15, %r15           # sprite index 0 and no bank-2 model: draw nothing
    jnz 3f
    pushq %rdx
    xorl %edx, %edx
    cmpq $0, %rdx
    popq %rdx
    jmp *paiso_C2_cont(%rip)
3:  cmpq $0x366, %r15
    jbe 1f
    cmpq $0x3fff, %r15
    jbe 2f
1:  cmpq $0, 0x50(%rax,%r15,8)
    jmp *paiso_C2_cont(%rip)
2:  pushq %rcx
    leaq paiso_tbl(%rip), %rcx
    cmpq $0, (%rcx,%r15,8)
    popq %rcx
    jmp *paiso_C2_cont(%rip)

# ---- C3: draw path 2 load  (mov r15, [rax + r15*8 + 0x50]); rax is overwritten right after -------
    .globl paiso_stubC3
paiso_stubC3:
    testq %r15, %r15            # the same for draw path 2 (rax is free here)
    jnz 3f
    cmpl $0, paiso_tree_note(%rip)
    je 3f
    cmpl $0x366, paiso_bare_id(%rip)
    jbe 3f
    movslq paiso_bare_id(%rip), %r15
    leaq paiso_tbl(%rip), %rax
    movq (%rax,%r15,8), %r15
    jmp *paiso_C3_cont(%rip)
3:  cmpq $0x366, %r15
    jbe 1f
    cmpq $0x3fff, %r15
    jbe 2f
1:  movq 0x50(%rax,%r15,8), %r15
    jmp *paiso_C3_cont(%rip)
2:  leaq paiso_tbl(%rip), %rax
    movq (%rax,%r15,8), %r15
    jmp *paiso_C3_cont(%rip)


# ---- J: loader name lookup failed  (js fail  after  test eax,eax) ---------------------------------
# The game's sprite-name table only knows the base bank and objects_d11. For a name it does not know, ask paiso_resolve_missing
# (names of objects_d11_2 -> id); a hit carries on loading the model with that id in rdi, a miss takes the original failure path.
    .globl paiso_stubJ
paiso_stubJ:
    js 1f
    jmp *paiso_J_ok(%rip)
1:  leaq -0x41(%rbp), %rcx
    cmpq $0x10, -0x29(%rbp)
    jb 2f
    movq -0x41(%rbp), %rcx
2:
    pushq %rbx                 # aligned call: the game enters these stubs with the stack at either alignment, and C++
    movq %rsp, %rbx            # code using SSE (std::string / std::set) crashed on a misaligned one (movaps, 7 Nov 2026)
    andq $-16, %rsp
    subq $0x20, %rsp
    call paiso_resolve_missing
    movq %rbx, %rsp
    popq %rbx
    testl %eax, %eax
    js 3f
    movslq %eax, %rdi
    jmp *paiso_J_have(%rip)
3:  jmp *paiso_J_fail(%rip)

# ---- K: 'does this sprite id have a 3D model, so the flat 2D sprite can be skipped?'  (site 0x429e37: ja short; mov rcx,rax) ----
# The game only knows ids 0..870 there. For ids above that, ask our own model table: a model means: do not draw the flat sprite.
    .globl paiso_stubK
paiso_stubK:
    ja 1f
    movq %rax, %rcx
    jmp *paiso_K_cont(%rip)
1:  cmpl $0x3fff, %eax
    ja 2f
    pushq %rdx
    leaq paiso_tbl(%rip), %rdx
    movq (%rdx,%rax,8), %rdx
    testq %rdx, %rdx
    popq %rdx
    je 2f
    movl %r9d, %edi
2:  jmp *paiso_K_skip(%rip)
