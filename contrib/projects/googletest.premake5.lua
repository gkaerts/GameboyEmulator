project "googletest"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    flags {
        "MultiProcessorCompile"
    }

    local project_dir = "../submodules/googletest/googletest"
    local source_dir = project_dir .. "/src"
    local include_dir = project_dir .. "/include"
    files {
        source_dir .. "/gtest-all.cc",
        source_dir .. "/**.h",
        include_dir .. "/**.h"
    }
    
    includedirs {
        include_dir,
        project_dir
    }

    defines {
        "HAVE_STD_REGEX=1"
    }

    libdirs {
        "%{wks.location}/%{cfg.buildcfg}"
    }

    filter "toolset:msc*"
        defines { "_CRT_SECURE_NO_WARNINGS" }
    filter {}

    
    targetdir "%{wks.location}/%{cfg.buildcfg}/"