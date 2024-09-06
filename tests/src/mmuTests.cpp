#include "gtest/gtest.h"

#include "MMU.hpp"

TEST(MMUTests, UnmappedRegionReadsReturn0xFF)
{
    emu::SM83::MMU mmu;
    EXPECT_EQ(emu::SM83::MMURead(mmu, 0xAABB), 0xFF);
}

TEST(MMUTests, UnmappedRegionWritesDontCrash)
{
    emu::SM83::MMU mmu;
    emu::SM83::MMUWrite(mmu, 0xABCD, 0xAA);
}

TEST(MMUTests, CanWriteToMappedRegion)
{
    emu::SM83::MMU mmu;

    uint8_t localMem[512] = {};
    emu::SM83::MapMemoryRegion(mmu, 0xF000, sizeof(localMem), localMem, 0);
    emu::SM83::MMUWrite(mmu, 0xF0FA, 0xA1);

    EXPECT_EQ(localMem[0xFA], 0xA1);
}

TEST(MMUTests, CanReadFromMappedRegion)
{
    emu::SM83::MMU mmu;

    uint8_t localMem[512] = {};
    localMem[0xFB] = 0xA2;

    emu::SM83::MapMemoryRegion(mmu, 0xF000, sizeof(localMem), localMem, 0);
    EXPECT_EQ(emu::SM83::MMURead(mmu, 0xF0FB), 0xA2);
}

TEST(MMUTests, CanUnmapRegion)
{
    emu::SM83::MMU mmu;

    uint8_t localMem[512] = {};
    localMem[0xFB] = 0xA2;

    emu::SM83::MapMemoryRegion(mmu, 0xF000, sizeof(localMem), localMem, 0);
    emu::SM83::UnmapMemoryRegion(mmu, 0xF000, sizeof(localMem));

    EXPECT_EQ(emu::SM83::MMURead(mmu, 0xF0FB), 0xFF);
}

TEST(MMUTests, CannotWriteToReadOnlyRegions)
{
    emu::SM83::MMU mmu;

    uint8_t localMem[512] = {};
    localMem[0x13] = 0xA3;

    emu::SM83::MapMemoryRegion(mmu, 0xF000, sizeof(localMem), localMem, emu::SM83::MMRF_ReadOnly);
    emu::SM83::MMUWrite(mmu, 0xF013, 0xA4);

    EXPECT_EQ(emu::SM83::MMURead(mmu, 0xF013), 0xA3);
}

TEST(MMUTests, CanRedirectZeroSegment)
{
    emu::SM83::MMU mmu;

    uint8_t redirectMem[256] = {};
    redirectMem[0x13] = 0xAA;

    uint8_t localMem[256] = {};
    localMem[0x13] = 0xA3;

    emu::SM83::MapMemoryRegion(mmu, 0x0000, sizeof(localMem), localMem, 0);
    EXPECT_EQ(emu::SM83::MMURead(mmu, 0x0013), 0xA3);

    emu::SM83::RedirectZeroSegment(mmu, redirectMem);
    EXPECT_EQ(emu::SM83::MMURead(mmu, 0x0013), 0xAA);

    emu::SM83::MMUWrite(mmu, 0x0013, 0xB1);
    EXPECT_EQ(emu::SM83::MMURead(mmu, 0x0013), 0xAA);

    emu::SM83::RemoveZeroSegmentRedirect(mmu);
    EXPECT_EQ(emu::SM83::MMURead(mmu, 0x0013), 0xA3);

    emu::SM83::MMUWrite(mmu, 0x0013, 0xB1);
    EXPECT_EQ(emu::SM83::MMURead(mmu, 0x0013), 0xB1);

}