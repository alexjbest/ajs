














































	.globl	__gmpn_and_n
	.type	__gmpn_and_n,@function
__gmpn_and_n:

	mov	$3,%r8
	lea	-24(%rsi,%rcx,8),%rsi
	lea	-24(%rdx,%rcx,8),%rdx
	lea	-24(%rdi,%rcx,8),%rdi
	sub	%rcx,%r8
	jnc	skiplp
.align	16,	0x90
lp:
	movdqu	(%rdx,%r8,8),%xmm0
	movdqu	16(%rdx,%r8,8),%xmm1
	movdqu	16(%rsi,%r8,8),%xmm3
	movdqu	(%rsi,%r8,8),%xmm2
	pand	%xmm2,%xmm0
	movdqu	%xmm0,(%rdi,%r8,8)
	pand	%xmm3,%xmm1
	add	$4,%r8
	movdqu	%xmm1,-16(%rdi,%r8,8)
	jnc	lp
skiplp:
	cmp	$2,%r8
	ja	case0
	je	case1
	jp	case2	
case3:	movdqu	(%rdx,%r8,8),%xmm0
	mov	16(%rdx,%r8,8),%rax
	mov	16(%rsi,%r8,8),%rcx
	movdqu	(%rsi,%r8,8),%xmm2
	pand	%xmm2,%xmm0
	movdqu	%xmm0,(%rdi,%r8,8)
	and	%rcx,%rax
	mov	%rax,16(%rdi,%r8,8)
case0:	ret
case2:	movdqu	(%rdx,%r8,8),%xmm0
	movdqu	(%rsi,%r8,8),%xmm2
	pand	%xmm2,%xmm0
	movdqu	%xmm0,(%rdi,%r8,8)
	ret
case1:	mov	(%rdx,%r8,8),%rax
	mov	(%rsi,%r8,8),%rcx
	and	%rcx,%rax
	mov	%rax,(%rdi,%r8,8)
	ret
	.size	__gmpn_and_n,.-__gmpn_and_n
