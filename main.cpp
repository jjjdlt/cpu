#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <assert.h>

using Word = unsigned short;
using Byte = unsigned char;
using u32 = unsigned int;

struct Mem {
    static constexpr u32 MAX_MEM = 1024 * 64;
    Byte Data[MAX_MEM];

    void Initialize() {
        for (u32 i = 0; i < MAX_MEM; ++i) {
            Data[i] = 0;
        }
    }

    Byte operator[](u32 Address) const {
        return Data[Address];
    }

    Byte& operator[](u32 Address) {
        return Data[Address];
    }

    // Utility function to write 2 bytes
    void WriteWord(Word Value, u32 Address) {
        Data[Address] = Value & 0xFF;
        Data[Address + 1] = (Value >> 8);
    }
};

struct CPU {
    Word PC;    // Program Counter
    Word SP;    // Stack Pointer

    Byte A, X, Y;  // Registers

    // Status flags
    Byte C : 1;    // Carry
    Byte Z : 1;    // Zero
    Byte I : 1;    // Interrupt Disable
    Byte D : 1;    // Decimal
    Byte B : 1;    // Break
    Byte V : 1;    // Overflow
    Byte N : 1;    // Negative

    // Instruction set
    static constexpr Byte
        INS_LDA_IM = 0xA9,
        INS_LDA_ZP = 0xA5,
        INS_LDA_ZPX = 0xB5,
        INS_JSR = 0x20,
        INS_RTS = 0x60,
        INS_CLC = 0x18,
        INS_SEC = 0x38,
        INS_LDX_IM = 0xA2,
        INS_LDY_IM = 0xA0,
        INS_TAX = 0xAA,
        INS_TAY = 0xA8,
        INS_TXA = 0x8A,
        INS_TYA = 0x98;

    void Reset(Mem& memory) {
        PC = 0xFFFC;
        SP = 0x0100;
        D = I = 0;
        C = Z = B = V = N = 0;
        A = X = Y = 0;
        memory.Initialize();
    }

    Byte FetchByte(u32& Cycles, Mem& memory) {
        Byte Data = memory[PC];
        PC++;
        Cycles--;
        return Data;
    }

    Word FetchWord(u32& Cycles, Mem& memory) {
        Word Data = memory[PC];
        PC++;
        Data |= (memory[PC] << 8);
        PC++;
        Cycles -= 2;
        return Data;
    }

    // Read from address methods
    Byte ReadByte(u32& Cycles, Byte Address, Mem& memory) {
        Byte Data = memory[Address];
        Cycles--;
        return Data;
    }

    // Stack operations
    void PushWordToStack(u32& Cycles, Mem& memory, Word Value) {
        memory[SP] = (Value >> 8);
        memory[SP - 1] = Value & 0xFF;
        SP -= 2;
        Cycles -= 2;
    }

    Word PopWordFromStack(u32& Cycles, Mem& memory) {
        Word Value = memory[SP + 1] | (memory[SP + 2] << 8);
        SP += 2;
        Cycles -= 2;
        return Value;
    }

    // Flag operations
    void SetZeroAndNegativeFlags(Byte Register) {
        Z = (Register == 0);
        N = (Register & 0b10000000) > 0;
    }

    void Execute(u32 Cycles, Mem& memory) {
        while (Cycles >= 0) {
            Byte Ins = FetchByte(Cycles, memory);

            printf("Executing instruction 0x%02X at PC: 0x%04X (Cycles remaining: %d)\n",
                   Ins, PC-1, Cycles);

            switch (Ins) {
                case INS_LDA_IM: {
                    if (Cycles < 1) return;  // Need 1 more cycle for the value
                    Byte Value = FetchByte(Cycles, memory);
                    A = Value;
                    SetZeroAndNegativeFlags(A);
                    printf("LDA_IM: Loaded 0x%02X into A\n", A);
                } break;

                case INS_LDA_ZP: {
                    if (Cycles < 2) return;  // Need 2 more cycles
                    Byte ZeroPageAddr = FetchByte(Cycles, memory);
                    A = ReadByte(Cycles, ZeroPageAddr, memory);
                    SetZeroAndNegativeFlags(A);
                } break;

                case INS_LDA_ZPX: {
                    if (Cycles < 3) return;  // Need 3 more cycles
                    Byte ZeroPageAddr = FetchByte(Cycles, memory);
                    ZeroPageAddr += X;
                    Cycles--;
                    A = ReadByte(Cycles, ZeroPageAddr, memory);
                    SetZeroAndNegativeFlags(A);
                } break;

                case INS_JSR: {
                    if (Cycles < 3) return;  // Need 3 more cycles
                    Word SubAddr = FetchWord(Cycles, memory);
                    PushWordToStack(Cycles, memory, PC - 1);
                    PC = SubAddr;
                    Cycles--;
                } break;

                case INS_RTS: {
                    if (Cycles < 4) return;  // Need 4 cycles
                    Word ReturnAddr = PopWordFromStack(Cycles, memory);
                    PC = ReturnAddr + 1;
                    Cycles -= 2;
                } break;

                case INS_CLC: {
                    C = 0;
                    Cycles--;
                } break;

                case INS_SEC: {
                    C = 1;
                    Cycles--;
                } break;

                case INS_LDX_IM: {
                    if (Cycles < 1) return;
                    X = FetchByte(Cycles, memory);
                    SetZeroAndNegativeFlags(X);
                } break;

                case INS_LDY_IM: {
                    if (Cycles < 1) return;
                    Y = FetchByte(Cycles, memory);
                    SetZeroAndNegativeFlags(Y);
                } break;

                case INS_TAX: {
                    X = A;
                    SetZeroAndNegativeFlags(X);
                    Cycles--;
                    printf("TAX: Transferred 0x%02X from A to X\n", X);
                } break;

                case INS_TAY: {
                    Y = A;
                    SetZeroAndNegativeFlags(Y);
                    Cycles--;
                } break;

                case INS_TXA: {
                    A = X;
                    SetZeroAndNegativeFlags(A);
                    Cycles--;
                } break;

                case INS_TYA: {
                    A = Y;
                    SetZeroAndNegativeFlags(A);
                    Cycles--;
                } break;

                default: {
                    printf("Instruction not handled: %02X at PC=%04X\n", Ins, PC-1);
                    return;
                } break;
            }
        }
    }
};

void RunTests() {
    printf("Starting 6502 CPU tests...\n\n");

    // Test 1: LDA Immediate
    {
        printf("Test 1: Load Accumulator Immediate\n");
        Mem mem;
        CPU cpu;
        cpu.Reset(mem);

        mem[0xFFFC] = CPU::INS_LDA_IM;
        mem[0xFFFD] = 0x42;

        printf("Running Test 1 - PC: 0x%04X\n", cpu.PC);
        cpu.Execute(2, mem);

        if (cpu.A == 0x42) {
            printf("✓ Accumulator loaded with 0x42 successfully\n");
        } else {
            printf("❌ Accumulator test failed. Expected 0x42, got 0x%02X\n", cpu.A);
        }

        if (cpu.Z == 0) {
            printf("✓ Zero flag correctly not set\n");
        } else {
            printf("❌ Zero flag incorrectly set\n");
        }

        if (cpu.N == 0) {
            printf("✓ Negative flag correctly not set\n");
        } else {
            printf("❌ Negative flag incorrectly set\n");
        }
        printf("\n");
    }

    // Test 2: Register Transfer (TAX)
    {
        printf("Test 2: Register Transfer (TAX)\n");
        Mem mem;
        CPU cpu;
        cpu.Reset(mem);

        mem[0xFFFC] = CPU::INS_LDA_IM;
        mem[0xFFFD] = 0x37;
        mem[0xFFFE] = CPU::INS_TAX;

        printf("Test program:\n");
        printf("0xFFFC: LDA_IM (0x%02X)\n", mem[0xFFFC]);
        printf("0xFFFD: 0x%02X\n", mem[0xFFFD]);
        printf("0xFFFE: TAX (0x%02X)\n", mem[0xFFFE]);

        printf("\nBefore execution:\n");
        printf("PC: 0x%04X, A: 0x%02X, X: 0x%02X\n", cpu.PC, cpu.A, cpu.X);

        printf("\nCycle breakdown:\n");
        printf("LDA_IM: 2 cycles (1 for opcode, 1 for value)\n");
        printf("TAX: 1 cycle\n");
        printf("Total needed: 3 cycles\n\n");

        cpu.Execute(3, mem);

        printf("\nAfter execution:\n");
        printf("PC: 0x%04X, A: 0x%02X, X: 0x%02X\n", cpu.PC, cpu.A, cpu.X);

        if (cpu.X == 0x37) {
            printf("✓ X register contains correct value 0x37\n");
        } else {
            printf("❌ X register contains wrong value. Expected 0x37, got 0x%02X\n", cpu.X);
        }
    }
}

int main() {
    bool runTests = true;

    if (runTests) {
        RunTests();
    } else {
        Mem mem;
        CPU cpu;
        cpu.Reset(mem);
        mem[0xFFFC] = CPU::INS_LDA_IM;
        mem[0xFFFD] = 0x42;
        cpu.Execute(2, mem);
    }

    return 0;
}