solution "yastream"
    configurations { "Debug", "Release" }
    language "C++"

    includedirs
    {
        "../nui3/include",
        "Tools"
    }

    defines { "_MINUI3_" }

    configuration { "Debug*" }
        defines { "_DEBUG", "DEBUG" }
        flags   { "Symbols" }
        targetdir "bin/debug"
        libdirs { "../nui3/bin/debug/" }

    configuration { "Release*" }
        defines { "NDEBUG" }
        flags   { "Optimize" }
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
          "Tools/Mp3Parser/*",
        }

        links
        {
            "minui3"
        }

