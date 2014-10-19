//
//  rsvm/main.c
//
//  Created by Michael Henderson on 10/19/14.
//  Copyright (c) 2014 Michael D Henderson. All rights reserved.
//
//  a really simple virtual machine from BCPL's INTCODE interpreter
//    http://www.cl.cam.ac.uk/~mr10/bcplman.pdf
//    http://www.gtoal.com/languages/bcpl/amiga/bcpl/booting.txt
//
//  this vm contains only 7 instructions. it's easy to extend via
//  the exop instruction.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// define the word size. it influences the size of registers and the
// number of bytes that the program counter updates on each step. the
// plan is support 8-, 16-, 32-, and 64-bit words and floating point.
//
#define RSVM_WORD_SIZE 1 // 8-bit register

#if (RSVM_WORD_SIZE == 1)
typedef unsigned char  rsword; //  1-byte program counter
typedef unsigned short rsreg;  // 16-bit  registers
#else
#error you must specify the word size to use
#endif

// stacks
//   globalVariables
//   programCall
// registers
//   pc   program counter/control register
//   a    accumulator
//   b    auxiliary accumulator
//   d    address register
//   g    global variable base pointer
//   p    stack frame pointer/index
// core
//
// TODO: express in terms of
//         memory address register
//         memory data    register
//
typedef struct rsvm {
    struct {
        int level;
        int steps;
    } debug;
    int halted;
    
    rsword c; //pc; // program counter
    rsword d;  // address register; effective address calculations
    rsword p;  // index into stack frame (top of program stack)
    rsword g;  // index into global variable list
    rsword a;  // main accumulator
    rsword b;  // additional accumulator

    size_t  coreSize; // number of words allocated to core
    rsword  gv[512];  // global variable array;
    rsword  pv[512];  // program call stack;
    rsword  core[1];
} rsvm;

// no matter what the word size is, instructions are:
//   4 bits for the function/operation
//   1 bit each for the D, P, G, and I modifiers
//   remaining bits are used as data
// the 8 bits that we use are always in the word's "high" byte so
// that we don't have to shift things around to use the data.
//
// instruction layout assuming word size to be 8 bits.
//   bit   7     is  the D bit, D loaded from following word
//   bit   6     is  the P bit, P to be added to D
//   bit   5     is  the G bit, G to be added to D
//   bit   4     is  the I bit, D loaded indirectly from D
//   bits  3...0 are the function bits
// note that only 256 words are addressable by the program counter.
//
// instruction layout assuming word size to be 16 bits.
//   bit  15     is  the D bit, D loaded from following word
//   bit  14     is  the P bit, P to be added to D
//   bit  13     is  the G bit, G to be added to D
//   bit  13     is  the I bit, D loaded indirectly from D
//   bits 11...8 are the function bits
//   bits  7...0 are used in various ways
//               8 bits, 1 byte, range -128..+127
// note that with a 16 bit address, core is limited to 65k words
//
// instruction layout assuming word size to be 32 bits.
//   bit  31     is  the D bit, D loaded from following word
//   bit  30     is  the P bit, P to be added to D
//   bit  29     is  the G bit, G to be added to D
//   bit  28     is  the I bit, D loaded indirectly from D
//   bits 27..24 are the function bits
//   bits 23...0 are used in various ways
//               24 bits, 3 bytes, range -8,388,608..+8,388,607
//
// instruction layout assuming word size to be 64 bits.
//   bit  63     is  the D bit, D loaded from following word
//   bit  62     is  the P bit, P to be added to D
//   bit  61     is  the G bit, G to be added to D
//   bit  60     is  the I bit, D loaded indirectly from D
//   bits 59..56 are the function bits
//   bits 55...0 are used in various ways
//               56 bits, 7 bytes, range is wowsers
//
#if (RSVM_WORD_SIZE == 1)
#define RSOP_DBIT(op) ((op) & 0x80)
#define RSOP_PBIT(op) ((op) & 0x40)
#define RSOP_GBIT(op) ((op) & 0x20)
#define RSOP_IBIT(op) ((op) & 0x10)
#define RSOP_FUNC(op) ((op) & 0x0f)
#define RSOP_DATA(op) ((op) & 0x00)
#else
#error you must specify the word size to use
#endif

// basic bit patterns for the instructions
//
#if (RSVM_WORD_SIZE == 1)
#define RSOPC_BITS_TO_SHIFT 0
#else
#error you must specify the word size to use
#endif
#define RSOPC_ADD      ((0x00) << RSOPC_BITS_TO_SHIFT)
#define RSOPC_CALL     ((0x01) << RSOPC_BITS_TO_SHIFT)
#define RSOPC_JMP      ((0x02) << RSOPC_BITS_TO_SHIFT)
#define RSOPC_JMPT     ((0x03) << RSOPC_BITS_TO_SHIFT)
#define RSOPC_JMPF     ((0x04) << RSOPC_BITS_TO_SHIFT)
#define RSOPC_LOAD     ((0x05) << RSOPC_BITS_TO_SHIFT)
#define RSOPC_STORE    ((0x06) << RSOPC_BITS_TO_SHIFT)
#define RSOPC_EXOP     ((0x07) << RSOPC_BITS_TO_SHIFT)

// formatting for logs and et cetera
//
#if (RSVM_WORD_SIZE == 1)
#define RSWORDFMT "0x%02x"
#else
#error you must specify the word size to use
#endif

// feel free to replace the allocator
//
#define rsvmalloc(x) rsvm__alloc__(__FILE__, __FUNCTION__, __LINE__, (x))
void *rsvm__alloc__(const char *file, const char *function, int line, size_t size);

rsvm *rsvm_alloc(size_t coreSize, int debugLevel, int debugSteps);
void  rsvm_dump(rsvm *vm);
void  rsvm_dump_register(rsvm *vm, const char *registers);
void  rsvm_dump_word(rsvm *vm, size_t address, rsword op);
void  rsvm_emit(rsvm *vm, size_t address, rsword word);
void  rsvm_exec(rsvm *vm);
void  rsvm_loader(rsvm *vm, size_t address, const char *code);
void  rsvm_reset(rsvm *vm);
const char *rsvm_util_op2mnemonic(rsword op);

void *rsvm__alloc__(const char *file, const char *function, int line, size_t size) {
    void *v = malloc(size);
    if (!v) {
        perror(__FUNCTION__);
        fprintf(stderr, "debug:\t%s %s %d\n", file, function, line);
        fprintf(stderr, "error: out of memory - requested %zu bytes\n", size);
        exit(2);
    }
    return v;
}

rsvm *rsvm_alloc(size_t coreSize, int debugLevel, int debugSteps) {
    coreSize = (coreSize < 16) ? 16 : coreSize;
    rsvm *vm = rsvmalloc(sizeof(*vm) + (sizeof(rsword) * coreSize));
    vm->coreSize    = coreSize;
    vm->halted      = 0;
    vm->debug.level = debugLevel;
    vm->debug.steps = debugSteps;
    vm->c = vm->a = vm->b = vm->d = vm->g = vm->g = 0;
    if (vm->debug.level) {
        int idx;
        for (idx = 0; idx <= coreSize; idx++) {
            vm->core[idx] = -1;
        }
    }
    return vm;
}

void rsvm_dump(rsvm *vm) {
    printf("...vm: ------------------------------------\n");
    printf(".....: vm             %p\n", vm);
    printf(".....: coreSize       %zu\n", vm->coreSize);
    printf(".....: programCounter %8d\n", vm->c);
    rsvm_dump_register(vm, "cdabpg");
}

void rsvm_dump_register(rsvm *vm, const char *registers) {
    while (*registers) {
        switch (*(registers++)) {
            case 'a': printf(".....: a              " RSWORDFMT "\n", vm->a); break;
            case 'b': printf(".....: b              " RSWORDFMT "\n", vm->b); break;
            case 'c': printf(".....: c              " RSWORDFMT "\n", vm->c); break;
            case 'd': printf(".....: d              " RSWORDFMT "\n", vm->d); break;
            case 'g': printf(".....: g              " RSWORDFMT "\n", vm->g); break;
            case 'p': printf(".....: p              " RSWORDFMT "\n", vm->p); break;
            default:  break;
        }
    }
}

void rsvm_dump_word(rsvm *vm, size_t address, rsword word) {
    printf(".word: %08zu ", address);
    printf(RSWORDFMT " " RSWORDFMT " ", word, RSOP_FUNC(word));
    printf("%c%c%c%c ", RSOP_DBIT(word) ? 'd':'.', RSOP_PBIT(word) ? 'p':'.', RSOP_GBIT(word) ? 'g':'.', RSOP_IBIT(word) ? 'i':'.');
    if (sizeof(rsword) > 1) {
        printf(RSWORDFMT " %d ", RSOP_DATA(word), RSOP_DATA(word));
    }
    printf("%s\n", rsvm_util_op2mnemonic(RSOP_FUNC(word)));
}

void rsvm_emit(rsvm *vm, size_t address, rsword word) {
    // verify that we have space in the core
    //
    if (!(address < vm->coreSize)) {
        printf("error: %s %d\n\temit address out of range\n", __FUNCTION__, __LINE__);
        rsvm_dump(vm);
        exit(2);
    }

    if (vm->debug.level > 5) {
        printf(".emit: %8ld => " RSWORDFMT "\n", address, word);
    }
    
    // emit the word to the core
    //
    vm->core[address] = word;
}

// An instruction is executed as follows.
//  1. Fetch word from the store
//  2. Program Counter is incremented by the word size
//  3. Effective Address is computed
//     a. Assign the address field to D
//     b. If the P bit is set, add P to D
//     c. If the G bit is set, add G to D
//     d. If the I bit is set, load D from core[D]
//  4. Perform the operation specified by the function field
//
void rsvm_exec(rsvm *vm) {
    if (vm->debug.steps != -1 && vm->debug.steps-- <= 0) {
        printf("error: %s %d\nerror: exceeded step limit\n", __FUNCTION__, __LINE__);
        exit(2);
    }
    
    if (vm->halted) {
        return;
    }
    
    // verify that we're executing steps in the core
    //
    if (!(0 <= vm->c && vm->c < vm->coreSize)) {
        printf("error: %s %d\n\tprogram counter out of range\n", __FUNCTION__, __LINE__);
        rsvm_dump(vm);
        exit(2);
    }
    
    rsvm_dump_word(vm, vm->c, vm->core[vm->c]);
    
    // fetch the instruction from core
    rsword code     = vm->core[vm->c++];
    rsword function = RSOP_FUNC(code);
    rsword dBit     = RSOP_DBIT(code);
    rsword pBit     = RSOP_PBIT(code);
    rsword gBit     = RSOP_GBIT(code);
    rsword iBit     = RSOP_IBIT(code);
    rsword addrBits = RSOP_DATA(code);
    
    // if the D bit is set, the address is the value of the next cell.
    // other wise, it is just the program counter plus the address offset.
    // if the P bit is set, the P register is added to D.
    // if the G bit is set, the G register is added to D.
    // if the I bit is set, the D register is an indirect reference.
    //
    vm->d = dBit ? vm->core[vm->c++] : vm->c + addrBits;
    if (pBit) {
        vm->d += vm->p;
    }
    if (gBit) {
        vm->d += vm->g;
    }
    if (iBit) {
        if (!(vm->d < vm->coreSize)) {
            printf("error: %s %d\n\tindirect address out of range\n", __FUNCTION__, __LINE__);
            rsvm_dump(vm);
            exit(2);
        }
        vm->d = vm->core[vm->d];
    }
    
    switch (function) {
        case RSOPC_ADD:
            vm->a += vm->d;
            break;
        case RSOPC_CALL:
            vm->d += vm->p;
            vm->core[vm->d] = vm->p;
            vm->core[vm->d + 1] = vm->c;
            vm->p = vm->d;
            vm->c = vm->a;
            break;
        case RSOPC_EXOP:
            printf(".warn: exop not implemented\n");
            break;
        case RSOPC_JMP:
            vm->c = vm->d;
            break;
        case RSOPC_JMPF:
            if (!vm->a) {
                vm->c = vm->d;
            }
            break;
        case RSOPC_JMPT:
            if (vm->a) {
                vm->c = vm->d;
            }
            break;
        case RSOPC_LOAD:
            vm->b = vm->a;
            vm->a = vm->d;
            break;
        case RSOPC_STORE:
            if (!(vm->d < vm->coreSize)) {
                printf("error: %s %d\n\taddress out of range\n", __FUNCTION__, __LINE__);
                rsvm_dump(vm);
                exit(2);
            }
            vm->core[vm->d] = vm->a;
            break;
        default:
            printf("error:\t%s %d\nerror: unknown function " RSWORDFMT "\n", __FUNCTION__, __LINE__, function);
            vm->halted = 1;
            break;
    }
}

void  rsvm_loader(rsvm *vm, size_t address, const char *code) {
    unsigned char function     = 0;
    unsigned char dBit         = 0;
    unsigned char pBit         = 0;
    unsigned char gBit         = 0;
    unsigned char iBit         = 0;
    rsword        oBits        = 0;
    const char   *mnemonic     = 0;
    rsword        word;

    printf(".code: %s\n", code);
    
    while (*code && address < vm->coreSize) {
        if (isspace(*code)) {
            while (isspace(*code)) {
                code++;
            }
            continue;
        }
        
        if (*code == ';') {
            // comment to the end of the line
            while (*code && !(*code == '\n')) {
                code++;
            }
            continue;
        }

        // accept word-sized chunks of hex data (0..F)+
        //
        if (isdigit(*code) || ('A' <= *code && *code <= 'F')) {
            int maxDigits = 2 * sizeof(rsword);
            unsigned long number = 0;
            do {
                if (isdigit(*code)) {
                    number = (number << 4) + (*(code++) - '0');
                } else if ('A' <= *code && *code <= 'F') {
                    number = (number << 4) + (*(code++) - 'A' + 10);
                } else {
                    break;
                }
            } while (*code && --maxDigits);
            
            word  = (rsword)number;
            rsvm_emit(vm, address++, word);
            
            // reset everything to prepare for the next instruction
            //
            function = 0;
            mnemonic = 0;
            dBit = pBit = gBit = iBit = oBits = 0;
            
            continue;
        }
        
        switch (*(code++)) {
            case 'g': gBit     = -1; break;
            case 'd': dBit     = -1; break;
            case 'i': iBit     = -1; break;
            case 'p': pBit     = -1; break;
            case 'a': function = RSOPC_ADD  ; mnemonic = rsvm_util_op2mnemonic(function); break;
            case 'f': function = RSOPC_JMPF ; mnemonic = rsvm_util_op2mnemonic(function); break;
            case 'j': function = RSOPC_JMP  ; mnemonic = rsvm_util_op2mnemonic(function); break;
            case 'k': function = RSOPC_CALL ; mnemonic = rsvm_util_op2mnemonic(function); break;
            case 'l': function = RSOPC_LOAD ; mnemonic = rsvm_util_op2mnemonic(function); break;
            case 's': function = RSOPC_STORE; mnemonic = rsvm_util_op2mnemonic(function); break;
            case 't': function = RSOPC_JMPT ; mnemonic = rsvm_util_op2mnemonic(function); break;
            case 'x': function = RSOPC_EXOP ; mnemonic = rsvm_util_op2mnemonic(function); break;
                break;
            default: // ignore all unknown input
                break;
        }
        
        if (mnemonic) {
            word  = RSOP_FUNC(function) ^ RSOP_DBIT(dBit) ^ RSOP_PBIT(pBit) ^ RSOP_GBIT(gBit) ^ RSOP_IBIT(iBit) ^ RSOP_DATA(oBits);
            rsvm_emit(vm, address++, word);
            
            // reset everything to prepare for the next instruction
            //
            function = 0;
            mnemonic = 0;
            dBit = pBit = gBit = iBit = oBits = 0;
        }
    }
    if (*code) {
        printf("error:\t%s %d\nerror: out of core memory\n", __FUNCTION__, __LINE__);
        exit(2);
    }
}

void rsvm_reset(rsvm *vm) {
    vm->a = vm->b = vm->c = vm->d = vm->g = vm->g = 0;
    vm->debug.level = 0;
    vm->debug.steps = -1;
}

const char *rsvm_util_op2mnemonic(rsword op) {
    switch (RSOP_FUNC(op)) {
        case RSOPC_ADD  : return "add"  ;
        case RSOPC_CALL : return "call" ;
        case RSOPC_EXOP : return "exop" ;
        case RSOPC_JMP  : return "jmp"  ;
        case RSOPC_JMPF : return "jmpf" ;
        case RSOPC_JMPT : return "jmpt" ;
        case RSOPC_LOAD : return "load" ;
        case RSOPC_STORE: return "store";
    }
    return "opinv";
}

int main(int argc, const char * argv[]) {
    rsvm *vm = rsvm_alloc(64 * 1024, 10, 8);
    rsvm_dump(vm);
    
    const char *program = "";

#if (RSVM_WORD_SIZE == 1)
    program = "q da 12 q h gpil px ga pgs k j t f l l 12da34daDEdaADdaBEdaEFda ; comments welcome";
#else
    program = "q da 1234 q h gpil px ga pgs k j t f l l 1234 DEAD BEEF ; comments welcome";
#endif

    rsvm_loader(vm, 0, program);
    
    int idx;
    for (idx = 0; idx < 10; idx++) {
        rsvm_exec(vm);
    }

    return 0;
}
