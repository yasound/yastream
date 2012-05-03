solution "yastream"
    configurations { "Debug", "Release" }
    language "C++"

    includedirs
    {
        "../nui3/include",
        "tools"
    }

    defines { "_MINUI3_" }

    linkoptions { "-rdynamic" }

    configuration { "Debug*" }
        defines { "_DEBUG", "DEBUG" }
        flags   { "Symbols" }
        targetdir "bin/debug"
        libdirs { "../nui3/bin/debug/" }

    configuration { "Release*" }
        defines { "NDEBUG" }
        flags   { "Optimize", "Symbols" }
        targetdir "bin/release"
        libdirs { "../nui3/bin/release/" }


    if _ACTION == "clean" then
        os.rmdir("bin")
    end

    -- put your projects here
    project "yastream"
        kind     "ConsoleApp"

        files
        {
          "src/*",
          "tools/Mp3Parser/*",
        }

        links
        {
            "minui3"
        }

