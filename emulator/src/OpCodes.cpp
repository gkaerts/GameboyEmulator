#include "OpCodes.hpp"

#include <array>

namespace emu::SM83
{
    namespace
    {

        const char* OPCODE_NAMES[] =
        {
            #include "OpCodeNames.hpp"
        };
        static_assert(sizeof(OPCODE_NAMES) / sizeof(OPCODE_NAMES[0]) == 256);

        constexpr const uint32_t MAX_MCYCLE_COUNT = 6;
        struct Instruction
        {
            uint32_t _cycleCount;
            std::array<MCycle, MAX_MCYCLE_COUNT> _cycles;
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

        inline RegisterOperand WideRegisterLSB(RegisterOperand reg)
        {
            return RegisterOperand((uint8_t(reg) - uint8_t(RegisterOperand::WideRegisterStart)) * 2 + 0);
        }

        inline RegisterOperand WideRegisterMSB(RegisterOperand reg)
        {
            return RegisterOperand((uint8_t(reg) - uint8_t(RegisterOperand::WideRegisterStart)) * 2 + 1);
        }

        MCycle::ALU MakeALU(
            ALUOp op,
            RegisterOperand operandA,
            RegisterOperand operandB)
        {
            return {
                ._op = op,
                ._operandA = operandA,
                ._operandB = operandB,
                ._dest = operandA
            };
        }

        MCycle::ALU MakeALUExt(
            ALUOp op,
            RegisterOperand operandA,
            RegisterOperand operandB,
            RegisterOperand dest)
        {
            return {
                ._op = op,
                ._operandA = operandA,
                ._operandB = operandB,
                ._dest = dest
            };
        }

        MCycle::ALU NoALU()
        {
            return {
                ._op = ALUOp::Nop,
                ._operandA = RegisterOperand::None,
                ._operandB = RegisterOperand::None,
                ._dest = RegisterOperand::None
            };
        }

        MCycle::IDU MakeIDU(
            IDUOp op,
            RegisterOperand operand)
        {
            return {
                ._op = op,
                ._operand = operand,
                ._dest = operand
            };
        }

        MCycle::IDU MakeIDUExt(
            IDUOp op,
            RegisterOperand operand,
            RegisterOperand dest)
        {
            return {
                ._op = op,
                ._operand = operand,
                ._dest = dest
            };
        }

        MCycle::IDU NoIDU()
        {
            return {
                ._op = IDUOp::Nop,
                ._operand = RegisterOperand::None,
                ._dest = RegisterOperand::None
            };
        }

        MCycle::MemOp MakeMemRead(
            RegisterOperand addressSrc,
            RegisterOperand dest)
        {
            return {
                ._flags = MCycle::MemOp::MOF_Active,
                ._reg = dest,
                ._addressSrc = addressSrc
            };
        }

        MCycle::MemOp MakeMemWrite(
            RegisterOperand src,
            RegisterOperand addressSrc)
        {
            return {
                ._flags = MCycle::MemOp::MOF_Active | MCycle::MemOp::MOF_IsMemWrite,
                ._reg = src,
                ._addressSrc = addressSrc
            };
        }

        MCycle::MemOp MakeMemReadWithOffset(
            RegisterOperand addressSrc,
            RegisterOperand dest)
        {
            return {
                ._flags = MCycle::MemOp::MOF_Active | MCycle::MemOp::MOF_UseOffsetAddress,
                ._reg = dest,
                ._addressSrcBeforeOffset = addressSrc
            };
        }

        MCycle::MemOp MakeMemWriteWithOffset(
            RegisterOperand src,
            RegisterOperand addressDest)
        {
            return {
                ._flags = MCycle::MemOp::MOF_Active | MCycle::MemOp::MOF_IsMemWrite | MCycle::MemOp::MOF_UseOffsetAddress,
                ._reg = src,
                ._addressSrcBeforeOffset = addressDest
            };
        }

        MCycle::MemOp NoMem()
        {
            return {
                ._flags = 0,
            };
        }

        MCycle::Misc MakeMisc(uint8_t miscFlags, RegisterOperand operand = RegisterOperand::None, uint8_t optValue = 0)
        {
            return {
                ._flags = miscFlags,
                ._operand = operand,
                ._optValue = optValue
            };
        }

        MCycle::Misc NoMisc()
        {
            return {
                ._flags = MCycle::Misc::MF_None,
                ._operand = RegisterOperand::None
            };
        }

        MCycle MakeCycle(
            const MCycle::ALU& alu,
            const MCycle::IDU& idu,
            const MCycle::MemOp& memOp,
            const MCycle::Misc& misc = NoMisc())
        {
            return {
                ._alu = alu,
                ._idu = idu,
                ._memOp = memOp,
                ._misc = misc
            };
        }

        Instruction MakeInstruction(std::initializer_list<MCycle> cycles)
        {
            EMU_ASSERT(cycles.size() <= MAX_MCYCLE_COUNT);
            Instruction instruction =
            {
                ._cycleCount = uint32_t(cycles.size()),
            };

            std::memcpy(instruction._cycles.data(), cycles.begin(), sizeof(MCycle) * std::min(MAX_MCYCLE_COUNT, instruction._cycleCount));
            return instruction;
        }

        void PopulateQuadrant00BasicIncDecInstructions()
        {
            auto MakeIncInstruction = [](RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                        MakeCycle(MakeALU(ALUOp::Inc, operand, operand), NoIDU(), NoMem())
                    });
            };

            auto MakeDecInstruction = [](RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                        MakeCycle(MakeALU(ALUOp::Dec, operand, operand), NoIDU(), NoMem())
                    });
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
            auto MakeIncInstruction = [](RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, operand), NoMem()),
                        MakeCycle(NoALU(), NoIDU(), NoMem())                         // Wait cycle (Overlapping fetch cycle needs IDU for PC increment)
                    });
            };

            auto MakeDecInstruction = [](RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, operand), NoMem()),
                        MakeCycle(NoALU(), NoIDU(), NoMem())                         // Wait cycle (Overlapping fetch cycle needs IDU for PC increment)
                    });
            };

            INSTRUCTIONS[0x03] = MakeIncInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0x13] = MakeIncInstruction(RegisterOperand::RegDE);
            INSTRUCTIONS[0x23] = MakeIncInstruction(RegisterOperand::RegHL);
            INSTRUCTIONS[0x33] = MakeIncInstruction(RegisterOperand::RegSP);

            INSTRUCTIONS[0x0B] = MakeDecInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0x1B] = MakeDecInstruction(RegisterOperand::RegDE);
            INSTRUCTIONS[0x2B] = MakeDecInstruction(RegisterOperand::RegHL);
            INSTRUCTIONS[0x3B] = MakeDecInstruction(RegisterOperand::RegSP);
        }

        void PopulateQuadrant00ImmediateLDInstructions()
        {
            auto MakeImmLDInstruction = [](RegisterOperand operand) -> Instruction
            {
               return MakeInstruction({
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                        MakeCycle(MakeALU(ALUOp::Nop, operand, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                    });
            };

            INSTRUCTIONS[0x06] = MakeImmLDInstruction(RegisterOperand::RegB);
            INSTRUCTIONS[0x0E] = MakeImmLDInstruction(RegisterOperand::RegC);
            INSTRUCTIONS[0x16] = MakeImmLDInstruction(RegisterOperand::RegD);
            INSTRUCTIONS[0x1E] = MakeImmLDInstruction(RegisterOperand::RegE);
            INSTRUCTIONS[0x26] = MakeImmLDInstruction(RegisterOperand::RegH);
            INSTRUCTIONS[0x2E] = MakeImmLDInstruction(RegisterOperand::RegL);
            INSTRUCTIONS[0x3E] = MakeImmLDInstruction(RegisterOperand::RegA);
        }

        void PopulateQuadrant0016BitImmediateLDInstructions()
        {
            auto MakeImmLDInstruction = [](RegisterOperand operand) -> Instruction
            {
               return MakeInstruction({
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW)),
                        MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, operand))
                    });
            };

            INSTRUCTIONS[0x01] = MakeImmLDInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0x11] = MakeImmLDInstruction(RegisterOperand::RegDE);
            INSTRUCTIONS[0x21] = MakeImmLDInstruction(RegisterOperand::RegHL);
            INSTRUCTIONS[0x31] = MakeImmLDInstruction(RegisterOperand::RegSP);
        }

        void PopulateQuadrant0016BitALUInstructions()
        {
            auto Make16BitAddInstruction = [](RegisterOperand operandA, RegisterOperand operandB) -> Instruction
            {
                return MakeInstruction({
                    MakeCycle(MakeALU(ALUOp::AddKeepZ, WideRegisterLSB(operandA), WideRegisterLSB(operandB)), NoIDU(), NoMem()),
                    MakeCycle(MakeALU(ALUOp::AdcKeepZ, WideRegisterMSB(operandA), WideRegisterMSB(operandB)), NoIDU(), NoMem())
                });
            };

            INSTRUCTIONS[0x09] = Make16BitAddInstruction(RegisterOperand::RegHL, RegisterOperand::RegBC);
            INSTRUCTIONS[0x19] = Make16BitAddInstruction(RegisterOperand::RegHL, RegisterOperand::RegDE);
            INSTRUCTIONS[0x29] = Make16BitAddInstruction(RegisterOperand::RegHL, RegisterOperand::RegHL);
            INSTRUCTIONS[0x39] = Make16BitAddInstruction(RegisterOperand::RegHL, RegisterOperand::RegSP);
        }

        void PopulateQuadrant00IndirectStoreLDInstructions()
        {
            auto MakeIndStoreLDInstruction = [](RegisterOperand src, RegisterOperand dest) -> Instruction
            {
               return MakeInstruction({
                        MakeCycle(NoALU(), NoIDU(), MakeMemWrite(src, dest)),
                        MakeCycle(NoALU(), NoIDU(), NoMem())                                               // Wait cycle because fetch cycle needs address bus for opcode fetch
                    });
            };

            auto MakeIndStoreLDInstructionWithIncDec = [](RegisterOperand src, RegisterOperand dest, IDUOp op) -> Instruction
            {
               return MakeInstruction({
                        MakeCycle(NoALU(), MakeIDU(op, dest), MakeMemWrite(src, dest)),
                        MakeCycle(NoALU(), NoIDU(), NoMem())                                               // Wait cycle because fetch cycle needs address bus for opcode fetch
                    });
            };

            INSTRUCTIONS[0x02] = MakeIndStoreLDInstruction(RegisterOperand::RegA, RegisterOperand::RegBC);
            INSTRUCTIONS[0x12] = MakeIndStoreLDInstruction(RegisterOperand::RegA, RegisterOperand::RegDE);
            INSTRUCTIONS[0x22] = MakeIndStoreLDInstructionWithIncDec(RegisterOperand::RegA, RegisterOperand::RegHL, IDUOp::Inc);
            INSTRUCTIONS[0x32] = MakeIndStoreLDInstructionWithIncDec(RegisterOperand::RegA, RegisterOperand::RegHL, IDUOp::Dec);
        }

        void PopulateQuadrant00IndirectLDInstructions()
        {
            auto MakeIndirectLDInstruction = [](RegisterOperand src, RegisterOperand dest) -> Instruction
            {
                return MakeInstruction({
                    MakeCycle(NoALU(), NoIDU(), MakeMemRead(src, RegisterOperand::TempRegZ)),
                    MakeCycle(MakeALU(ALUOp::Nop, dest, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                });
            };

            auto MakeIndirectLDInstructionWithIncDec = [](RegisterOperand src, RegisterOperand dest, IDUOp op) -> Instruction
            {
                return MakeInstruction({
                    MakeCycle(NoALU(), MakeIDU(op, src), MakeMemRead(src, RegisterOperand::TempRegZ)),
                    MakeCycle(MakeALU(ALUOp::Nop, dest, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                });
            };

            INSTRUCTIONS[0x0A] = MakeIndirectLDInstruction(RegisterOperand::RegBC, RegisterOperand::RegA);
            INSTRUCTIONS[0x1A] = MakeIndirectLDInstruction(RegisterOperand::RegDE, RegisterOperand::RegA);
            INSTRUCTIONS[0x2A] = MakeIndirectLDInstructionWithIncDec(RegisterOperand::RegHL, RegisterOperand::RegA, IDUOp::Inc);
            INSTRUCTIONS[0x3A] = MakeIndirectLDInstructionWithIncDec(RegisterOperand::RegHL, RegisterOperand::RegA, IDUOp::Dec);
        }

        void PopulateQuadrant00IndirectIncDecInstructions()
        {
            auto MakeIndirectIncDecInstruction = [](ALUOp op, RegisterOperand dest) -> Instruction
            {
                return MakeInstruction({
                    MakeCycle(NoALU(), NoIDU(), MakeMemRead(dest, RegisterOperand::TempRegZ)),
                    MakeCycle(MakeALU(op, RegisterOperand::TempRegZ, RegisterOperand::TempRegZ), NoIDU(), MakeMemWrite(RegisterOperand::TempRegZ, dest)),
                    MakeCycle(NoALU(), NoIDU(), NoMem())
                });
            };

            INSTRUCTIONS[0x34] = MakeIndirectIncDecInstruction(ALUOp::Inc, RegisterOperand::RegHL);
            INSTRUCTIONS[0x35] = MakeIndirectIncDecInstruction(ALUOp::Dec, RegisterOperand::RegHL);
        }

        void PopulateQuadrant00BitwiseInstructions()
        {
            auto MakeALUInstruction = [](ALUOp op, RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                    MakeCycle(MakeALU(op, operand, operand), NoIDU(), NoMem())
                });
            };

            INSTRUCTIONS[0x07] = MakeALUInstruction(ALUOp::Rlc, RegisterOperand::RegA);
            INSTRUCTIONS[0x17] = MakeALUInstruction(ALUOp::Rl, RegisterOperand::RegA);
            INSTRUCTIONS[0x0F] = MakeALUInstruction(ALUOp::Rrc, RegisterOperand::RegA);
            INSTRUCTIONS[0x1F] = MakeALUInstruction(ALUOp::Rr, RegisterOperand::RegA);
        }

        void PopulateQuadrant00MiscInstructions()
        {
            // STOP
            INSTRUCTIONS[0x10] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_StopExecution))
            });

            // LD (HL), d8
            INSTRUCTIONS[0x36] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), NoIDU(), MakeMemWrite(RegisterOperand::TempRegZ, RegisterOperand::RegHL)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // LD (a16), SP
            INSTRUCTIONS[0x08] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegWZ), MakeMemWrite(RegisterOperand::RegSPL, RegisterOperand::RegWZ)),
                MakeCycle(NoALU(), NoIDU(), MakeMemWrite(RegisterOperand::RegSPH, RegisterOperand::RegWZ)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // DAA
            INSTRUCTIONS[0x27] = MakeInstruction({
                MakeCycle(MakeALU(ALUOp::Da, RegisterOperand::RegA, RegisterOperand::RegA), NoIDU(), NoMem())
            });

            // SCF
            INSTRUCTIONS[0x37] = MakeInstruction({
                MakeCycle(MakeALU(ALUOp::Scf, RegisterOperand::RegA, RegisterOperand::RegA), NoIDU(), NoMem())
            });

            // CPL
            INSTRUCTIONS[0x2F] = MakeInstruction({
                MakeCycle(MakeALU(ALUOp::Cpl, RegisterOperand::RegA, RegisterOperand::RegA), NoIDU(), NoMem())
            });

            // CCF
            INSTRUCTIONS[0x3F] = MakeInstruction({
                MakeCycle(MakeALU(ALUOp::Ccf, RegisterOperand::RegA, RegisterOperand::RegA), NoIDU(), NoMem())
            });

            // JR r8
            INSTRUCTIONS[0x18] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(MakeALU(ALUOp::Add, RegisterOperand::TempRegZ, RegisterOperand::RegPCL), MakeIDUExt(IDUOp::Adjust, RegisterOperand::RegPCH, RegisterOperand::TempRegW), NoMem(), MakeMisc(MCycle::Misc::MF_ALUKeepFlags)),
                MakeCycle(NoALU(), MakeIDUExt(IDUOp::Inc, RegisterOperand::RegWZ, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegWZ, RegisterOperand::RegIR))

            });
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
                    INSTRUCTIONS[op] = MakeInstruction({
                            MakeCycle(MakeALU(ALUOp::Nop, OpCodeRegisterIndexToRegisterOperand(y), OpCodeRegisterIndexToRegisterOperand(z)), NoIDU(), NoMem())
                        });
                }
            }
        }

        void PopulateQuadrant01IndirectLDInstructions()
        {
            auto MakeIndLDInstruction = [](RegisterOperand operand) -> Instruction
            {
               return MakeInstruction({
                        MakeCycle(NoALU(), NoIDU(), MakeMemRead(RegisterOperand::RegHL, RegisterOperand::TempRegZ)),
                        MakeCycle(MakeALU(ALUOp::Nop, operand, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                    });
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
               return MakeInstruction({
                        MakeCycle(NoALU(), NoIDU(), MakeMemWrite(operand, RegisterOperand::RegHL)),
                        MakeCycle(NoALU(), NoIDU(), NoMem())                                               // Wait cycle because fetch cycle needs address bus for opcode fetch
                    });
            };

            INSTRUCTIONS[0x70] = MakeIndStoreLDInstruction(RegisterOperand::RegB);
            INSTRUCTIONS[0x71] = MakeIndStoreLDInstruction(RegisterOperand::RegC);
            INSTRUCTIONS[0x72] = MakeIndStoreLDInstruction(RegisterOperand::RegD);
            INSTRUCTIONS[0x73] = MakeIndStoreLDInstruction(RegisterOperand::RegE);
            INSTRUCTIONS[0x74] = MakeIndStoreLDInstruction(RegisterOperand::RegH);
            INSTRUCTIONS[0x75] = MakeIndStoreLDInstruction(RegisterOperand::RegL);
            INSTRUCTIONS[0x77] = MakeIndStoreLDInstruction(RegisterOperand::RegA);
        }

        void PopulateQuadrant01MiscInstructions()
        {
            // HALT
            INSTRUCTIONS[0x76] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_HaltExecution))
            });
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
                    INSTRUCTIONS[op] = MakeInstruction({
                            MakeCycle(MakeALU(ALUOp(y), RegisterOperand::RegA, OpCodeRegisterIndexToRegisterOperand(z)), NoIDU(), NoMem())
                        });
                }
            }
        }

        void PopulateQuadrant10IndirectALUInstructions()
        {
            auto MakeIndALUInstruction = [](ALUOp op) -> Instruction
            {
                return MakeInstruction({
                        MakeCycle(NoALU(), NoIDU(), MakeMemRead(RegisterOperand::RegHL, RegisterOperand::TempRegZ)),
                        MakeCycle(MakeALU(op, RegisterOperand::RegA, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                    });
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
                return MakeInstruction({
                        MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                        MakeCycle(MakeALU(op, RegisterOperand::RegA, RegisterOperand::TempRegZ), NoIDU(), NoMem())
                    });
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

        void PopulateQuadrant11PushPopInstructions()
        {
            auto MakePushInstruction = [](RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), NoMem()),
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), MakeMemWrite(WideRegisterMSB(operand), RegisterOperand::RegSP)),
                    MakeCycle(NoALU(), NoIDU(), MakeMemWrite(WideRegisterLSB(operand), RegisterOperand::RegSP)),
                    MakeCycle(NoALU(), NoIDU(), NoMem())
                });
            };

            auto MakePopInstruction = [](RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegZ)),
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegW)),
                    MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, operand))
                });
            };

            INSTRUCTIONS[0xC1] = MakePopInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0xD1] = MakePopInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0xE1] = MakePopInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0xF1] = MakePopInstruction(RegisterOperand::RegAF);

            INSTRUCTIONS[0xC5] = MakePushInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0xD5] = MakePushInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0xE5] = MakePushInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0xF5] = MakePushInstruction(RegisterOperand::RegAF);
        }

        void PopulateQuadrant11MiscInstructions()
        {
            // LD (C), A
            INSTRUCTIONS[0xE2] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), MakeMemWriteWithOffset(RegisterOperand::RegA, RegisterOperand::RegC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // LD A, (C)
            INSTRUCTIONS[0xF2] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), MakeMemReadWithOffset(RegisterOperand::RegC, RegisterOperand::RegA)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // LDH (a8), A
            INSTRUCTIONS[0xE0] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), NoIDU(), MakeMemWriteWithOffset(RegisterOperand::RegA, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // LD A, (a8)
            INSTRUCTIONS[0xF0] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), NoIDU(), MakeMemReadWithOffset(RegisterOperand::TempRegZ, RegisterOperand::RegA)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // ADD SP, e
            INSTRUCTIONS[0xE8] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(MakeALU(ALUOp::Add, RegisterOperand::TempRegZ, RegisterOperand::RegSPL), NoIDU(), NoMem()),
                MakeCycle(MakeALU(ALUOp::Adjust, RegisterOperand::TempRegW, RegisterOperand::RegSPH), NoIDU(), NoMem()),
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegSP))
            });

            // JP HL (note the custom opcode fetch!)
            INSTRUCTIONS[0xE9] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDUExt(IDUOp::Inc, RegisterOperand::RegHL, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegHL, RegisterOperand::RegIR))
            });

            // LD HL, SP+e
            INSTRUCTIONS[0xF8] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(MakeALUExt(ALUOp::Add, RegisterOperand::TempRegZ, RegisterOperand::RegSPL, RegisterOperand::RegL), NoIDU(), NoMem()),
                MakeCycle(MakeALU(ALUOp::Adjust, RegisterOperand::RegH, RegisterOperand::RegSPH), NoIDU(), NoMem()),
            });

            // LD SP, HL
            INSTRUCTIONS[0xF9] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDUExt(IDUOp::Nop, RegisterOperand::RegHL, RegisterOperand::RegSP), NoMem()),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // LD (a16), A
            INSTRUCTIONS[0xEA] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), NoIDU(), MakeMemWrite(RegisterOperand::RegA, RegisterOperand::RegWZ)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // LD (a16), A
            INSTRUCTIONS[0xFA] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), NoIDU(), MakeMemRead(RegisterOperand::RegWZ, RegisterOperand::TempRegZ)),
                MakeCycle(MakeALU(ALUOp::Nop, RegisterOperand::RegA, RegisterOperand::TempRegZ), NoIDU(), NoMem())
            });

            // JP a16
            INSTRUCTIONS[0xC3] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            auto MakeRSTInstructions = [](uint8_t valueToWrite) -> Instruction
            {
                return MakeInstruction({
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), NoMem()),
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCH, RegisterOperand::RegSP)),
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Nop, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCL, RegisterOperand::RegSP), MakeMisc(MCycle::Misc::MF_WriteValueToWideRegister, RegisterOperand::RegPC, valueToWrite)),
                    MakeCycle(NoALU(), NoIDU(), NoMem())
                });
            };

            INSTRUCTIONS[0xC7] = MakeRSTInstructions(0x00);
            INSTRUCTIONS[0xD7] = MakeRSTInstructions(0x10);
            INSTRUCTIONS[0xE7] = MakeRSTInstructions(0x20);
            INSTRUCTIONS[0xF7] = MakeRSTInstructions(0x30);

            INSTRUCTIONS[0xCF] = MakeRSTInstructions(0x08);
            INSTRUCTIONS[0xDF] = MakeRSTInstructions(0x18);
            INSTRUCTIONS[0xEF] = MakeRSTInstructions(0x28);
            INSTRUCTIONS[0xFF] = MakeRSTInstructions(0x38);


            // RET
            INSTRUCTIONS[0xC9] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // RETI
            INSTRUCTIONS[0xD9] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister | MCycle::Misc::MF_EnableInterrupts, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // DI
            INSTRUCTIONS[0xF3] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_DisableInterrupts))
            });

            // EI
            INSTRUCTIONS[0xFB] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_EnableInterrupts))
            });

            // CALL a16
            INSTRUCTIONS[0xCD] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), NoMem()),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCH, RegisterOperand::RegSP)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Nop, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCL, RegisterOperand::RegSP), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

        }

        void PopulateInstructions()
        {
            // NOP
            INSTRUCTIONS[0x00] = MakeInstruction({
                    MakeCycle(NoALU(), NoIDU(), NoMem())
                });

            PopulateQuadrant00BasicIncDecInstructions();
            PopulateQuadrant00ImmediateLDInstructions();
            PopulateQuadrant0016BitIncDecInstructions();
            PopulateQuadrant0016BitImmediateLDInstructions();
            PopulateQuadrant0016BitALUInstructions();
            PopulateQuadrant00IndirectStoreLDInstructions();
            PopulateQuadrant00IndirectLDInstructions();
            PopulateQuadrant00IndirectIncDecInstructions();
            PopulateQuadrant00BitwiseInstructions();
            PopulateQuadrant00MiscInstructions();

            PopulateQuadrant01BasicLDInstructions();
            PopulateQuadrant01IndirectLDInstructions();
            PopulateQuadrant01IndirectStoreLDInstructions();
            PopulateQuadrant01MiscInstructions();

            PopulateQuadrant10BasicALUInstructions();
            PopulateQuadrant10IndirectALUInstructions();

            PopulateQuadrant11ImmALUInstructions();
            PopulateQuadrant11PushPopInstructions();
            PopulateQuadrant11MiscInstructions();
        }

        struct OpCodeStaticInit
        {
            OpCodeStaticInit()
            {
                PopulateInstructions();
            }

        } OPCODE_STATIC_INIT;

        const MCycle FETCH_MCYCLE = MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::RegIR));
    }

    const MCycle& GetFetchMCycle() { return FETCH_MCYCLE; };

    uint8_t GetMCycleCount(uint8_t opCode)
    {
        return INSTRUCTIONS[opCode]._cycleCount;
    }

    const MCycle& GetMCycle(uint8_t opCode, uint8_t mCycleIndex)
    {
        const Instruction& i = INSTRUCTIONS[opCode];
        EMU_ASSERT("MCycle index out of bounds" && mCycleIndex < i._cycleCount);

        return i._cycles[mCycleIndex];
    }

    const char* GetOpcodeName(uint8_t opCode)
    {
        return OPCODE_NAMES[opCode];
    }
}