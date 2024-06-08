#pragma once

#include "common.hpp"

namespace emu::LR35902
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
                uint16_t FA;            // Flags and accumulator
                uint16_t SP, PC;        // Stack pointer and program counter
            } _reg16;

            struct
            {
                uint8_t C, B, E, D, L, H;
                uint8_t F, A;
                uint8_t SPL, SPH, PCL, PCH;
            } _reg8;

            uint16_t _reg16Arr[6];
            uint8_t _reg8Arr[12];
        };
    };

    struct IO
    {
        struct PinsOut
        {
            // System Control
            uint8_t M1 : 1;
            uint8_t MREQ : 1;
            uint8_t IORQ : 1;
            uint8_t RD : 1;
            uint8_t WR : 1;
            uint8_t RFSH : 1;

            // CPU Control
            uint8_t HALT : 1;

            // CPU Bus Control
            uint8_t BUSACK : 1;
        };

        uint16_t _address;  // Address bus (pins A0-A15)
        uint8_t _data;      // Data bus (pins D0 - D7)
        PinsOut _outPins;

    };

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

        None = 0xFF
    };

    enum class MemReadSrc : uint8_t
    {
        RegHL = 0x02,
        RegSP = 0x04,
        RegPC = 0x05,

        None = 0xFF
    };

    enum class MemWriteDest : uint8_t
    {
        RegHL = 0x02,

        None = 0xFF
    };

    struct MCycle
    {
        enum class Type
        {
            Fetch = 0,
            MemRead,
            MemWrite
        };

        Type _type;
        ALUOp _aluOp;
        RegisterOperand _operandB;
        RegisterOperand _storeDest;

        MemReadSrc _memReadSrc;
        MemWriteDest _memWriteDest;
    };

    enum TCycle
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
        MCycle::Type _currMCycleType;
        uint8_t _mCycleIndex;
        TCycle _tCycleIndex;

        uint8_t IR;
        uint8_t _temp;
    };

    struct CPU
    {
        IO _io;
        Registers _registers;
        Decoder _decoder;
    };

    void Boot(CPU* cpu, uint16_t initSP, uint16_t initPC);
    void Tick(CPU* cpu, uint8_t* memory, uint32_t cycles);
}
