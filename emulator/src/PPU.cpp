
#include "PPU.hpp"
#include "SM83.hpp"

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

        template <typename PixelType>
        void PixelFIFOPush(PixelFIFO<PixelType>& fifo, PixelType data)
        {
            EMU_ASSERT(fifo._size < 8)
            fifo._pixels[fifo._size++] = data;
        }

        template <typename PixelType>
        PixelType PixelFIFOPop(PixelFIFO<PixelType>& fifo)
        {
            EMU_ASSERT(fifo._size > 0 && "Pixel FIFO empty!");
            PixelType val = fifo._pixels[0];

            for (uint8_t i = 0; i < fifo._size - 1; ++i)
            {
                fifo._pixels[i] = fifo._pixels[i + 1];
            }

            fifo._size--;
            fifo._pixels[fifo._size] = {};

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

        constexpr const uint8_t INVALID_SPRITE_IDX = 0xFF;
        uint8_t FindNextSpriteToRender(uint8_t startSpriteIdx, uint8_t pixelXPos, const ObjectFetcher& objFetch)
        {
            for (uint8_t i = startSpriteIdx; i < objFetch._spriteCount; ++i)
            {
                // if (i >= 1)
                //     break;

                if (pixelXPos + 8 >= objFetch._spriteList[i]._posX &&
                    pixelXPos + 8 < objFetch._spriteList[i]._posX + 8)
                {
                    return i;
                }
            }

            return INVALID_SPRITE_IDX;
        }


        void TickObjectFetcher(uint16_t currCycle, uint8_t LY, LCDControl lcdc, ObjectFetcher& objFetch, const uint8_t* oam)
        {
            if (currCycle % 2 == 1)
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

        bool TickPixelFIFOs(uint16_t currCycle, uint8_t SCX, uint8_t BGP, uint8_t OBP0, uint8_t OBP1, PixelFIFO<BGPixel>& bgFifo, PixelFIFO<SpritePixel>& spriteFifo, LCDControl lcdc, void* userData, FnDisplayPixelWrite displayFn)
        {
            EMU_ASSERT(currCycle >= CYCLES_PER_OAM_SCAN && "Pixel fetch stage should not be running during OAM scan");
            if (bgFifo._size > 0)
            {
                BGPixel bgPixel = PixelFIFOPop(bgFifo);

                uint8_t color = (BGP >> (bgPixel._colorIndex * 2)) & 0x03;
                
                if (spriteFifo._size > 0)
                {
                    SpritePixel spritePixel = PixelFIFOPop(spriteFifo);

                    if ((spritePixel._colorIndex != 0 && !spritePixel._priority) || 
                        (spritePixel._priority && bgPixel._colorIndex == 0) ||
                        !lcdc._bits._bgWindowEnabled)
                    {
                        if (lcdc._bits._spriteEnabled)
                        {
                            uint8_t spritePalette = spritePixel._palette ?  OBP1 : OBP0;
                            color = (spritePalette >> (spritePixel._colorIndex * 2)) & 0x03;
                        }
                    }
                }

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
            if (currCycle % 2 == 1)
            {
                switch (pixelFetch._currStage)
                {
                case PixelFetchStage::FetchTileNumber:
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
                    break;

                case PixelFetchStage::FetchTileDataLow:
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
                    break;

                case PixelFetchStage::FetchTileDataHigh:
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
                    break;

                case PixelFetchStage::PushToFIFO:
                {
                    // Only push pixels if BG FIFO is empty, otherwise keep retrying
                    if (pixelFetch._bgFIFO._size == 0)
                    {
                        for (int pixelIdx = 7; pixelIdx >= 0; --pixelIdx)
                        {
                            uint8_t colorIdx = 
                                (pixelFetch._currTileLow >> pixelIdx) << 1 |
                                (pixelFetch._currTileHigh >> pixelIdx);

                            BGPixel pix =
                            {
                                ._colorIndex = colorIdx,
                                ._priority = 0, // TODO: CGB support
                            };

                            PixelFIFOPush(pixelFetch._bgFIFO, pix);
                        }

                        pixelFetch._currTileXPos++;
                        pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
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

        bool TickPixelFetcherSprite(uint16_t currCycle, uint8_t LY, uint8_t SCY, PixelFetcher& pixelFetch, ObjectFetcher& objFetch, uint8_t spriteIdx, const uint8_t* vram)
        {
            bool fifoPopulated = false;
            if (currCycle % 2 == 1)
            {
                switch (pixelFetch._currStage)
                {
                case PixelFetchStage::FetchTileNumber:
                {
                    pixelFetch._currTileIdx = objFetch._spriteList[spriteIdx]._tileIdx;
                    pixelFetch._currStage = PixelFetchStage::FetchTileDataLow;
                }
                    break;

                case PixelFetchStage::FetchTileDataLow:
                {
                    // Offset which row to fetch by current scanline and vertical scroll register
                    uint16_t tileOffset = 2 * ((LY + SCY) % 8);

                    uint8_t lowBits = FetchSpriteTileData(vram, pixelFetch._currTileIdx, tileOffset + 0);
                    pixelFetch._currTileLow = lowBits;
                    pixelFetch._currStage = PixelFetchStage::FetchTileDataHigh;
                }
                    break;

                case PixelFetchStage::FetchTileDataHigh:
                {
                    // Same as low bits, but offset by one byte in the fetch
                    uint16_t tileOffset = 2 * ((LY + SCY) % 8);

                    uint8_t highBits = FetchSpriteTileData(vram, pixelFetch._currTileIdx, tileOffset + 1);
                    pixelFetch._currTileHigh = highBits;
                    pixelFetch._currStage = PixelFetchStage::PushToFIFO;
                }
                    break;

                case PixelFetchStage::PushToFIFO:
                {
                    for (int pixelIdx = 7; pixelIdx >= 0; --pixelIdx)
                    {
                        uint8_t colorIdx = 
                            (pixelFetch._currTileLow >> pixelIdx) << 1 |
                            (pixelFetch._currTileHigh >> pixelIdx);

                        SpritePixel pix =
                        {
                            ._colorIndex = colorIdx,
                            ._palette = objFetch._spriteList[spriteIdx]._attribs._dmgPalette,
                            ._priority = objFetch._spriteList[spriteIdx]._attribs._priority,
                            ._sourceID = spriteIdx
                        };

                        pixelFetch._spriteFIFO._pixels[pixelIdx] = pix;
                        //PixelFIFOPush(pixelFetch._spriteFIFO, pix);
                    }

                    pixelFetch._spriteFIFO._size = 8;
                    pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                    fifoPopulated = true;   
                }
                    break;
                
                default:
                    break;
                }
            }
            
            return fifoPopulated;
        }

        bool TickPixelFetcher(uint16_t currCycle, uint8_t LY, uint8_t SCX, uint8_t SCY, LCDControl lcdc, PixelFetcher& pixelFetch, ObjectFetcher& objFetch, uint8_t spriteIdx, const uint8_t* vram)
        {
            if (pixelFetch._currMode != PixelFetcher::Mode::Sprite)
            {
                return TickPixelFetcherBackground(
                    pixelFetch._currMode == PixelFetcher::Mode::Window,
                    currCycle,
                    LY,
                    SCX,
                    SCY,
                    lcdc,
                    pixelFetch,
                    vram);
            }
            else
            {
                return TickPixelFetcherSprite(
                    currCycle, 
                    LY,
                    SCY, 
                    pixelFetch, 
                    objFetch, 
                    spriteIdx, 
                    vram);
            }
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
        ppu._nextSpriteIdx = 0;

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

            if (ppu._currCycle + 1 > CYCLES_PER_OAM_SCAN)
            {
                // Move to pixel fetch stage
                ppu._currMode = PPU::Mode::PixelFetch;
                ppu._pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                ppu._pixelFetch._currMode = PixelFetcher::Mode::Background;
                ppu._pixelFetch._currTileXPos = 0;
                ppu._pixelFetch._numBGTilesFetchedInCurrScanline = 0;

                // Make OAM memory accessible again
                MapMemoryRegion(mmu, OAM_ADDR, OAM_SIZE, ppu._oam, 0);
            }
        }
            break;

        case PPU::Mode::PixelFetch:
        {
            // Make VRAM inaccessible until next stage
            UnmapMemoryRegion(mmu, VRAM_ADDR, VRAM_SIZE);

            if (TickPixelFetcher(ppu._currCycle, pIO.LY, pIO.SCX, pIO.SCY, lcdc, ppu._pixelFetch, ppu._objFetch, ppu._nextSpriteIdx, ppu._vram))
            {
                ppu._nextSpriteIdx = FindNextSpriteToRender(ppu._nextSpriteIdx, ppu._currPixelXPos, ppu._objFetch);
                if (ppu._nextSpriteIdx != INVALID_SPRITE_IDX)
                {
                    // A sprite needs to be rendered at this pixel. Reset the pixel fetcher into sprite mode
                    ppu._pixelFetch._prevMode = ppu._pixelFetch._currMode;
                    ppu._pixelFetch._currMode = PixelFetcher::Mode::Sprite;
                    ppu._pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                    ppu._nextSpriteIdx++;
                }
                else if (ppu._pixelFetch._currMode == PixelFetcher::Mode::Sprite)
                {
                    ppu._pixelFetch._currMode = ppu._pixelFetch._prevMode;
                    ppu._nextSpriteIdx = 0;
                }
            }

            // Push pixels while we're not fetching sprites
            if (ppu._pixelFetch._currMode != PixelFetcher::Mode::Sprite)
            {
                if (TickPixelFIFOs(ppu._currCycle, pIO.SCX, pIO.BGP, pIO.OBP0, pIO.OBP1, ppu._pixelFetch._bgFIFO, ppu._pixelFetch._spriteFIFO, lcdc, ppu._pixelWriteUserData, ppu._pixelWriteFn))
                {
                    ppu._currPixelXPos++;
                    
                    // Switch into window fetching mode if needed
                    if (lcdc._bits._windowDisplayEnable &&
                        pIO.LY >= pIO.WY &&
                        ppu._currPixelXPos >= pIO.WX - 7 &&
                        ppu._pixelFetch._currMode != PixelFetcher::Mode::Window)
                    {
                        ppu._pixelFetch._currMode = PixelFetcher::Mode::Window;
                        ppu._pixelFetch._currStage = PixelFetchStage::FetchTileNumber;
                        ppu._pixelFetch._currTileXPos = 0;
                        ppu._pixelFetch._bgFIFO._size = 0;
                    }
                }
            }

            if (ppu._currPixelXPos >= SCREEN_WIDTH)
            {
                // Move to HBLANK stage
                EMU_ASSERT(ppu._currCycle >= 80 + 172 && ppu._currCycle <= 80 + 289);
                EMU_ASSERT(ppu._pixelFetch._bgFIFO._size == 0);
                EMU_ASSERT(ppu._pixelFetch._spriteFIFO._size == 0);

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

                if (ppu._windowScanlineHitThisFrame && pIO.WX < SCREEN_WIDTH)
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
                    ppu._nextSpriteIdx = 0;
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