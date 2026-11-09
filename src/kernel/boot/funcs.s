.macro pushaq
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
.endm

.macro popaq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
.endm


.section .data
gdtr:
	.word 0 # For limit storage
	.quad 0 # For base storage

.section .text
.global set_gdt
set_gdt:
	movw %di, gdtr
	movq %rsi, gdtr+2
	lgdt gdtr

reload_segments:
	pushq $0x08
	leaq .reload_CS(%rip), %rax # RIP relative addressing I believe
	pushq %rax
	lretq # Long return

.reload_CS:
	movw $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    ret

.global enable_shit
# void enable_shit(void);
enable_shit:
	ret

.global enable_sse
# void enable_sse(void);
enable_sse: # I had to seperate this from the check_shit and enable_shit functions since
	# the CPU kept triple faulting and the Create GDT, Allocator, and enable_shit all depended
	# on one of the other ones going before it. Which means we switch out enable_shit to here.

	# enable SSE I think?
	mov %cr0, %rax
	and $0xFFFB, %ax
	or $0x2, %ax
	mov %rax, %cr0
	mov %cr4, %rax
	or $(3 << 9), %rax
	mov %rax, %cr4
	ret


.global check_phys_addr_size
# uint8_t check_phys_addr_size(void);
check_phys_addr_size:
	pushq %rbx
	pushq %rcx
	pushq %rdx
	# 0x80000008 eax 7:0 = the value we want
	# 0x80000000 eax needs to be higher than the leaf we want from
	movl $0x80000008, %eax
	cpuid

	cmp $0x80000008, %eax
	jnc 0f # if %eax >= $0x80000008

	movl $0x80000008, %eax
	cpuid

	
	movb %al, %bl
	xor %rax, %rax
	movb %bl, %al

	popq %rdx
	popq %rcx
	popq %rbx
	ret

0:
	popq %rdx
	popq %rcx
	popq %rbx
	movq $36, %rax # We shall assume its 36 bits if its not supported.
	ret

.global check_shit
# int check_shit(void);
# Note to self! Later only save the registers that we touch! 
check_shit:
	pushaq # Save all our registers using the macro

	movl $0x0, %eax # Check if the CPU supports the highest leaf with features we check here
	cpuid
	cmp $0x7, %eax # Check it against 0x7
	jnc 2f # %eax >= $0x7

	popaq
	movq $1, %rax
	ret

2:
	movl $0x1, %eax # Check if this supports PCID
	cpuid
	bt $17, %ecx # Check bit 17 of ecx
	jc 3f
	
	popaq
	movq $2, %rax
	ret


3:
	xor %ecx, %ecx # Zero ecx

	movl $0x7, %eax # Check if this supports INVPCID
	cpuid
	bt $10, %ebx # Check bit 10 of ebx
	jc 0f
	popaq
	
	movq $3, %rax

0: 

	popaq # restore all our registers using the macro
	movq $0, %rax
	ret # Return true, as in supported