    .text
    .p2align 4
    .globl paiso_fwd_gluBeginCurve
paiso_fwd_gluBeginCurve:
    movl $0, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluBeginPolygon
paiso_fwd_gluBeginPolygon:
    movl $1, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluBeginSurface
paiso_fwd_gluBeginSurface:
    movl $2, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluBeginTrim
paiso_fwd_gluBeginTrim:
    movl $3, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluBuild1DMipmaps
paiso_fwd_gluBuild1DMipmaps:
    movl $4, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluCylinder
paiso_fwd_gluCylinder:
    movl $5, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluDeleteNurbsRenderer
paiso_fwd_gluDeleteNurbsRenderer:
    movl $6, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluDeleteQuadric
paiso_fwd_gluDeleteQuadric:
    movl $7, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluDeleteTess
paiso_fwd_gluDeleteTess:
    movl $8, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluDisk
paiso_fwd_gluDisk:
    movl $9, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluEndCurve
paiso_fwd_gluEndCurve:
    movl $10, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluEndPolygon
paiso_fwd_gluEndPolygon:
    movl $11, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluEndSurface
paiso_fwd_gluEndSurface:
    movl $12, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluEndTrim
paiso_fwd_gluEndTrim:
    movl $13, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluErrorUnicodeStringEXT
paiso_fwd_gluErrorUnicodeStringEXT:
    movl $14, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluGetNurbsProperty
paiso_fwd_gluGetNurbsProperty:
    movl $15, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluGetString
paiso_fwd_gluGetString:
    movl $16, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluGetTessProperty
paiso_fwd_gluGetTessProperty:
    movl $17, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluLoadSamplingMatrices
paiso_fwd_gluLoadSamplingMatrices:
    movl $18, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluNewNurbsRenderer
paiso_fwd_gluNewNurbsRenderer:
    movl $19, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluNewQuadric
paiso_fwd_gluNewQuadric:
    movl $20, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluNewTess
paiso_fwd_gluNewTess:
    movl $21, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluNextContour
paiso_fwd_gluNextContour:
    movl $22, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluNurbsCallback
paiso_fwd_gluNurbsCallback:
    movl $23, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluNurbsCurve
paiso_fwd_gluNurbsCurve:
    movl $24, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluNurbsProperty
paiso_fwd_gluNurbsProperty:
    movl $25, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluNurbsSurface
paiso_fwd_gluNurbsSurface:
    movl $26, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluPartialDisk
paiso_fwd_gluPartialDisk:
    movl $27, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluPickMatrix
paiso_fwd_gluPickMatrix:
    movl $28, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluProject
paiso_fwd_gluProject:
    movl $29, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluPwlCurve
paiso_fwd_gluPwlCurve:
    movl $30, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluQuadricCallback
paiso_fwd_gluQuadricCallback:
    movl $31, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluQuadricDrawStyle
paiso_fwd_gluQuadricDrawStyle:
    movl $32, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluQuadricNormals
paiso_fwd_gluQuadricNormals:
    movl $33, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluQuadricOrientation
paiso_fwd_gluQuadricOrientation:
    movl $34, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluQuadricTexture
paiso_fwd_gluQuadricTexture:
    movl $35, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluScaleImage
paiso_fwd_gluScaleImage:
    movl $36, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluSphere
paiso_fwd_gluSphere:
    movl $37, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluTessBeginContour
paiso_fwd_gluTessBeginContour:
    movl $38, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluTessBeginPolygon
paiso_fwd_gluTessBeginPolygon:
    movl $39, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluTessCallback
paiso_fwd_gluTessCallback:
    movl $40, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluTessEndContour
paiso_fwd_gluTessEndContour:
    movl $41, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluTessEndPolygon
paiso_fwd_gluTessEndPolygon:
    movl $42, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluTessNormal
paiso_fwd_gluTessNormal:
    movl $43, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluTessProperty
paiso_fwd_gluTessProperty:
    movl $44, %eax
    jmp paiso_fwd_common
    .globl paiso_fwd_gluTessVertex
paiso_fwd_gluTessVertex:
    movl $45, %eax
    jmp paiso_fwd_common
paiso_fwd_common:
    pushq %rcx
    pushq %rdx
    pushq %r8
    pushq %r9
    subq $0x68, %rsp
    movdqu %xmm0, 0x20(%rsp)
    movdqu %xmm1, 0x30(%rsp)
    movdqu %xmm2, 0x40(%rsp)
    movdqu %xmm3, 0x50(%rsp)
    movl %eax, %ecx
    call paiso_resolve_fwd
    movdqu 0x20(%rsp), %xmm0
    movdqu 0x30(%rsp), %xmm1
    movdqu 0x40(%rsp), %xmm2
    movdqu 0x50(%rsp), %xmm3
    addq $0x68, %rsp
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    jmp *%rax
