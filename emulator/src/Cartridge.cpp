#include "Cartridge.hpp"
#include "MMU.hpp"

namespace emu::SM83
{
    namespace
    {
        enum CartAddresses
        {
            ADDR_CART_TYPE =        0x0147,
            ADDR_ROM_SIZE =         0x0148,
            ADDR_RAM_SIZE =         0x0149,
            ADDR_HEADER_CHECKSUM =  0x014D,
        };

        constexpr const uint16_t RAM_BANK_SIZE = 8 * 1024;
        constexpr const uint8_t RAM_BANK_COUNT_LUT[] =
        {
            0,
            0,
            1,
            4,
            16,
            8
        };

        bool CartridgeHasRAM(uint8_t cartType)
        {
            switch (cartType)
            {
            case 0x02:
            case 0x03:
            case 0x08:
            case 0x09:
            case 0x0C:
            case 0x0D:
            case 0x10:
            case 0x12:
            case 0x13:
            case 0x1A:
            case 0x1B:
            case 0x1D:
            case 0x1E:
            case 0x22:
            case 0xFF:
                return true;
                break;
            
            default:
                return false;
                break;
            }
        }

        bool CartridgeHasBattery(uint8_t cartType)
        {
            switch (cartType)
            {
            case 0x03:
            case 0x06:
            case 0x09:
            case 0x0D:
            case 0x0F:
            case 0x10:
            case 0x13:
            case 0x1B:
            case 0x1E:
            case 0x22:
            case 0xFF:
                return true;
                break;
            
            default:
                return false;
                break;
            }
        }

        MBCType CartridgeMBCType(uint8_t cartType)
        {
            MBCType mbc = MBCType::None;
            switch (cartType)
            {
            case 0x01:
            case 0x02:
            case 0x03:
                mbc = MBCType::MBC1;
                break;
            case 0x05:
            case 0x06:
                mbc = MBCType::MBC2;
                break;
            case 0x0B:
            case 0x0C:
            case 0x0D:
                mbc = MBCType::MMM01;
                break;
            case 0x0F:
            case 0x10:
            case 0x12:
            case 0x13:
                mbc = MBCType::MBC3;
                break;
            case 0x19:
            case 0x1A:
            case 0x1B:
            case 0x1C:
            case 0x1D:
            case 0x1E:
                mbc = MBCType::MBC5;
                break;
            case 0x20:
                mbc = MBCType::MBC6;
                break;
            case 0x22:
                mbc = MBCType::MBC7;
                break;
            case 0xFE:
                mbc = MBCType::HuC3;
                break;
            case 0xFF:
                mbc = MBCType::HuC1;
                break;
            default:
                break;
            }

            return mbc;
        }

        uint8_t CartridgeHeaderChecksum(uint8_t* rom)
        {
            uint8_t cs = 0;
            for (uint16_t addr = 0x0134; addr <= 0x014C; ++addr)
            {
                cs = cs - rom[addr] - 1;
            }

            return cs;
        }
    }

    bool LoadROM(Cartridge& cart, uint8_t* rom, uint32_t romSize)
    {
        if (!rom || !romSize)
        {
            return false;
        }

        uint32_t expectedROMSize = (32 * 1024) * (1 << rom[ADDR_ROM_SIZE]);
        if (romSize != expectedROMSize)
        {
            return false;
        }

        uint8_t checksum = CartridgeHeaderChecksum(rom);
        if (checksum != rom[ADDR_HEADER_CHECKSUM])
        {
            return false;
        }

        cart._rom = rom;
        cart._romSize = romSize;

        if (CartridgeHasRAM(rom[ADDR_CART_TYPE]))
        {
            uint8_t numRAMBanks = RAM_BANK_COUNT_LUT[rom[ADDR_RAM_SIZE]];
            cart._ram = std::make_unique<uint8_t[]>(numRAMBanks * RAM_BANK_SIZE);
            cart._ramBankCount = numRAMBanks;
        }
        else
        {
            cart._ram.reset();
            cart._ramBankCount = 0;
        }
        
        cart._mbc._type = CartridgeMBCType(rom[ADDR_CART_TYPE]);

        return true;
    }

    void MapCartridgeROM(Cartridge& cart, MMU& mmu)
    {
        MapMemoryRegion(mmu, 0x0000, 16 * 1024, cart._rom, MMRF_ReadOnly);
        MapMemoryRegion(mmu, 0x4000, 16 * 1024, cart._rom + 16 * 1024, MMRF_ReadOnly);
    }

    namespace
    {

        enum
        {
            MBC1_REG_RAM_ENABLED = 0xA,

            MBC1_REG_RAM_ENABLE_BEGIN = 0x0000,
            MBC1_REG_RAM_ENABLED_END =  0x1FFF,

            MBC1_REG_ROM_BANK_BEGIN =   0x2000,
            MBC1_REG_ROM_BANK_END =     0x3FFF,

            MBC1_REG_RAM_BANK_BEGIN =   0x4000,
            MBC1_REG_RAM_BANK_END =     0x5FFF,

            MBC1_REG_BANK_MODE_BEGIN =  0x6000,
            MBC1_REG_BANK_MODE_END =    0x7FFF
        };

        void TickMBC1(Cartridge& cart, MMU& mmu)
        {
            MBC& mbc = cart._mbc;
            if (mmu._RW == MMU_WRITE)
            {
                // Check for RAM enable writes
                if (mmu._address >= MBC1_REG_RAM_ENABLE_BEGIN &&
                    mmu._address <= MBC1_REG_RAM_ENABLED_END)
                {
                    if (mbc._MBC1._RAMEnable != (mmu._data & 0xF))
                    {
                        mbc._MBC1._RAMEnable = mmu._data & 0xF;
                        if (mbc._MBC1._RAMEnable == MBC1_REG_RAM_ENABLED)
                        {
                            uint8_t* ramPtr = cart._ram.get() + (8 * 1024) * mbc._MBC1._RAMBankNumber;
                            MapMemoryRegion(mmu, 0xA000, 8 * 1024, ramPtr, 0);
                        }
                        else
                        {
                            UnmapMemoryRegion(mmu, 0xA000, 8 * 1024);
                        }
                    }
                }

                // Check for ROM bank number writes
                else if (mmu._address >= MBC1_REG_ROM_BANK_BEGIN &&
                         mmu._address <= MBC1_REG_ROM_BANK_END)
                {
                    uint8_t bankNumber = mmu._data & 0x1F;
                    if (bankNumber == 0)
                    {
                        bankNumber = 1;
                    }

                    uint32_t romBankCount = cart._romSize / (16 * 1024);

                    if (bankNumber < romBankCount &&
                        bankNumber != mbc._MBC1._ROMBankNumber)
                    {
                        mbc._MBC1._ROMBankNumber = bankNumber;
                        uint8_t* romPtr = cart._rom + (16 * 1024) * mbc._MBC1._ROMBankNumber;
                        MapMemoryRegion(mmu, 0x4000, 16 * 1024, romPtr, MMRF_ReadOnly);
                    }
                }

                // Check for RAM bank number writes
                else if (mmu._address >= MBC1_REG_RAM_BANK_BEGIN &&
                         mmu._address <= MBC1_REG_RAM_BANK_END)
                {
                    uint8_t bankNumber = mmu._data & 0x03;
                    if (bankNumber < cart._ramBankCount &&
                        bankNumber != mbc._MBC1._RAMBankNumber)
                    {
                        mbc._MBC1._RAMBankNumber = bankNumber;
                        uint8_t* ramPtr = cart._ram.get() + (8 * 1024) * mbc._MBC1._RAMBankNumber;
                        MapMemoryRegion(mmu, 0xA000, 8 * 1024, ramPtr, 0);
                    }
                }

                // Check for RAM bank number writes
                else if (mmu._address >= MBC1_REG_BANK_MODE_BEGIN &&
                         mmu._address <= MBC1_REG_BANK_MODE_END)
                {
                    mbc._MBC1._BankModeSelect = mmu._data & 0x1;
                }
            }
        }
    }

    void TickMBC(Cartridge& cart, MMU& mmu)
    {
        switch (cart._mbc._type)
        {
        case MBCType::None:
            break;

        case MBCType::MBC1:
        {
            TickMBC1(cart, mmu);
        }
            break;
        
        default:
            EMU_ASSERT(0 && "MBC type not supported!");
            break;
        }
    }
}