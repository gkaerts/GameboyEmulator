#include "gtest/gtest.h"

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


#include "LR35902.hpp"
TEST(CPUTests, SimpleTest)
{
    emu::LR35902::CPU cpu;
    emu::LR35902::Boot(&cpu, 0, 0);

    uint8_t memory[2048] =
    {
        0x3C,       // INC A
        0x3C,       // INC A
        0x3C,       // INC A
        0x47,       // LD B, A
        0x80,       // ADD A, B
        0xAF,       // XOR A
        0x3E, 0xA5, // LD A, A5h
        0x06, 0x02, // LD B, 02h
        0x80,       // ADD A, B
        0x3E, 0x00, // LD A, 00h
        0x67,       // LD H, A
        0x3E, 0xFF, // LD A, FFh
        0x6F,       // LD L, A
        0x70,       // LD (HL), B
        0x00,       // NOP
    };

    emu::LR35902::Tick(&cpu, memory, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 1);

    emu::LR35902::Tick(&cpu, memory, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 2);

    emu::LR35902::Tick(&cpu, memory, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 3);

    emu::LR35902::Tick(&cpu, memory, 4);
    ASSERT_EQ(cpu._registers._reg8.B, 3);

    emu::LR35902::Tick(&cpu, memory, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 6);

    emu::LR35902::Tick(&cpu, memory, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 0);

    emu::LR35902::Tick(&cpu, memory, 8);
    ASSERT_EQ(cpu._registers._reg8.A, 0xA5);

    emu::LR35902::Tick(&cpu, memory, 8);
    ASSERT_EQ(cpu._registers._reg8.B, 0x02);

    emu::LR35902::Tick(&cpu, memory, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 0xA7);

    emu::LR35902::Tick(&cpu, memory, 32);
    ASSERT_EQ(memory[0xFF], 0x02);
}