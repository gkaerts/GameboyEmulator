#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

#include "SM83.hpp"
#include "MMU.hpp"

#include <vector>
#include <fstream>

using namespace nlohmann;

bool IsTestableOpCode(uint8_t opCode)
{
    switch (opCode)
    {
    case 0x10:  // STOP
    case 0x76:  // HALT
    case 0xF3:  // DI
    case 0xFB:  // EI

    // Invalid Opcodes
    case 0xD3:
    case 0xE3:
    case 0xE4:
    case 0xF4:
    case 0xDB:
    case 0xEB:
    case 0xEC:
    case 0xFC:
    case 0xDD:
    case 0xED:
    case 0xFD:
        return false;
        break;
    
    default:
        return true;
        break;
    }
}

class OpCodeTest : public testing::TestWithParam<uint16_t>
{
    public:

        virtual void SetUp() override
        {
            _memory = std::make_unique<uint8_t[]>(64 * 1024);
            emu::SM83::MapMemoryRegion(_mmu, 0, 64 * 1024, _memory.get(), 0);

            if (IsTestableOpCode(uint8_t(GetParam())))
            {
                char fileName[256]= {};
                snprintf(fileName, 
                    sizeof(fileName), 
                    "contrib/submodules/GameboyCPUTests/v2/%02x.json", 
                    uint8_t(GetParam()));

                std::ifstream f(fileName);
                _testData = json::parse(f);
            }
        }
        
        emu::SM83::CPU _cpu;
        emu::SM83::MMU _mmu;
        std::unique_ptr<uint8_t[]> _memory;
        json _testData;
};



#define OPCODE_TEST_OUT std::cout << "[          ] "

void SetCPUState(emu::SM83::CPU& cpu, const json& state, uint8_t* memory, uint8_t ir)
{
    cpu._registers._reg8.A = state["a"];
    cpu._registers._reg8.B = state["b"];
    cpu._registers._reg8.C = state["c"];
    cpu._registers._reg8.D = state["d"];
    cpu._registers._reg8.E = state["e"];
    cpu._registers._reg8.F = state["f"];
    cpu._registers._reg8.H = state["h"];
    cpu._registers._reg8.L = state["l"];

    cpu._registers._reg16.PC = state["pc"];
    cpu._registers._reg16.SP = state["sp"];

    cpu._registers._reg8.IR = ir;

    const json& ramState = state["ram"];
    for (const json& ramEntry : ramState)
    {
        size_t location = ramEntry[0];
        uint8_t value = ramEntry[1];
        
        memory[location] = value;
    }
}

TEST_P(OpCodeTest, TestOpCode)
{
    uint8_t opCode = uint8_t(GetParam());
    if (!IsTestableOpCode(opCode))
    {
        return;
    }

    OPCODE_TEST_OUT << "OP - " << emu::SM83::GetOpcodeName(emu::SM83::InstructionTable::Default, opCode) << std::endl;

    for (const json& test : _testData)
    {
        std::string testName = test["name"];
        OPCODE_TEST_OUT << testName << std::endl;
        //if (testName == "f8 b8 30")
        //    __debugbreak();

        emu::SM83::BootCPU(_cpu, 0, 0, 1);

        
        SetCPUState(_cpu, test["initial"], _memory.get(), opCode);

        const json& cycles = test["cycles"];
        for (const json& cycle : cycles)
        {
            emu::SM83::TickCPU(_cpu, _mmu, 4);

            if (!cycle.is_null())
            {
                ASSERT_EQ(_cpu._io._address, uint16_t(cycle[0]));
                ASSERT_EQ(_cpu._io._data, uint8_t(cycle[1]));      

                // if (!cycle[2].is_null())
                // {
                //     ASSERT_NE(_cpu._io._outPins.MRQ, 0);
                //     if (cycle[2] == "read")
                //     {
                //         ASSERT_NE(_cpu._io._outPins.RD, 0);
                //     }
                //     else if (cycle[2] == "write")
                //     {
                //         ASSERT_NE(_cpu._io._outPins.WR, 0);
                //     }
                // }
            }
        }

        // Check final state
        const json& finalState = test["final"];
        ASSERT_EQ(_cpu._registers._reg8.A, uint8_t(finalState["a"]));
        ASSERT_EQ(_cpu._registers._reg8.B, uint8_t(finalState["b"]));
        ASSERT_EQ(_cpu._registers._reg8.C, uint8_t(finalState["c"]));
        ASSERT_EQ(_cpu._registers._reg8.D, uint8_t(finalState["d"]));
        ASSERT_EQ(_cpu._registers._reg8.E, uint8_t(finalState["e"]));
        ASSERT_EQ(_cpu._registers._reg8.F, uint8_t(finalState["f"]));
        ASSERT_EQ(_cpu._registers._reg8.H, uint8_t(finalState["h"]));
        ASSERT_EQ(_cpu._registers._reg8.L, uint8_t(finalState["l"]));

        ASSERT_EQ(_cpu._registers._reg16.PC, uint16_t(finalState["pc"]));
        ASSERT_EQ(_cpu._registers._reg16.SP, uint16_t(finalState["sp"]));

        const json& ramState = finalState["ram"];
        for (const json& ramEntry : ramState)
        {
            ASSERT_EQ(_memory[size_t(ramEntry[0])], uint8_t(ramEntry[1]));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    SM83, 
    OpCodeTest, 
    testing::Range<uint16_t>(0x00, 0x100),
    //testing::Values<uint16_t>(0xCB),
    [](const testing::TestParamInfo<OpCodeTest::ParamType>& info) {
        std::string name = "OpCode_0xFF";
        std::snprintf(name.data(), name.length() + 1, "OpCode_0x%02X", info.param);
        return name;
    });

TEST(UseCaseTests, MemCopyTest)
{
    std::unique_ptr<uint8_t[]> RAM = std::make_unique<uint8_t[]>(64 * 1024);

    emu::SM83::MMU mmu;
    emu::SM83::MapMemoryRegion(mmu, 0, 64 * 1024, RAM.get(), 0);


    emu::SM83::CPU cpu;
    emu::SM83::BootCPU(cpu, 0, 0, 1);
    emu::SM83::MapPeripheralIOMemory(cpu, mmu);

    // Payload
    RAM[0x1000] = 0xF0;
    RAM[0x1001] = 0xF1;
    RAM[0x1002] = 0xF2;
    RAM[0x1003] = 0xF3;
    RAM[0x1004] = 0xF4;
    RAM[0x1005] = 0xF5;
    RAM[0x1006] = 0xF6;
    RAM[0x1007] = 0xF7;

    // Program
    const uint8_t program[] =
    {
        0x31, 0xFE, 0xFF, // 0x00: LD SP, $fffe
        0xAF,             // 0x03: XOR A
        0x11, 0x00, 0x10, // 0x04: LD DE, 0x1000
        0x21, 0x00, 0x20, // 0x07: LD HL, 0x2000
        0x06, 0x08,       // 0x0A: LD B, 0x08
        0xCD, 0x12, 0x00, // 0x0C: CALL 0x0012
        0xC3, 0x00, 0x30, // 0x0F: JP 0x3000

        0x1A,             // 0x12: LD A, (DE)
        0x22,             // 0x13: LD (HL+), A
        0x1C,             // 0x14: INC E
        0x05,             // 0x15: DEC B
        0xC2, 0x12, 0x0,  // 0x16: JP NZ 0x0012
        0xC9,             // 0x19: RET
        0x00,             // 0x20: NOP

    };
    memcpy(RAM.get(), program, sizeof(program));

    for (int i = 0; i < 1000; ++i)
    {
        emu::SM83::TickCPU(cpu, mmu, 4);

        if (cpu._decoder._nextMCycleIndex == 0)
        {
            std::printf("%s\n", emu::SM83::GetOpcodeName(cpu._decoder._table, cpu._registers._reg8.IR));
        }
    }

    EXPECT_EQ(RAM[0x2000], 0xF0);
}

TEST(UseCaseTests, InterruptTests)
{
    std::unique_ptr<uint8_t[]> RAM = std::make_unique<uint8_t[]>(64 * 1024);

    constexpr const uint8_t program[] =
    {
        0x31, 0xFE, 0xFF, // 0x00: LD SP, $fffe         - Set up stack
        0x3E, 0x01,       // 0x03: LD A, 0x01           - Prepare 0x01 into A
        0xE0, 0x0F,       // 0x05: LD ($FF00+0F), A     - Pull A into IF register
        0xE0, 0xFF,       // 0x07: LD ($FF00+FF), A     - Pull A into IE register
        0xFB,             // 0x09: EI                   - Enable interrupts, should jump to 0x40 as interrupt handler
        0x00,             // 0x0A: NOP
        0xC3, 0x0A, 0x00, // 0x0B: JP $000A             - Infinite loop


        // Sea of NOPs until 0x40
        0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF3,

        0x3E, 0x2F,         // 0x40: LD A, 0x2F         - Load dummy value into A as proof we hit the interrupt
        0xD9,               // 0x42: RETI               - Return and enable interrupts
    };
    memcpy(RAM.get(), program, sizeof(program));

    emu::SM83::MMU mmu;
    emu::SM83::MapMemoryRegion(mmu, 0, 64 * 1024, RAM.get(), 0);


    emu::SM83::CPU cpu;
    emu::SM83::BootCPU(cpu, 0, 0, 1);
    emu::SM83::MapPeripheralIOMemory(cpu, mmu);
    
    for (int i = 0; i < 1000; ++i)
    {
        if (cpu._decoder._nextMCycleIndex == 0)
        {
            std::printf("$%04x: %s\n", cpu._registers._reg16.PC, emu::SM83::GetOpcodeName(cpu._decoder._table, cpu._registers._reg8.IR));
        }

        emu::SM83::TickCPU(cpu, mmu, 4);
    }

    EXPECT_EQ(cpu._registers._reg8.A, 0x2F);
    EXPECT_EQ(cpu._registers._reg8.IME, 1);
}