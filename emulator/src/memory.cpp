#include "memory.hpp"
#include "SM83.hpp"
#include "DMGBoot.hpp"
namespace emu::SM83
{
    MemoryController MakeTestMemoryController()
    {
        return {
            ._memory = {},
            ._writeFn = [](MemoryController& mc, uint16_t addr, uint8_t val)
            {
                mc._memory[addr] = val;
            },

            ._readFn = [](MemoryController& mc, uint16_t addr)
            {
                return mc._memory[addr];
            }
        };
    }

    namespace
    {
        enum MemRegionFlags
        {
            MRF_ReadOnly = 0x01,
            MRF_Mirror = 0x02,
            MRF_Unused = 0x04,
        };
        struct MemRegion
        {
            uint16_t _start;
            uint16_t _end;
            uint32_t _flags;

            uint16_t _optMirrorStart;
            uint16_t _optMirrorEnd;
        };

        constexpr const MemRegion REGION_ROM_BANK0 = { 0x0000, 0x4000, MRF_ReadOnly };
        constexpr const MemRegion REGION_ROM_BANKN = { 0x4000, 0x8000, MRF_ReadOnly };
        constexpr const MemRegion REGION_VRAM =      { 0x8000, 0xA000 };
        constexpr const MemRegion REGION_EXT_RAM =   { 0xA000, 0xC000 };
        constexpr const MemRegion REGION_WRAM0 =     { 0xC000, 0xD000 };
        constexpr const MemRegion REGION_WRAM1 =     { 0xD000, 0xE000 };
        constexpr const MemRegion REGION_ECHO_RAM =  { 0xE000, 0xFE00, MRF_Mirror, 0xC000, 0xDE00 };
        constexpr const MemRegion REGION_OAM =       { 0xFE00, 0xFEA0 };
        constexpr const MemRegion REGION_UNUSED =    { 0xFEA0, 0xFF00, MRF_Unused };
        constexpr const MemRegion REGION_IO_REGS =   { 0xFF00, 0xFF80 };
        constexpr const MemRegion REGION_HRAM =      { 0xFF80, 0xFFFF };
        constexpr const MemRegion REGION_IE =        { 0xFFFF, 0x0000 };

        constexpr const MemRegion GB_MEM_REGIONS[] =
        {
            REGION_ROM_BANK0,
            REGION_ROM_BANKN,
            REGION_VRAM,
            REGION_EXT_RAM,
            REGION_WRAM0,
            REGION_WRAM1,
            REGION_ECHO_RAM,
            REGION_OAM,
            REGION_UNUSED,
            REGION_IO_REGS,
            REGION_HRAM,
            REGION_IE
        };

        constexpr const uint16_t IO_JOYPAD      = 0xFF00;

        constexpr const uint16_t IO_SERIAL0     = 0xFF01;
        constexpr const uint16_t IO_SERIAL1     = 0xFF02;

        constexpr const uint16_t IO_DIV         = 0xFF04;
        constexpr const uint16_t IO_TIMA        = 0xFF05;
        constexpr const uint16_t IO_TMA         = 0xFF06;
        constexpr const uint16_t IO_TAC         = 0xFF07;

        constexpr const uint16_t IO_INTERRUPT   = 0xFF0F;

        constexpr const uint16_t IO_AUDIO_BEGIN = 0xFF10;
        constexpr const uint16_t IO_AUDIO_END   = 0xFF26;

        constexpr const uint16_t IO_WAVE_BEGIN  = 0xFF30;
        constexpr const uint16_t IO_WAVE_END    = 0xFF3F;

        constexpr const uint16_t IO_LCD_BEGIN   = 0xFF40;
        constexpr const uint16_t IO_LCD_END     = 0xFF4B;

        constexpr const uint16_t IO_BOOT_CTRL   = 0xFF50;

        void GBMemWrite(MemoryController& mc, uint16_t addr, uint8_t val)
        {
            // Disallow enabling of IO_BOOT_CTRL
            if (addr == IO_BOOT_CTRL && val != 0)
            {
                return;
            }

            for (const MemRegion& region : GB_MEM_REGIONS)
            {
                if (addr >= region._start && (addr < region._end || region._end == 0x0000))
                {
                    if (region._flags & MRF_ReadOnly || region._flags & MRF_Unused)
                    {
                        return;
                    }

                    if (region._flags & MRF_Mirror)
                    {
                        EMU_ASSERT(region._end - region._start == region._optMirrorEnd - region._optMirrorStart && "Memory region range does not equal its mirror range!");
                        addr = (addr - region._start) + region._optMirrorStart;
                    }

                    mc._memory[addr] = val;

                    return;
                }
            }
        }
    }

    uint8_t GBMemRead(MemoryController& mc, uint16_t addr)
    {
        // Check for boot control and redirect to boot rom if needed
        if (addr < sizeof(DMG_BOOT_ROM) && mc._memory[IO_BOOT_CTRL] == 0)
        {
            return DMG_BOOT_ROM[addr];
        }

        // Handle regular memory regions
        for (const MemRegion& region : GB_MEM_REGIONS)
        {
            if (addr >= region._start && (addr < region._end || region._end == 0x0000))
            {
                if (region._flags & MRF_Unused)
                {
                    return 0;
                }

                if (region._flags & MRF_Mirror)
                {
                    EMU_ASSERT(region._end - region._start == region._optMirrorEnd - region._optMirrorStart && "Memory region range does not equal its mirror range!");
                    addr = (addr - region._start) + region._optMirrorStart;
                }

                return mc._memory[addr];
            }
        }

        return 0;
    }

    MemoryController MakeGBMemoryController()
    {
        return {
            ._memory = {},
            ._writeFn = GBMemWrite,
            ._readFn = GBMemRead,
        };
    }

    void TickMemoryController(MemoryController& memCtrl, IO& cpuIO)
    {
        // Check if memory is requested
        if (cpuIO._outPins.MRQ)
        {
            // Put memory onto data bus?
            if (cpuIO._outPins.RD)
            {
                cpuIO._data = memCtrl._readFn(memCtrl, cpuIO._address);
            }

            // Put data bus into memory?
            else if (cpuIO._outPins.WR)
            {
                memCtrl._writeFn(memCtrl, cpuIO._address, cpuIO._data);
            }
        }
    }
}