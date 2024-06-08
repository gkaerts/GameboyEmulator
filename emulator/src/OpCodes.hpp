#include "LR35902.hpp"
#include "ALU.hpp"

namespace emu::LR35902
{

    uint8_t GetMCycleCount(uint8_t opCode);
    MCycle GetMCycle(uint8_t opCode, uint8_t mCycleIndex);
    MCycle::Type GetNextMCycleType(uint8_t opCode, uint8_t mCycleIndex);
}