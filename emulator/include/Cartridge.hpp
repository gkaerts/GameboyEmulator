#pragma once

#include "common.hpp"
#include <memory>

namespace emu::SM83
{
    struct MMU;

    enum class MBCType
    {
        None,
        MBC1,
        MBC2,
        MBC3,
        MBC5,
        MBC6,
        MBC7,
        MMM01,
        HuC1,
        HuC3,
        MBC1M,
    };

    struct MBC
    {
        MBCType _type;
        union
        {
            struct
            {
                uint8_t _RAMEnable;
                uint8_t _ROMBankNumber : 5;
                uint8_t _RAMBankNumber: 2;
                uint8_t _BankModeSelect: 1;
            } _MBC1;
            
        };
    };

    constexpr const uint8_t CARTRIDGE_MAX_RAM_BANKS = 16;
    struct Cartridge
    {
        uint8_t* _rom;
        uint32_t _romSize;

        std::unique_ptr<uint8_t[]> _ram;
        uint8_t _ramBankCount;

        MBC _mbc = {};
    };

    bool LoadROM(Cartridge& cart, uint8_t* rom, uint32_t romSize);
    void MapCartridgeROM(Cartridge& cart, MMU& mmu);
    
    void TickMBC(Cartridge& cart, MMU& mmu);

}