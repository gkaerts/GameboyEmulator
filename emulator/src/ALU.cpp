#include "ALU.hpp"
#include "SM83.hpp"

namespace emu::SM83
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

        bool SetFlags(bool C, bool H, bool N, bool nonZero)
        {
            uint8_t flags = 0;
            flags |= C ? SF_Carry : 0;
            flags |= H ? SF_HalfCarry : 0;
            flags |= N ? SF_Subtract : 0;
            flags |= nonZero ? 0 : SF_Zero;

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
        case ALUOp::Rl:
            resultWide = operandBWide << 1;
            flagsOut = SetFlags(
                HasCarry(resultWide),
                false,
                false,
                true);
            break;
        case ALUOp::Rlc:
        {
            resultWide = operandBWide << 1;
            uint16_t rotateCarry = (resultWide & 0x100) ? 1 : 0;
            resultWide = (resultWide & 0xFFFE) + rotateCarry;
            flagsOut = SetFlags(
                HasCarry(resultWide),
                false,
                false,
                true);
        }
            break;
        case ALUOp::Rr:
            resultWide = operandBWide >> 1;
            flagsOut = SetFlags(
                HasCarry(resultWide),
                false,
                false,
                true);
            break;
        case ALUOp::Rrc:
        {
            uint16_t rotateCarry = (operandBWide & 0x01) << 7;
            resultWide = operandBWide >> 1;
            resultWide = (resultWide & 0xFF7F) | rotateCarry;
            flagsOut = SetFlags(
                HasCarry(resultWide),
                false,
                false,
                true);
        }
            break;        
        case ALUOp::Da:

            // Decimal adjust. See http://z80-heaven.wikidot.com/instructions-set:daa
            resultWide = operandBWide;
            if ((flagsIn & SF_HalfCarry) || (resultWide & 0x0F) > 0x09)
            {
                resultWide += 0x06;
            }

            if (((resultWide & 0xF0) >> 4) > 0x09)
            {
                resultWide += 0x60;
            }

            flagsOut = SetFlags(
                HasCarry(resultWide),
                false,
                flagsIn & SF_Subtract,
                resultWide);
            break;
        case ALUOp::Scf:
            flagsOut = SetFlags(
                true,
                false,
                false,
                !(flagsIn & SF_Zero));
            break;
        case ALUOp::Ccf:
            flagsOut = SetFlags(
                !(flagsIn & SF_Carry),
                false,
                false,
                !(flagsIn & SF_Zero));
            break;
        case ALUOp::Cpl:
            resultWide = ~operandBWide;
            flagsOut = SetFlags(
                flagsIn & SF_Carry,
                true,
                true,
                !(flagsIn & SF_Zero));
            break;
        case ALUOp::Nop:
            resultWide = operandBWide;
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

    IDUOutput ProcessIDUOp(
        IDUOp op,
        uint16_t operand)
    {
        uint16_t result = 0;

        switch (op)
        {
        case IDUOp::Inc:
            result = operand + 1;
            break;
        case IDUOp::Dec:
            result = operand - 1;
            break;
        default:
            break;
        }

        return {
            ._result = result
        };
    }
}