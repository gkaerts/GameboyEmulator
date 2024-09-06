#pragma once

#include "common.hpp"

namespace emu::SM83
{
    static constexpr uint16_t MMU_SEGMENT_SIZE = 256;
    static constexpr uint16_t MMU_SEGMENT_COUNT = (64 * 1024) / 256;
    struct MMU
    {
        uint8_t* _segmentPtrs[MMU_SEGMENT_COUNT + 1] = {};
        uint8_t _segmentFlags[MMU_SEGMENT_COUNT + 1] = {};
    };

    enum MMRegionFlags
    {
        MMRF_ReadOnly = 0x01,
        MMRF_Redirect = 0x02,
    };

    enum class MMRegionHandle : uint64_t {};

    void MapMemoryRegion(MMU& mmu, uint16_t address, uint32_t size, uint8_t* ptr, uint8_t flags);
    void UnmapMemoryRegion(MMU& mmu, uint16_t address, uint32_t size);

    void RedirectZeroSegment(MMU& mmu, uint8_t* ptr);
    void RemoveZeroSegmentRedirect(MMU& mmu);

    void MMUWrite(MMU& mmu, uint16_t address, uint8_t val);
    uint8_t MMURead(MMU& mmu, uint16_t address);

}