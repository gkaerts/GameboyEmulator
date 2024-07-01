#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

#include "SM83.hpp"
#include "memory.hpp"

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

    // Unimplemented Opcodes
    case 0x18:
    case 0x20:
    case 0x28:
    case 0x30:
    case 0x38:

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

class OpCodeTest : public testing::TestWithParam<uint8_t>
{
    public:

        virtual void SetUp() override
        {
            _memory.resize(64 * 1024 * 1024);
            _memCtrl = {
                ._memory = _memory.data()
            };

            if (IsTestableOpCode(GetParam()))
            {
                char fileName[256]= {};
                snprintf(fileName, 
                    sizeof(fileName), 
                    "contrib/submodules/GameboyCPUTests/v2/%02x.json", 
                    GetParam());

                std::ifstream f(fileName);
                _testData = json::parse(f);
            }
        }
        
        emu::SM83::CPU _cpu;
        emu::SM83::MemoryController _memCtrl;
        std::vector<uint8_t> _memory;
        json _testData;
};



#define OPCODE_TEST_OUT std::cout << "[          ] "

void SetCPUState(emu::SM83::CPU& cpu, const json& state, std::vector<uint8_t>& memory, uint8_t ir)
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
    uint8_t opCode = GetParam();
    if (!IsTestableOpCode(opCode))
    {
        return;
    }

    OPCODE_TEST_OUT << "OP - " << emu::SM83::GetOpcodeName(opCode) << std::endl;

    for (const json& test : _testData)
    {
        std::string testName = test["name"];
        OPCODE_TEST_OUT << testName << std::endl;
        if (testName == "f8 b8 30")
            __debugbreak();

        emu::SM83::Boot(&_cpu, 0, 0);
        SetCPUState(_cpu, test["initial"], _memory, opCode);

        const json& cycles = test["cycles"];
        for (const json& cycle : cycles)
        {
            emu::SM83::Tick(&_cpu, _memCtrl, 4);

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
    testing::Range<uint8_t>(0x00, 0xC0),
    //testing::Values<uint8_t>(0xF9),
    [](const testing::TestParamInfo<OpCodeTest::ParamType>& info) {
        std::string name = "OpCode_0xFF";
        std::snprintf(name.data(), name.length() + 1, "OpCode_0x%02X", info.param);
        return name;
    });