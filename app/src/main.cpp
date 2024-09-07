#include "SM83.hpp"
#include "PPU.hpp"
#include "MMU.hpp"

#include <chrono>
#include <vector>
#include <Windows.h>

#include <cstdio>
namespace 
{
    struct DrawContext
    {
        uint32_t _framebuffer[emu::SM83::SCREEN_WIDTH * emu::SM83::SCREEN_HEIGHT];
        int _currPixel = 0;
    };

    void PPUDrawPixel(void* userData, uint8_t color2bpp)
    {
        static const uint32_t GBColors[] =
        {
            0xFFFFFFFF,
            0xAAAAAAAA,
            0x55555555,
            0x00000000,
        };

        DrawContext* ctxt = (DrawContext*)userData;
        ctxt->_framebuffer[ctxt->_currPixel] = GBColors[color2bpp & 0x03];
        ctxt->_currPixel++;
        ctxt->_currPixel = (ctxt->_currPixel % (emu::SM83::SCREEN_WIDTH * emu::SM83::SCREEN_HEIGHT));
    }

    LRESULT CALLBACK EmuWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    const wchar_t WINDOW_CLASS_NAME[] = L"Emulator Window Class";
    const wchar_t WINDOW_TITLE[] = L"Emulator";

    void RegisterWin32WindowClass()
    {
        WNDCLASS wc =
        {
            .lpfnWndProc = EmuWindowProc,
            .hInstance = GetModuleHandle(nullptr),
            .hIcon = NULL,
            .lpszClassName = WINDOW_CLASS_NAME
        };

        RegisterClass(&wc);
    }

    HWND CreateWin32Window(DrawContext* drawCtxt)
    {
        RECT windowRect = { 0, 0, emu::SM83::SCREEN_WIDTH, emu::SM83::SCREEN_HEIGHT };
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
        return CreateWindowEx(
            0,
            WINDOW_CLASS_NAME,
            WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            NULL,
            NULL,
            GetModuleHandle(nullptr),
            drawCtxt);
    }

    LRESULT CALLBACK EmuWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        
        switch(uMsg)
        {
            case WM_PAINT:
            {
                DrawContext* drawCtxt = (DrawContext*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
                if (drawCtxt)
                {
                    PAINTSTRUCT ps = {};
                    HDC hdc = BeginPaint(hwnd, &ps);

                    RECT clientRect = {};
                    GetClientRect(hwnd, &clientRect);
                    
                    BITMAPINFOHEADER bmih = {0};
                    bmih.biSize     = sizeof(BITMAPINFOHEADER);
                    bmih.biWidth    = emu::SM83::SCREEN_WIDTH;
                    bmih.biHeight   = -emu::SM83::SCREEN_HEIGHT;
                    bmih.biPlanes   = 1;
                    bmih.biBitCount = 32;
                    bmih.biCompression  = BI_RGB ;
                    bmih.biSizeImage    = 0;
                    bmih.biXPelsPerMeter    =   10;
                    bmih.biYPelsPerMeter    =   10;

                    BITMAPINFO dbmi = {0};
                    dbmi.bmiHeader = bmih;

                    StretchDIBits(
                        hdc,
                        0, 0,
                        clientRect.right - clientRect.left,
                        clientRect.bottom - clientRect.top,
                        0, 0,
                        emu::SM83::SCREEN_WIDTH,
                        emu::SM83::SCREEN_HEIGHT,
                        drawCtxt->_framebuffer,
                        &dbmi,
                        DIB_RGB_COLORS,
                        SRCCOPY);

                    EndPaint(hwnd, &ps);
                }
            }
            break;
            case WM_CLOSE:
            {
                DestroyWindow(hwnd);
            }
            break;
            case WM_DESTROY:
            {
                PostQuitMessage(0);
            }
            break;
            case WM_CREATE:
            {
                CREATESTRUCT* createStruct = (CREATESTRUCT*)lParam;
                DrawContext* drawCtxt = (DrawContext*)createStruct->lpCreateParams;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)drawCtxt);
            }
            break;
            default:
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        return 0;
    }

    bool HandleEvents()
    {
        MSG msg = {};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
            {
                return false;
            }
        }

        return true;
    }

    
}

int main(int argc, char* argv[])
{
    RegisterWin32WindowClass();

    DrawContext drawCtxt;
    HWND hwnd = CreateWin32Window(&drawCtxt);
    ShowWindow(hwnd, SW_NORMAL);
    UpdateWindow(hwnd);

    emu::SM83::CPU cpu;
    emu::SM83::PPU ppu;
    emu::SM83::MMU mmu;

    // Cartridge memory
    std::unique_ptr<uint8_t[]> ROMBank0 = std::make_unique<uint8_t[]>(16 * 1024);
    std::unique_ptr<uint8_t[]> ROMBank1 = std::make_unique<uint8_t[]>(16 * 1024);
    emu::SM83::MapMemoryRegion(mmu, 0x0000, 16 * 1024, ROMBank0.get(), emu::SM83::MMRF_ReadOnly);
    emu::SM83::MapMemoryRegion(mmu, 0x4000, 16 * 1024, ROMBank1.get(), emu::SM83::MMRF_ReadOnly);

    // Video memory
     std::unique_ptr<uint8_t[]> VRAM = std::make_unique<uint8_t[]>(8 * 1024);
     std::unique_ptr<uint8_t[]> OAMBank = std::make_unique<uint8_t[]>(256);
    emu::SM83::MapMemoryRegion(mmu, 0x8000, 8 * 1024, VRAM.get(), 0);
    emu::SM83::MapMemoryRegion(mmu, 0xFE00, 256, OAMBank.get(), 0);

    // Work RAM + echo
    std::unique_ptr<uint8_t[]> WRAMBank0 = std::make_unique<uint8_t[]>(4 * 1024);
    std::unique_ptr<uint8_t[]> WRAMBank1 = std::make_unique<uint8_t[]>(4 * 1024);
    emu::SM83::MapMemoryRegion(mmu, 0xC000, 4 * 1024, WRAMBank0.get(), 0);
    emu::SM83::MapMemoryRegion(mmu, 0xD000, 4 * 1024, WRAMBank1.get(), 0);
    emu::SM83::MapMemoryRegion(mmu, 0xE000, 4 * 1024, WRAMBank0.get(), 0);  // Echo RAM
    emu::SM83::MapMemoryRegion(mmu, 0xF000, 4 * 1024, WRAMBank1.get(), 0);  // Echo RAM

    if (argc > 1)
    {
        const char* romPath = argv[1];
        FILE* file = nullptr;
        if (!fopen_s(&file, romPath, "rb"))
        {
            if (file)
            {
                fread_s(ROMBank0.get(), 16 * 1024, 1, 16 * 1024, file);
                fread_s(ROMBank1.get(), 16 * 1024, 1, 16 * 1024, file);
                fclose(file);
            }
        }
    }

    emu::SM83::BootCPU(cpu, 0, 0);
    emu::SM83::MapPeripheralIOMemory(cpu, mmu);

    emu::SM83::BootPPU(ppu, VRAM.get(), OAMBank.get(), PPUDrawPixel, &drawCtxt);

    auto time = std::chrono::system_clock::now();

    uint16_t PC = cpu._registers._reg16.PC;
    bool pastBootROM = false;
    
    while (true)
    {
        if (!HandleEvents())
        {
            break;
        }

        for (int j = 0; j < 154; ++j)
        {
            for (int i = 0; i < 456; ++i)
            {
                emu::SM83::TickCPU(cpu, mmu, 1);
                //emu::SM83::TickDMA(memCtrl);
                emu::SM83::TickPPU(ppu, mmu, cpu._peripheralIO);
            }
        }
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);
    }

    DestroyWindow(hwnd);

    return 0;
}