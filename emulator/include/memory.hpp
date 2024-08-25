#pragma once

#include "common.hpp"

namespace emu::SM83
{
    struct MemoryController;
    using MemCtrlWriteFn = void(*)(MemoryController&, uint16_t, uint8_t);
    using MemCtrlReadFn = uint8_t(*)(MemoryController&, uint16_t);

    struct MemoryController
    {
        uint8_t _memory[64 * 1024];
        MemCtrlWriteFn _writeFn;
        MemCtrlReadFn _readFn;
    };

    MemoryController MakeTestMemoryController();
    MemoryController MakeGBMemoryController();

    struct IO;
    void TickMemoryController(MemoryController& memCtrl, IO& cpuIO);
}