.code16
.att_syntax
.globl _start
_start:
	movw %cs,%ax 
	movw %ax,%ds 
	movw %ax,%ss 
	movw _start,%sp

	movw $0x1000,%ax	# Загрузка адреса ядра
	movw %ax,%es
	movw $0x0000,%bx
	movb $0x02,%ah
	movb $1,%dl
	movb $0,%dh
	movb $0,%ch
	movb $1,%cl
	movb $18,%al
	int $0x13
#---------ЭТИ ДВА СТОЛБЦА РПИЧИНА МОИХ 3х НЕДЛЬНЫХ МУЧЕНИЙ ИЗ_ЗА ИЗ ОТСУТСТВИЯ ЯДРО РАБОТАЛО НЕККОРЕКТНО Я ХОЧУ УМЕРЕТЬ----------------------------
	movw $0x2400,%bx
	movb $1,%dl
	movb $1,%dh
	movb $0,%ch
	movb $1,%cl
	movb $18,%al
	movb $0x02,%ah
	int $0x13

	movw $0x4800,%bx
	movb $1,%dl
	movb $0,%dh
	movb $0,%ch
	movb $65,%cl
	movb $18,%al
	movb $0x02,%ah
	int $0x13	
#---------------------------------------------------------------------------------------------------------------------------------------------------
	inb $0x92, %al
	orb $2, %al
	outb %al, $0x92

	movb $0x0e, %ah	# Тут происходит вывод строки(блок команд для вывода)
	movw $loading_str, %bx
	call puts

	movb $'\n',%al
	int $0x10	
	movb $'\r',%al
	int $0x10


	movb $0,%cl
	jmp color

color_is_chosen:
	movb %cl, 0x9000

	lgdt gdt_info
	cli	
	movl %cr0, %eax
	orb $1, %al
	movl %eax, %cr0

	ljmp $0x8, $protected_mode 


puts:
	movb 0(%bx), %al
	test %al, %al
	jz end_puts
	movb $0x0e, %ah
	int $0x10
	addw $1, %bx
	jmp puts

loading_str:
	.asciz "Loading ConvertOS..."
choose_str:
	.asciz "Hello!Choose color for system:\n\r"
gray_str:
	.asciz "gray"
white_str:
	.asciz "white"
yellow_str:
	.asciz "yellow"
blue_str:
	.asciz "blue"
red_str:
	.asciz "red"
green_str:
	.asciz "green"
pike_str:
	.asciz " <"
endS_str:
	.asciz "\n\r"


print_pike:
	movw $pike_str,%bx
	int $0x10
	ret

end_puts:
	ret
inf_loop:
	jmp inf_loop 

gdt_info:
	.word gdt_info - gdt
	.word gdt, 0
gdt:
	.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	.byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
	.byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00

color:

	movw $3,%ax	# Очистка экрана
	int $0x10

	movb $0x0e, %ah
	movw $choose_str, %bx
	call puts
	
	call print
	jmp input
	


print:
	movw $gray_str,%bx	 #for gray
	call puts
	cmpb $0,%cl
	jne gray

	movw $pike_str,%bx
	call puts

 	gray:
	movw $endS_str,%bx
	call puts


	movw $white_str,%bx	 #for white
	call puts
	cmpb $1,%cl
	jne white

	movw $pike_str,%bx
	call puts

	white:
	movw $endS_str,%bx
	call puts


	movw $yellow_str,%bx	 #for yellow
	call puts
	cmpb $2,%cl
	jne yellow

	movw $pike_str,%bx
	call puts

	yellow:
	movw $endS_str,%bx
	call puts
	
	
	movw $blue_str,%bx	 #for blue
	call puts
	cmpb $3,%cl
	jne blue

	movw $pike_str,%bx
	call puts

	blue:
	movw $endS_str,%bx
	call puts


	movw $red_str,%bx	 #for red
	call puts
	cmpb $4,%cl
	jne red

	movw $pike_str,%bx
	call puts

	red:
	movw $endS_str,%bx
	call puts


	movw $green_str,%bx	 #for green
	call puts
	cmpb $5,%cl
	jne green

	movw $pike_str,%bx
	call puts

	green:
	movw $endS_str,%bx
	call puts
	ret


input:
	movb $0x00,%ah		#input from keyboard
	int $0x16
	cmpb $13,%al		#compare to enter
	je color_is_chosen

	cmpb $0x48,%ah		#compare to up
	je up
	cmpb $0x50,%ah
	je down
	jmp color

up:

	addb $5,%cl
	cmpb $6,%cl
	jb color
	subb $6,%cl

	jmp color
down:

	addb $1,%cl
	cmpb $6,%cl
	jb color
	subb $6,%cl

	jmp color

.code32
protected_mode:
	movw $0x10, %ax
	movw %ax, %es
	movw %ax, %ds
	movw %ax, %ss
	call 0x10000


.zero (512 - (. -_start) - 2)
.byte 0x55, 0xAA 
