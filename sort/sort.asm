
add  $s1, $zero, $imm1, 0x100, 0         # set $s1 = base address of array which we know sits at address 0x100
add  $t0, $zero, $zero, 0, 0             # initialize $t0 = 0 which in our case will be the counter for the outer loop named I


outer_loop:
sub  $s0, $imm1, $t0, 15, 0              # setting $s0 = 15 - i which will be the limit for the inner loop which can get smaller by 1 every iteration
beq  $imm1, $s0, $zero, done, 0          # if 15 - i = 0 it means that i = 15 and therefore we are done so we branch to done
add  $t1, $zero, $zero, 0, 0             # set $t1 = 0 which in our case will be the the counter for the inner loop j 


inner_loop:
bge  $imm1, $t1, $s0, next_outer, 0      # stop condition for the inner loop, if j >= 15-i we branch to the next outer loop iteration 
lw   $t2, $s1, $t1, 0, 0                 # setting $t2 = arr[j], which in our case is MEM[base + j]
add  $a0, $s1, $t1, 0, 0                 # $a0 = base + j which is the address for the current arr[j]
lw   $a1, $a0, $imm1, 1, 0               # by using $a0 we set the value of $a1 to be arrr[j+1] which sits in MEM[base + j +1]
bge  $imm1, $t2, $a1, no_swap, 0         # if arr[j] >= arr[j+1] then no swap is needed so we can branch to no_swap
sw   $a1, $s1, $t1, 0, 0                 # moving the larger value left, meaning arr[j+1] to MEM[base + j]
sw   $t2, $a0, $imm1, 1, 0               # moving the smaller value right, meaning arr[j] to MEM[base + j +1]

no_swap:
add  $t1, $t1, $imm1, 1, 0               # j++
beq  $imm1, $zero, $zero, inner_loop, 0  # unconditional jump back to inner loop


next_outer:
add  $t0, $t0, $imm1, 1, 0               # i++
beq  $imm1, $zero, $zero, outer_loop, 0  # unconditional jump back to outer loop


done:
halt $zero, $zero, $zero, 0, 0           # finish running the program

.array 0x100 13, 2, 99, 7, 55, 42, 17, 17, 5, 88, 1, 64, 23, 9, 100, 31