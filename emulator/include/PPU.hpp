#pragma once

#include "common.hpp"
#include "MMU.hpp"

namespace emu::SM83
{
    constexpr const uint16_t SCREEN_WIDTH = 160;
    constexpr const uint16_t SCREEN_HEIGHT = 144;

    struct OAMAttribs
    {
        uint8_t _priority : 1;
        uint8_t _yFlip : 1;
        uint8_t _xFlip : 1;
        uint8_t _dmgPalette : 1;
        uint8_t _bank : 1;
        uint8_t _cgbPalette : 3;
    };

    struct OAMEntry
    {
        uint8_t _posY;
        uint8_t _posX;
        uint8_t _tileIdx;

        union
        {
            OAMAttribs _attribs;
            uint8_t _attribsu8;
        };
    };

    constexpr const uint8_t MAX_OAM_ENTRIES_PER_SCANLINE = 10;
    struct ObjectFetcher
    {
        OAMEntry _spriteList[MAX_OAM_ENTRIES_PER_SCANLINE] = {};
        uint8_t _spriteCount = 0;
    };

    struct BGPixel
    {
        uint8_t _colorIndex : 2;
        uint8_t _priority : 1;
    };

    struct SpritePixel
    {
        uint8_t _colorIndex : 2;
        uint8_t _palette : 1;
        uint8_t _priority : 1;
        uint8_t _sourceID : 4;
    };

    template <typename PixelType>
    struct PixelFIFO
    {
        PixelType _pixels[8];
        uint8_t _size;
    };

    enum class PixelFetchStage
    {
        FetchTileNumber = 0,
        FetchTileDataLow,
        FetchTileDataHigh,
        PushToFIFO,
    };

    struct PixelFetcher
    {
        enum class Mode
        {
            Background = 0,
            Window ,
            Sprite,
        };

        PixelFetchStage _currStage = PixelFetchStage::FetchTileNumber;
        Mode _currMode = Mode::Background;
        Mode _prevMode = Mode::Background;

        uint8_t _currTileIdx = 0;
        uint8_t _currTileHigh = 0;
        uint8_t _currTileLow = 0;
        uint16_t _currTileXPos = 0;

        uint16_t _windowLineCounter = 0;
        uint16_t _numBGTilesFetchedInCurrScanline = 0;

        PixelFIFO<BGPixel> _bgFIFO = {};
        PixelFIFO<SpritePixel> _spriteFIFO = {};
    };

    using FnDisplayPixelWrite = void(*)(void*, uint8_t pixel2bpp);

    struct PPU
    {
        enum class Mode
        {
            HBlank = 0,
            VBlank = 1,
            ObjectFetch = 2,
            PixelFetch = 3
        };

        Mode _currMode;
        uint16_t _currCycle;

        ObjectFetcher _objFetch = {};
        PixelFetcher _pixelFetch = {};

        uint8_t _currPixelXPos = 0;
        bool _windowScanlineHitThisFrame = false;
        uint8_t _nextSpriteIdx = 0;

        uint8_t* _vram = nullptr;
        uint8_t* _oam = nullptr;

        FnDisplayPixelWrite _pixelWriteFn = nullptr;
        void* _pixelWriteUserData = nullptr;
    };

    struct PeripheralIO;

    void BootPPU(PPU& ppu, uint8_t* vram, uint8_t* oam, FnDisplayPixelWrite pixelWriteFn, void* userData);
    void TickPPU(PPU& ppu, MMU& mmu, PeripheralIO& pIO);
}