
#include "PPU.hpp"
#include "SM83.hpp"
#include <intrin.h>
#include <algorithm>

namespace emu::SM83
{
    namespace
    {
        constexpr const uint16_t VRAM_ADDR = 0x8000;
        constexpr const uint16_t VRAM_SIZE = 8 * 1024;

        constexpr const uint16_t OAM_ADDR = 0xFE00;
        constexpr const uint16_t OAM_SIZE = 256;

        constexpr const uint16_t TILE_DATA_BEGIN = 0x8000;
        constexpr const uint16_t TILE_DATA_END = 0x9800;

        constexpr const uint16_t TILE_MAP_0_BEGIN = 0x9800;
        constexpr const uint16_t TILE_MAP_0_END = 0x9C00; 

        constexpr const uint16_t TILE_MAP_1_BEGIN = 0x9C00;
        constexpr const uint16_t TILE_MAP_1_END = 0xA000;

        constexpr const uint16_t CYCLES_PER_SCANLINE = 456;
        constexpr const uint16_t CYCLES_PER_OAM_SCAN = 80;
        constexpr const uint16_t SCANLINE_COUNT = 154;

        constexpr const uint8_t SPRITE_SIZE = 8;
        constexpr const uint8_t SPRITE_SIZE_TALL = 16;

        constexpr const uint8_t INT_BIT_VBLANK = 1 << 0;
        constexpr const uint8_t INT_BIT_STAT = 1 << 1;

        struct LCDControl
        {
            union
            {
                uint8_t _u8;

                struct
                {
                    uint8_t _bgWindowEnabled : 1;
                    uint8_t _spriteEnabled : 1;
                    uint8_t _spriteSize : 1;
                    uint8_t _bgTileMapSelect : 1;
                    uint8_t _tileDataSelect : 1;
                    uint8_t _windowDisplayEnable : 1;
                    uint8_t _windowTileMapSelect : 1;
                    uint8_t _displayEnable : 1;
                } _bits;
            };  
        };
        static_assert(sizeof(LCDControl) == sizeof(uint8_t));

        bool PixelFIFOEmpty(PixelFIFO& fifo)
        {
            return !fifo._count;
        }

        bool PixelFIFOFull(PixelFIFO& fifo)
        {
            return fifo._count == 8;
        }

        void PixelFIFOClear(PixelFIFO& fifo)
        {
            fifo._count = 0;
        }

        void PixelFIFOPushBGTile(PixelFIFO& fifo, uint8_t tileLow, uint8_t tileHigh)
        {
            EMU_ASSERT(PixelFIFOEmpty(fifo));

            fifo._indicesLow = tileLow;
            fifo._indicesHigh = tileHigh;
            fifo._paletteIDsLow = 0;
            fifo._paletteIDsHigh = 0;
            fifo._priorities = 0;
            fifo._count += 8;
        }

        constexpr const uint8_t PALETTE_ID_BGP = 0;
        constexpr const uint8_t PALETTE_ID_OBP0 = 1;
        constexpr const uint8_t PALETTE_ID_OBP1 = 2;

        void PixelFIFOPushSpriteTile(PixelFIFO& fifo, uint8_t tileLow, uint8_t tileHigh, uint8_t palette, uint8_t priority)
        {
            uint8_t priorityBits = (priority ? 0xFF : 0x00);
            uint8_t paletteBitsHigh = (palette & 0x02) ? 0xFF : 0x00;
            uint8_t paletteBitsLow = (palette & 0x01) ? 0xFF : 0x00;

            uint8_t priorityDiff = fifo._priorities ^ priorityBits;
            uint8_t pixelsToOverwrite = ~(fifo._indicesLow | fifo._indicesHigh) | (priorityDiff & ~priorityBits);

            fifo._indicesLow = (fifo._indicesLow & ~pixelsToOverwrite) | (pixelsToOverwrite & tileLow);
            fifo._indicesHigh = (fifo._indicesHigh & ~pixelsToOverwrite) | (pixelsToOverwrite & tileHigh);
            fifo._paletteIDsLow = (fifo._paletteIDsLow & ~pixelsToOverwrite) | (pixelsToOverwrite & paletteBitsLow);
            fifo._paletteIDsHigh = (fifo._paletteIDsHigh & ~pixelsToOverwrite) | (pixelsToOverwrite & paletteBitsHigh);
            fifo._priorities = (fifo._priorities & ~pixelsToOverwrite) | (pixelsToOverwrite & priorityBits);
            fifo._count = 8;
        }

        void ShiftFIFO(PixelFIFO& fifo)
        {
            fifo._indicesHigh = fifo._indicesHigh << 1;
            fifo._indicesLow = fifo._indicesLow << 1;
            fifo._paletteIDsHigh = fifo._paletteIDsHigh << 1;
            fifo._paletteIDsLow = fifo._paletteIDsLow << 1;
            fifo._count--;
        }

        uint8_t PixelFIFOPop(PixelFIFO& bgFifo, PixelFIFO& spriteFifo, uint8_t BGP, uint8_t OBP0, uint8_t OBP1)
        {
            EMU_ASSERT(!PixelFIFOEmpty(bgFifo));

            const uint8_t palettes[] =
            {
                BGP, OBP0, OBP1
            };

            // Build visible sprite pixel mask
            uint8_t opaqueSpritePixels = spriteFifo._indicesLow | spriteFifo._indicesHigh;
            uint8_t spritesHavePriority = ~spriteFifo._priorities;
            uint8_t transparentBGPixels = ~(bgFifo._indicesLow | bgFifo._indicesHigh);
            uint8_t visibleSpritePixels = opaqueSpritePixels & (spritesHavePriority | transparentBGPixels);

            // Build merged FIFO
            uint8_t indicesHigh = (visibleSpritePixels & spriteFifo._indicesHigh) | (~visibleSpritePixels & bgFifo._indicesHigh);
            uint8_t indicesLow = (visibleSpritePixels & spriteFifo._indicesLow) | (~visibleSpritePixels & bgFifo._indicesLow);
            uint8_t paletteIDsHigh = (visibleSpritePixels & spriteFifo._paletteIDsHigh) | (~visibleSpritePixels & bgFifo._paletteIDsHigh);
            uint8_t paletteIDsLow = (visibleSpritePixels & spriteFifo._paletteIDsLow) | (~visibleSpritePixels & bgFifo._paletteIDsLow);

            uint8_t colorIdx = 
                (indicesHigh & 0x80) >> 6 |
                (indicesLow & 0x80) >> 7;

            uint8_t paletteID = 
                (paletteIDsHigh & 0x80) >> 6 |
                (paletteIDsLow & 0x80) >> 7;

            ShiftFIFO(bgFifo);

            if (!PixelFIFOEmpty(spriteFifo))
            {
                ShiftFIFO(spriteFifo);
            }

            uint8_t val = (palettes[paletteID] >> (colorIdx * 2)) & 0x03;
            return val;
        }

        uint8_t VRAMRead(const uint8_t* vram, uint16_t addr)
        {
            EMU_ASSERT(addr >= VRAM_ADDR && addr < VRAM_ADDR + VRAM_SIZE);
            return vram[addr - VRAM_ADDR];
        }

        void VRAMWrite(uint8_t* vram, uint16_t addr, uint8_t val)
        {
            EMU_ASSERT(addr >= VRAM_ADDR && addr < VRAM_ADDR + VRAM_SIZE);
            vram[addr - VRAM_ADDR] = val;
        }

        uint8_t OAMRead(const uint8_t* oam, uint16_t addr)
        {
            EMU_ASSERT(addr >= OAM_ADDR && addr < OAM_ADDR + OAM_SIZE);
            return oam[addr - OAM_ADDR];
        }

        void OAMWrite(uint8_t* oam, uint16_t addr, uint8_t val)
        {
            EMU_ASSERT(addr >= OAM_ADDR && addr < OAM_ADDR + OAM_SIZE);
            oam[addr - OAM_ADDR] = val;
        }

        uint8_t FetchBGTileData(const uint8_t* vram, LCDControl lcdc, uint8_t tileIdx, uint16_t offset)
        {
            uint16_t addr = lcdc._bits._tileDataSelect ?
                0x8000 + (tileIdx * 16) :          // $8000 mode
                0x9000 + (int8_t(tileIdx) * 16);   // $8800 mode

            return VRAMRead(vram, addr + offset);
        }

        uint8_t FetchSpriteTileData(const uint8_t* vram, uint8_t tileIdx, uint16_t offset)
        {
            uint16_t addr = 0x8000 + (tileIdx * 16);
            return VRAMRead(vram, addr + offset);
        }

        uint16_t FindVisibleSprites(uint8_t pixelXPos, const ObjectFetcher& objFetch)
        {
            uint16_t visibleSprites = 0;
            for (uint8_t i = 0; i < objFetch._spriteCount; ++i)
            {
                if (pixelXPos + 8 == objFetch._spriteList[i]._posX)
                {
                    visibleSprites |= (1 << i);
                }
            }

            return visibleSprites;
        }


        void TickObjectFetcher(uint16_t currCycle, uint8_t LY, LCDControl lcdc, ObjectFetcher& objFetch, const uint8_t* oam)
        {
            if (currCycle % 2 == 0)
            {
                // Check a new OAM entry every two cycles
                uint16_t addr = OAM_ADDR + (currCycle / 2) * sizeof(OAMEntry);
                OAMEntry oamEntry = 
                {
                    ._posY = OAMRead(oam, addr + 0),
                    ._posX = OAMRead(oam, addr + 1),
                    ._tileIdx = OAMRead(oam, addr + 2),
                    ._attribsu8 = OAMRead(oam, addr + 3),
                };

                uint8_t spriteHeight = (lcdc._bits._spriteSize > 0) ? SPRITE_SIZE_TALL : SPRITE_SIZE;

                if (objFetch._spriteCount < MAX_OAM_ENTRIES_PER_SCANLINE &&
                    LY + 16 >= oamEntry._posY &&
                    LY + 16 < oamEntry._posY + spriteHeight)
                {
                    objFetch._spriteList[objFetch._spriteCount++] = oamEntry;
                }
            }          
        }

        bool TickPixelFIFOs(uint16_t currCycle, uint8_t SCX, uint8_t BGP, uint8_t OBP0, uint8_t OBP1, PixelFIFO& bgFifo, PixelFIFO& spriteFifo, LCDControl lcdc, void* userData, FnDisplayPixelWrite displayFn)
        {
            EMU_ASSERT(currCycle >= CYCLES_PER_OAM_SCAN && "Pixel fetch stage should not be running during OAM scan");
            if (!PixelFIFOEmpty(bgFifo))
            {
                uint8_t color = PixelFIFOPop(bgFifo, spriteFifo, BGP, OBP0, OBP1);

                // Handle horizontal scroll by discarding pixels at the start of the scanline
                if ((currCycle - CYCLES_PER_OAM_SCAN) > (SCX % 8))
                {
                    if (displayFn)
                    {
                        displayFn(userData, color);
                    }

                    return true;
                }
            }

            return false;
        }

        bool TickPixelFetcherBackground(bool isWindowFetch, uint16_t currCycle, uint8_t LY, uint8_t SCX, uint8_t SCY, LCDControl lcdc, PixelFetcher& pixelFetch, const uint8_t* vram)
        {
            bool fifoPopulated = false;
            
            {
                switch (pixelFetch._currStage)
                {
                case PixelFetchStage::FetchTileNumber:
                {
                    // 2 cycles - memory access
                    if (currCycle % 2 == 1)
                    {
                        uint16_t tileMapAddress = 0;

                        if (isWindowFetch)
                        {
                            tileMapAddress = lcdc._bits._windowTileMapSelect ?
                                TILE_MAP_1_BEGIN :
                                TILE_MAP_0_BEGIN;
                        }
                        else
                        {
                            // Normal background pixel fetch
                            tileMapAddress = lcdc._bits._bgTileMapSelect ?
                                TILE_MAP_1_BEGIN :
                                TILE_MAP_0_BEGIN;
                        }

                        // Address for tile at current X tile position
                        uint16_t offset = pixelFetch._currTileXPos;
                        

                        if (isWindowFetch)
                        {
                            // Window fetch based on current window line
                            offset += 32 * (pixelFetch._windowLineCounter / 8);
                        }
                        else
                        {
                            // Offset for horizontal tile scrolling
                            offset = (offset + (SCX / 8)) & 0x1F;

                            // Background fetch scrolls based on SCX and current scanline (LY)
                            offset += 32 * (((LY + SCY) & 0xFF) / 8);
                        }

                        offset = offset & 0x3FF;

                        pixelFetch._currTileIdx = VRAMRead(vram, tileMapAddress + offset);
                        pixelFetch._currStage = PixelFetchStage::FetchTileDataLow;
                    }
                }
                    break;

                case PixelFetchStage::FetchTileDataLow:
                {
                    // 2 cycles - memory access
                    if (currCycle % 2 == 1)
                    {
                        // Offset which row to fetch by current scanline and vertical scroll register
                        uint16_t tileOffset = isWindowFetch ?
                            2 * (pixelFetch._windowLineCounter % 8) :
                            2 * ((LY + SCY) % 8);

                        uint8_t lowBits = lcdc._bits._bgWindowEnabled ? 
                            FetchBGTileData(vram, lcdc, pixelFetch._currTileIdx, tileOffset + 0) :
                            0;

                        pixelFetch._currTileLow = lowBits;
                        pixelFetch._currStage = PixelFetchStage::FetchTileDataHigh;
                    }
                }
                    break;

                case PixelFetchStage::FetchTileDataHigh:
                {
                    // 2 cycles - memory access
                    if (currCycle % 2 == 1)
                    {
                        // Same as low bits, but offset by one byte in the fetch
                        uint16_t tileOffset = isWindowFetch ?
                            2 * (pixelFetch._windowLineCounter % 8) :
                            2 * ((LY + SCY) % 8);

                        uint8_t highBits = lcdc._bits._bgWindowEnabled ? 
                            FetchBGTileData(vram, lcdc, pixelFetch._currTileIdx, tileOffset + 1) :
                            0;
                            
                        pixelFetch._currTileHigh = highBits;
                        pixelFetch._currStage = PixelFetchStage::PushToFIFO;

                        if (pixelFetch._numBGTilesFetchedInCurrScanline == 0)
                        {
                            // Reset to first stage if this is the first time we've fetched a tile
                            pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                            pixelFetch._currTileXPos = 0;
                        }

                        pixelFetch._numBGTilesFetchedInCurrScanline++;
                    }
                }
                    break;

                case PixelFetchStage::PushToFIFO:
                {
                    // 1+ cycles
                    // Only push pixels if BG FIFO is empty, otherwise keep retrying
                    if (PixelFIFOEmpty(pixelFetch._bgFifo))
                    {
                        PixelFIFOPushBGTile(pixelFetch._bgFifo, pixelFetch._currTileLow, pixelFetch._currTileHigh);
                        pixelFetch._currTileXPos++;
                        pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                        pixelFetch._currTileIdx = 0;
                        fifoPopulated = true;
                    }
                }
                    break;
                
                default:
                    break;
                }
            }

            return fifoPopulated;
        }

        uint8_t ReverseBits(uint8_t b)
        {
            b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
            b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
            b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
            return b;
        }

        bool TickPixelFetcherSprite(uint16_t currCycle, uint8_t currPixelXPos, uint8_t LY, const LCDControl& lcdc, PixelFetcher& pixelFetch, ObjectFetcher& objFetch, uint8_t spriteIdx, const uint8_t* vram)
        {
            bool fifoPopulated = false;
            OAMEntry sprite = objFetch._spriteList[spriteIdx];
            uint8_t spriteHeight = lcdc._bits._spriteSize ? 16 : 8;
                      
            switch (pixelFetch._currStage)
            {
            case PixelFetchStage::FetchTileNumber:
            {
                // 1 cycle
                pixelFetch._currTileIdx = sprite._tileIdx;
                if (lcdc._bits._spriteSize > 0)
                {
                    pixelFetch._currTileIdx = pixelFetch._currTileIdx & 0xFE;
                }
                
                pixelFetch._currStage = PixelFetchStage::FetchTileDataLow;
            }
                break;

            case PixelFetchStage::FetchTileDataLow:
            {
                // 2 cycles - memory access
                if (currCycle % 2 == 1)
                {
                    uint16_t rowIdx = (LY + 16) - sprite._posY;
                    uint16_t tileOffset = 2 * (rowIdx % spriteHeight);
                    if (sprite._attribs._yFlip)
                    {
                        tileOffset = 2 * ((spriteHeight - 1) - (rowIdx % spriteHeight));
                    }

                    uint8_t lowBits = lcdc._bits._spriteEnabled ? 
                        FetchSpriteTileData(vram, pixelFetch._currTileIdx, tileOffset + 0) :
                        0;

                    pixelFetch._currTileLow = lowBits;
                    if (sprite._attribs._xFlip)
                    {
                        pixelFetch._currTileLow = ReverseBits(pixelFetch._currTileLow);
                    }

                    pixelFetch._currStage = PixelFetchStage::FetchTileDataHigh;
                }
            }
                break;

            case PixelFetchStage::FetchTileDataHigh:
            {
                // 2 cycles - memory access
                if (currCycle % 2 == 1)
                {
                    // Same as low bits, but offset by one byte in the fetch
                    uint16_t rowIdx = (LY + 16) - sprite._posY;
                    uint16_t tileOffset = 2 * (rowIdx % spriteHeight);
                    if (sprite._attribs._yFlip)
                    {
                        tileOffset = 2 * ((spriteHeight - 1) - (rowIdx % spriteHeight));
                    }

                    uint8_t highBits = lcdc._bits._spriteEnabled ? 
                        FetchSpriteTileData(vram, pixelFetch._currTileIdx, tileOffset + 1) :
                        0;

                    pixelFetch._currTileHigh = highBits;
                    if (sprite._attribs._xFlip)
                    {
                        pixelFetch._currTileHigh = ReverseBits(pixelFetch._currTileHigh);
                    }

                    pixelFetch._currStage = PixelFetchStage::PushToFIFO;
                }
            }
                break;

            case PixelFetchStage::PushToFIFO:
            {
                // 1 cycle
                PixelFIFOPushSpriteTile(
                    pixelFetch._spriteFifo, 
                    pixelFetch._currTileLow, 
                    pixelFetch._currTileHigh,
                    sprite._attribs._dmgPalette ? PALETTE_ID_OBP1 : PALETTE_ID_OBP0,
                    sprite._attribs._priority);
            
                pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                pixelFetch._visibleSpriteBits &= ~(1 << spriteIdx);
                fifoPopulated = true;   
            }
                break;
            
            default:
                break;
            }
            
            
            return fifoPopulated;
        }
    }

    void BootPPU(PPU& ppu, uint8_t* vram, uint8_t* oam, FnDisplayPixelWrite pixelWriteFn, void* userData)
    {
        ppu._currMode = PPU::Mode::ObjectFetch;
        ppu._currCycle = 0;

        ppu._objFetch = {};
        ppu._pixelFetch = {};

        ppu._currPixelXPos = 0;
        ppu._windowScanlineHitThisFrame = false;

        ppu._vram = vram;
        ppu._oam = oam;

        ppu._pixelWriteFn = pixelWriteFn;
        ppu._pixelWriteUserData = userData;
    }

    void TickPPU(PPU& ppu, MMU& mmu, PeripheralIO& pIO)
    {
        const LCDControl lcdc =
        {
            ._u8 = pIO.LCDC
        };

        if (!lcdc._bits._displayEnable)
        {
            if (ppu._currMode == PPU::Mode::ObjectFetch)
            {
                MapMemoryRegion(mmu, OAM_ADDR, OAM_SIZE, ppu._oam, 0);
            }
            else if (ppu._currMode == PPU::Mode::PixelFetch)
            {
                MapMemoryRegion(mmu, VRAM_ADDR, VRAM_SIZE, ppu._vram, 0);
            }
            return;
        }

        switch (ppu._currMode)
        {
        case PPU::Mode::ObjectFetch:
        {
            // Make OAM inaccessible until next stage
            UnmapMemoryRegion(mmu, OAM_ADDR, OAM_SIZE);
            TickObjectFetcher(ppu._currCycle, pIO.LY, lcdc, ppu._objFetch, ppu._oam);

            if (ppu._currCycle + 1 >= CYCLES_PER_OAM_SCAN)
            {
                // Move to pixel fetch stage
                ppu._currMode = PPU::Mode::PixelFetch;
                ppu._pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                ppu._pixelFetch._currMode = PixelFetcher::Mode::Background;
                ppu._pixelFetch._currTileXPos = 0;
                ppu._pixelFetch._numBGTilesFetchedInCurrScanline = 0;
                ppu._pixelFetch._visibleSpriteBits = 0;

                // Make OAM memory accessible again
                MapMemoryRegion(mmu, OAM_ADDR, OAM_SIZE, ppu._oam, 0);
            }
        }
            break;

        case PPU::Mode::PixelFetch:
        {
            // Make VRAM inaccessible until next stage
            UnmapMemoryRegion(mmu, VRAM_ADDR, VRAM_SIZE);

            if (!ppu._pixelFetch._visibleSpriteBits)
            {
                uint16_t visibleSpriteBits = FindVisibleSprites(ppu._currPixelXPos, ppu._objFetch);
                if (visibleSpriteBits)
                {
                    ppu._pixelFetch._visibleSpriteBits = visibleSpriteBits;

                    // Potential penalty up to 6 cycles due to resetting of background FIFO when entering sprite rendering mode
                    ppu._pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                }
            }

            // Switch into window fetching mode if needed
            if (lcdc._bits._windowDisplayEnable &&
                pIO.LY >= pIO.WY &&
                ppu._currPixelXPos >= pIO.WX - 7 &&
                ppu._pixelFetch._currMode != PixelFetcher::Mode::Window)
            {
                ppu._pixelFetch._currMode = PixelFetcher::Mode::Window;
                ppu._pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                ppu._pixelFetch._currTileXPos = 0;

                // Potential 6 cycle penalty when entering Window mode due to FIFO clear
                PixelFIFOClear(ppu._pixelFetch._bgFifo);
            }
           
            if (!ppu._pixelFetch._visibleSpriteBits || PixelFIFOEmpty(ppu._pixelFetch._bgFifo))
            {   
                TickPixelFetcherBackground(
                    ppu._pixelFetch._currMode == PixelFetcher::Mode::Window,
                    ppu._currCycle,
                    pIO.LY,
                    pIO.SCX,
                    pIO.SCY,
                    lcdc,
                    ppu._pixelFetch,
                    ppu._vram);
            }
            
            if (ppu._pixelFetch._visibleSpriteBits && !PixelFIFOEmpty(ppu._pixelFetch._bgFifo))
            {
                unsigned long index = 0;
                _BitScanForward(&index, ppu._pixelFetch._visibleSpriteBits);
                TickPixelFetcherSprite(
                    ppu._currCycle,
                    ppu._currPixelXPos,
                    pIO.LY,
                    lcdc,
                    ppu._pixelFetch,
                    ppu._objFetch,
                    uint8_t(index),
                    ppu._vram);
            }

            // Push pixels while we're not fetching sprites
            if (!ppu._pixelFetch._visibleSpriteBits)
            {
                if (TickPixelFIFOs(ppu._currCycle, pIO.SCX, pIO.BGP, pIO.OBP0, pIO.OBP1, ppu._pixelFetch._bgFifo, ppu._pixelFetch._spriteFifo, lcdc, ppu._pixelWriteUserData, ppu._pixelWriteFn))
                {
                    ppu._currPixelXPos++;
                }
            }

            if (ppu._currPixelXPos >= SCREEN_WIDTH)
            {
                // Move to HBLANK stage
                EMU_ASSERT(ppu._currCycle + 1 >= 80 + 172 && ppu._currCycle + 1 <= 80 + 289);
                ppu._currMode = PPU::Mode::HBlank;

                if ((pIO.STAT & (1 << 3)))
                {
                    // Mode 0 enabled for stat interrupt
                    pIO.IF |= INT_BIT_STAT;
                }

                // Make VRAM accessible again
                MapMemoryRegion(mmu, VRAM_ADDR, VRAM_SIZE, ppu._vram, 0);
            }
        }
            break;
        case PPU::Mode::HBlank:
        case PPU::Mode::VBlank:
        {
            if (ppu._currCycle + 1 >= CYCLES_PER_SCANLINE)
            {
                if (!ppu._windowScanlineHitThisFrame)
                {
                    ppu._windowScanlineHitThisFrame = lcdc._bits._windowDisplayEnable && (pIO.LY >= pIO.WY) && pIO.WX < SCREEN_WIDTH;
                }

                if (lcdc._bits._windowDisplayEnable && ppu._windowScanlineHitThisFrame && pIO.WX < SCREEN_WIDTH)
                {
                    ppu._pixelFetch._windowLineCounter++;
                }

                // Move to next scanline, either ObjectFetch or VBlank
                ppu._currPixelXPos = 0;
                pIO.LY = (pIO.LY + 1) % SCANLINE_COUNT;

                if (pIO.LY == pIO.LYC && (pIO.STAT & 1 << 6))
                {
                    // LYC mode enabled for stat interrupt
                    pIO.IF |= INT_BIT_STAT;
                }

                if (ppu._currMode == PPU::Mode::HBlank && pIO.LY >= SCREEN_HEIGHT)
                {
                    ppu._currMode = PPU::Mode::VBlank;
                    pIO.IF |= INT_BIT_VBLANK;

                    if((pIO.STAT & (1 << 4)))
                    {
                        // Mode 1 enabled for stat interrupt
                        pIO.IF |= INT_BIT_STAT;
                    }

                    ppu._pixelFetch._windowLineCounter = 0;
                    ppu._windowScanlineHitThisFrame = false;
                }
                else if (ppu._currMode == PPU::Mode::HBlank || 
                        (ppu._currMode == PPU::Mode::VBlank && pIO.LY == 0))
                {
                    ppu._currMode = PPU::Mode::ObjectFetch;
                    ppu._objFetch._spriteCount = 0;
                    
                    if ((pIO.STAT & (1 << 5)))
                    {
                        // Mode 2 enabled for stat interrupt
                        pIO.IF |= INT_BIT_STAT;
                    }
                }
            }
        }
            break;
        
        default:
            break;
        }

        ppu._currCycle = (ppu._currCycle + 1) % CYCLES_PER_SCANLINE;

        // Update STAT
        pIO.STAT = (pIO.STAT & 0xFC) | (uint8_t(ppu._currMode) & 0x03);
        if (pIO.LY == pIO.LYC)
        {
            pIO.STAT |= 1 << 2;
        }
        else
        {
            pIO.STAT &= ~(1 << 2);
        }
    }

};