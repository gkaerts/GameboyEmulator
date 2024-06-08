-- Options
newoption {
    trigger = "platform",
    description = "Platform to generate project and solution files for",
    allowed = {
        { "win64",              "Windows x86-64" },
    },
    default = "win64"
}

PLATFORM_PROPERTIES = {
    win64 = {
        IncludeTestsInBuild = true,
    }
}

workspace "emulator"

    -- Windows x64 options
    platforms { _OPTIONS["platform"] }
    filter { "options:platform=win64" }
        system "windows"
        architecture "x86_64"

    -- Workspace build configurations
    configurations { "Debug", "Release" }

    filter "configurations:Debug"
        symbols "On"
        runtime "Debug"
        defines { "DEBUG" }
        optimize "Off"
        inlining "Disabled"

    filter "configurations:Release"
        symbols "On"
        runtime "Release"
        defines { "NDEBUG" }
        optimize "Speed"
        inlining "Auto"

    filter {}

    -- Build location
    location("build/" .. _OPTIONS["platform"])

    -- Projects
    include "emulator"
    include "app"

    if PLATFORM_PROPERTIES[_OPTIONS["platform"]].IncludeTestsInBuild then
        include "contrib/projects/googletest.premake5"
        include "tests"
    end
