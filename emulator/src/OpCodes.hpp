#include "SM83.hpp"
#include "ALU.hpp"

namespace emu::SM83
{
    constexpr const uint8_t INT_VBLANK = 0x40;
    constexpr const uint8_t INT_STAT = 0x48;
    constexpr const uint8_t INT_TIMER = 0x50;
    constexpr const uint8_t INT_SERIAL = 0x58;
    constexpr const uint8_t INT_JOYPAD = 0x60;

    constexpr const uint8_t INT_BIT_VBLANK = 1 << 0;
    constexpr const uint8_t INT_BIT_STAT = 1 << 1;
    constexpr const uint8_t INT_BIT_TIMER = 1 << 2;
    constexpr const uint8_t INT_BIT_SERIAL = 1 << 3;
    constexpr const uint8_t INT_BIT_JOYPAD = 1 << 4;

    const MCycle& GetFetchMCycle();

    uint8_t GetMCycleCount(InstructionTable table, uint8_t opCode);
    const MCycle& GetMCycle(InstructionTable table, uint8_t opCode, uint8_t mCycleIndex);
}