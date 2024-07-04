#pragma once

#include "common.hpp"

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
                uint16_t IR_IE;
            } _reg16;

            struct
            {
                uint8_t C, B, E, D, L, H;
                uint8_t F, A;
                uint8_t SPL, SPH, PCL, PCH;
                uint8_t Z, W;
                uint8_t IE, IR;
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
            };

            uint16_t _flags;
            RegisterOperand _operand;
            uint8_t _optValue;
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

    struct Decoder
    {
        enum Flags
        {
            DF_None = 0x0,
            DF_ExecutionStopped = 0x01,         // Not sure if this is the right place for this
            DF_ExecutionHalted = 0x02,
        };

        uint8_t _flags;
        uint8_t _nextMCycleIndex;
        TCycleState _tCycleState;

        uint8_t IR;
        uint8_t _temp;

        MCycle _currMCycle;
    };

    struct CPU
    {
        IO _io;
        Registers _registers;
        Decoder _decoder;
    };

    struct MemoryController;

    void Boot(CPU* cpu, uint16_t initSP, uint16_t initPC);
    void Tick(CPU* cpu, MemoryController& memCtrl, uint32_t cycles);

    const char* GetOpcodeName(uint8_t opCode);
}
