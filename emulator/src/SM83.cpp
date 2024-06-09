#include "SM83.hpp"
#include "ALU.hpp"
#include "OpCodes.hpp"
#include "memory.hpp"

#include <cstring>
#include <initializer_list>

namespace emu::SM83
{
    namespace
    {

        TCycleState NextTCycle(TCycleState state)
        {
            return state == T4_1 ? T1_0 : TCycleState(uint8_t(state) + 1);
        }

        void ProcessCurrentMCycle(
            IO& io, 
            Registers& regs,
            Decoder& decoder)
        {
            MCycle& mCycle = decoder._currMCycle;

            switch (decoder._tCycleState)
            {
                case T1_0:
                {
                    // Set M1 pin if we're in a fetch cycle and overlap MCycle with fetch cycle if required
                    if (decoder._flags & Decoder::DF_CurrentCycleIsFetchCycle)
                    {
                        const MCycle& fetchCyle = GetFetchMCycle();

                        EMU_ASSERT(mCycle._idu._op == IDUOp::Nop);
                        EMU_ASSERT(mCycle._memOp._type == MCycle::MemOp::Type::None);

                        mCycle._idu = fetchCyle._idu;
                        mCycle._memOp = fetchCyle._memOp;

                        io._outPins.M1 = 1;
                    }

                    // Pull address for memory operation (if any) onto address bus
                    if (mCycle._memOp._type != MCycle::MemOp::Type::None)
                    {
                        io._address = regs._reg16Arr[uint8_t(mCycle._memOp._readSrcOrWriteDest)];
                    }
                }
                    break;

                case T1_1:
                {
                    // Set MRQ and read or write pin based on the memory operation type
                    if (mCycle._memOp._type == MCycle::MemOp::Type::Read)
                    {
                        io._outPins.MRQ = 1;
                        io._outPins.RD = 1;
                    }

                    // Set value of memory write operation onto data bus
                    else if (mCycle._memOp._type == MCycle::MemOp::Type::Write)
                    {
                        io._outPins.MRQ = 1;
                        io._data = regs._reg8Arr[uint8_t(mCycle._memOp._readDestOrWriteSrc)];
                    }
                    
                    // Handle IDU operation
                    if (mCycle._idu._op != IDUOp::Nop)
                    {
                        uint16_t& iduOperand = regs._reg16Arr[uint8_t(mCycle._idu._operand)];
                        IDUOutput iduResult = ProcessIDUOp(mCycle._idu._op, iduOperand);
                        iduOperand = iduResult._result;
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
                    if (mCycle._memOp._type == MCycle::MemOp::Type::Read)
                    {
                        regs._reg8Arr[uint8_t(mCycle._memOp._readDestOrWriteSrc)] = io._data;
                    }

                    // Handle ALU operation 
                    if (mCycle._alu._operandA != RegisterOperand::None && 
                        mCycle._alu._operandB != RegisterOperand::None)
                    {
                        uint8_t& aluOperandA = regs._reg8Arr[uint8_t(mCycle._alu._operandA)];
                        uint8_t aluOperandB = regs._reg8Arr[uint8_t(mCycle._alu._operandB)];

                        ALUOutput aluResult = ProcessALUOp(
                            mCycle._alu._op,
                            regs._reg8.F,
                            aluOperandA,
                            aluOperandB);
                        
                        aluOperandA = aluResult._result;
                        regs._reg8.F = aluResult._flags;

                        // Sort of a hack? If our destination operand is a temporary register, put ALU result on data bus
                        if (mCycle._alu._operandA == RegisterOperand::TempRegZ ||
                            mCycle._alu._operandA == RegisterOperand::TempRegW)
                        {
                            io._data = aluResult._result;
                        }
                    }

                    
                    // If memory write was requested notify memory controller
                    else if (mCycle._memOp._type == MCycle::MemOp::Type::Write)
                    {
                        io._outPins.WR = 1;
                    }
                }
                    break;

                case T3_0:
                {
                    // Clear M1 pin
                    io._outPins.M1 = 0; 

                    // Handle Misc operations
                    if (mCycle._misc._flags & MCycle::Misc::MF_WriteWZToWideRegister)
                    {
                        regs._reg16Arr[uint8_t(mCycle._misc._wideOperand)] = regs._reg16Arr[uint8_t(WideRegisterOperand::RegWZ)];
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
                    break;
                case T4_1:
                {
                    if (mCycle._misc._flags & MCycle::Misc::MF_StopExecution)
                    {
                        decoder._flags |= Decoder::DF_ExecutionStopped;
                    }

                    if (mCycle._misc._flags & MCycle::Misc::MF_HaltExecution)
                    {
                        decoder._flags |= Decoder::DF_ExecutionHalted;
                    }

                    // Next MCycle
                    if (decoder._flags & Decoder::DF_CurrentCycleIsFetchCycle)
                    {
                        decoder._mCycleIndex = 0;
                    }
                    else
                    {
                        decoder._mCycleIndex++;
                    }
                    
                    decoder._flags &= ~Decoder::DF_CurrentCycleIsFetchCycle;
                    
                    // Last MCycle of an instruction overlaps with a new fetch cycle
                    if (decoder._mCycleIndex == GetMCycleCount(regs._reg8.IR) - 1)
                    {
                        decoder._flags |= Decoder::DF_CurrentCycleIsFetchCycle;
                    }
                    
                    mCycle = GetMCycle(regs._reg8.IR, decoder._mCycleIndex);
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

    void Boot(CPU* cpu, uint16_t initSP, uint16_t initPC)
    {
        // Clear and set up registers
        std::memset(&cpu->_registers, 0, sizeof(cpu->_registers));
        
        cpu->_registers._reg16.SP = initSP;
        cpu->_registers._reg16.PC = initPC;

        // Set up IO state
        cpu->_io._address = 0;
        cpu->_io._data = 0;
        cpu->_io._outPins.MRQ = 0;
        cpu->_io._outPins.RD = 0;
        cpu->_io._outPins.WR = 0;

        // Set up decoder state
        cpu->_decoder._currMCycle = GetMCycle(0, 0);
        cpu->_decoder._flags = Decoder::DF_CurrentCycleIsFetchCycle;
        cpu->_decoder._mCycleIndex = 0;
        cpu->_decoder._tCycleState = T1_0;
    }

    void Tick(CPU* cpu, MemoryController& memCtrl, uint32_t cycles)
    {
        if (cpu->_decoder._flags & Decoder::DF_ExecutionStopped)
        {
            return;
        }

        // Note: Overlapping execution! Last M-cycle of an instruction always overlaps with the fetch cycle for the next instruction
        // Documentation saying an instruction is only 1 cycle only indicates the "effective" cycles spent without overlap
        // The "execution" M-cycles (i.e. not M1) can overlap with the fetch of the next opcode
        for (uint32_t i = 0; i < cycles; ++i)
        {
            if (cpu->_decoder._flags & Decoder::DF_ExecutionHalted)
            {
                continue;
            }

            // Two half ticks!
            ProcessCurrentMCycle(cpu->_io, cpu->_registers, cpu->_decoder);
            ProcessCurrentMCycle(cpu->_io, cpu->_registers, cpu->_decoder);

            TickMemoryController(memCtrl, cpu->_io);
        }
    }
}