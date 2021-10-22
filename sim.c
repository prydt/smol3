#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define WORDSIZE 2 // 16 bits
#define ADDRSPACE 65536 // 2^16
#define REGISTERS 8

/* #define NDEBUG */ // uncomment to turn off assert

// memory address space
int16_t mem[ADDRSPACE];

// 8 16-bit general purpose registers
int16_t reg[REGISTERS];

// 16-bit program counter
int16_t pc;

// processor status register
//
// note: below the indexes are reversed
//       ie 0 is the rightmost bit
// PSR[0] = P
// PSR[1] = Z
// PSR[2] = N
//
// PSR[15] = privilege mode of the process being run
// privilege modes: 0 (supervisor mode), 1 (user mode)
//
//
// PSR[10:8] = priority level of the process being run
int16_t psr = 0;

// prints out n in binary
// TODO remove
void print_int_as_bin(int16_t n)
{
    for (int i = 15; i >= 0; i--)
    {
        printf("%d", (n >> i) & 1);
    }
    printf("\n");
}

// convert int16_t to a binary string representation
// for debugging
char *int_to_binstr(int16_t n)
{
    char *str = malloc(sizeof(char) * 16);
    for (int i = 15; i >= 0; i--)
    {
        str[15 - i] = '0' + ((n >> i) & 1);
    }
    return str;
}

// returns an int16_t of the ints in range [start, end]
// note: this range is inclusive and is from right to left
// ie 0 is the rightmost bit and 15 is the leftmost bit
int16_t bits_in_range(int16_t insn, int16_t start, int16_t end)
{
    return (insn & (0xFFFF >> (15 - start))) >> end;
}

// access specific fields in an instruction
int16_t opcode(int16_t insn) { return bits_in_range(insn, 15, 12); }
int16_t DR(int16_t insn) { return bits_in_range(insn, 11, 9); }
int16_t SR1(int16_t insn) { return bits_in_range(insn, 8, 6); }
int16_t SR2(int16_t insn) { return bits_in_range(insn, 2, 0); }
int16_t imm5(int16_t insn) { return bits_in_range(insn, 4, 0); }
int16_t N(int16_t insn) { return bits_in_range(insn, 11, 11); }
int16_t Z(int16_t insn) { return bits_in_range(insn, 10, 10); }
int16_t P(int16_t insn) { return bits_in_range(insn, 9, 9); }
int16_t offset6(int16_t insn) { return bits_in_range(insn, 5, 0); }
int16_t PCoffset9(int16_t insn) { return bits_in_range(insn, 8, 0); }
int16_t PCoffset11(int16_t insn) { return bits_in_range(insn, 10, 0); }
int16_t baseR(int16_t insn) { return bits_in_range(insn, 8, 6); }
int16_t trapvec8(int16_t insn) { return bits_in_range(insn, 7, 0); }

// sign extend bits, index is the most significant bit
int16_t sext(int16_t insn, int16_t index)
{
    return (insn & ((1 << index) - 1)) - (insn & (1 << index));
}

// sets P condition code to val, val should be either 0 or 1
void setP(int16_t val)
{
    assert(val == 0 || val == 1);
    psr |= val;
}

// sets Z condition code to val, val should be either 0 or 1
void setZ(int16_t val)
{
    assert(val == 0 || val == 1);
    psr |= val << 1;
}

// sets N condition code to val, val should be either 0 or 1
void setN(int16_t val)
{
    assert(val == 0 || val == 1);
    psr |= val << 2;
}

// return kth bit of insn
int16_t get_bit(int16_t insn, int k)
{
    return (insn >> k) & 1;
}

int16_t getP() { return get_bit(psr, 0); }
int16_t getZ() { return get_bit(psr, 1); }
int16_t getN() { return get_bit(psr, 2); }

// set reg[DR] = val and set condition codes accordingly
void setDR(int16_t DR, int16_t val)
{
    reg[DR] = val;
    if (val > 0)
    {
        // set P=1, Z=0, N=0
        setP(1);
        setZ(0);
        setN(0);
    }
    else if (val < 0)
    {
        // set P=0, Z=0, N=1
        setP(0);
        setZ(0);
        setN(1);
    }
    else
    {
        // set P=0, Z=1, N=0
        setP(0);
        setZ(1);
        setN(0);
    }
}

// run given instruction
void execute_instruction(int16_t insn)
{
    switch(opcode(insn))
    {
    case 0b0001:
        // ADD
        int16_t sum;
        if (bits_in_range(insn, 5, 5) == 0)
        {
            sum = reg[SR1(insn)] + reg[SR2(insn)];
        }
        else
        {
            sum = reg[SR1(insn)] + sext(imm5(insn), 4);
        }
        setDR(DR(insn), sum);
        break;
    case 0b0101:
        // AND
        int16_t out;
        if (bits_in_range(insn, 5, 5) == 0)
        {
            out = reg[SR1(insn)] & reg[SR2(insn)];
        }
        else
        {
            out = reg[SR1(insn)] + sext(imm5(insn), 4);
        }
        setDR(DR(insn), out);
        break;
    case 0b0000:
        // BR
        if (get_bit(insn, 11) == getN() || get_bit(insn, 10) == getZ()
                || get_bit(insn, 9) == getP())
        {
            pc += sext(PCoffset9(insn), 8);
        }
        break;
    case 0b1100:
        // JMP / RET
        pc = reg[baseR(insn)];
        break;
    case 0b0100:
        // JSR / JSRR
        reg[7] = pc;
        if (get_bit(insn, 11) == 0)
        {
            // JSRR
            pc = reg[baseR(insn)];
        }
        else
        {
            // JSR
            pc += sext(PCoffset11(insn), 10);
        }
        break;
    case 0b0010:
        // LD
        setDR(DR(insn), mem[pc + sext(PCoffset9(insn), 8)]);
        break;
    case 0b1010:
        // LDI
        setDR(DR(insn), mem[mem[pc + sext(PCoffset9(insn), 8)]]);
        break;
    case 0b0110:
        // LDR
        setDR(DR(insn), mem[reg[baseR(insn)] + sext(offset6(insn), 5)]);
        break;
    case 0b1110:
        // LEA
        setDR(DR(insn), pc + sext(PCoffset9(insn), 8));
        break;
    case 0b1001:
        // NOT
        setDR(DR(insn), ~SR1(insn));
        break;
    case 0b1000:
        // RTI

        // TODO implement later
        // figure out what the supervisor stack is
        printf("RTI not implemented yet\n");
        break;
    case 0b0011:
        // ST
        mem[pc + sext(PCoffset9(insn), 8)] = reg[DR(insn)];
        break;
    case 0b1011:
        // STI
        mem[mem[pc + sext(PCoffset9(insn), 8)]] = reg[DR(insn)];
        break;
    case 0b0111:
        // STR
        mem[reg[baseR(insn)] + sext(offset6(insn), 5)] = reg[DR(insn)];
        break;
    case 0b1111:
        // TRAP

        // TODO implement TRAP, zext, and trap vector table
        printf("TRAP not implemented yet\n");
        break;
    default:
        // reserved
        // TODO implement actual illegal opcode behavior
        printf("illegal opcode exception");
        exit(1);
        break;
    }
}

// debugging function to view contents of registers
void print_regs()
{
    for (int i = 0; i < 8; i++)
    {
        printf("reg%d: %s\n", i, int_to_binstr(reg[i]));
    }
}

int main()
{
/* #define TESTING */
#ifdef TESTING
    print_int_as_bin(0b1010101010110000);
    print_int_as_bin(bits_in_range(0b1010101010110000, 8, 0));
    print_int_as_bin(bits_in_range(0b1010101010110000, 15, 8));
    print_int_as_bin(bits_in_range(0b1010101010110000, 15, 15));
    print_int_as_bin(bits_in_range(0b1010101010110000, 0, 0));
    print_int_as_bin(imm5(0b0000000000011010));
    print_int_as_bin(sext(imm5(0b0000000000011010), 4));
    print_int_as_bin(psr);
    setP(1);
    print_int_as_bin(psr);
    setN(1);
    print_int_as_bin(psr);
    setZ(1);
    print_int_as_bin(psr);
#endif

    // test program
    // 0010000000001111
    // 1001000000111111
    // 0001000000100001
    // 0011000000001111
    print_regs();
}
