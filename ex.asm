main:
	add	%rsi,%rdi
	mov	%rdi,%rax
	add	%rdx,%rcx
	add	%rcx,%rax
	adc	%rsi,%rsi
	jnz	end
	adc	%rsi,%rsi
	lea	1(%eax,%eax,2),%eax
end:
	ret
