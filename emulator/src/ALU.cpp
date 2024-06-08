#include "ALU.hpp"
#include "LR35902.hpp"

namespace emu::LR35902
{
    namespace
    {
        bool HasCarry(uint16_t wideResult)
        {
            return wideResult & (1 << 8);
        }

        bool HasHalfCarry(uint8_t operandA, 
            uint8_t operandB)
        {
            return ((operandA & 0xF) - (operandB & 0xF)) & 0x10;
        }

        bool SetFlags(bool C, bool H, bool N, uint16_t resultWide)
        {
            uint8_t flags = 0;
            flags |= C ? SF_Carry : 0;
            flags |= H ? SF_HalfCarry : 0;
            flags |= N ? SF_Subtract : 0;
            flags |= (resultWide == 0) ? SF_Zero : 0;

            return flags;
        }
    }

    ALUOutput ProcessALUOp(
        ALUOp op, 
        uint8_t flagsIn, 
        uint8_t operandA, 
        uint8_t operandB)
    {
        uint8_t flagsOut = flagsIn;

        uint16_t resultWide = 0; // Easier handling of carry bit
        uint16_t operandAWide = operandA;
        uint16_t operandBWide = operandB;

        uint8_t carryValue = (flagsIn & SF_Carry) ? 1 : 0;
        switch (op)
        {
        case ALUOp::Add:
            resultWide = operandAWide + operandBWide;
            flagsOut = SetFlags(
                HasCarry(resultWide), 
                HasHalfCarry(operandA, operandB), 
                false, 
                resultWide);
            break;
        case ALUOp::Adc:
            resultWide = operandAWide + operandBWide + carryValue;
            flagsOut = SetFlags(
                HasCarry(resultWide), 
                HasHalfCarry(operandA, operandB), 
                false, 
                resultWide);
            break;
        case ALUOp::Sub:
            resultWide = operandAWide - operandBWide;
            flagsOut = SetFlags(
                HasCarry(resultWide), 
                HasHalfCarry(operandA, operandB), 
                true, 
                resultWide);
            break;
        case ALUOp::Sbc:
            resultWide = operandAWide - operandBWide - carryValue;
            flagsOut = SetFlags(
                HasCarry(resultWide), 
                HasHalfCarry(operandA, operandB), 
                true, 
                resultWide);
            break;
        case ALUOp::And:
            resultWide = operandAWide & operandBWide;
            flagsOut = SetFlags(
                false, 
                true, 
                false, 
                resultWide);
            break;
        case ALUOp::Xor:
            resultWide = operandAWide ^ operandBWide;
            flagsOut = SetFlags(
                false,
                false,
                false,
                resultWide);
            break;
        case ALUOp::Or:
            resultWide = operandAWide | operandBWide;
            flagsOut = SetFlags(
                false,
                false,
                false,
                resultWide);
            break;
        case ALUOp::Cp:
            resultWide = operandAWide - operandBWide;
            flagsOut = SetFlags(
                !HasCarry(resultWide),
                !HasHalfCarry(operandA, operandB),
                true,
                resultWide);
            break;
        case ALUOp::Inc:
            resultWide = operandBWide + 1;
            flagsOut = SetFlags(
                (flagsIn & SF_Carry) > 0,
                HasHalfCarry(1, operandB),
                false,
                resultWide);
            break;
        case ALUOp::Dec:
            resultWide = operandBWide - 1;
            flagsOut = SetFlags(
                    (flagsIn & SF_Carry) > 0,
                    HasHalfCarry(1, operandB),
                    true,
                    resultWide);
            break;            
        default:
            break;
        }

        return 
        {
            ._result = uint8_t(resultWide & 0xFF),
            ._flags = flagsOut
        };
    }
}