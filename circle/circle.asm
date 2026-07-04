#registers usage#
#   $s0 = Yc (center row)      $s1 = Xc (center column)
#   $a0 = R                    $a1 = R^2
#   $a2 = Y (current row)      $a3 = Yend (last row)
#   $t0 = dy^2                 $t1 = X (current column)    $t2 = Xend (last column)
#   $gp = 255 (white)          $sp = 1 (draw command)
#   $v0 = scratch              $ra = pixel address

lw  $v0, $zero, $imm1, 0x100, 0     # $v0 = center offset
srl $s0, $v0, $imm1, 8, 0           # Yc = offset >> 8   offset / 256
and $s1, $v0, $imm1, 255, 0         # Xc = offset & 255  offset % 256
lw  $a0, $zero, $imm1, 0x101, 0     # $a0 = R (the radius)
mul $a1, $a0, $a0, 0, 0             # $a1 = R^2
add $gp, $zero, $imm1, 255, 0       # $gp = 255 
add $sp, $zero, $imm1, 1, 0         # $sp = 1   monitor draw command
sub $a2, $s0, $a0, 0, 0             # Y    = Yc - R   which is the first row
add $a3, $s0, $a0, 0, 0             # Yend = Yc + R   is the last row

#Outer loop over rows Y#
L_Y_LOOP:
blt $imm1, $a3, $a2, L_END, 0       # if Yend < Y then we are finished and can halt
sub $v0, $a2, $s0, 0, 0             # dy  = Y - Yc
mul $t0, $v0, $v0, 0, 0             # dy2 = dy * dy
sub $t1, $s1, $a0, 0, 0             # X    = Xc - R   which is the first column
add $t2, $s1, $a0, 0, 0             # Xend = Xc + R   is the last column

#Inner loop over columns X#
L_X_LOOP:
blt $imm1, $t2, $t1, L_NEXT_Y, 0     # if Xend < X we reached the end of the row and can go to the next y 
sub $v0, $t1, $s1, 0, 0              # dx   = X - Xc
mul $v0, $v0, $v0, 0, 0              # dx2  = dx * dx
add $v0, $v0, $t0, 0, 0              # dist = dx2 + dy2
blt $imm1, $a1, $v0, L_NEXT_X, 0     # if R^2 < dist then we are out of the circle and can go to the next x
sll $ra, $a2, $imm1, 8, 0            # addr = Y << 8   = Y * 256
add $ra, $ra, $t1, 0, 0              # addr = addr + X
out $ra, $zero, $imm1, 20, 0         # monitoraddr (20) = addr
out $gp, $zero, $imm1, 21, 0         # monitordata (21) = 255 which is white
out $sp, $zero, $imm1, 22, 0         # monitorcmd  (22) = 1   draws the actual pixel 

L_NEXT_X:
add $t1, $t1, $imm1, 1, 0            # X++
beq $imm1, $zero, $zero, L_X_LOOP, 0 # repeat inner loop

L_NEXT_Y:
add $a2, $a2, $imm1, 1, 0            # Y++
beq $imm1, $zero, $zero, L_Y_LOOP, 0 # repeat outer loop

L_END:
halt $zero, $zero, $zero, 0, 0       # finish

.word 0x100 32896
.word 0x101 30