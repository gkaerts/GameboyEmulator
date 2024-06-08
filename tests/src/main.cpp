#include "gtest/gtest.h"

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


#include "SM83.hpp"
#include "memory.hpp"
TEST(CPUTests, SimpleTest)
{
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

        0x06, 0x00, // LB B, 00h
        0x0E, 0x00, // LD C, 00h
        0x03,       // INC BC
        0x03,       // INC BC,
        0x0B,       // DEC BC
        0x00,       // NOP
    };


    emu::SM83::MemoryController memCtrl =
    {
        ._memory = memory
    };

    
    emu::SM83::CPU cpu;
    emu::SM83::Boot(&cpu, 0, 0);

    // First fetch cycle
    emu::SM83::Tick(&cpu, memCtrl, 4);

    // Start executing instructions
    emu::SM83::Tick(&cpu, memCtrl, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 1);

    emu::SM83::Tick(&cpu, memCtrl, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 2);

    emu::SM83::Tick(&cpu, memCtrl, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 3);

    emu::SM83::Tick(&cpu, memCtrl, 4);
    ASSERT_EQ(cpu._registers._reg8.B, 3);

    emu::SM83::Tick(&cpu, memCtrl, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 6);

    emu::SM83::Tick(&cpu, memCtrl, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 0);

    emu::SM83::Tick(&cpu, memCtrl, 8);
    ASSERT_EQ(cpu._registers._reg8.A, 0xA5);

    emu::SM83::Tick(&cpu, memCtrl, 8);
    ASSERT_EQ(cpu._registers._reg8.B, 0x02);

    emu::SM83::Tick(&cpu, memCtrl, 4);
    ASSERT_EQ(cpu._registers._reg8.A, 0xA7);

    emu::SM83::Tick(&cpu, memCtrl, 32);
    ASSERT_EQ(memory[0xFF], 0x02);

    emu::SM83::Tick(&cpu, memCtrl, 24);
    ASSERT_EQ(cpu._registers._reg16.BC, 1);

    emu::SM83::Tick(&cpu, memCtrl, 8);
    ASSERT_EQ(cpu._registers._reg16.BC, 2);

    emu::SM83::Tick(&cpu, memCtrl, 8);
    ASSERT_EQ(cpu._registers._reg16.BC, 1);
}