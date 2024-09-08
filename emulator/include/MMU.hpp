#pragma once

#include "common.hpp"

namespace emu::SM83
{
    static constexpr uint16_t MMU_SEGMENT_SIZE = 256;
    static constexpr uint16_t MMU_SEGMENT_COUNT = (64 * 1024) / 256;

    static constexpr uint8_t MMU_READ = 1;
    static constexpr uint8_t MMU_WRITE = 2;
    struct MMU
    {
        uint8_t* _segmentPtrs[MMU_SEGMENT_COUNT + 1] = {};
        uint8_t _segmentFlags[MMU_SEGMENT_COUNT + 1] = {};

        uint16_t _address = 0;
        uint16_t _RW = 0;
        uint8_t _data = 0;
    };

    enum MMRegionFlags
    {
        MMRF_ReadOnly = 0x01,
        MMRF_Redirect = 0x02,
        MMRF_DMALock = 0x04,
    };

    enum class MMRegionHandle : uint64_t {};

    void MapMemoryRegion(MMU& mmu, uint16_t address, uint32_t size, uint8_t* ptr, uint8_t flags);
    void UnmapMemoryRegion(MMU& mmu, uint16_t address, uint32_t size);

    void RedirectZeroSegment(MMU& mmu, uint8_t* ptr);
    void RemoveZeroSegmentRedirect(MMU& mmu);

    void MMUWrite(MMU& mmu, uint16_t address, uint8_t val);
    uint8_t MMURead(MMU& mmu, uint16_t address);

}