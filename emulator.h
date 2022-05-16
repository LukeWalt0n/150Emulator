#ifndef __EMULATOR_H__

#define __EMULATOR_H__ 1

#define MAX_PROG_LEN 250	//maximum number of lines a program can have
#define MAX_LINE_LEN 50		//maximum number of characters a line of code may have
#define MAX_OPCODE   10		//number of opcodes supported (length of opcode_str and opcode_func)
#define MAX_REGISTER 32		//number of registers (size of register_str)
#define MAX_ARG_LEN  20		//used when tokenizing a program line, max size of a token

extern const char *register_str[MAX_REGISTER];

extern char prog[MAX_PROG_LEN][MAX_LINE_LEN];
extern int prog_len;

/* Elements for running the emulator */
extern unsigned int registers[MAX_REGISTER];
extern unsigned int pc;
extern unsigned int text[MAX_PROG_LEN];


int load_program();
int make_bytecode();
int exec_bytecode();


#endif // EMULATOR_H
