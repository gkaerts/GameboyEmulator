#pragma once
#include "SM83.hpp"

namespace emu::SM83
{
    struct ALUOutput
    {
        uint8_t _result;
        uint8_t _flags;
    };

    enum ProcessALUOpFlags
    {
        PAOF_None = 0x0,
        PAOF_ZSignHigh = 0x01,
        PAOF_ALUHasCarry = 0x04 // Only for IDU ops
    };

    ALUOutput ProcessALUOp(
        ALUOp op,
        uint8_t flagsIn, 
        uint8_t operandA, 
        uint8_t operandB,
        int opFlags);


    struct IDUOutput
    {
        uint16_t _result;
    };

    IDUOutput ProcessIDUOp(
        IDUOp op,
        uint16_t operand,
        int opFlags);

}