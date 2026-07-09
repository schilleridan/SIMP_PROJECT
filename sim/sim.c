#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NREG          16           /* number of general purpose registers */
#define MEM_SIZE      4096         /* main memory depth (12-bit address)  */
#define PC_MASK       0xFFF        /* PC is 12 bits */
#define NIO           23           /* number of I/O registers */
#define DISK_SECTORS  128          /* number of disk sectors */
#define SECTOR_WORDS  128          /* words per disk sector  */
#define DISK_WORDS    (DISK_SECTORS * SECTOR_WORDS)
#define MON_SIZE      (256 * 256)  /* monitor size */
#define DISK_DELAY    1024         /* clock cycles for a disk command  */
#define MAX_LINE      600          /* max line length 500 */

/* I/O registers indexes */
enum {
    IO_IRQ0ENABLE = 0, IO_IRQ1ENABLE, IO_IRQ2ENABLE,
    IO_IRQ0STATUS, IO_IRQ1STATUS, IO_IRQ2STATUS,
    IO_IRQHANDLER, IO_IRQRETURN, IO_CLKS,
    IO_LEDS, IO_DISPLAY7SEG,
    IO_TIMERENABLE, IO_TIMERCURRENT, IO_TIMERMAX,
    IO_DISKCMD, IO_DISKSECTOR, IO_DISKBUFFER, IO_DISKSTATUS,
    IO_RESERVED18, IO_RESERVED19,
    IO_MONITORADDR, IO_MONITORDATA, IO_MONITORCMD
};

/* names of the hardware registers for hwregtrace.txt */
static const char* io_names[NIO] = {
    "irq0enable", "irq1enable", "irq2enable",
    "irq0status", "irq1status", "irq2status",
    "irqhandler", "irqreturn", "clks",
    "leds", "display7seg",
    "timerenable", "timercurrent", "timermax",
    "diskcmd", "disksector", "diskbuffer", "diskstatus",
    "reserved", "reserved",
    "monitoraddr", "monitordata", "monitorcmd"
};

/* Machine state variables */
static int           reg[NREG];           /* general purpose registers */
static unsigned int  mem[MEM_SIZE];        /* main memory  */
static unsigned int  io[NIO];              /* hardware registers */
static unsigned int  disk[DISK_WORDS];     /* disk content */
static unsigned char monitor[MON_SIZE];    /* monitor frame buffer */

static unsigned int  pc = 0;               /* program counter */
static unsigned int  cycle = 0;            /* clock cycle counter  */
static int           halted = 0;           /* if halt is set  */
static int           in_isr = 0;           /* set as 1 while inside an interrupt handler  */

/* irq2 line, list of cycle numbers where it rises */
static unsigned int* irq2_list = NULL;
static int           irq2_count = 0;
static int           irq2_idx = 0;       /* next entry */

/* DMA state */
static int           disk_busy = 0;        /* 1 while a command is currently in effect  */
static int           disk_remaining = 0;   /* cycles left until the command is done */

/* output files */
static FILE* f_trace, * f_hwtrace, * f_leds, * f_7seg;


/* Sign extension */
static int sign_extend12(unsigned int imm12)
{
    if (imm12 & 0x800)
        return (int)(imm12 | 0xFFFFF000u);
    return (int)imm12;
}

static int  cur_imm1;   /* sign-extended imm12 of the current instruction  */
static int  cur_imm2;   /* second word of the current instruction if there is  */

/* read the value of a register in an operation */
static int read_reg(int idx)
{
    if (idx == 0) return 0;
    if (idx == 1) return cur_imm1;
    if (idx == 2) return cur_imm2;
    return reg[idx];
}

/* write to a register, except to $zero/$imm1/$imm2 which are read only so they are ignored */
static void write_reg(int idx, int value)
{
    if (idx <= 2) return;
    reg[idx] = value;
}

/*I/O register access for in/out instructions */
/* logging a  hardware register access to hwregtrace.txt */
static void hwtrace(const char* rw, int idx, unsigned int data)
{
    if (idx < 0 || idx >= NIO) return;
    fprintf(f_hwtrace, "%08X %s %s %08X\n", cycle, rw, io_names[idx], data);
}

/* execute "in", R[rd] = IORegister[addr] */
static unsigned int io_read(int addr)
{
    unsigned int val;
    if (addr < 0 || addr >= NIO) return 0;
    if (addr == IO_MONITORCMD) val = 0;      /* reading monitorcmd always returns 0 */
    else                       val = io[addr];
    hwtrace("READ", addr, val);
    return val;
}

/* execute "out", IORegister[addr] = value */
static void io_write(int addr, unsigned int value)
{
    if (addr < 0 || addr >= NIO) return;

    hwtrace("WRITE", addr, value);

    /* LEDs, logging a line only if the value changes */
    if (addr == IO_LEDS && value != io[IO_LEDS]) {
        io[IO_LEDS] = value;
        fprintf(f_leds, "%08X %08X\n", cycle, value);
        return;
    }
    /* 7 segment display, same as LEDs */
    if (addr == IO_DISPLAY7SEG && value != io[IO_DISPLAY7SEG]) {
        io[IO_DISPLAY7SEG] = value;
        fprintf(f_7seg, "%08X %08X\n", cycle, value);
        return;
    }

    /* drawing a pixel in monitor when writing 1 to monitorcmd */
    if (addr == IO_MONITORCMD) {
        io[IO_MONITORCMD] = value;
        if (value == 1) {
            unsigned int a = io[IO_MONITORADDR] & 0xFFFF;
            if (a < MON_SIZE)
                monitor[a] = (unsigned char)(io[IO_MONITORDATA] & 0xFF);
        }
        return;
    }

    /* start a read/write command only if the disk is free */
    if (addr == IO_DISKCMD) {
        io[IO_DISKCMD] = value;
        if ((value == 1 || value == 2) && io[IO_DISKSTATUS] == 0) {
            io[IO_DISKSTATUS] = 1;          /* meaning the disk is busy */
            disk_busy = 1;
            disk_remaining = DISK_DELAY;
        }
        return;
    }

    /* if all the above dont happen then the default is to just store the value */
    io[addr] = value;
}

/* perform the DMA transfer for the active disk command */
static void disk_complete(void)
{
    int i;
    unsigned int sector = io[IO_DISKSECTOR] & 0x7F;        /* 7-bit sector */
    unsigned int buf = io[IO_DISKBUFFER] & PC_MASK;     /* 12-bit address */
    unsigned int cmd = io[IO_DISKCMD];

    if (cmd == 1) {                       /* read sector to memory buffer  */
        for (i = 0; i < SECTOR_WORDS; i++)
            mem[(buf + i) & (MEM_SIZE - 1)] = disk[sector * SECTOR_WORDS + i];
    }
    else if (cmd == 2) {                /* write memory buffer to sector */
        for (i = 0; i < SECTOR_WORDS; i++)
            disk[sector * SECTOR_WORDS + i] = mem[(buf + i) & (MEM_SIZE - 1)];
    }

    io[IO_DISKCMD] = 0;                 /* command dinished */
    io[IO_DISKSTATUS] = 0;                 /* the disk is free again */
    io[IO_IRQ1STATUS] = 1;                 /* signal the disk interrupt */
    disk_busy = 0;
}


static void device_tick(unsigned int dev_cycle)
{
    /* sample irq2 for this cycle */
    while (irq2_idx < irq2_count && irq2_list[irq2_idx] == dev_cycle) {
        io[IO_IRQ2STATUS] = 1;
        irq2_idx++;
    }

    /* Timer */
    if (io[IO_TIMERENABLE] == 1) {
        if (io[IO_TIMERCURRENT] == io[IO_TIMERMAX]) {
            io[IO_IRQ0STATUS] = 1;
            io[IO_TIMERCURRENT] = 0;
        }
        else {
            io[IO_TIMERCURRENT]++;
        }
    }

    /* counting down for the disk nad finishinf after the required amount of cycles has passed */
    if (disk_busy) {
        disk_remaining--;
        if (disk_remaining <= 0)
            disk_complete();
    }
}

/* trace */
static void write_trace(unsigned int exec_cycle, unsigned int trace_pc,
    unsigned int inst)
{
    int i;
    fprintf(f_trace, "%08X %03X %08X", exec_cycle, trace_pc, inst);
    /* R0 is 0, R1 is imm1; R2 is imm2 or 0 , R3 to R15 are the file. */
    fprintf(f_trace, " %08X", 0);
    fprintf(f_trace, " %08X", (unsigned int)cur_imm1);
    fprintf(f_trace, " %08X", (unsigned int)cur_imm2);
    for (i = 3; i < NREG; i++)
        fprintf(f_trace, " %08X", (unsigned int)reg[i]);
    fprintf(f_trace, "\n");
}

/* instruction execution, returns the next PC. n is the number of wordsor cycles in the instruction either 1 or 2 */
static unsigned int execute(unsigned int inst, unsigned int n)
{
    int opcode = (inst >> 24) & 0xFF;
    int rd = (inst >> 20) & 0xF;
    int rs = (inst >> 16) & 0xF;
    int rt = (inst >> 12) & 0xF;

    int vs = read_reg(rs);
    int vt = read_reg(rt);
    unsigned int next_pc = (pc + n) & PC_MASK;

    switch (opcode) {
    case 0:  write_reg(rd, vs + vt);                       break;  /* add */
    case 1:  write_reg(rd, vs - vt);                       break;  /* sub */
    case 2:  write_reg(rd, vs * vt);                       break;  /* mul */
    case 3:  write_reg(rd, vs * vt + reg[rd]);             break;  /* mac */
    case 4:  write_reg(rd, vs & vt);                       break;  /* and */
    case 5:  write_reg(rd, vs | vt);                       break;  /* or */
    case 6:  write_reg(rd, vs ^ vt);                       break;  /* xor */
    case 7:  write_reg(rd, (int)((unsigned int)vs << (vt & 31)));  break; /* sll */
    case 8:  write_reg(rd, vs >> (vt & 31));               break; /* sra */
    case 9:  write_reg(rd, (int)((unsigned int)vs >> (vt & 31)));  break; /* srl */
    case 10: if (vs == vt) next_pc = read_reg(rd) & PC_MASK; break; /* beq */
    case 11: if (vs != vt) next_pc = read_reg(rd) & PC_MASK; break; /* bne */
    case 12: if (vs < vt) next_pc = read_reg(rd) & PC_MASK; break; /* blt */
    case 13: if (vs > vt) next_pc = read_reg(rd) & PC_MASK; break; /* bgt */
    case 14: if (vs <= vt) next_pc = read_reg(rd) & PC_MASK; break; /* ble */
    case 15: if (vs >= vt) next_pc = read_reg(rd) & PC_MASK; break; /* bge */
    case 16: /* jal */
        write_reg(rd, (int)((pc + n) & PC_MASK));   /* save the return address  */
        next_pc = read_reg(rs) & PC_MASK;
        break;
    case 17: /* lw */
        write_reg(rd, (int)mem[(vs + vt) & (MEM_SIZE - 1)]);
        break;
    case 18: /* sw */
        mem[(vs + vt) & (MEM_SIZE - 1)] = (unsigned int)read_reg(rd);
        break;
    case 19: /* reti */
        next_pc = io[IO_IRQRETURN] & PC_MASK;
        in_isr = 0;
        break;
    case 20: /* in */
        write_reg(rd, (int)io_read(vs + vt));
        break;
    case 21: /* out */
        io_write(vs + vt, (unsigned int)read_reg(rd));
        break;
    case 22: /* halt */
        halted = 1;
        break;
    default:
        /* if the opcode is unkown then just fall through */
        break;
    }

    return next_pc;
}

/* loading input */
/* loading a file into a word array up to MAX words */
static void load_words(const char* path, unsigned int* arr, int max)
{
    FILE* f = fopen(path, "r");
    char line[MAX_LINE];
    int i = 0;
    if (f == NULL) return;                  /* incase the file is missing then return all 0 */
    while (i < max && fgets(line, sizeof(line), f) != NULL) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        arr[i++] = (unsigned int)strtoul(line, NULL, 16);
    }
    fclose(f);
}

/* loading irq2in.txt */
static void load_irq2(const char* path)
{
    FILE* f = fopen(path, "r");
    char line[MAX_LINE];
    int cap = 16;
    if (f == NULL) return;
    irq2_list = (unsigned int*)malloc(cap * sizeof(unsigned int));
    while (fgets(line, sizeof(line), f) != NULL) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '\r') continue;
        if (irq2_count == cap) {
            cap *= 2;
            irq2_list = (unsigned int*)realloc(irq2_list, cap * sizeof(unsigned int));
        }
        irq2_list[irq2_count++] = (unsigned int)strtoul(p, NULL, 10);
    }
    fclose(f);
}

/* writing the finla output files */
/* write a word array while tcutting trailing zero words */
static void write_mem_trimmed(const char* path, unsigned int* arr, int count)
{
    FILE* f = fopen(path, "w");
    int last = -1, i;
    if (f == NULL) return;
    for (i = 0; i < count; i++)
        if (arr[i] != 0) last = i;
    for (i = 0; i <= last; i++)
        fprintf(f, "%08X\n", arr[i]);
    fclose(f);
}

/* write the whole disk */
static void write_disk(const char* path)
{
    FILE* f = fopen(path, "w");
    int i;
    if (f == NULL) return;
    for (i = 0; i < DISK_WORDS; i++)
        fprintf(f, "%08X\n", disk[i]);
    fclose(f);
}

/* write registers R3-R15 */
static void write_regout(const char* path)
{
    FILE* f = fopen(path, "w");
    int i;
    if (f == NULL) return;
    for (i = 3; i < NREG; i++)
        fprintf(f, "%08X\n", (unsigned int)reg[i]);
    fclose(f);
}

/* writing monitor.txt one pixel per line while trailing zeros are trimmed */
static void write_monitor_txt(const char* path)
{
    FILE* f = fopen(path, "w");
    int last = -1, i;
    if (f == NULL) return;
    for (i = 0; i < MON_SIZE; i++)
        if (monitor[i] != 0) last = i;
    for (i = 0; i <= last; i++)
        fprintf(f, "%02X\n", monitor[i]);
    fclose(f);
}

/* writing monitor.yuv */
static void write_monitor_yuv(const char* path)
{
    FILE* f = fopen(path, "wb");
    if (f == NULL) return;
    fwrite(monitor, 1, MON_SIZE, f);
    fclose(f);
}

/* Main */
int main(int argc, char* argv[])
{
    if (argc != 14) {
        printf("Usage: sim.exe memin.txt diskin.txt irq2in.txt memout.txt "
            "regout.txt trace.txt hwregtrace.txt cycles.txt leds.txt "
            "display7seg.txt diskout.txt monitor.txt monitor.yuv\n");
        return 1;
    }

    /* loading the inputs */
    load_words(argv[1], mem, MEM_SIZE);     /* memin.txt */
    load_words(argv[2], disk, DISK_WORDS);   /* diskin.txt */
    load_irq2(argv[3]);                      /* irq2in.txt */

    /* open the output files for writing */
    f_trace = fopen(argv[6], "w");        /* trace.txt */
    f_hwtrace = fopen(argv[7], "w");        /* hwregtrace.txt */
    f_leds = fopen(argv[9], "w");        /* leds.txt */
    f_7seg = fopen(argv[10], "w");        /* display7seg.txt */
    if (!f_trace || !f_hwtrace || !f_leds || !f_7seg) {
        printf("Error: cannot open output files\n");
        return 1;
    }

    /*  fetch -> decode -> execute loop */
    while (!halted) {
        unsigned int inst, second, exec_cycle, irq;
        int rd, rs, rt, n;
        unsigned int k;

        /* interrupt check by using the registered status from the prev cycle */
        irq = (io[IO_IRQ0ENABLE] & io[IO_IRQ0STATUS]) |
            (io[IO_IRQ1ENABLE] & io[IO_IRQ1STATUS]) |
            (io[IO_IRQ2ENABLE] & io[IO_IRQ2STATUS]);
        if (irq && !in_isr) {
            io[IO_IRQRETURN] = pc;               /* remember where we were  */
            pc = io[IO_IRQHANDLER] & PC_MASK;    /* jumping to the handler  */
            in_isr = 1;
        }

        /* fetching and decoding the first word */
        inst = mem[pc & (MEM_SIZE - 1)];
        rd = (inst >> 20) & 0xF;
        rs = (inst >> 16) & 0xF;
        rt = (inst >> 12) & 0xF;

        /* an instruction uses a second word iff one of its operand fields has R2. meaning these instructions will take 2 cycles */
        n = (rd == 2 || rs == 2 || rt == 2) ? 2 : 1;
        second = (n == 2) ? mem[(pc + 1) & (MEM_SIZE - 1)] : 0;

        cur_imm1 = sign_extend12(inst & 0xFFF);
        cur_imm2 = (n == 2) ? (int)second : 0;

        /* the cycle is the second cycle of the instruction */
        exec_cycle = cycle + n - 1;
        io[IO_CLKS] = exec_cycle;

        /* trace this instruction */
        write_trace(exec_cycle, pc, inst);

        /* The hwregtrace,leds, 7seg  use the same cycle number */
        cycle = exec_cycle;            /* set cycle so logging uses it  */

        /* execute  */
        {
            unsigned int next_pc = execute(inst, n);
            pc = next_pc;
        }

        /* advance the devices for every cycle this instruction occupied */
        cycle = exec_cycle - (n - 1);  /* restore to the first occupied cycle */
        for (k = 0; k < (unsigned int)n; k++)
            device_tick(cycle + k);
        cycle = exec_cycle + 1;        /* next free cycle  */
    }

    /* closing the files */
    fclose(f_trace);
    fclose(f_hwtrace);
    fclose(f_leds);
    fclose(f_7seg);

    /* writing the final state output files*/
    write_mem_trimmed(argv[4], mem, MEM_SIZE);   /* memout.txt  */
    write_regout(argv[5]);                       /* regout.txt  */
    {
        FILE* f = fopen(argv[8], "w");           /* cycles.txt */
        if (f) { fprintf(f, "%08X\n", cycle); fclose(f); }
    }
    write_disk(argv[11]);                         /* diskout.txt */
    write_monitor_txt(argv[12]);                  /* monitor.txt */
    write_monitor_yuv(argv[13]);                  /* monitor.yuv */

    free(irq2_list);
    return 0;
}
