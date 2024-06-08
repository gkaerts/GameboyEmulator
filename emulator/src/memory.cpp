#include "memory.hpp"
#include "SM83.hpp"

namespace emu::SM83
{
    void TickMemoryController(MemoryController& memCtrl, IO& cpuIO)
    {
        // Check if memory is requested
        if (cpuIO._outPins.MRQ)
        {
            // Put memory onto data bus?
            if (cpuIO._outPins.RD)
            {
                cpuIO._data = memCtrl._memory[cpuIO._address];
            }

            // Put data bus into memory?
            else if (cpuIO._outPins.WR)
            {
                memCtrl._memory[cpuIO._address] = cpuIO._data;
            }
        }
    }
}