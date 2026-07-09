#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE 500
#define MAX_FIELD_LEN 51
#define MEM_SIZE 4096
#define MAX_LABELS 500


/* ============================================
Structs and globals
============================================== */

typedef struct {
    char name[MAX_FIELD_LEN];
    int address;
} Label;

/* ============================================
 Utility functions
============================================== */

/* Removes everything after the first '#' character in a line.
 * Used to ignore comments in the assembly input file. */

void remove_comment(char line[])
{
    char* comment_start;

    comment_start = strchr(line, '#');

    if (comment_start != NULL) {
        *comment_start = '\0';
    }
}

/* Removes whitespaces from a string */

void trim(char str[])
{
    int start = 0;
    int end;
    int i = 0;

    while (str[start] != '\0' &&
        (str[start] == ' ' || str[start] == '\t' || str[start] == '\n' || str[start] == '\r')) {
        start++;
    }

    end = (int)strlen(str) - 1;

    while (end >= start &&
        (str[end] == ' ' || str[end] == '\t' || str[end] == '\n' || str[end] == '\r')) {
        end--;
    }

    while (start <= end) {
        str[i] = str[start];
        i++;
        start++;
    }

    str[i] = '\0';
}
/* Parses a numeric string into a 32-bit unsigned value.
   Supports decimal and hex numbers. */

int parse_number(const char* str, unsigned int* value)
{
    char* endptr;
    unsigned long unsigned_result;
    long signed_result;

    if (str[0] == '-') {
        signed_result = strtol(str, &endptr, 0);

        if (endptr == str) {
            return 0;
        }

        if (*endptr != '\0') {
            return 0;
        }

        *value = (unsigned int)signed_result;
        return 1;
    }

    unsigned_result = strtoul(str, &endptr, 0);

    if (endptr == str) {
        return 0;
    }

    if (*endptr != '\0') {
        return 0;
    }

    *value = (unsigned int)unsigned_result;
    return 1;
}

/* ============================================
 Parser function
 ============================================== */

/* Parses an instruction line into its six fields: 
   opcode, rd, rs, rt, imm1, imm2 
   returns 1 if successful, 0 if unsuccessful */

int parse_instruction_line(char line[],
    char opcode[],
    char rd[],
    char rs[],
    char rt[],
    char imm1[],
    char imm2[])
{
    char* parts[5];
    char* token;
    int count = 0;

    token = strtok(line, ",");

    while (token != NULL && count < 5) {
        trim(token);
        parts[count] = token;
        count++;

        token = strtok(NULL, ",");
    }

    if (count != 5) {
        return 0;
    }

    if (sscanf(parts[0], "%s %s", opcode, rd) != 2) {
        return 0;
    }

    strcpy(rs, parts[1]);
    strcpy(rt, parts[2]);
    strcpy(imm1, parts[3]);
    strcpy(imm2, parts[4]);

    return 1;
}

 /* ============================================
 Opcode/register conversion
 ============================================== */
#define NUM_REGISTERS 16
#define NUM_OPCODES 23

const char* registers[NUM_REGISTERS] = {
    "$zero", "$imm1", "$imm2", "$v0", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$s0", "$s1", "$gp", "$sp", "$ra"
};

const char* opcodes[NUM_OPCODES] = {
    "add", "sub", "mul", "mac", "and", "or", "xor", "sll",
    "sra", "srl", "beq", "bne", "blt", "bgt", "ble", "bge",
    "jal", "lw", "sw", "reti", "in", "out", "halt"
};

/* Converts a string containing the register's name into its index */

int get_register_number(const char* reg_name)
{
    int i;

    for (i = 0; i < NUM_REGISTERS; i++) {
        if (strcmp(reg_name, registers[i]) == 0) {
            return i;
        }
    }

    return -1;
}

/* Converts opcode string to its value */

int get_opcode_number(const char* op_name)
{
    int i;

    for (i = 0; i < NUM_OPCODES; i++) {
        if (strcmp(op_name, opcodes[i]) == 0) {
            return i;
        }
    }

    return -1;
}
 /* ============================================
 Labels / Symbols
 ============================================== */

/* Checks if a lines contains a label definition. Identified by the char ':'. */

int is_label_definition(char line[])
{
    char* colon;

    colon = strchr(line, ':');

    if (colon != NULL) {
        return 1;
    }

    return 0;
}

/* Extracts the label name from a line containing a label that ends with ':',
   without ':'. */

void extract_label_name(char line[], char label_name[])
{
    char* colon;
    int len;

    colon = strchr(line, ':');

    if (colon == NULL) {
        label_name[0] = '\0';
        return;
    }

    len = (int)(colon - line);

    strncpy(label_name, line, len);
    label_name[len] = '\0';

    trim(label_name);
}

/* Removes a label from a line, keeping only the instruction. */

void remove_label_from_line(char line[])
{
    char* colon;
    char temp[MAX_LINE + 1];

    colon = strchr(line, ':');

    if (colon == NULL) {
        return;
    }

    strcpy(temp, colon + 1);
    trim(temp);
    strcpy(line, temp);
}

/* Searches for a label by name in the labels table, returns index if found and -1 if not found */

int find_label(Label labels[], int label_count, const char* name)
{
    int i;

    for (i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

/* Adds a new label and its address to the labels table.
   Fails if the table is full or the label already exists. 
   Returns 1 on success, 0 on failure */

int add_label(Label labels[], int* label_count, const char* name, int address)
{
    if (*label_count >= MAX_LABELS) {
        return 0;
    }

    if (find_label(labels, *label_count, name) != -1) {
        return 0;
    }

    strcpy(labels[*label_count].name, name);
    labels[*label_count].address = address;
    (*label_count)++;

    return 1;
}

/* Resolves an immediate into a numeric value 
   Immediate could be a number or a label.
   If it is a label, the label address is used */

int resolve_immediate(const char* imm, Label labels[], int label_count, unsigned int* value)
{
    int label_index;

    if (parse_number(imm, value)) {
        return 1;
    }

    label_index = find_label(labels, label_count, imm);

    if (label_index != -1) {
        *value = (unsigned int)labels[label_index].address;
        return 1;
    }

    return 0;
}

/* Performs the first pass over the .asm file. 
   Collects all label definitions and stores their addresses
   a local address is advanced according to the number of words each instruction occupies. */

int first_pass(FILE* input, Label labels[], int* label_count)
{
    char line[MAX_LINE + 1];
    char label_name[MAX_FIELD_LEN];

    char opcode[MAX_FIELD_LEN];
    char rd[MAX_FIELD_LEN];
    char rs[MAX_FIELD_LEN];
    char rt[MAX_FIELD_LEN];
    char imm1[MAX_FIELD_LEN];
    char imm2[MAX_FIELD_LEN];

    int pc;
    int rd_num;
    int rs_num;
    int rt_num;

    pc = 0;

    while (fgets(line, MAX_LINE + 1, input) != NULL) {
        remove_comment(line);
        trim(line);

        if (line[0] == '\0') {
            continue;
        }

        if (is_label_definition(line)) {
            extract_label_name(line, label_name);

            if (label_name[0] != '\0') {
                if (!add_label(labels, label_count, label_name, pc)) {
                    printf("Error: duplicate label or too many labels: %s\n", label_name);
                    return 0;
                }
            }

            remove_label_from_line(line);

            if (line[0] == '\0') {
                continue;
            }
        }

        /* Directives will be handled later */
        if (line[0] == '.') {
            continue;
        }

        if (!parse_instruction_line(line, opcode, rd, rs, rt, imm1, imm2)) {
            printf("Pass 1 parse error: %s\n", line);
            return 0;
        }

        rd_num = get_register_number(rd);
        rs_num = get_register_number(rs);
        rt_num = get_register_number(rt);

        if (rd_num == -1 || rs_num == -1 || rt_num == -1) {
            printf("Pass 1 register error\n");
            return 0;
        }

        pc++;

        if (rd_num == 2 || rs_num == 2 || rt_num == 2) {
            pc++;
        }
    }

    return 1;
}

 /* ============================================
 .word/.array 
 ============================================== */

/* Handles .word 
    writes the data directly to memory[address] 
    updates max_address if needed. */

int handle_word(char line[], unsigned int memory[], int* max_address)
{
    char directive[MAX_FIELD_LEN];
    char address_str[MAX_FIELD_LEN];
    char data_str[MAX_FIELD_LEN];

    unsigned int address;
    unsigned int data;

    if (sscanf(line, "%s %s %s", directive, address_str, data_str) != 3) {
        return 0;
    }

    if (strcmp(directive, ".word") != 0) {
        return 0;
    }

    if (!parse_number(address_str, &address)) {
        return 0;
    }

    if (!parse_number(data_str, &data)) {
        return 0;
    }

    if (address >= MEM_SIZE) {
        return 0;
    }

    memory[(int)address] = (unsigned int)data;

    if ((int)address + 1 > *max_address) {
        *max_address = (int)address + 1;
    }

    return 1;
}

/* Handles .array. writes the data to consecutive memory addresses starting at the given address */

int handle_array(char line[], unsigned int memory[], int* max_address)
{
    char directive[MAX_FIELD_LEN];
    char address_str[MAX_FIELD_LEN];
    char data_part[MAX_LINE + 1];

    char* token;
    unsigned int address;
    unsigned int data;
    int current_address;

    if (sscanf(line, "%s %s %[^\n]", directive, address_str, data_part) != 3) {
        return 0;
    }

    if (strcmp(directive, ".array") != 0) {
        return 0;
    }

    if (!parse_number(address_str, &address)) {
        return 0;
    }

    if (address < 0 || address >= MEM_SIZE) {
        return 0;
    }

    current_address = (int)address;

    token = strtok(data_part, ",");

    if (token == NULL) {
        return 0;
    }

    while (token != NULL) {
        trim(token);

        if (token[0] == '\0') {
            return 0;
        }

        if (!parse_number(token, &data)) {
            return 0;
        }

        if (current_address < 0 || current_address >= MEM_SIZE) {
            return 0;
        }

        memory[current_address] = (unsigned int)data;
        current_address++;

        token = strtok(NULL, ",");
    }

    if (current_address > *max_address) {
        *max_address = current_address;
    }

    return 1;
}

 /* ============================================
 encoding functions 
 ============================================== */

 /*
  * Encodes an instruction into a 32-bit machine word.
  * The instruction format is:
  * bits 31-24: opcode
  * bits 23-20: rd
  * bits 19-16: rs
  * bits 15-12: rt
  * bits 11-0 : imm1
  * Only the lower 12 bits of imm1 are encoded.
  */

unsigned int encode_instruction(int opcode, int rd, int rs, int rt, unsigned int imm1)
{
    unsigned int inst = 0;

    inst |= ((unsigned int)opcode & 0xFF) << 24;
    inst |= ((unsigned int)rd & 0xF) << 20;
    inst |= ((unsigned int)rs & 0xF) << 16;
    inst |= ((unsigned int)rt & 0xF) << 12;
    inst |= ((unsigned int)imm1 & 0xFFF);

    return inst;
}

 /* ============================================
 Main
 ============================================== */

 /*
  * Main assembler flow:
  * 1. Open input .asm file and output memin.txt file.
  * 2. First pass: collect labels and their addresses.
  * 3. Second pass: parse and encode instructions/directives into memory.
  * 4. Write the memory image to memin.txt.
  */

int main(int argc, char* argv[])
{
    FILE* input;
    FILE* output;
    char line[MAX_LINE + 1];

    unsigned int memory[MEM_SIZE] = { 0 };
    int pc = 0;
    int i;
    int max_address = 0;

    Label labels[MAX_LABELS];
    int label_count = 0;

    char opcode[MAX_FIELD_LEN] = "";
    char rd[MAX_FIELD_LEN] = "";
    char rs[MAX_FIELD_LEN] = "";
    char rt[MAX_FIELD_LEN] = "";
    char imm1[MAX_FIELD_LEN] = "";
    char imm2[MAX_FIELD_LEN] = "";

    int opcode_num;
    int rd_num;
    int rs_num;
    int rt_num;

    unsigned int imm1_num;
    unsigned int imm2_num;

    unsigned int encoded_instruction;

    if (argc != 3) {
        printf("Usage: asm.exe program.asm memin.txt\n");
        return 1;
    }

    input = fopen(argv[1], "r");
    if (input == NULL) {
        printf("Error: cannot open input file\n");
        return 1;
    }

    output = fopen(argv[2], "w");
    if (output == NULL) {
        printf("Error: cannot open output file\n");
        fclose(input);
        return 1;
    }

    if (!first_pass(input, labels, &label_count)) {
        fclose(input);
        fclose(output);
        return 1;
    }

    rewind(input);
    pc = 0;

    while (fgets(line, MAX_LINE + 1, input) != NULL){
        remove_comment(line);
        trim(line);

        if (line[0] == '\0') {
            continue;
        }
        if (is_label_definition(line)) {
            remove_label_from_line(line);

            if (line[0] == '\0') {
                continue;
            }
        }

        if (strncmp(line, ".word", 5) == 0) {
            if (!handle_word(line, memory, &max_address)) {
                printf("Invalid .word directive: %s\n", line);
                continue;
            }

            continue;
        }

        if (strncmp(line, ".array", 6) == 0) {
            if (!handle_array(line, memory, &max_address)) {
                printf("Invalid .array directive: %s\n", line);
                continue;
            }

            continue;
        }

        if (parse_instruction_line(line, opcode, rd, rs, rt, imm1, imm2)) {
            opcode_num = get_opcode_number(opcode);
            rd_num = get_register_number(rd);
            rs_num = get_register_number(rs);
            rt_num = get_register_number(rt);

            if (!resolve_immediate(imm1, labels, label_count, &imm1_num)) {
                printf("Invalid imm1 or unknown label: %s\n", imm1);
                continue;
            }

            if (!resolve_immediate(imm2, labels, label_count, &imm2_num)) {
                printf("Invalid imm2 or unknown label: %s\n", imm2);
                continue;
            }

            encoded_instruction = encode_instruction(opcode_num, rd_num, rs_num, rt_num, imm1_num);

            memory[pc] = encoded_instruction;
            pc++;

            if (pc > max_address) {
                max_address = pc;
            }

            if (rd_num == 2 || rs_num == 2 || rt_num == 2) {
                memory[pc] = (unsigned int)imm2_num;
                pc++;

                if (pc > max_address) {
                    max_address = pc;
                }
            }
        }
        else {
            printf("Could not parse line: %s\n", line);
        }

    }
   
    for (i = 0; i < max_address; i++) {
        fprintf(output, "%08X\n", memory[i]);
    }
    fclose(input);
    fclose(output);

    return 0;
}