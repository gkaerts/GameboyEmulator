#include "LR35902.hpp"
#include "ALU.hpp"
#include "OpCodes.hpp"

#include <cstring>
#include <initializer_list>

namespace emu::LR35902
{
    namespace
    {

        struct OpcodeFetchPins
        {
            // System Control
            uint8_t M1 : 1;
            uint8_t MREQ : 1;
            uint8_t RFSH : 1;
            uint8_t RD : 1;
        };

        const OpcodeFetchPins OPCODE_FETCH_CYCLE_PINS[]
        {
            { .M1 = 1, .MREQ = 0, .RFSH = 0, .RD = 0},  // T1/0
            { .M1 = 1, .MREQ = 1, .RFSH = 0, .RD = 1},  // T1/1

            { .M1 = 1, .MREQ = 1, .RFSH = 0, .RD = 1},  // T2/0
            { .M1 = 1, .MREQ = 1, .RFSH = 0, .RD = 1},  // T2/1

            { .M1 = 0, .MREQ = 0, .RFSH = 1, .RD = 0},  // T3/0
            { .M1 = 0, .MREQ = 1, .RFSH = 1, .RD = 0},  // T3/1

            { .M1 = 0, .MREQ = 1, .RFSH = 1, .RD = 0},  // T4/0
            { .M1 = 0, .MREQ = 0, .RFSH = 1, .RD = 0},  // T4/1
        };

        void ApplyOpcodeFetchPinState(IO::PinsOut& ioPins, OpcodeFetchPins pins)
        {
            ioPins.M1 = pins.M1;
            ioPins.MREQ = pins.MREQ;
            ioPins.RFSH = pins.RFSH;
            ioPins.RD = pins.RD;
        }

        void DecodeOpCode(MCycle mCycle, Decoder& decoder, Registers& regs, IO& io, uint8_t* memory)
        {
            if (mCycle._aluOp != ALUOp::Nop)
            {
                ALUOutput aluOutput = ProcessALUOp(
                    mCycle._aluOp, 
                    regs._reg8.F,
                    regs._reg8.A,
                    decoder._temp);

                regs._reg8.F = aluOutput._flags;
                decoder._temp = aluOutput._result;
            }

            if (mCycle._storeDest != RegisterOperand::None)
            {
                regs._reg8Arr[uint8_t(mCycle._storeDest)] = decoder._temp;
            }
            else if (mCycle._memWriteDest != MemWriteDest::None)
            {
                memory[io._address] = decoder._temp;
            }
        }

        void OpcodeFetchCycle(TCycle cycle, 
            IO& io, 
            Registers& regs,
            Decoder& decoder,
            uint8_t* memory)
        {
            switch (cycle)
            {
                // T1/0 - Pull PC onto address bus
                case T1_0:
                    io._address = regs._reg16.PC;
                    decoder._tCycleIndex = T1_1;
                break;
                // T1/1 - Increment PC
                case T1_1:
                    regs._reg16.PC++;
                    decoder._tCycleIndex = T2_0;
                break;

                // T2/0 - Fetch data from address line into data line
                case T2_0:
                    io._data = memory[io._address];
                    decoder._tCycleIndex = T2_1;
                    break;
                // T2/1 - Pull data into instruction register
                case T2_1:
                    decoder.IR = io._data;
                    decoder._tCycleIndex = T3_0;
                    break;

                case T3_0:
                    io._address = 0; // TODO: Is the refresh address a thing on the LR35902?
                    io._data = 0;

                    MCycle mCycle = GetMCycle(decoder.IR, 0);
                    decoder._temp = regs._reg8Arr[uint8_t(mCycle._operandB)];

                    DecodeOpCode(mCycle, decoder, regs, io, memory);
                    decoder._tCycleIndex = T3_1;
                    break;
                case T3_1:
                    decoder._tCycleIndex = T4_0;
                    break;
                case T4_0:
                    decoder._tCycleIndex = T4_1;
                    break;
                case T4_1:
                    decoder._currMCycleType = GetNextMCycleType(decoder.IR, 0);
                    decoder._mCycleIndex = (decoder._mCycleIndex + 1) % GetMCycleCount(decoder.IR);
                    decoder._tCycleIndex = T1_0;
                    break;

                default:
                {
                    EMU_ASSERT("Invalid TCycle Value" && false);
                }
                    break;
            }
            ApplyOpcodeFetchPinState(io._outPins, OPCODE_FETCH_CYCLE_PINS[cycle]);
        }

        struct MemReadPins
        {
            uint8_t MREQ : 1;
            uint8_t RD : 1;
        };

        const MemReadPins MEM_READ_CYCLE_PINS[]
        {
            { .MREQ = 0, .RD = 0 },
            { .MREQ = 1, .RD = 1 },

            { .MREQ = 1, .RD = 1 },
            { .MREQ = 1, .RD = 1 },

            { .MREQ = 1, .RD = 1 },
            { .MREQ = 1, .RD = 1 },

            { .MREQ = 1, .RD = 1 },
            { .MREQ = 0, .RD = 0 },
        };

        void ApplyMemReadPinState(IO::PinsOut& ioPins, MemReadPins pins)
        {
            ioPins.MREQ = pins.MREQ;
            ioPins.RD = pins.RD;
        }

        void MemReadCycle(TCycle cycle,
            IO& io, 
            Registers& regs,
            Decoder& decoder,
            uint8_t* memory)
        {
            MCycle mCycle = GetMCycle(decoder.IR, decoder._mCycleIndex);
            EMU_ASSERT("Invalid memory read source" && mCycle._memReadSrc != MemReadSrc::None);

            switch (cycle)
            {
                // T1/0 - Pull address from source onto bus
                case T1_0:
                    io._address = regs._reg16Arr[uint8_t(mCycle._memReadSrc)];
                    decoder._tCycleIndex = T1_1;
                    break;
                case T1_1:
                    if (mCycle._memReadSrc == MemReadSrc::RegPC)
                    {
                        regs._reg16.PC++;
                    }
                    decoder._tCycleIndex = T2_0;
                break;

                // T2/0 - Pull data at address into address bus
                case T2_0:
                    io._data = memory[io._address];
                    decoder._tCycleIndex = T2_1;
                    break;
                case T2_1:
                    decoder._temp = io._data;
                    decoder._tCycleIndex = T3_0;
                    break;

                case T3_0:
                    DecodeOpCode(mCycle, decoder, regs, io, memory);
                    decoder._tCycleIndex = T3_1;
                    break;
                case T3_1:
                    decoder._tCycleIndex = T4_0;
                    break;
                case T4_0:
                    decoder._tCycleIndex = T4_1;
                    break;
                case T4_1:
                    decoder._currMCycleType = GetNextMCycleType(decoder.IR, decoder._mCycleIndex);
                    decoder._mCycleIndex = (decoder._mCycleIndex + 1) % GetMCycleCount(decoder.IR);
                    decoder._tCycleIndex = T1_0;
                    break;

                default:
                {
                    EMU_ASSERT("Invalid TCycle Value" && false);
                }
                    break;
            }
            
            ApplyMemReadPinState(io._outPins, MEM_READ_CYCLE_PINS[cycle]);
        }
    
        struct MemWritePins
        {
            uint8_t MREQ: 1;
            uint8_t WR: 1;
        };

        const MemWritePins MEM_WRITE_CYCLE_PINS[]
        {
            { .MREQ = 0, .WR = 0 },
            { .MREQ = 1, .WR = 0 },

            { .MREQ = 1, .WR = 0 },
            { .MREQ = 1, .WR = 1 },

            { .MREQ = 1, .WR = 1 },
            { .MREQ = 1, .WR = 1 },

            { .MREQ = 1, .WR = 1 },
            { .MREQ = 0, .WR = 0 },
        };

        void ApplyMemWritePinState(IO::PinsOut& ioPins, MemWritePins pins)
        {
            ioPins.MREQ = pins.MREQ;
            ioPins.WR = pins.WR;
        }

        void MemWriteCycle(TCycle cycle,
            IO& io, 
            Registers& regs,
            Decoder& decoder,
            uint8_t* memory)
        {
            MCycle mCycle = GetMCycle(decoder.IR, decoder._mCycleIndex);
            EMU_ASSERT("Invalid memory write dest" && mCycle._memWriteDest != MemWriteDest::None);
            EMU_ASSERT("Mem write cycle can not have a register as store dest" && mCycle._storeDest == RegisterOperand::None);

            switch (cycle)
            {
                // T1/0 - Pull write destination address onto bus
                case T1_0:
                    io._address = regs._reg16Arr[uint8_t(mCycle._memWriteDest)];
                    decoder._tCycleIndex = T1_1;
                    break;
                case T1_1:
                    decoder._tCycleIndex = T2_0;
                break;

                // T2/0 - Pull operand register value onto data bus
                case T2_0:
                    io._data = regs._reg8Arr[uint8_t(mCycle._operandB)];
                    decoder._tCycleIndex = T2_1;
                    break;
                case T2_1:
                    decoder._temp = io._data;
                    decoder._tCycleIndex = T3_0;
                    break;

                case T3_0:
                    DecodeOpCode(mCycle, decoder, regs, io, memory);
                    decoder._tCycleIndex = T3_1;
                    break;
                case T3_1:
                    decoder._tCycleIndex = T4_0;
                    break;
                case T4_0:
                    decoder._tCycleIndex = T4_1;
                    break;
                case T4_1:
                    decoder._currMCycleType = GetNextMCycleType(decoder.IR, decoder._mCycleIndex);
                    decoder._mCycleIndex = (decoder._mCycleIndex + 1) % GetMCycleCount(decoder.IR);
                    decoder._tCycleIndex = T1_0;
                    break;

                default:
                {
                    EMU_ASSERT("Invalid TCycle Value" && false);
                }
                    break;
            }
            
            ApplyMemWritePinState(io._outPins, MEM_WRITE_CYCLE_PINS[cycle]);
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
        cpu->_io._outPins.MREQ = false;

        // Set up decoder state
        cpu->_decoder._currMCycleType = MCycle::Type::Fetch;
        cpu->_decoder._mCycleIndex = 0;
        cpu->_decoder._tCycleIndex = T1_0;
    }

    void Tick(CPU* cpu, uint8_t* memory, uint32_t cycles)
    {
        for (uint32_t i = 0; i < cycles; ++i)
        {
            uint8_t tCycle = cpu->_decoder._tCycleIndex;

            switch (cpu->_decoder._currMCycleType)
            {
            case MCycle::Type::Fetch:
                OpcodeFetchCycle(TCycle(tCycle + 0), cpu->_io, cpu->_registers, cpu->_decoder, memory);
                OpcodeFetchCycle(TCycle(tCycle + 1), cpu->_io, cpu->_registers, cpu->_decoder, memory);
                break;

            case MCycle::Type::MemRead:
                MemReadCycle(TCycle(tCycle + 0), cpu->_io, cpu->_registers, cpu->_decoder, memory);
                MemReadCycle(TCycle(tCycle + 1), cpu->_io, cpu->_registers, cpu->_decoder, memory);
            break;

            case MCycle::Type::MemWrite:
                MemWriteCycle(TCycle(tCycle + 0), cpu->_io, cpu->_registers, cpu->_decoder, memory);
                MemWriteCycle(TCycle(tCycle + 1), cpu->_io, cpu->_registers, cpu->_decoder, memory);
            break;
            
            default:
                EMU_ASSERT("Invalid MCycle type" && false);
                break;
            }
        }
    }
}