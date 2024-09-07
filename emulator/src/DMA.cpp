#include "DMA.hpp"
#include "SM83.hpp"
#include "MMU.hpp"

namespace emu::SM83
{
    namespace
    {
        constexpr const uint16_t DMA_CYCLE_DURATION = 640;

        void LockMMURegionForDMA(MMU& mmu, uint16_t address, uint16_t size)
        {
            EMU_ASSERT((address % MMU_SEGMENT_SIZE) == 0);
            EMU_ASSERT(size > 0 && (size % MMU_SEGMENT_SIZE) == 0);
            EMU_ASSERT(uint32_t(address) + size <= 0x10000);

            uint16_t startSegment = address / MMU_SEGMENT_SIZE;
            uint16_t numSegments = size / MMU_SEGMENT_SIZE;

            for (uint16_t i = 0; i < numSegments; ++i)
            {
                mmu._segmentFlags[startSegment + i] |= MMRF_DMALock;
            }
        }

        void UnlockMMURegionForDMA(MMU& mmu, uint16_t address, uint16_t size)
        {
            EMU_ASSERT((address % MMU_SEGMENT_SIZE) == 0);
            EMU_ASSERT(size > 0 && (size % MMU_SEGMENT_SIZE) == 0);
            EMU_ASSERT(uint32_t(address) + size <= 0x10000);

            uint16_t startSegment = address / MMU_SEGMENT_SIZE;
            uint16_t numSegments = size / MMU_SEGMENT_SIZE;

            for (uint16_t i = 0; i < numSegments; ++i)
            {
                mmu._segmentFlags[startSegment + i] &= ~MMRF_DMALock;
            }
        }

        void DMAWrite(MMU& mmu, uint16_t address, uint8_t val)
        {
            EMU_ASSERT(address >= 0xFE00 && address < 0xFEA0);

            uint16_t segmentIdx = address / MMU_SEGMENT_SIZE;
            if (mmu._segmentFlags[segmentIdx] & MMRF_Redirect)
            {
                segmentIdx = MMU_SEGMENT_COUNT;
            }

            if (mmu._segmentFlags[segmentIdx] & MMRF_ReadOnly ||
                !mmu._segmentPtrs[segmentIdx])
            {
                return;
            }

            uint16_t offsetInSegment = address % MMU_SEGMENT_SIZE;
            mmu._segmentPtrs[segmentIdx][offsetInSegment] = val;
        }

        uint8_t DMARead(MMU& mmu, uint16_t address)
        {
            uint16_t segmentIdx = address / MMU_SEGMENT_SIZE;
            if (mmu._segmentFlags[segmentIdx] & MMRF_Redirect)
            {
                segmentIdx = MMU_SEGMENT_COUNT;
            }

            if (!mmu._segmentPtrs[segmentIdx])
            {
                return 0xFF;
            }

            uint16_t offsetInSegment = address % MMU_SEGMENT_SIZE;
            return mmu._segmentPtrs[segmentIdx][offsetInSegment];
        }

    }
    void TickOAMDMA(DMACtrl& dma, MMU& mmu, PeripheralIO& pIO)
    {
        if (pIO.OAM_DMA != dma._lastDMARegValue)
        {
            // Lock VRAM and OAM
            LockMMURegionForDMA(mmu, 0x8000, 8 * 1024);
            LockMMURegionForDMA(mmu, 0xFE00, 256);

            // Lock work RAM + echo
            LockMMURegionForDMA(mmu, 0xC000, 4 * 1024);
            LockMMURegionForDMA(mmu, 0xD000, 4 * 1024);
            LockMMURegionForDMA(mmu, 0xE000, 4 * 1024);  // Echo RAM
            LockMMURegionForDMA(mmu, 0xF000, 4 * 1024);  // Echo RAM

            dma._dmaActive = 1;
            dma._dmaCycles = 0;
            dma._lastDMARegValue = pIO.OAM_DMA;
        }

        if (dma._dmaActive)
        {
            if ((dma._dmaCycles % 4) == 0)
            {
                uint16_t src = (uint16_t(pIO.OAM_DMA) << 8) + (dma._dmaCycles / 4);
                uint16_t dest = 0xFE00 + (dma._dmaCycles / 4);
                DMAWrite(mmu, dest, DMARead(mmu, src));
            }
            
            dma._dmaCycles++;

            if (dma._dmaCycles >= DMA_CYCLE_DURATION)
            {
                dma._dmaActive = 0;
                dma._dmaCycles = 0;
                dma._lastDMARegValue = 0;
                pIO.OAM_DMA = 0;

                // Unlock VRAM and OAM
                UnlockMMURegionForDMA(mmu, 0x8000, 8 * 1024);
                UnlockMMURegionForDMA(mmu, 0xFE00, 256);

                // Unlock work RAM + echo
                UnlockMMURegionForDMA(mmu, 0xC000, 4 * 1024);
                UnlockMMURegionForDMA(mmu, 0xD000, 4 * 1024);
                UnlockMMURegionForDMA(mmu, 0xE000, 4 * 1024);  // Echo RAM
                UnlockMMURegionForDMA(mmu, 0xF000, 4 * 1024);  // Echo RAM
            }
        }
    }
}