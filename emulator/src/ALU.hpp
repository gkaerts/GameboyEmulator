#pragma once
#include "SM83.hpp"

namespace emu::SM83
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


    struct IDUOutput
    {
        uint16_t _result;
    };

    IDUOutput ProcessIDUOp(
        IDUOp op,
        uint16_t operand);

}