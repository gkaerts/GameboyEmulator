#include "SM83.hpp"
#include "ALU.hpp"

namespace emu::SM83
{
    const MCycle& GetFetchMCycle();

    uint8_t GetMCycleCount(InstructionTable table, uint8_t opCode);
    const MCycle& GetMCycle(InstructionTable table, uint8_t opCode, uint8_t mCycleIndex);
}