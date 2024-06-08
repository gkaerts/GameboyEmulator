#include "OpCodes.hpp"

#include <array>

namespace emu::SM83
{
    namespace
    {
        constexpr const uint32_t MAX_MCYCLE_COUNT = 4;
        struct Instruction
        {
            uint32_t cycleCount;
            std::array<MCycle, MAX_MCYCLE_COUNT> cycles;
        };

        Instruction INSTRUCTIONS[0xFF] = {};

        constexpr const RegisterOperand REGISTER_OPERAND_LUT[]
        {
            RegisterOperand::RegB,
            RegisterOperand::RegC,
            RegisterOperand::RegD,
            RegisterOperand::RegE,
            RegisterOperand::RegH,
            RegisterOperand::RegL,
            RegisterOperand::None,
            RegisterOperand::RegA
        };

        inline RegisterOperand OpCodeRegisterIndexToRegisterOperand(uint8_t regIdx)
        {
            return REGISTER_OPERAND_LUT[regIdx];
        }

        MCycle::ALU MakeALU(
            ALUOp op,
            RegisterOperand operandA,
            RegisterOperand operandB)
        {
            return {
                ._op = op,
                ._operandA = operandA,
                ._operandB = operandB
            };
        }

        MCycle::ALU NoALU()
        {
            return {
                ._op = ALUOp::Nop,
                ._operandA = RegisterOperand::None,
                ._operandB = RegisterOperand::None
            };
        }

        MCycle::IDU MakeIDU(
            IDUOp op,
            WideRegisterOperand operand)
        {
            return {
                ._op = op,
                ._operand = operand
            };
        }

        MCycle::IDU NoIDU()
        {
            return {
                ._op = IDUOp::Nop,
                ._operand = WideRegisterOperand::None
            };
        }

        MCycle::MemOp MakeMemRead(
            WideRegisterOperand src,
            RegisterOperand dest)
        {
            return {
                ._type = MCycle::MemOp::Type::Read,
                ._readSrcOrWriteDest = src,
                ._readDestOrWriteSrc = dest
            };
        }

        MCycle::MemOp MakeMemWrite(
            RegisterOperand src,
            WideRegisterOperand dest)
        {
            return {
                ._type = MCycle::MemOp::Type::Write,
                ._readSrcOrWriteDest = dest,
                ._readDestOrWriteSrc = src
            };
        }

        MCycle::MemOp NoMem()
        {
            return {
                ._type = MCycle::MemOp::Type::None,
                ._readSrcOrWriteDest = WideRegisterOperand::None,
                ._readDestOrWriteSrc = RegisterOperand::None
            };
        }

        MCycle MakeCycle(
            const MCycle::ALU& alu,
            const MCycle::IDU& idu,
            const MCycle::MemOp& memOp)
        {
            return {
                ._alu = alu,
                ._idu = idu,
                ._memOp = memOp
            };
        }

        void PopulateQuadrant00BasicIncDecInstructions()
        {
            auto MakeIncInstruction = [](RegisterOperand operand) -> Instruction
            {
                return {
                    .cycleCount = 1,
                    .cycles = {
                        MakeCycle(MakeALU(ALUOp::Inc, operand, operand), NoIDU(), NoMem())
                    }
                };
            };

            auto MakeDecInstruction = [](RegisterOperand operand) -> Instruction
            {
                return {
                    .cycleCount = 1,
                    .cycles = {
                        MakeCycle(MakeALU(ALUOp::Dec, operand, operand), NoIDU(), NoMem())
                    }
                };
            };


            INSTRUCTIONS[0x04] = MakeIncInstruction(RegisterOperand::RegB);
            INSTRUCTIONS[0x0C] = MakeIncInstruction(RegisterOperand::RegC);
            INSTRUCTIONS[0x14] = MakeIncInstruction(RegisterOperand::RegD);
            INSTRUCTIONS[0x1C] = MakeIncInstruction(RegisterOperand::RegE);
            INSTRUCTIONS[0x24] = MakeIncInstruction(RegisterOperand::RegH);
            INSTRUCTIONS[0x2C] = MakeIncInstruction(RegisterOperand::RegL);
            INSTRUCTIONS[0x3C] = MakeIncInstruction(RegisterOperand::RegA);

            INSTRUCTIONS[0x05] = MakeDecInstruction(RegisterOperand::RegB);
            INSTRUCTIONS[0x0D] = MakeDecInstruction(RegisterOperand::RegC);
            INSTRUCTIONS[0x15] = MakeDecInstruction(RegisterOperand::RegD);
            INSTRUCTIONS[0x1D] = MakeDecInstruction(RegisterOperand::RegE);
            INSTRUCTIONS[0x25] = MakeDecInstruction(RegisterOperand::RegH);
            INSTRUCTIONS[0x2D] = MakeDecInstruction(RegisterOperand::RegL);
            INSTRUCTIONS[0x3D] = MakeDecInstruction(RegisterOperand::RegA);
        }

        void PopulateQuadrant0016BitIncDecInstructions()
        {
            auto MakeIncInstruction = [](WideRegisterOperand operand) -> Instruction
            {
                return {
                    .cycleCount = 2,
                    .cycles = {
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, operand), NoMem()),
                        MakeCycle(NoALU(), NoIDU(), NoMem())                         // Wait cycle (Overlapping fetch cycle needs IDU for PC increment)
                    }
                };
            };

            auto MakeDecInstruction = [](WideRegisterOperand operand) -> Instruction
            {
                return {
                    .cycleCount = 1,
                    .cycles = {
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, operand), NoMem()),
                        MakeCycle(NoALU(), NoIDU(), NoMem())                         // Wait cycle (Overlapping fetch cycle needs IDU for PC increment)
                    }
                };
            };

            INSTRUCTIONS[0x03] = MakeIncInstruction(WideRegisterOperand::RegBC);
            INSTRUCTIONS[0x13] = MakeIncInstruction(WideRegisterOperand::RegDE);
            INSTRUCTIONS[0x23] = MakeIncInstruction(WideRegisterOperand::RegHL);
            INSTRUCTIONS[0x33] = MakeIncInstruction(WideRegisterOperand::RegSP);

            INSTRUCTIONS[0x0B] = MakeDecInstruction(WideRegisterOperand::RegBC);
            INSTRUCTIONS[0x1B] = MakeDecInstruction(WideRegisterOperand::RegDE);
            INSTRUCTIONS[0x2B] = MakeDecInstruction(WideRegisterOperand::RegHL);
            INSTRUCTIONS[0x3B] = MakeDecInstruction(WideRegisterOperand::RegSP);
        }

        void PopulateQuadrant00ImmediateLDInstructions()
        {
            auto MakeImmLDInstruction = [](RegisterOperand operand) -> Instruction
            {
                return {
                    .cycleCount = 2,
                    .cycles = {
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, WideRegisterOperand::RegPC), MakeMemRead(WideRegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                        MakeCycle(MakeALU(ALUOp::Nop, operand, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                    }
                };
            };

            INSTRUCTIONS[0x06] = MakeImmLDInstruction(RegisterOperand::RegB);
            INSTRUCTIONS[0x0E] = MakeImmLDInstruction(RegisterOperand::RegC);
            INSTRUCTIONS[0x16] = MakeImmLDInstruction(RegisterOperand::RegD);
            INSTRUCTIONS[0x1E] = MakeImmLDInstruction(RegisterOperand::RegE);
            INSTRUCTIONS[0x26] = MakeImmLDInstruction(RegisterOperand::RegH);
            INSTRUCTIONS[0x2E] = MakeImmLDInstruction(RegisterOperand::RegL);
            INSTRUCTIONS[0x3E] = MakeImmLDInstruction(RegisterOperand::RegA);
        }

        void PopulateQuadrant01BasicLDInstructions()
        {
            // LD instructions: |01|yyy|zzz|
            // yyy: Dest, zzz: Src
            for (uint8_t y = 0; y < 8; ++y)
            {
                if (y == 6) continue;               // Handle (HL) operand separately
                for (uint8_t z = 0; z < 8; ++z)
                {
                    if (z == 6) continue;           // Handle (HL) operand separately

                    uint8_t op = 0x40 | ((y & 0x07) << 3) | (z & 0x07);
                    INSTRUCTIONS[op] = { 
                        .cycleCount = 1, 
                        .cycles = { 
                            MakeCycle(MakeALU(ALUOp::Nop, OpCodeRegisterIndexToRegisterOperand(y), OpCodeRegisterIndexToRegisterOperand(z)), NoIDU(), NoMem())
                        }
                    };
                }
            }
        }

        void PopulateQuadrant01IndirectLDInstructions()
        {
            auto MakeIndLDInstruction = [](RegisterOperand operand) -> Instruction
            {
                return {
                    .cycleCount = 2,
                    .cycles = {
                        MakeCycle(NoALU(), NoIDU(), MakeMemRead(WideRegisterOperand::RegHL, RegisterOperand::TempRegZ)),
                        MakeCycle(MakeALU(ALUOp::Nop, operand, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                    }
                };
            };

            INSTRUCTIONS[0x46] = MakeIndLDInstruction(RegisterOperand::RegB);
            INSTRUCTIONS[0x4E] = MakeIndLDInstruction(RegisterOperand::RegC);
            INSTRUCTIONS[0x56] = MakeIndLDInstruction(RegisterOperand::RegD);
            INSTRUCTIONS[0x5E] = MakeIndLDInstruction(RegisterOperand::RegE);
            INSTRUCTIONS[0x66] = MakeIndLDInstruction(RegisterOperand::RegH);
            INSTRUCTIONS[0x6E] = MakeIndLDInstruction(RegisterOperand::RegL);
            INSTRUCTIONS[0x7E] = MakeIndLDInstruction(RegisterOperand::RegA);
        }

        void PopulateQuadrant01IndirectStoreLDInstructions()
        {
            auto MakeIndStoreLDInstruction = [](RegisterOperand operand) -> Instruction
            {
                return {
                    .cycleCount = 2,
                    .cycles = {
                        MakeCycle(NoALU(), NoIDU(), MakeMemWrite(operand, WideRegisterOperand::RegHL)),
                        MakeCycle(NoALU(), NoIDU(), NoMem())                                               // Wait cycle because fetch cycle needs address bus for opcode fetch
                    }
                };
            };

            INSTRUCTIONS[0x70] = MakeIndStoreLDInstruction(RegisterOperand::RegB);
            INSTRUCTIONS[0x71] = MakeIndStoreLDInstruction(RegisterOperand::RegC);
            INSTRUCTIONS[0x72] = MakeIndStoreLDInstruction(RegisterOperand::RegD);
            INSTRUCTIONS[0x73] = MakeIndStoreLDInstruction(RegisterOperand::RegE);
            INSTRUCTIONS[0x74] = MakeIndStoreLDInstruction(RegisterOperand::RegH);
            INSTRUCTIONS[0x75] = MakeIndStoreLDInstruction(RegisterOperand::RegL);
            INSTRUCTIONS[0x77] = MakeIndStoreLDInstruction(RegisterOperand::RegA);
        }

        void PopulateQuadrant10BasicALUInstructions()
        {
            // Basic ALU instructions |10|yyy|zzz|
            // yyy: ALU operation (except INC/DEC), zzz: Operand B

            for (uint8_t y = 0; y < 8; ++y)
            {
                for (uint8_t z = 0; z < 8; ++z)
                {
                    if (z == 6) continue;           // Handle (HL) operand separately

                    uint8_t op = 0x80 | ((y & 0x07) << 3) | (z & 0x07);
                    INSTRUCTIONS[op] = { 
                        .cycleCount = 1, 
                        .cycles = {
                            MakeCycle(MakeALU(ALUOp(y), RegisterOperand::RegA, OpCodeRegisterIndexToRegisterOperand(z)), NoIDU(), NoMem())
                        }
                    };
                }
            }
        }

        void PopulateQuadrant10IndirectALUInstructions()
        {
            auto MakeIndALUInstruction = [](ALUOp op) -> Instruction
            {
                return {
                    .cycleCount = 2,
                    .cycles = {
                        MakeCycle(NoALU(), NoIDU(), MakeMemRead(WideRegisterOperand::RegHL, RegisterOperand::TempRegZ)),
                        MakeCycle(MakeALU(op, RegisterOperand::RegA, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                    }
                };
            };

            INSTRUCTIONS[0x86] = MakeIndALUInstruction(ALUOp::Add);
            INSTRUCTIONS[0x8E] = MakeIndALUInstruction(ALUOp::Adc);
            INSTRUCTIONS[0x96] = MakeIndALUInstruction(ALUOp::Sub);
            INSTRUCTIONS[0x9E] = MakeIndALUInstruction(ALUOp::Sbc);
            INSTRUCTIONS[0xA6] = MakeIndALUInstruction(ALUOp::And);
            INSTRUCTIONS[0xAE] = MakeIndALUInstruction(ALUOp::Xor);
            INSTRUCTIONS[0xB6] = MakeIndALUInstruction(ALUOp::Or);
            INSTRUCTIONS[0xBE] = MakeIndALUInstruction(ALUOp::Cp);
        }

        void PopulateQuadrant11ImmALUInstructions()
        {
            auto MakeImmALUInstruction = [](ALUOp op) -> Instruction
            {
                return {
                    .cycleCount = 2,
                    .cycles = {
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, WideRegisterOperand::RegPC), MakeMemRead(WideRegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                        MakeCycle(MakeALU(op, RegisterOperand::RegA, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                    }
                };
            };

            INSTRUCTIONS[0xC6] = MakeImmALUInstruction(ALUOp::Add);
            INSTRUCTIONS[0xCE] = MakeImmALUInstruction(ALUOp::Adc);
            INSTRUCTIONS[0xD6] = MakeImmALUInstruction(ALUOp::Sub);
            INSTRUCTIONS[0xDE] = MakeImmALUInstruction(ALUOp::Sbc);
            INSTRUCTIONS[0xE6] = MakeImmALUInstruction(ALUOp::And);
            INSTRUCTIONS[0xEE] = MakeImmALUInstruction(ALUOp::Xor);
            INSTRUCTIONS[0xF6] = MakeImmALUInstruction(ALUOp::Or);
            INSTRUCTIONS[0xFE] = MakeImmALUInstruction(ALUOp::Cp);
        }

        void PopulateInstructions()
        {
            // NOP
            INSTRUCTIONS[0x00] = {
                .cycleCount = 1,
                .cycles = {
                    MakeCycle(NoALU(), NoIDU(), NoMem())
                }
            };

            PopulateQuadrant00BasicIncDecInstructions();
            PopulateQuadrant00ImmediateLDInstructions();
            PopulateQuadrant0016BitIncDecInstructions();
            PopulateQuadrant01BasicLDInstructions();
            PopulateQuadrant01IndirectLDInstructions();
            PopulateQuadrant01IndirectStoreLDInstructions();
            PopulateQuadrant10BasicALUInstructions();
            PopulateQuadrant10IndirectALUInstructions();
            PopulateQuadrant11ImmALUInstructions();
        }

        struct OpCodeStaticInit
        {
            OpCodeStaticInit()
            {
                PopulateInstructions();
            }

        } OPCODE_STATIC_INIT;

        const MCycle FETCH_MCYCLE = MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, WideRegisterOperand::RegPC), MakeMemRead(WideRegisterOperand::RegPC, RegisterOperand::RegIR));
    }

    const MCycle& GetFetchMCycle() { return FETCH_MCYCLE; };

    uint8_t GetMCycleCount(uint8_t opCode)
    {
        return INSTRUCTIONS[opCode].cycleCount;
    }

    const MCycle& GetMCycle(uint8_t opCode, uint8_t mCycleIndex)
    {
        const Instruction& i = INSTRUCTIONS[opCode];
        EMU_ASSERT("MCycle index out of bounds" && mCycleIndex < i.cycleCount);

        return i.cycles[mCycleIndex];
    }
}