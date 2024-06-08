project "emulatorApp"

    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    flags { "FatalWarnings", "MultiProcessorCompile" }

    -- Shared source files
    files {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp",
        "src/**.c"
    }

    includedirs {
        "../emulator/include"
    }

    links { "emulator" }

    libdirs {
        "%{wks.location}/%{cfg.buildcfg}"
    }

    targetdir "%{wks.location}/%{cfg.buildcfg}/"