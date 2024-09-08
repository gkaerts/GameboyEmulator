#include "MMU.hpp"
#include "SM83.hpp"

namespace emu::SM83
{
    void MapMemoryRegion(MMU& mmu, uint16_t address, uint32_t size, uint8_t* ptr, uint8_t flags)
    {
        EMU_ASSERT((address % MMU_SEGMENT_SIZE) == 0);
        EMU_ASSERT(size > 0 && (size % MMU_SEGMENT_SIZE) == 0);
        EMU_ASSERT(uint32_t(address) + size <= 0x10000);

        uint16_t startSegment = address / MMU_SEGMENT_SIZE;
        uint16_t numSegments = size / MMU_SEGMENT_SIZE;

        for (uint16_t i = 0; i < numSegments; ++i)
        {
            mmu._segmentPtrs[startSegment + i] = ptr + i * MMU_SEGMENT_SIZE;
            mmu._segmentFlags[startSegment + i] = flags;
        }
    }

    void UnmapMemoryRegion(MMU& mmu, uint16_t address, uint32_t size)
    {
        EMU_ASSERT((address % MMU_SEGMENT_SIZE) == 0);
        EMU_ASSERT(size > 0 && (size % MMU_SEGMENT_SIZE) == 0);
        EMU_ASSERT(uint32_t(address) + size <= 0x10000);

        uint16_t startSegment = address / MMU_SEGMENT_SIZE;
        uint16_t numSegments = size / MMU_SEGMENT_SIZE;

        for (uint16_t i = 0; i < numSegments; ++i)
        {
            mmu._segmentPtrs[startSegment + i] = nullptr;
            mmu._segmentFlags[startSegment + i] = 0;
        }
    }

    void RedirectZeroSegment(MMU& mmu, uint8_t* ptr)
    {
        mmu._segmentFlags[0] |= MMRF_Redirect;
        mmu._segmentFlags[MMU_SEGMENT_COUNT] = MMRF_ReadOnly;
        mmu._segmentPtrs[MMU_SEGMENT_COUNT] = ptr;
    }

    void RemoveZeroSegmentRedirect(MMU& mmu)
    {
        mmu._segmentPtrs[MMU_SEGMENT_COUNT] = nullptr;
        mmu._segmentFlags[MMU_SEGMENT_COUNT] = 0;
        mmu._segmentFlags[0] &= ~MMRF_Redirect;
    }

    void MMUWrite(MMU& mmu, uint16_t address, uint8_t val)
    {
        mmu._address = address;
        mmu._RW = MMU_WRITE;
        mmu._data = val;

        uint16_t segmentIdx = address / MMU_SEGMENT_SIZE;
        if (mmu._segmentFlags[segmentIdx] & MMRF_Redirect)
        {
            segmentIdx = MMU_SEGMENT_COUNT;
        }

        if (mmu._segmentFlags[segmentIdx] & MMRF_ReadOnly ||
            mmu._segmentFlags[segmentIdx] & MMRF_DMALock ||
            !mmu._segmentPtrs[segmentIdx])
        {
            return;
        }

        uint16_t offsetInSegment = address % MMU_SEGMENT_SIZE;
        mmu._segmentPtrs[segmentIdx][offsetInSegment] = val;
    }

    uint8_t MMURead(MMU& mmu, uint16_t address)
    {
        mmu._address = address;
        mmu._RW = MMU_READ;

        uint16_t segmentIdx = address / MMU_SEGMENT_SIZE;
        if (mmu._segmentFlags[segmentIdx] & MMRF_Redirect)
        {
            segmentIdx = MMU_SEGMENT_COUNT;
        }

        uint8_t val = 0;
        if (!mmu._segmentPtrs[segmentIdx] || 
            mmu._segmentFlags[segmentIdx] & MMRF_DMALock)
        {
            val = 0xFF;
        }
        else
        {
            uint16_t offsetInSegment = address % MMU_SEGMENT_SIZE;
            val = mmu._segmentPtrs[segmentIdx][offsetInSegment];
        }

        mmu._data = val;
        return val;
    }
}