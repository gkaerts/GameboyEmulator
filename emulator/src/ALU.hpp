#pragma once
#include "LR35902.hpp"

namespace emu::LR35902
{

    struct ALUOutput
    {
        uint8_t _result;
        uint8_t _flags;
    };

    ALUOutput ProcessALUOp(
        ALUOp op, 
        uint8_t flagsIn, 
        uint8_t operandA, 
        uint8_t operandB);
}