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

        const char* PREFIX_OPCODE_NAMES[] =
        {
            #include "PrefixOpCodeNames.hpp"
        };
        static_assert(sizeof(PREFIX_OPCODE_NAMES) / sizeof(PREFIX_OPCODE_NAMES[0]) == 256);

        const char** OPCODE_NAME_TABLES[] =
        {
            OPCODE_NAMES,
            PREFIX_OPCODE_NAMES
        };

        constexpr const uint32_t MAX_MCYCLE_COUNT = 8;
        struct Instruction
        {
            uint32_t _cycleCount;
            std::array<MCycle, MAX_MCYCLE_COUNT> _cycles;
        };

        Instruction INSTRUCTIONS[0x100] = {};
        Instruction PREFIX_INSTRUCTIONS[0x100] = {};
        Instruction INTERRUPT_INSTRUCTIONS[0x100] = {};

        const Instruction* INSTRUCTION_TABLES[] = 
        {
            INSTRUCTIONS,
            PREFIX_INSTRUCTIONS,
            INTERRUPT_INSTRUCTIONS
        };

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

        MCycle::Misc MakeMisc(uint16_t miscFlags, RegisterOperand operand = RegisterOperand::None, uint16_t optValue = 0)
        {
            return {
                ._flags = miscFlags,
                ._operand = operand,
                ._optValue = optValue
            };
        }

        MCycle::Misc MakeMiscCheckC(uint8_t cycleIndexToJumpTo)
        {
            return {
                ._flags = MCycle::Misc::MF_ConditionCheckC,
                ._operand = RegisterOperand::None,
                ._optValue = cycleIndexToJumpTo
            };
        }

        MCycle::Misc MakeMiscCheckNC(uint8_t cycleIndexToJumpTo)
        {
            return {
                ._flags = MCycle::Misc::MF_ConditionCheckNC,
                ._operand = RegisterOperand::None,
                ._optValue = cycleIndexToJumpTo
            };
        }

        MCycle::Misc MakeMiscCheckZ(uint8_t cycleIndexToJumpTo)
        {
            return {
                ._flags = MCycle::Misc::MF_ConditionCheckZ,
                ._operand = RegisterOperand::None,
                ._optValue = cycleIndexToJumpTo
            };
        }

        MCycle::Misc MakeMiscCheckNZ(uint8_t cycleIndexToJumpTo)
        {
            return {
                ._flags = MCycle::Misc::MF_ConditionCheckNZ,
                ._operand = RegisterOperand::None,
                ._optValue = cycleIndexToJumpTo
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

        const MCycle FETCH_MCYCLE = MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::RegIR));

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
                    MakeCycle(MakeALU(op, operand, operand), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_ALUClearZero))
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

            // JR NZ, r8
            INSTRUCTIONS[0x20] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ), MakeMiscCheckNZ(2)),

                // NZ false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // NZ true
                MakeCycle(MakeALU(ALUOp::Add, RegisterOperand::TempRegZ, RegisterOperand::RegPCL), MakeIDUExt(IDUOp::Adjust, RegisterOperand::RegPCH, RegisterOperand::TempRegW), NoMem(), MakeMisc(MCycle::Misc::MF_ALUKeepFlags)),
                MakeCycle(NoALU(), MakeIDUExt(IDUOp::Inc, RegisterOperand::RegWZ, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegWZ, RegisterOperand::RegIR))
            });

            // JR Z, r8
            INSTRUCTIONS[0x28] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ), MakeMiscCheckZ(2)),

                // Z false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // Z true
                MakeCycle(MakeALU(ALUOp::Add, RegisterOperand::TempRegZ, RegisterOperand::RegPCL), MakeIDUExt(IDUOp::Adjust, RegisterOperand::RegPCH, RegisterOperand::TempRegW), NoMem(), MakeMisc(MCycle::Misc::MF_ALUKeepFlags)),
                MakeCycle(NoALU(), MakeIDUExt(IDUOp::Inc, RegisterOperand::RegWZ, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegWZ, RegisterOperand::RegIR))
            });

            // JR NC, r8
            INSTRUCTIONS[0x30] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ), MakeMiscCheckNC(2)),

                // NC false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // NC true
                MakeCycle(MakeALU(ALUOp::Add, RegisterOperand::TempRegZ, RegisterOperand::RegPCL), MakeIDUExt(IDUOp::Adjust, RegisterOperand::RegPCH, RegisterOperand::TempRegW), NoMem(), MakeMisc(MCycle::Misc::MF_ALUKeepFlags)),
                MakeCycle(NoALU(), MakeIDUExt(IDUOp::Inc, RegisterOperand::RegWZ, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegWZ, RegisterOperand::RegIR))
            });

            // JR C, r8
            INSTRUCTIONS[0x38] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ), MakeMiscCheckC(2)),

                // C false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // C true
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
                MakeCycle(NoALU(), MakeIDU(IDUOp::Nop, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::RegIR), MakeMisc(MCycle::Misc::MF_HaltExecution))
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
            INSTRUCTIONS[0xD1] = MakePopInstruction(RegisterOperand::RegDE);
            INSTRUCTIONS[0xE1] = MakePopInstruction(RegisterOperand::RegHL);
            INSTRUCTIONS[0xF1] = MakePopInstruction(RegisterOperand::RegAF);

            INSTRUCTIONS[0xC5] = MakePushInstruction(RegisterOperand::RegBC);
            INSTRUCTIONS[0xD5] = MakePushInstruction(RegisterOperand::RegDE);
            INSTRUCTIONS[0xE5] = MakePushInstruction(RegisterOperand::RegHL);
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

            // JP NZ, a16
            INSTRUCTIONS[0xC2] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW), MakeMiscCheckNZ(3)),

                // NZ false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // NZ true
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // JP Z, a16
            INSTRUCTIONS[0xCA] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW), MakeMiscCheckZ(3)),

                // Z false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // Z true
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // JP NC, a16
            INSTRUCTIONS[0xD2] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW), MakeMiscCheckNC(3)),

                // NC false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // NC true
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // JP Z, a16
            INSTRUCTIONS[0xDA] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW), MakeMiscCheckC(3)),

                // C false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // C true
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // RET NZ
            INSTRUCTIONS[0xC0] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMiscCheckNZ(2)),
                
                // NZ false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // NZ true
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem()),
            });

            // RET Z
            INSTRUCTIONS[0xC8] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMiscCheckZ(2)),
                
                // Z false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // Z true
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem()),
            });

            // RET NC
            INSTRUCTIONS[0xD0] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMiscCheckNC(2)),
                
                // NC false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // NC true
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem()),
            });

            // RET C
            INSTRUCTIONS[0xD8] = MakeInstruction({
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMiscCheckC(2)),
                
                // C false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // C true
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegSP), MakeMemRead(RegisterOperand::RegSP, RegisterOperand::TempRegW)),
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem()),
            });

            // CALL NZ, a16
            INSTRUCTIONS[0xC4] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW), MakeMiscCheckNZ(3)),

                // false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // true
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), NoMem()),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCH, RegisterOperand::RegSP)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Nop, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCL, RegisterOperand::RegSP), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // CALL Z, a16
            INSTRUCTIONS[0xCC] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW), MakeMiscCheckZ(3)),

                // false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // true
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), NoMem()),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCH, RegisterOperand::RegSP)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Nop, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCL, RegisterOperand::RegSP), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // CALL NC, a16
            INSTRUCTIONS[0xD4] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW), MakeMiscCheckNC(3)),

                // false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // true
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), NoMem()),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCH, RegisterOperand::RegSP)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Nop, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCL, RegisterOperand::RegSP), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // CALL C, a16
            INSTRUCTIONS[0xDC] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegZ)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::TempRegW), MakeMiscCheckC(3)),

                // false
                MakeCycle(NoALU(), NoIDU(), NoMem(), MakeMisc(MCycle::Misc::MF_LastCycle)),

                // true
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), NoMem()),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCH, RegisterOperand::RegSP)),
                MakeCycle(NoALU(), MakeIDU(IDUOp::Nop, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCL, RegisterOperand::RegSP), MakeMisc(MCycle::Misc::MF_WriteWZToWideRegister, RegisterOperand::RegPC)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });

            // PREFIX CB
            // Implement as a fetch cycle + a dummy cycle which will be overwritten by the next MCycle from the prefix table
            INSTRUCTIONS[0xCB] = MakeInstruction({
                MakeCycle(NoALU(), MakeIDU(IDUOp::Inc, RegisterOperand::RegPC), MakeMemRead(RegisterOperand::RegPC, RegisterOperand::RegIR), MakeMisc(MCycle::Misc::MF_PrefixCB)),
                MakeCycle(NoALU(), NoIDU(), NoMem())
            });
        }

        void PopulatePrefixCBInstructions()
        {
            auto MakePrefixCBALUInstruction = [](ALUOp op, RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                    FETCH_MCYCLE,
                    MakeCycle(MakeALU(op, operand, operand), NoIDU(), NoMem())
                });
            };

            auto MakePrefixCBIndirectALUInstruction = [](ALUOp op, RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                    FETCH_MCYCLE,
                    MakeCycle(NoALU(), NoIDU(), MakeMemRead(operand, RegisterOperand::TempRegZ)),
                    MakeCycle(MakeALU(op, RegisterOperand::TempRegZ, RegisterOperand::TempRegZ), NoIDU(), MakeMemWrite(RegisterOperand::TempRegZ, operand)),
                    MakeCycle(NoALU(), NoIDU(), NoMem())
                });
            };

            auto MakePrefixCBIndirectBitInstruction = [](ALUOp op, RegisterOperand operand) -> Instruction
            {
                return MakeInstruction({
                    FETCH_MCYCLE,
                    MakeCycle(NoALU(), NoIDU(), MakeMemRead(operand, RegisterOperand::TempRegZ)),
                    MakeCycle(MakeALU(op, RegisterOperand::TempRegZ, RegisterOperand::TempRegZ), NoIDU(), NoMem()),
                });
            };

            for (uint8_t i = 0; i < 8; ++i)
            {
                using MakeInstructionFn = Instruction(*)(ALUOp, RegisterOperand);

                MakeInstructionFn makeInstructionFn = MakePrefixCBALUInstruction;
                MakeInstructionFn makeBitInstructionFn = MakePrefixCBALUInstruction;

                RegisterOperand operand = OpCodeRegisterIndexToRegisterOperand(i);
                if (operand == RegisterOperand::None) 
                {
                    operand = RegisterOperand::RegHL;
                    makeInstructionFn = MakePrefixCBIndirectALUInstruction;
                    makeBitInstructionFn = MakePrefixCBIndirectBitInstruction;
                }

                // RLC r8
                PREFIX_INSTRUCTIONS[0x00 + i] = makeInstructionFn(ALUOp::Rlc, operand);

                // RRC r8
                PREFIX_INSTRUCTIONS[0x08 + i] = makeInstructionFn(ALUOp::Rrc, operand);

                // RL r8
                PREFIX_INSTRUCTIONS[0x10 + i] = makeInstructionFn(ALUOp::Rl, operand);

                // RR r8
                PREFIX_INSTRUCTIONS[0x18 + i] = makeInstructionFn(ALUOp::Rr, operand);

                // SLA r8
                PREFIX_INSTRUCTIONS[0x20 + i] = makeInstructionFn(ALUOp::Sla, operand);

                // SRA r8
                PREFIX_INSTRUCTIONS[0x28 + i] = makeInstructionFn(ALUOp::Sra, operand);

                // SWAP r8
                PREFIX_INSTRUCTIONS[0x30 + i] = makeInstructionFn(ALUOp::Swap, operand);

                // SRL r8
                PREFIX_INSTRUCTIONS[0x38 + i] = makeInstructionFn(ALUOp::Srl, operand);

                
                for (uint8_t bit = 0; bit < 8; ++bit)
                {
                    uint8_t irOffset = bit * 8 + i;

                    // BIT u3, r8
                    PREFIX_INSTRUCTIONS[0x40 + irOffset] = makeBitInstructionFn(ALUOp(int(ALUOp::Bit0) + bit), operand);

                    // RES u3, r8
                    PREFIX_INSTRUCTIONS[0x80 + irOffset] = makeInstructionFn(ALUOp(int(ALUOp::Res0) + bit), operand);

                    // SET u3, r8
                    PREFIX_INSTRUCTIONS[0xC0 + irOffset] = makeInstructionFn(ALUOp(int(ALUOp::Set0) + bit), operand);
                }
            }
        }

        void PopulateInterruptInstructions()
        {
            auto MakeInterruptInstruction = [](uint8_t interrupt)
            {
                return MakeInstruction(
                {
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegPC), NoMem(), MakeMisc(MCycle::Misc::MF_DisableInterrupts)),
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), NoMem()),
                    MakeCycle(NoALU(), MakeIDU(IDUOp::Dec, RegisterOperand::RegSP), MakeMemWrite(RegisterOperand::RegPCH, RegisterOperand::RegSP)),
                    MakeCycle(NoALU(), NoIDU(), MakeMemWrite(RegisterOperand::RegPCL, RegisterOperand::RegSP), MakeMisc(MCycle::Misc::MF_WriteValueToWideRegister, RegisterOperand::RegPC, interrupt)),
                    MakeCycle(NoALU(), NoIDU(), NoMem())
                });
            };

            INTERRUPT_INSTRUCTIONS[INT_VBLANK] = MakeInterruptInstruction(INT_VBLANK);
            INTERRUPT_INSTRUCTIONS[INT_STAT] = MakeInterruptInstruction(INT_STAT);
            INTERRUPT_INSTRUCTIONS[INT_TIMER] = MakeInterruptInstruction(INT_TIMER);
            INTERRUPT_INSTRUCTIONS[INT_SERIAL] = MakeInterruptInstruction(INT_SERIAL);
            INTERRUPT_INSTRUCTIONS[INT_JOYPAD] = MakeInterruptInstruction(INT_JOYPAD);
        }

        void PopulateInstructions()
        {
            // Interrupt handling
            PopulateInterruptInstructions();

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

            PopulatePrefixCBInstructions();
        }

        struct OpCodeStaticInit
        {
            OpCodeStaticInit()
            {
                PopulateInstructions();
            }

        } OPCODE_STATIC_INIT;

        
    }

    const MCycle& GetFetchMCycle() { return FETCH_MCYCLE; };

    uint8_t GetMCycleCount(InstructionTable table, uint8_t opCode)
    {
        return INSTRUCTION_TABLES[int(table)][opCode]._cycleCount;
    }

    const MCycle& GetMCycle(InstructionTable table, uint8_t opCode, uint8_t mCycleIndex)
    {
        const Instruction& i = INSTRUCTION_TABLES[int(table)][opCode];
        EMU_ASSERT("MCycle index out of bounds" && mCycleIndex < i._cycleCount);

        return i._cycles[mCycleIndex];
    }

    const char* GetOpcodeName(InstructionTable table, uint8_t opCode)
    {
        if (table != InstructionTable::Interrupt)
        {
            return OPCODE_NAME_TABLES[int(table)][opCode];
        }

        switch (opCode)
        {
        case 0x40:
            return "INT_$40";
            break;
        case 0x48:
            return "INT_$48";
            break;
        case 0x50:
            return "INT_$50";
            break;
        case 0x58:
            return "INT_$58";
            break;
        case 0x60:
            return "INT_$60";
            break;
        default:
            break;
        }

        EMU_ASSERT(0);
        return "UNKNOWN";
    }
}