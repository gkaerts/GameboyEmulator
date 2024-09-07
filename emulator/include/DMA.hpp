#pragma once

#include "common.hpp"

namespace emu::SM83
{
    struct PeripheralIO;
    struct MMU;

    struct DMACtrl
    {
        uint16_t _dmaCycles = 0;
        uint8_t _dmaActive = 0;
        uint8_t _lastDMARegValue = 0;
    };

    void TickOAMDMA(DMACtrl& dma, MMU& mmu, PeripheralIO& pIO);
}