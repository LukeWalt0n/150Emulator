addi $a0 $zero 1
addi $t0 $zero 10
loop: sll $a0 $a0 1
addi $t0 $t0 -1
blez $t0 loop
