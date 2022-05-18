#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "emulator.h"

#define XSTR(x) STR(x)		//can be used for MAX_ARG_LEN in sscanf
#define STR(x) #x

#define ADDR_TEXT    0x00400000 //where the .text area starts in which the program lives
#define TEXT_POS(a)  ((a==ADDR_TEXT)?(0):(a - ADDR_TEXT)/4) //can be used to access text[]
#define ADDR_POS(j)  (j*4 + ADDR_TEXT)                      //convert text index to address


void getRtypeInfo(int i);
void addi(int i);
void add(int i);
void andi(int i);
void sll(int i);
int blez(int i);
int bne(int i);
void getItypeInfo(int i);

const char *register_str[] = {"$zero",
                              "$at", "$v0", "$v1",
                              "$a0", "$a1", "$a2", "$a3",
                              "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
                              "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
                              "$t8", "$t9",
                              "$k0", "$k1",
                              "$gp",
                              "$sp", "$fp", "$ra"};

/* Space for the assembler program */
char prog[MAX_PROG_LEN][MAX_LINE_LEN];
int prog_len = 0;

/* Elements for running the emulator */
unsigned int registers[MAX_REGISTER] = {0}; // the registers
unsigned int pc = 0;                        // the program counter

//Bytecodes stored in here.
unsigned int text[MAX_PROG_LEN] = {0}; // the text memory with our instructions

/* function to create bytecode for instruction nop
   conversion result is passed in bytecode
   function always returns 0 (conversion OK) */
typedef int (*opcode_function)(unsigned int, unsigned int*, char*, char*, char*, char*);

//Adds the given immediate value to the bytecode in terms of hex.
int add_imi(unsigned int *bytecode, int imi){
	if (imi<-32768 || imi>32767) return (-1);
	*bytecode|= (0xFFFF & imi);
	return(0);
}

int add_sht(unsigned int *bytecode, int sht){
	if (sht<0 || sht>31) return(-1);
	*bytecode|= (0x1F & sht) << 6;\
	return(0);
}

//Adds the index of the desired register to the bytecode.
int add_reg(unsigned int *bytecode, char *reg, int pos){
	int i;
	for(i=0;i<MAX_REGISTER;i++){
		if(!strcmp(reg,register_str[i])){
    

		*bytecode |= (i << pos);
    
    
			return(0);
		}
	}
	return(-1);
}


int add_addr(unsigned int *bytecode, int addr){
    *bytecode |= ((addr>>2) & 0x3FFFFF);
    return 0;
}

int add_lbl(unsigned int offset, unsigned int *bytecode, char *label){
	char l[MAX_ARG_LEN+1];
	int j=0;
  //Runs back through the program, to find where the loop is.
	while(j<prog_len){
		memset(l,0,MAX_ARG_LEN+1);
		sscanf(&prog[j][0],"%" XSTR(MAX_ARG_LEN) "[^:]:", l);
		if (label!=NULL && !strcmp(l, label)) return(add_imi( bytecode, j-(offset+1)) );
		j++;
	}
	return (-1);
}

//
int add_text_addr(unsigned int *bytecode, char *label){
	char l[MAX_ARG_LEN+1];
	int j=0;
	while(j<prog_len){
		memset(l,0,MAX_ARG_LEN+1);
		sscanf(&prog[j][0],"%" XSTR(MAX_ARG_LEN) "[^:]:", l);
		if (label!=NULL && !strcmp(l, label)) return(add_addr( bytecode, ADDR_POS(j)));
		j++;
	}
	return (-1);
}

int opcode_nop(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0;
	return (0);
}

int opcode_add(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x20; 				// op,shamt,funct
	if (add_reg(bytecode,arg1,11)<0) return (-1); 	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source1 register
	if (add_reg(bytecode,arg3,16)<0) return (-1);	// source2 register
	return (0);
}

int opcode_addi(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x20000000; 				// op
	if (add_reg(bytecode,arg1,16)<0) return (-1);	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source1 register
	if (add_imi(bytecode,atoi(arg3))) return (-1);	// constant
	return (0);
}

int opcode_andi(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x30000000; 				// op
	if (add_reg(bytecode,arg1,16)<0) return (-1); 	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source1 register
	if (add_imi(bytecode,atoi(arg3))) return (-1);	// constant
	return (0);
}

int opcode_blez(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x18000000; 				// op
	if (add_reg(bytecode,arg1,21)<0) return (-1);	// register1
	if (add_lbl(offset,bytecode,arg2)) return (-1); // jump
	return (0);
}

int opcode_bne(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x14000000; 				// op
	if (add_reg(bytecode,arg1,21)<0) return (-1); 	// register1
	if (add_reg(bytecode,arg2,16)<0) return (-1);	// register2
	if (add_lbl(offset,bytecode,arg3)) return (-1); // jump
	return (0);
}

int opcode_srl(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x2; 					// op
	if (add_reg(bytecode,arg1,11)<0) return (-1);   // destination register
	if (add_reg(bytecode,arg2,16)<0) return (-1);   // source1 register
	if (add_sht(bytecode,atoi(arg3))<0) return (-1);// shift
	return(0);
}

int opcode_sll(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0; 					// op
	if (add_reg(bytecode,arg1,11)<0) return (-1);	// destination register
	if (add_reg(bytecode,arg2,16)<0) return (-1); 	// source1 register
	if (add_sht(bytecode,atoi(arg3))<0) return (-1);// shift
	return(0);
}

int opcode_jr(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x8; 					// op
	if (add_reg(bytecode,arg1,21)<0) return (-1);	// source register
	return(0);
}

int opcode_jal(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x0C000000; 					// op
	if (add_text_addr(bytecode, arg1)<0) return (-1);// find and add address
	return(0);
}

const char *opcode_str[] = {"nop", "add", "addi", "andi", "blez", "bne", "srl", "sll", "jal", "jr"};
opcode_function opcode_func[] = {&opcode_nop, &opcode_add, &opcode_addi, &opcode_andi, &opcode_blez, &opcode_bne, &opcode_srl, &opcode_sll, &opcode_jal, &opcode_jr};

/* a function to print the state of the machine */
int print_registers() {
  int i;
  printf("registers:\n");
  for (i = 0; i < MAX_REGISTER; i++) {
    printf(" %d: %d\n", i, registers[i]);
  }
  printf(" Program Counter: 0x%08x\n", pc);
  return (0);
}


unsigned int func;
unsigned int src1, src2, dest, shamt, sum, jd;
signed int con;



/* function to execute bytecode */
int exec_bytecode() {

  printf("EXECUTING PROGRAM ...\n");
  pc = ADDR_TEXT; // set program counter to the start of our program
  //Bytecodes are stored in text[].
  unsigned int o;

  
    

  
  for(int i = TEXT_POS(pc); i < prog_len;){

    //Get the opcode.
    o = (text[i] >> 24) & 0xff;
    
    //printf("%d: %s --> OPCODE: 0x%02x\n", i, &prog[i][0], o);
    
    


    //If the opcode is 0x00, it is R type
    if(o == 0x00){

      getRtypeInfo(i);
    
      if(func == 0x0){
        //sll
        //Have to handle bytecodes differently for each function other than just 
        shamt = ((text[i] >> 8) & 0xff) /32;
        dest = ((text[i] >> 12) & 0xf) * 2;
        src2 = (text[i] >> 16) & 0xf;
        src1 = (text[i] >> 20) & 0xf;

        registers[dest] = registers[src2] << shamt;
        //printf("The R-type function is sll\n");
        //printf("Destination register: %s  value: 0x%04x       Src1 Register: %s  value: 0x%04x           SHAMT: %d\n", 
         //register_str[dest], registers[dest], register_str[src2], registers[src2], shamt);
        i++;
        
        
      }
      if(func == 0x2){
        printf("The R-type function is srl\n");
        i++;
        
        
      }
      if(func == 0x8){
        printf("The R-type function is jr\n");
        i++;
        
      }
      else{
        //If it cannot find the func for any where the last digit is the func, then it may be add so check.
        //Get the two last digits e.g., 0x000000020.
        func = text[i] & 0xff;
        
        if(func == 0x20){
          
          registers[dest] = registers[src1] + registers[src2];
          printf("Destination Register: %s\n", register_str[dest]);
          printf("Source 1 Register: %s\n", register_str[src1]);
          printf("Source 2 Register: %s\n", register_str[src2]);
          printf("0x%08x\n", registers[dest]);
          print_registers();
          i++;
        }
      }
    


    }
    else if(o == 0x03){
      //Jump and link option
    }
    else{
      //I-type instructions:
      // addi , andi , blez , bne , 
      getItypeInfo(i);
      switch(o){
          case 0x20:
            addi(i);
            i++;
            break;

          case 0x30:
            andi(i);
            i++;
            break;

          case 0x19:
            i = blez(i);
            //printf("I is currently : %d\n", i);
            break;

          case 0x14:
            //bne
            src1 = ((text[i] >> 20) & 0xf)/2;
            src2 = (text[i] >> 16) & 0xf;
            i = bne(i);
            break;

          case 0x21:
          //Special case for negative numbers??
            dest = (text[i] >> 16) & 0xf;
            con = text[i] & 0xffff;
            con = -(65536 - con);
            addi(i);
            i++;
            break;
          


      }
      if(i == -1){
          print_registers();

          break;
      }
      
    }
  }

  return (0);
}


unsigned createMask(unsigned a, unsigned b)
{
   unsigned r = 0;
   for (unsigned i=a; i<=b; i++)
       r |= 1 << i;

   return r;
}

void getRtypeInfo(int i){
  printf("Get R type information\n");
  func = createMask(0, 6) & text[i];
  printf("FUNC: 0x%08x\n", func);

  shamt = createMask(7, 11) & text[i];
  printf("SHAMT: 0x%08x\n", shamt);

  src2 = createMask(12, 16) & text[i];
  printf("SRC2: 0x%08x\n", src2);

  src1 = createMask(17, 21) & text[i];
  printf("SRC1: 0x%08x\n", src1);

  dest = createMask(22, 26) & text[i];
  printf("DEST: 0x%08x\n", dest);
}

void getItypeInfo(int i){
  printf("Get I Type information\n");
  con = text[i] & 0xffff;
  dest = (text[i] >> 16) & 0x001f;   //rt
  src1 = (text[i] >> 21 ) & 0x03e;   //rs
  //printf("%s")
}

void getJtypeInfo(int i){

}

void andi(int i){
    src1 = ((text[i] >> 20) & 0xf)/2;
    dest = (text[i] >> 16 & 0xf);
    con = text[i] & 0xff;
    //printf("Dest: %d, src1: %d, con: %d\n", dest, src1, con);
    registers[dest] = (registers[src1]&con);

}

void addi(int i){
  registers[dest] = registers[src1] + con;
}

int blez(int i){
 
  con = text[i] & 0xffff;
  unsigned int currentPos = ADDR_POS(i);
  
  if(registers[src1] <= 0){
    return -1;
  }
  else{
    //printf("RETURN ADDRESS: 0x%08x\n", _POS(currentPos + ((-65536+con))));

    return TEXT_POS(currentPos + ((-65536+con)))-1;
  }
  
}

int bne(int i){
    con = text[i] & 0xffff;
    unsigned int currentPos = ADDR_POS(i);

    //If the two values are equal, then return -1.
    if(registers[src1] == registers[src2]){
      return -1;
    }
    else{
      return TEXT_POS(currentPos + ((-65536+con)));
    }
}






/* function to create bytecode */
int make_bytecode() {
  unsigned int
      bytecode; // holds the bytecode for each converted program instruction
  int i, j = 0;    // instruction counter (equivalent to program line)

  char label[MAX_ARG_LEN + 1];
  char opcode[MAX_ARG_LEN + 1];
  char arg1[MAX_ARG_LEN + 1];
  char arg2[MAX_ARG_LEN + 1];
  char arg3[MAX_ARG_LEN + 1];

  printf("ASSEMBLING PROGRAM ...\n");
  while (j < prog_len) {
    memset(label, 0, sizeof(label));
    memset(opcode, 0, sizeof(opcode));
    memset(arg1, 0, sizeof(arg1));
    memset(arg2, 0, sizeof(arg2));
    memset(arg3, 0, sizeof(arg3));

    bytecode = 0;

    if (strchr(&prog[j][0], ':')) { // check if the line contains a label
      
      if (sscanf(
              &prog[j][0],
              "%" XSTR(MAX_ARG_LEN) "[^:]: %" XSTR(MAX_ARG_LEN) "s %" XSTR(
                  MAX_ARG_LEN) "s %" XSTR(MAX_ARG_LEN) "s %" XSTR(MAX_ARG_LEN) "s",
              label, opcode, arg1, arg2,
              arg3) < 2) { // parse the line with label
        printf("parse error line %d\n", j);
        return (-1);
      }
    } else {
      //Read the program line by line, parsing each section by the max arg length.
      if (sscanf(&prog[j][0],
                 "%" XSTR(MAX_ARG_LEN) "s %" XSTR(MAX_ARG_LEN) "s %" XSTR(
                  MAX_ARG_LEN) "s %" XSTR(MAX_ARG_LEN) "s", opcode, arg1, arg2, arg3) < 1) { // parse the line without label
        printf("parse error line %d\n", j);
        return (-1);
      }
    }

  //Loop through each possible opcode
    for (i=0; i<MAX_OPCODE; i++){
      //Compare the opcode just read, and the current index in the array of opcodes, check if the given opcode exists.
        if (!strcmp(opcode, opcode_str[i]) && ((*opcode_func[i]) != NULL))
        {
          //Check that the given opcode works with the parameters.
          // *opcode_func[i] points to an array of the memory addresses of each opcode function.
            if ((*opcode_func[i])(j, &bytecode, opcode, arg1, arg2, arg3) < 0)
            {
                printf("ERROR: line %d opcode error (assembly: %s %s %s %s)\n", j, opcode, arg1, arg2, arg3);
                return (-1);
            }
            else
            {
                //Prints out the The text address, of the bytecode, then the bytecode itself/
                // addi $a0, $zero, 100
                //  0x00400000 0x20040064
                // The bytecode has 0x200 as it is add, followed by a 4 for register $a0, then 64 as in hex, 16 * 6 = 96 + 4 = 100.


                //If the opcode function works, then print out the bytecode
                printf("0x%08x 0x%08x\n", ADDR_TEXT + 4 * j, bytecode);
                text[j] = bytecode;
                break;
            }
        }
        if (i == (MAX_OPCODE - 1))
        {
            printf("ERROR: line %d unknown opcode\n", j);
            return (-1); 
        }
    }

    j++;
  }
  printf("... DONE!\n");
  return (0);
}

/* loading the program into memory */
int load_program(char *filename) {
  int j = 0;
  FILE *f;

  printf("LOADING PROGRAM %s ...\n", filename);

  f = fopen(filename, "r");
  if (f == NULL) {
      printf("ERROR: Cannot open program %s...\n", filename);
      return -1;
  }
  while (fgets(&prog[prog_len][0], MAX_LINE_LEN, f) != NULL) {
    prog_len++;
  }

  printf("PROGRAM:\n");
  for (j = 0; j < prog_len; j++) {
    printf("%d: %s", j, &prog[j][0]);
  }

  return (0);
}
