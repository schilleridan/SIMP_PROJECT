
add  $sp, $zero, $imm1, 0x7FF, 0       # initialize the stack pointer to the middle address of the memory
lw   $a0, $imm1, $zero, 0x100, 0       # initialize $a0 = n which sits at 0x100
jal  $ra, $imm1, $zero, factorial, 0   # call factorial(n)
sw   $v0, $imm1, $zero, 0x101, 0       # store the result %v0 to 0x101
halt $zero, $zero, $zero, 0, 0         # stop the program
  
factorial:
bne  $imm1, $a0, $zero, recurse, 0     # if n != 0 jump to recursive case 
add  $v0, $zero, $imm1, 1, 0           # base case where $v0 = 1
beq  $ra, $zero, $zero, 0, 0           # return to caller unconditionally 

recurse:
sw   $ra, $sp, $zero, 0, 0             # storing the return address in the current MEM[$sp]
sub  $sp, $sp, $imm1, 1, 0             # $sp-- 
sw   $a0, $sp, $zero, 0, 0             # saving n, meaning MEM[&sp] = $a0
sub  $sp, $sp, $imm1, 1, 0             # $sp-- 

sub  $a0, $a0, $imm1, 1, 0             # $a0 = n - 1
jal  $ra, $imm1, $zero, factorial, 0   # call factorial(n-1)

add  $sp, $sp, $imm1, 1, 0             # $sp++
lw   $t0, $sp, $zero, 0, 0             # setting $t0 = n since n = MEM[$sp]
add  $sp, $sp, $imm1, 1, 0             # $sp++
lw   $ra, $sp, $zero, 0, 0             # $ra = saved return address in MEM[$sp]

mul  $v0, $t0, $v0, 0, 0               # $v0 = n * factorial(n-1)
beq  $ra, $zero, $zero, 0, 0           # return to the original caller 

.word 0x100 5