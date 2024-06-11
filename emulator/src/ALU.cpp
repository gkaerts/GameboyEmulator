#include "ALU.hpp"
#include "SM83.hpp"

namespace emu::SM83
{
    namespace
    {
        bool HasCarry(uint16_t wideResult)
        {
            //return wideResult & (1 << 8);
            return wideResult > 0xFF;
        }

        bool HasHalfCarry(uint8_t operandA, 
            uint8_t operandB)
        {
            return ((operandA & 0xF) + (operandB & 0xF)) & 0x10;
        }

        bool HasHalfCarry(uint8_t operandA, 
            uint8_t operandB,
            uint8_t carryVal)
        {
            return (((operandA & 0xF) + (operandB & 0xF)) + carryVal) & 0x10;
        }

        bool HasHalfBorrow(uint8_t operandA, 
            uint8_t operandB)
        {
            return ((operandA & 0xF) - (operandB & 0xF)) & 0x10;
        }

        bool HasHalfBorrow(uint8_t operandA, 
            uint8_t operandB,
            uint8_t carryVal)
        {
            return (((operandA & 0xF) - (operandB & 0xF)) - carryVal) & 0x10;
        }

        struct FlagHelper
        {
            union
            {
                uint8_t _u8;
                struct
                {
                    uint8_t _unused : 4;
                    uint8_t _C : 1;
                    uint8_t _H : 1;
                    uint8_t _N : 1;
                    uint8_t _Z : 1;
                } _bits;
            };
        };
    }

    ALUOutput ProcessALUOp(
        ALUOp op, 
        uint8_t flagsIn, 
        uint8_t operandA, 
        uint8_t operandB)
    {
        FlagHelper flagsOut = { ._u8 = flagsIn };

        uint16_t resultWide = 0; // Easier handling of carry bit
        uint16_t operandAWide = operandA;
        uint16_t operandBWide = operandB;

        uint8_t carryValue = (flagsIn & SF_Carry) ? 1 : 0;
        switch (op)
        {
        case ALUOp::Add:
            resultWide = operandAWide + operandBWide;
            flagsOut._bits =
            {
                ._C = HasCarry(resultWide),
                ._H = HasHalfCarry(operandA, operandB),
                ._N = 0,
                ._Z = (resultWide & 0xFF) == 0,
            };
            break;
        case ALUOp::Adc:
            resultWide = operandAWide + operandBWide + carryValue;
            flagsOut._bits =
            {
                ._C = HasCarry(resultWide),
                ._H = HasHalfCarry(operandA, operandB, carryValue),
                ._N = 0,
                ._Z = (resultWide & 0xFF) == 0,
            };
            break;
        case ALUOp::Sub:
            resultWide = operandAWide - operandBWide;
            flagsOut._bits =
            {
                ._C = HasCarry(resultWide),
                ._H = HasHalfBorrow(operandA, operandB),
                ._N = 1,
                ._Z = (resultWide & 0xFF) == 0,
            };
            break;
        case ALUOp::Sbc:
            resultWide = operandAWide - operandBWide - carryValue;
            flagsOut._bits =
            {
                ._C = HasCarry(resultWide),
                ._H = HasHalfBorrow(operandA, operandB, carryValue),
                ._N = 1,
                ._Z = (resultWide & 0xFF) == 0,
            };
            break;
        case ALUOp::And:
            resultWide = operandAWide & operandBWide;
            flagsOut._bits =
            {
                ._C = 0,
                ._H = 1,
                ._N = 0,
                ._Z = (resultWide & 0xFF) == 0,
            };
            break;
        case ALUOp::Xor:
            resultWide = operandAWide ^ operandBWide;
            flagsOut._bits =
            {
                ._C = 0,
                ._H = 0,
                ._N = 0,
                ._Z = (resultWide & 0xFF) == 0,
            };
            break;
        case ALUOp::Or:
            resultWide = operandAWide | operandBWide;
            flagsOut._bits =
            {
                ._C = 0,
                ._H = 0,
                ._N = 0,
                ._Z = (resultWide & 0xFF) == 0,
            };
            break;
        case ALUOp::Cp:
            resultWide = operandAWide - operandBWide;
            flagsOut._bits =
            {
                ._C = HasCarry(resultWide),
                ._H = HasHalfBorrow(operandA, operandB),
                ._N = 1,
                ._Z = (resultWide & 0xFF) == 0,
            };

            resultWide = operandAWide;
            break;
        case ALUOp::Inc:
            resultWide = operandBWide + 1;
            flagsOut._bits =
            {
                ._C = flagsOut._bits._C,
                ._H = HasHalfCarry(operandA, 1),
                ._N = 0,
                ._Z = (resultWide & 0xFF) == 0,
            };
            break;
        case ALUOp::Dec:
            resultWide = operandBWide - 1;
            flagsOut._bits =
            {
                ._C = flagsOut._bits._C,
                ._H = HasHalfBorrow(operandB, 1),
                ._N = 1,
                ._Z = (resultWide & 0xFF) == 0,
            };
            break;
        case ALUOp::Rl:
            resultWide = (operandBWide << 1) + flagsOut._bits._C;
            flagsOut._bits =
            {
                ._C = uint8_t(operandBWide & 0x80) ? uint8_t(1) : uint8_t(0),
                ._H = 0,
                ._N = 0,
                ._Z = 0,
            };
            break;
        case ALUOp::Rlc:
        {
            resultWide = operandBWide << 1;
            uint16_t rotateCarry = (resultWide & 0x100) ? 1 : 0;
            resultWide = (resultWide & 0xFFFE) + rotateCarry;
            flagsOut._bits =
            {
                ._C = HasCarry(resultWide),
                ._H = 0,
                ._N = 0,
                ._Z = 0,
            };
        }
            break;
        case ALUOp::Rr:
            resultWide = (operandBWide >> 1) + (flagsOut._bits._C << 7); 
            flagsOut._bits =
            {
                ._C = uint8_t(operandBWide & 0x01),
                ._H = 0,
                ._N = 0,
                ._Z = 0,
            };
            break;
        case ALUOp::Rrc:
        {
            uint16_t rotateCarry = (operandBWide & 0x01) << 7;
            resultWide = operandBWide >> 1;
            resultWide = (resultWide & 0xFF7F) | rotateCarry;
            flagsOut._bits =
            {
                ._C = uint8_t(operandBWide & 0x01),
                ._H = 0,
                ._N = 0,
                ._Z = 0,
            };
        }
            break;        
        case ALUOp::Da:
        {
            // Decimal adjust. See http://z80-heaven.wikidot.com/instructions-set:daa
            resultWide = operandBWide;

            bool setCarry = false;
            uint8_t adjust = 0;
            if (flagsOut._bits._H || (!flagsOut._bits._N && (resultWide & 0xF) > 0x9))
            {
                adjust |= 0x06;
            }

            if (flagsOut._bits._C || (!flagsOut._bits._N && resultWide > 0x99))
            {
                adjust |= 0x60;
                setCarry = true;
            }

            resultWide = flagsOut._bits._N ? resultWide - adjust : resultWide + adjust;

            flagsOut._bits =
            {
                ._C = setCarry,
                ._H = 0,
                ._N = flagsOut._bits._N,
                ._Z = (resultWide & 0xFF) == 0,
            };
        }
            break;
        case ALUOp::Scf:
            resultWide = operandBWide;
            flagsOut._bits =
            {
                ._C = 1,
                ._H = 0,
                ._N = 0,
                ._Z = flagsOut._bits._Z,
            };
            break;
        case ALUOp::Ccf:
            resultWide = operandBWide;
            flagsOut._bits =
            {
                ._C = flagsOut._bits._C ? uint8_t(0) : uint8_t(1),
                ._H = 0,
                ._N = 0,
                ._Z = flagsOut._bits._Z,
            };
            break;
        case ALUOp::Cpl:
            resultWide = ~operandBWide;
            flagsOut._bits =
            {
                ._C = flagsOut._bits._C,
                ._H = 1,
                ._N = 1,
                ._Z = flagsOut._bits._Z,
            };
            break;
        case ALUOp::Nop:
            resultWide = operandBWide;
            break; 

        case ALUOp::AddKeepZ:
            resultWide = operandAWide + operandBWide;
            flagsOut._bits =
            {
                ._C = HasCarry(resultWide),
                ._H = HasHalfCarry(operandA, operandB),
                ._N = 0,
                ._Z = flagsOut._bits._Z,
            };
            break;
        case ALUOp::AdcKeepZ:
            resultWide = operandAWide + operandBWide + carryValue;
            flagsOut._bits =
            {
                ._C = HasCarry(resultWide),
                ._H = HasHalfCarry(operandA, operandB, carryValue),
                ._N = 0,
                ._Z = flagsOut._bits._Z,
            };
            break;
        default:
            break;
        }

        return 
        {
            ._result = uint8_t(resultWide & 0xFF),
            ._flags = flagsOut._u8
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