#include "SM83.hpp"
#include "ALU.hpp"
#include "OpCodes.hpp"
#include "MMU.hpp"
#include "DMGBoot.hpp"

#include <cstring>
#include <initializer_list>

namespace emu::SM83
{
    namespace
    {

        uint8_t LoadReg8(Registers& regs, RegisterOperand reg)
        {
            return (int(reg) < int(RegisterOperand::WideRegisterStart)) ?
                regs._reg8Arr[int(reg)] : 
                uint8_t(regs._reg16Arr[int(reg) - int(RegisterOperand::WideRegisterStart)] & 0xFF);
        }

        void StoreReg8(Registers& regs, RegisterOperand reg, uint8_t value)
        {
            if (int(reg) < int(RegisterOperand::WideRegisterStart))
            {
                regs._reg8Arr[int(reg)] = value;
            }
            else
            {
                regs._reg16Arr[int(reg) - int(RegisterOperand::WideRegisterStart)] = value;
            }
        }

        uint16_t LoadReg16(Registers& regs, RegisterOperand reg)
        {
            return (int(reg) < int(RegisterOperand::WideRegisterStart)) ?
                regs._reg8Arr[int(reg)] : 
                regs._reg16Arr[int(reg) - int(RegisterOperand::WideRegisterStart)];
        }

        void StoreReg16(Registers& regs, RegisterOperand reg, uint16_t value)
        {
            if (int(reg) < int(RegisterOperand::WideRegisterStart))
            {
                regs._reg8Arr[int(reg)] = uint8_t(value & 0xFF);
            }
            else
            {
                regs._reg16Arr[int(reg) - int(RegisterOperand::WideRegisterStart)] = value;
            }
        }

        TCycleState NextTCycle(TCycleState state)
        {
            return state == T4_1 ? T1_0 : TCycleState(uint8_t(state) + 1);
        }

        bool IsLastMCycle(InstructionTable table, const MCycle& cycle, uint8_t indexInOpcode, uint8_t opcode)
        {
            return (cycle._misc._flags & MCycle::Misc::MF_LastCycle) || (indexInOpcode == GetMCycleCount(table, opcode) - 1);
        }

        void FixupFlagRegister(Registers& regs)
        {
            uint8_t mask = (SF_Carry | SF_HalfCarry | SF_Subtract | SF_Zero);
            regs._reg8.F &= mask;
        }

        constexpr const uint16_t IO_REG_IE = 0xFFFF;
        constexpr const uint16_t IO_REG_IF = 0xFF0F;


        void ProcessCurrentMCycle(
            IO& io, 
            Registers& regs,
            Decoder& decoder,
            PeripheralIO& pIO)
        {
            if ((decoder._flags & Decoder::DF_ExecutionHalted) && decoder._tCycleState < T4_0)
            {
                decoder._tCycleState = NextTCycle(decoder._tCycleState);
                return;
            }

            switch (decoder._tCycleState)
            {
                case T1_0:
                {
                    decoder._currMCycle = GetMCycle(decoder._table, regs._reg8.IR, decoder._nextMCycleIndex);

                    // Set M1 pin if we're in a fetch cycle and overlap MCycle with fetch cycle if required
                    if (IsLastMCycle(decoder._table, decoder._currMCycle, decoder._nextMCycleIndex, regs._reg8.IR))
                    {
                        const MCycle& fetchCyle = GetFetchMCycle();

                        // Allow fetch cycle IDU op to be overwritten by instruction IDU op
                        if (decoder._currMCycle._idu._op == IDUOp::Nop &&
                            decoder._currMCycle._idu._operand == RegisterOperand::None &&
                            decoder._currMCycle._idu._dest == RegisterOperand::None)
                        {
                            decoder._currMCycle._idu = fetchCyle._idu;
                        }

                        // Same for memory op
                        if ((decoder._currMCycle._memOp._flags & MCycle::MemOp::MOF_Active) == 0)
                        {
                            decoder._currMCycle._memOp = fetchCyle._memOp;
                        }
                        io._outPins.M1 = 1;

                        // Revert to using default instruction table
                        decoder._table = InstructionTable::Default;
                        decoder._nextMCycleIndex = 0;
                    }
                    else
                    {
                        decoder._nextMCycleIndex++;
                    }

                    // Switch instruction tables here if needed
                    if (decoder._currMCycle._misc._flags & MCycle::Misc::MF_PrefixCB)
                    {
                        decoder._table = InstructionTable::PrefixCB;
                    }

                    // Pull address for memory operation (if any) onto address bus
                    if (decoder._currMCycle._memOp._flags & MCycle::MemOp::MOF_Active)
                    {
                        if (decoder._currMCycle._memOp._flags & MCycle::MemOp::MOF_UseOffsetAddress)
                        {
                            io._address = 0xFF00 + LoadReg8(regs, decoder._currMCycle._memOp._addressSrcBeforeOffset);
                        }
                        else
                        {
                            io._address = LoadReg16(regs, decoder._currMCycle._memOp._addressSrc);
                        }
                    }
                }
                    break;

                case T1_1:
                {
                    // Set MRQ and read or write pin based on the memory operation type
                    if (decoder._currMCycle._memOp._flags & MCycle::MemOp::MOF_Active)
                    {
                        if (decoder._currMCycle._memOp._flags & MCycle::MemOp::MOF_IsMemWrite)
                        {
                            io._outPins.MRQ = 1;
                        }
                        else
                        {
                            io._outPins.MRQ = 1;
                            io._outPins.RD = 1;
                        }
                    }
                    
                }
                    break;

                case T2_0:
                {
                    // T-Cycle waiting for memory controller to write to data bus if mem read was requested
                }
                    break;

                case T2_1:
                {
                    // If memory was requested it is now on the data bus. Put it in destination register
                    // This handles setting the instruction register in a fetch cycle!!
                    if ((decoder._currMCycle._memOp._flags & MCycle::MemOp::MOF_Active) &&
                        !(decoder._currMCycle._memOp._flags & MCycle::MemOp::MOF_IsMemWrite))
                    {
                        StoreReg8(regs, decoder._currMCycle._memOp._reg, io._data);
                        if (decoder._currMCycle._memOp._reg == RegisterOperand::TempRegZ)
                        {
                            // This feels like a hack :(
                            if (io._data & 0x80)
                            {
                                decoder._flags |= Decoder::DF_SignBitHigh;
                            }
                            else
                            {
                                decoder._flags &= ~Decoder::DF_SignBitHigh;
                            }
                        }
                    }
                    
                    int aluOpFlags = (decoder._flags & Decoder::DF_SignBitHigh) ? PAOF_ZSignHigh : 0;
                    uint8_t aluFlags = 0;

                    // Handle ALU operation 
                    if (decoder._currMCycle._alu._operandA != RegisterOperand::None && 
                        decoder._currMCycle._alu._operandB != RegisterOperand::None &&
                        decoder._currMCycle._alu._dest != RegisterOperand::None)
                    {
                        uint8_t aluOperandA = LoadReg8(regs, decoder._currMCycle._alu._operandA);
                        uint8_t aluOperandB = LoadReg8(regs, decoder._currMCycle._alu._operandB);
           
                        ALUOutput aluResult = ProcessALUOp(
                            decoder._currMCycle._alu._op,
                            regs._reg8.F,
                            aluOperandA,
                            aluOperandB,
                            aluOpFlags);

                        if (decoder._currMCycle._misc._flags & MCycle::Misc::MF_ALUClearZero)
                        {
                            aluResult._flags &= ~SF_Zero;
                        }

                        if ((decoder._currMCycle._misc._flags & MCycle::Misc::MF_ALUKeepFlags) == 0)
                        {
                            regs._reg8.F = aluResult._flags;
                        }


                        StoreReg8(regs, decoder._currMCycle._alu._dest, aluResult._result);

                        aluFlags = aluResult._flags;
                    }

                    // Handle IDU operation
                    if (decoder._currMCycle._idu._operand != RegisterOperand::None &&
                        decoder._currMCycle._idu._dest != RegisterOperand::None)
                    {
                        int iduOpFlags = aluOpFlags;
                        iduOpFlags |= (aluFlags & SF_Carry) ? PAOF_ALUHasCarry : 0;

                        uint16_t iduOperand = LoadReg16(regs, decoder._currMCycle._idu._operand);
                        IDUOutput iduResult = ProcessIDUOp(
                            decoder._currMCycle._idu._op, 
                            iduOperand,
                            iduOpFlags);
                            
                        StoreReg16(regs, decoder._currMCycle._idu._dest, iduResult._result);
                    }
                    
                    // If memory write was requested notify memory controller
                    if ((decoder._currMCycle._memOp._flags & MCycle::MemOp::MOF_Active) &&
                        (decoder._currMCycle._memOp._flags & MCycle::MemOp::MOF_IsMemWrite))
                    {
                        io._data = LoadReg8(regs, decoder._currMCycle._memOp._reg);
                        io._outPins.WR = 1;
                    }
                }
                    break;

                case T3_0:
                {
                    // Clear M1 pin
                    io._outPins.M1 = 0; 

                    // Handle Misc operations
                    if (decoder._currMCycle._misc._flags & MCycle::Misc::MF_WriteWZToWideRegister)
                    {
                        StoreReg16(regs, decoder._currMCycle._misc._operand, regs._reg16.TempWZ);
                        FixupFlagRegister(regs);    // In case we wrote to F
                    }
                    else if (decoder._currMCycle._misc._flags & MCycle::Misc::MF_WriteValueToWideRegister)
                    {
                        StoreReg16(regs, decoder._currMCycle._misc._operand, decoder._currMCycle._misc._optValue);
                    }

                    if (decoder._currMCycle._misc._flags & MCycle::Misc::MF_EnableInterrupts)
                    {
                        regs._reg8.IME = 1;
                    }
                    else if (decoder._currMCycle._misc._flags & MCycle::Misc::MF_DisableInterrupts)
                    {
                        regs._reg8.IME = 0;
                    }

                    // Conditional checks
                    if ((decoder._currMCycle._misc._flags & MCycle::Misc::MF_ConditionCheckC) && (regs._reg8.F & SF_Carry) ||
                        (decoder._currMCycle._misc._flags & MCycle::Misc::MF_ConditionCheckZ) && (regs._reg8.F & SF_Zero) ||
                        (decoder._currMCycle._misc._flags & MCycle::Misc::MF_ConditionCheckNC) && !(regs._reg8.F & SF_Carry) ||
                        (decoder._currMCycle._misc._flags & MCycle::Misc::MF_ConditionCheckNZ) && !(regs._reg8.F & SF_Zero))
                    {
                        decoder._nextMCycleIndex = uint8_t(decoder._currMCycle._misc._optValue);
                    }
                }
                    break;
                case T3_1:
                {
                    // Clear memory pins
                    io._outPins.MRQ = 0;
                    io._outPins.RD = 0;
                    io._outPins.WR = 0;
                }
                    break;
                case T4_0:
                {
                    
                }
                    break;
                case T4_1:
                {
                    if (decoder._currMCycle._misc._flags & MCycle::Misc::MF_StopExecution)
                    {
                        decoder._flags |= Decoder::DF_ExecutionStopped;
                    }

                    if (decoder._currMCycle._misc._flags & MCycle::Misc::MF_HaltExecution)
                    {
                        decoder._flags |= Decoder::DF_ExecutionHalted;
                    }

                    // HACK! Handle interrupts
                    if (decoder._nextMCycleIndex == 0)
                    {
                        uint8_t IE_IF = (pIO.IE & pIO.IF) & 0x1F;
                        if (IE_IF != 0)
                        {
                            if (regs._reg8.IME)
                            {
                                // Kind of a hack to hijack the instruction register, but should be harmless?
                                if (IE_IF & INT_BIT_VBLANK)
                                {
                                    regs._reg8.IR = INT_VBLANK;
                                }
                                else if (IE_IF & INT_BIT_STAT)
                                {
                                    regs._reg8.IR = INT_STAT;
                                }
                                else if (IE_IF & INT_BIT_TIMER)
                                {
                                    regs._reg8.IR = INT_TIMER;
                                }
                                else if (IE_IF & INT_BIT_SERIAL)
                                {
                                    regs._reg8.IR = INT_SERIAL;
                                }
                                else if (IE_IF & INT_BIT_JOYPAD)
                                {
                                    regs._reg8.IR = INT_JOYPAD;
                                }

                                decoder._table = InstructionTable::Interrupt;
                                pIO.IF = 0;
                            }

                            // Always resume when an interrupt could get triggered, even if interrupts aren't enabled
                            decoder._flags &= ~Decoder::DF_ExecutionHalted;
                        }
                    }
                }
                    break;

                default:
                {
                    EMU_ASSERT("Invalid TCycle Value" && false);
                }
                    break;
            }

            decoder._tCycleState = NextTCycle(decoder._tCycleState);
        }
    }

    void BootCPU(CPU& cpu, uint16_t initSP, uint16_t initPC, uint8_t initBootCtrl)
    {
        // Clear and set up registers
        std::memset(&cpu._registers, 0, sizeof(cpu._registers));

        // Clear peripheral IO
        std::memset(&cpu._peripheralIO, 0, sizeof(cpu._peripheralIO));
        
        cpu._registers._reg16.SP = initSP;
        cpu._registers._reg16.PC = initPC;

        // Set up IO state
        cpu._io._address = 0;
        cpu._io._data = 0;
        cpu._io._outPins.MRQ = 0;
        cpu._io._outPins.RD = 0;
        cpu._io._outPins.WR = 0;

        // Set up decoder state
        cpu._decoder._nextMCycleIndex = 0;
        cpu._decoder._tCycleState = T1_0;
        cpu._decoder._flags = 0;
        cpu._decoder._table = InstructionTable::Default;

        cpu._tcycle = 0;

        // Load boot ROM
        cpu._peripheralIO.BOOT_CTRL = initBootCtrl;
        std::memcpy(cpu._bootROM, DMG_BOOT_ROM, sizeof(DMG_BOOT_ROM));
    }

    constexpr const uint16_t ADDR_PERIPHERAL_IO = 0xFF00; 
    void MapPeripheralIOMemory(CPU& cpu, MMU& mmu)
    {
        MapMemoryRegion(mmu, ADDR_PERIPHERAL_IO, sizeof(PeripheralIO), reinterpret_cast<uint8_t*>(&cpu._peripheralIO), 0);
    }

    void TickCPU(CPU& cpu, MMU& mmu, uint32_t cycles)
    {
        if (cpu._decoder._flags & Decoder::DF_ExecutionStopped)
        {
            return;
        }

        // Note: Overlapping execution! Last M-cycle of an instruction always overlaps with the fetch cycle for the next instruction
        // Documentation saying an instruction is only 1 cycle only indicates the "effective" cycles spent without overlap
        // The "execution" M-cycles (i.e. not M1) can overlap with the fetch of the next opcode
        for (uint32_t i = 0; i < cycles; ++i)
        {
            // Redirect $0000 - $00FF to the boot rom if needed
            uint8_t BOOT_CTRL = cpu._peripheralIO.BOOT_CTRL;
            if (!BOOT_CTRL)
            {
                RedirectZeroSegment(mmu, cpu._bootROM);
            }

            // Tick clock
            uint16_t* SYSCLCK = reinterpret_cast<uint16_t*>(&cpu._peripheralIO.SYSCLCK);
            cpu._tcycle++;
            if ((cpu._tcycle % 4) == 0)
            {
                (*SYSCLCK)++;      

                // Tick programmable timer
                const uint8_t BIT_TIMA_ENABLED = (1 << 2);
                if ((cpu._peripheralIO.TAC & BIT_TIMA_ENABLED) != 0)
                {
                    const uint16_t TIMA_FREQUENCIES[] =
                    {
                        256,
                        4,
                        16,
                        64
                    };

                    uint16_t frequency = TIMA_FREQUENCIES[cpu._peripheralIO.TAC & 0x03];
                    if (((*SYSCLCK) % frequency) == 0)
                    {
                        cpu._peripheralIO.TIMA++;
                        if (cpu._peripheralIO.TIMA == 0)
                        {
                            cpu._peripheralIO.TIMA = cpu._peripheralIO.TMA;
                            cpu._peripheralIO.IF |= INT_BIT_TIMER;
                        }
                    }
                }
            }


            uint8_t DIV = cpu._peripheralIO.DIV;

            // Two half ticks!
            ProcessCurrentMCycle(cpu._io, cpu._registers, cpu._decoder, cpu._peripheralIO);
            ProcessCurrentMCycle(cpu._io, cpu._registers, cpu._decoder, cpu._peripheralIO);

            // MMU interaction
            if (cpu._io._outPins.MRQ)
            {
                // Put memory onto data bus?
                if (cpu._io._outPins.RD)
                {
                    cpu._io._data = MMURead(mmu, cpu._io._address);
                }

                // Put data bus into memory?
                else if (cpu._io._outPins.WR)
                {
                    MMUWrite(mmu, cpu._io._address, cpu._io._data);
                }
            }

            // Handle timer reset on write
            if (DIV != cpu._peripheralIO.DIV)
            {
                (*SYSCLCK) = 0;
            }

            // Handle boot control register change
            if (BOOT_CTRL == 0 && cpu._peripheralIO.BOOT_CTRL != 0)
            {
                RemoveZeroSegmentRedirect(mmu);
            }

            // And make it read-only from this point onward
            if (BOOT_CTRL != 0)
            {
                cpu._peripheralIO.BOOT_CTRL = BOOT_CTRL;
            }

            // Unused IO registers need to always be 0xFF
            memset(cpu._peripheralIO.UNKNOWN0, 0xFF, sizeof(cpu._peripheralIO.UNKNOWN0));
            memset(cpu._peripheralIO.UNKNOWN2, 0xFF, sizeof(cpu._peripheralIO.UNKNOWN2));
            memset(cpu._peripheralIO.UNKNOWN3, 0xFF, sizeof(cpu._peripheralIO.UNKNOWN3));
            memset(cpu._peripheralIO.UNKNOWN4, 0xFF, sizeof(cpu._peripheralIO.UNKNOWN4));
            memset(cpu._peripheralIO.UNKNOWN5, 0xFF, sizeof(cpu._peripheralIO.UNKNOWN5));

            // Joypad hack
            cpu._peripheralIO.JOYP = 0xF;
        }
    }
}