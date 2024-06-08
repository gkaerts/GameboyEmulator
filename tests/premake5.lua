project "emulatorTests"

    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    flags { "FatalWarnings", "MultiProcessorCompile" }

    files {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp",
        "src/**.c"
    }

    includedirs {
        "../emulator/include",
        "../contrib/submodules/googletest/googletest/include",
    }

    libdirs {
        "%{wks.location}/%{cfg.buildcfg}"
    }

    targetdir "%{wks.location}/%{cfg.buildcfg}/"

    links { "googletest", "emulator" }