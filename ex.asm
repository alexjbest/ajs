main:
	add	%rsi,%rdi
	mov	%rdi,%rax
	add	%rdx,%rcx
	add	%rcx,%rax
	adc	%rsi,%rsi
	
	ret
