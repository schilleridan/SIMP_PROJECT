
add $s0, $zero, $zero, 0, 0                     # $s0 = sector counter which we will start equal to 0 
add $s1, $imm1, $zero, 8, 0                     # $s1 = 8 which will represent the max sector
add $a0, $imm1, $zero, 0x100, 0                 # $a0 = sum_buffer base address (0x100)
add $a1, $imm1, $zero, 0x200, 0                 # $a1 = read_buffer base address (0x200)
add $a2, $imm1, $zero, 128, 0                   # $a2 = 128 which is the number of words per sector

#clear the sum buffer#
add $t0, $zero, $zero, 0, 0                     # $t0 = loop counter i = 0
clear_loop:
beq $imm1, $t0, $a2, sector_loop, 0             # if i == 128, break to sector_loop
add $t1, $a0, $t0, 0, 0                         # $t1 = sum_buffer + i
sw $zero, $t1, $zero, 0, 0                      # Mem[sum_buffer + i] = 0
add $t0, $t0, $imm1, 1, 0                       # i++
beq $imm1, $zero, $zero, clear_loop, 0          # jump to clear_loop

#Main loop over the sectors#
sector_loop:
beq $imm1, $s0, $s1, write_sector, 0            # if sector == 8 then we are done reading and can go to write

#Wait for disk to be free#
wait_read_ready:
in $t0, $imm1, $zero, 17, 0                     # read diskstatus
bne $imm1, $t0, $zero, wait_read_ready, 0       # if status != 0 then keep waiting

#Issue Read Command for sector $s0 into read_buffer $a1#
out $s0, $imm1, $zero, 15, 0                    # disksector = $s0
out $a1, $imm1, $zero, 16, 0                    # diskbuffer = $a1 
out $imm2, $imm1, $zero, 14, 1                  # diskcmd = 1 which is the read command

#Wait for disk read to complete#
wait_read_done:
in $t0, $imm1, $zero, 17, 0                     # read diskstatus
bne $imm1, $t0, $zero, wait_read_done, 0        # wait until status == 0


out $zero, $imm1, $zero, 4, 0                   # irq1status (register 4) = 0

#Add read_buffer to sum_buffer#
add $t0, $zero, $zero, 0, 0                     # $t0 = loop counter i = 0
add_loop:
beq $imm1, $t0, $a2, next_sector, 0             # if i == 128, break to next_sector
add $t1, $a0, $t0, 0, 0                         # $t1 = sum_buffer + i
add $t2, $a1, $t0, 0, 0                         # $t2 = read_buffer + i
lw $a3, $t1, $zero, 0, 0                        # $a3 = Mem[sum_buffer + i]
lw $t2, $t2, $zero, 0, 0                        # $t2 = Mem[read_buffer + i]
add $a3, $a3, $t2, 0, 0                         # $a3 = sum_buffer_val + read_buffer_val
sw $a3, $t1, $zero, 0, 0                        # Mem[sum_buffer + i] = $a3
add $t0, $t0, $imm1, 1, 0                       # i++
beq $imm1, $zero, $zero, add_loop, 0            # jump to add_loop

#Move to next sector#
next_sector:
add $s0, $s0, $imm1, 1, 0                       # sector++
beq $imm1, $zero, $zero, sector_loop, 0         # jump back to sector_loop

#Write the sum_buffer to sector 8#
write_sector:

# Wait for disk to be free
wait_write_ready:
in $t0, $imm1, $zero, 17, 0                     # read diskstatus
bne $imm1, $t0, $zero, wait_write_ready, 0      # wait until status == 0

# Issue Write Command for sector 8 from sum_buffer ($a0)
out $s1, $imm1, $zero, 15, 0                    # disksector = 8 
out $a0, $imm1, $zero, 16, 0                    # diskbuffer = $a0 
out $imm2, $imm1, $zero, 14, 2                  # diskcmd = 2 which is the write command

#Wait for disk write to complete before halting execution#
wait_write_done:
in $t0, $imm1, $zero, 17, 0                     # read diskstatus
bne $imm1, $t0, $zero, wait_write_done, 0       # wait until status == 0

#Clear the interrupt flag left by the disk#
out $zero, $imm1, $zero, 4, 0                   # irq1status = 0

halt $zero, $zero, $zero, 0, 0                  # finish execution
