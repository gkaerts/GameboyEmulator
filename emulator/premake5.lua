project "emulator"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    flags {
        "FatalWarnings",
        "MultiProcessorCompile"
    }

    files {
        "src/**.h",
        "src/**.hpp",
        "src/**.c",
        "src/**.cpp",
        "include/**.h",
        "include/**.hpp"
    }
    
    includedirs {
        "include"
    }

    libdirs {
        "%{wks.location}/%{cfg.buildcfg}"
    }
    
    targetdir "%{wks.location}/%{cfg.buildcfg}/"