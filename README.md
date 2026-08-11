# SIMP Processor - Assembler & Simulator

A C-based software implementation of the **SIMP (Simple RISC)** processor architecture, developed for the *Computer Organization* course at Tel Aviv University.

The project includes a two-pass assembler, a cycle-accurate simulator, a simulated I/O subsystem with interrupts and DMA, and sample assembly test programs.

---

## Architecture Overview

- **Word Size:** 32-bit data and instruction words.
- **Address Space:** 12-bit PC addressing up to 4096 memory entries.
- **Registers:** 16 32-bit architectural registers, including `$zero` (constant zero), `$imm1` and `$imm2` (sign-extended immediate value registers), `$sp` (stack pointer), and `$ra` (return address).
- **Instruction Set:** 23 opcodes supporting arithmetic (`add`, `sub`, `mul`, `mac`), bitwise logic (`and`, `or`, `xor`), shifts (`sll`, `sra`, `srl`), control flow (`beq`, `bne`, `blt`, `bgt`, `ble`, `bge`, `jal`), memory access (`lw`, `sw`), I/O operations (`in`, `out`, `reti`), and `halt`.

---

## Components

### 1. Assembler (`asm`)
- **Type:** Two-pass assembler written in C.
- **Functionality:** Reads SIMP assembly source files (`.asm`) and generates a machine memory image file (`memin.txt`).
- **Features:** Supports labels, positive/negative decimal and hexadecimal constants, single and dual immediates, comments, and pseudo-instructions (`.word`, `.array`).

### 2. Simulator (`sim`)
- **Type:** Cycle-accurate simulator written in C.
- **Functionality:** Executes SIMP machine code instruction-by-instruction from memory image (`memin.txt`).
- **Trace Output:** Produces execution logs (`trace.txt`, `hwregtrace.txt`), cycle count (`cycles.txt`), register dumps (`regout.txt`), and final memory state (`memout.txt`).

### 3. Integrated Peripherals & I/O
- **Interrupt Controller:** Supports 3 hardware interrupts (IRQ0: Timer, IRQ1: Hard Disk DMA, IRQ2: External Signal).
- **Programmable Timer:** 32-bit counter with configurable target max count (`timermax`).
- **DMA Hard Disk Controller:** Asynchronous sector transfer (128 sectors × 128 words) using Direct Memory Access.
- **Graphics Display:** 256×256 monochrome frame buffer monitor. Outputs frame data to `monitor.txt` and raw binary `monitor.yuv` (compatible with YUVPlayer).
- **Status Displays:** 32 output LEDs (`leds.txt`) and an 8-digit 7-segment display (`display7seg.txt`).

---

## Assembly Test Programs

1. **`factorial.asm`**: Recursive factorial algorithm demonstrating function calls and stack pointer management.
2. **`sort.asm`**: Descending bubble sort algorithm operating on 16 memory elements.
3. **`circle.asm`**: Frame-buffer graphics program that calculates and draws a filled white circle to the monitor.
4. **`disktest.asm`**: Asynchronous DMA disk test summing contents across sectors 0–7 and writing the results to sector 8.

---

## Build & Run Instructions

### Compilation
Using `gcc` (or Microsoft Visual Studio):

bash
# Compile Assembler
gcc -o asm assembler.c

# Compile Simulator
gcc -o sim simulator.c

# 1. Assemble assembly program
./asm program.asm memin.txt

# 2. Run simulator
./sim memin.txt diskin.txt irq2in.txt memout.txt regout.txt trace.txt hwregtrace.txt cycles.txt leds.txt display7seg.txt diskout.txt monitor.txt monitor.yuv

Author
Idan Schiller, Ido Ben David, Nir Arazi
