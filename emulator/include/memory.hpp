#pragma once

#include "common.hpp"

namespace emu::SM83
{
    struct MemoryController
    {
        uint8_t* _memory;
    };

    struct IO;
    void TickMemoryController(MemoryController& memCtrl, IO& cpuIO);
}