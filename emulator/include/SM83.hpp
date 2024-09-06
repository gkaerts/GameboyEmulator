#pragma once

#include "common.hpp"
#include <cstddef>

namespace emu::SM83
{
    enum StatusFlag
    {
        SF_Carry     = 1 << 4,
        SF_HalfCarry = 1 << 5,
        SF_Subtract  = 1 << 6,
        SF_Zero      = 1 << 7,
    };

    struct Registers
    {
        union
        {
            struct
            {
                uint16_t BC, DE, HL;    // General purpose
                uint16_t AF;            // Flags and accumulator
                uint16_t SP, PC;        // Stack pointer and program counter

                uint16_t TempWZ;        // Temporary internal registers
                uint16_t IR_IME;
            } _reg16;

            struct
            {
                uint8_t C, B, E, D, L, H;
                uint8_t F, A;
                uint8_t SPL, SPH, PCL, PCH;
                uint8_t Z, W;
                uint8_t IME, IR;
            } _reg8;

            uint16_t _reg16Arr[8];
            uint8_t _reg8Arr[16];
        };
    };

    struct IO
    {
        struct PinsOut
        {
            // System Control
            uint8_t M1 : 1;
            uint8_t MRQ : 1;
            uint8_t RD : 1;
            uint8_t WR : 1;
        };

        uint16_t _address;  // Address bus (pins A0-A15)
        uint8_t _data;      // Data bus (pins D0 - D7)
        PinsOut _outPins;
    };

    // Arithmetic Logic Unit (8 bit)
    enum class ALUOp
    {
        Add = 0,
        Adc,
        Sub,
        Sbc,
        And,
        Xor,
        Or,
        Cp,


        Inc,
        Dec,

        Rl,
        Rlc,
        Rr,
        Rrc,

        Da,
        Scf,
        Ccf,
        Cpl,

        AddKeepZ,
        AdcKeepZ,
        Adjust,

        Sla,
        Sra,
        Swap,
        Srl,

        Bit0,
        Bit1,
        Bit2,
        Bit3,
        Bit4,
        Bit5,
        Bit6,
        Bit7,

        Res0,
        Res1,
        Res2,
        Res3,
        Res4,
        Res5,
        Res6,
        Res7,

        Set0,
        Set1,
        Set2,
        Set3,
        Set4,
        Set5,
        Set6,
        Set7,

        Nop
    };

    // Increment Decrement Unit (16 bit)
    enum class IDUOp
    {
        Inc = 0,
        Dec,
        Adjust,

        Nop
    };

    enum class RegisterOperand : uint8_t
    {
        RegC = 0,
        RegB,
        RegE,
        RegD,
        RegL,
        RegH,
        RegF,
        RegA,
        RegSPL,
        RegSPH,
        RegPCL,
        RegPCH,
        TempRegZ,
        TempRegW,
        RegIE,
        RegIR,

        RegBC,
        RegDE,
        RegHL,
        RegAF,
        RegSP,
        RegPC,
        RegWZ,
        RegIRIE,

        None = 0xFF,
        WideRegisterStart = RegBC
    };

    struct MCycle
    {
        struct ALU
        {
            ALUOp _op;
            RegisterOperand _operandA;
            RegisterOperand _operandB;
            RegisterOperand _dest;
        };
            
        struct IDU
        {
            IDUOp _op;
            RegisterOperand _operand;
            RegisterOperand _dest;
        };

        struct MemOp
        {
            enum Flags
            {
                MOF_None = 0x0,
                MOF_Active = 0x01,
                MOF_IsMemWrite = 0x02, // Otherwise read
                MOF_UseOffsetAddress = 0x04,
            };
            
            uint8_t _flags;
            RegisterOperand _reg;
            union 
            {
                RegisterOperand _addressSrc;
                RegisterOperand _addressSrcBeforeOffset;
            };
            
        };

        struct Misc
        {
            enum Flags : uint16_t
            {
                MF_None = 0x0,
                MF_WriteWZToWideRegister = 0x01,
                MF_StopExecution = 0x02,
                MF_HaltExecution = 0x04,
                MF_WriteValueToWideRegister = 0x08,
                MF_EnableInterrupts = 0x10,
                MF_DisableInterrupts = 0x20,
                MF_ALUKeepFlags = 0x40,

                MF_LastCycle = 0x80,
                MF_ConditionCheckZ = 0x100,
                MF_ConditionCheckNZ = 0x200,
                MF_ConditionCheckC = 0x400,
                MF_ConditionCheckNC = 0x800,

                MF_PrefixCB = 0x1000,
                MF_ALUClearZero = 0x2000,
            };

            uint16_t _flags;
            RegisterOperand _operand;
            uint16_t _optValue;
        };

        ALU _alu;
        IDU _idu;
        MemOp _memOp;
        Misc _misc;
    };

    enum TCycleState
    {
        T1_0 = 0,
        T1_1,
        T2_0,
        T2_1,
        T3_0,
        T3_1,
        T4_0,
        T4_1
    };

    enum class InstructionTable : uint8_t
    {
        Default,
        PrefixCB,
        Interrupt,  // Special case for jumping to interrupt handlers, not really an instruction table
    };
    struct Decoder
    {
        enum Flags
        {
            DF_None = 0x0,
            DF_ExecutionStopped = 0x01,         // Not sure if this is the right place for this
            DF_ExecutionHalted = 0x02,
            DF_SignBitHigh = 0x04,              // Same with this one
        };

        uint8_t _flags;
        uint8_t _nextMCycleIndex;
        TCycleState _tCycleState;

        MCycle _currMCycle;
        InstructionTable _table;
    };

    struct PeripheralIO
    {
        // Input
        uint8_t JOYP;           // Joypad

        // Serial port  
        uint8_t SB;             // Serial transfer data
        uint8_t SC;             // Serial transfer control

        // Timer    
        uint8_t SYSCLCK;       // System clock, upper 4 bits make up the DIV register
        uint8_t DIV;
        uint8_t TIMA;           // Timer counter
        uint8_t TMA;            // Timer modulo
        uint8_t TAC;            // Timer control
        uint8_t UNKNOWN0[7];    // $FF08 - FF0E

        // Interrupts   
        uint8_t IF;             // Interrupt requests

        // Audio
        uint8_t NR[32];         // Audio registers
        uint8_t WaveRAM[16];    // Wave pattern RAM

        // LCD
        uint8_t LCDC;           // LCD control
        uint8_t STAT;           // LCD Stat
        uint8_t SCY;            // Scroll registers
        uint8_t SCX;
        uint8_t LY;             // Scanline index
        uint8_t LYC;            // Scanline compare
        uint8_t UNKNOWN1[1];    // $FF46
        uint8_t BGP;            // Background palette
        uint8_t OBP0;           // Object palette 0
        uint8_t OBP1;           // Object palette 1
        uint8_t WY;             // Window registers
        uint8_t WX;
        uint8_t UNKNOWN2[3];    // $FF4C-$FF4E
        uint8_t VBK;            // VRAM bank select (GBC)

        // Boot
        uint8_t BOOT_CTRL;

        // VRAM DMA (GBC)
        uint8_t HDMA1;
        uint8_t HDMA2;
        uint8_t HDMA3;
        uint8_t HDMA4;
        uint8_t HDMA5;

        uint8_t UNKNOWN3[18];   // $FF56 -$FF67

        // Color palettes (GBC)
        uint8_t BCPS;
        uint8_t BCPD;
        uint8_t OCPS;
        uint8_t OCPD;

        uint8_t UNKNOWN4[4];    // $FF6C - $FF6F

        // WRAM bank select (GBC)
        uint8_t SVBK;
        uint8_t UNKNOWN5[15];   // $FF71 - $FF7F

        // HRAM - $FF80 - $FFFE
        uint8_t HRAM[127];

        // Interrupt enable
        uint8_t IE;
    };
    static_assert(sizeof(PeripheralIO) == 256);

    struct CPU
    {
        IO _io;
        Registers _registers;
        Decoder _decoder;
        PeripheralIO _peripheralIO;

        uint8_t _bootROM[256];
        uint32_t _tcycle;
    };

    struct MMU;

    void BootCPU(CPU& cpu, uint16_t initSP, uint16_t initPC, uint8_t initBootCtrl = 0);
    void MapPeripheralIOMemory(CPU& cpu, MMU& mmu);
    void TickCPU(CPU& cpu, MMU& mmu, uint32_t cycles);

    const char* GetOpcodeName(InstructionTable table, uint8_t opCode);
}
