local action = _ACTION or ''

flags { "Unicode", "C++11" }
exceptionhandling "Off"
rtti "Off"
strictaliasing "Off"

workspace "lowpix"
    local workspace_location = "build/" .. _OS .. "/" .. action .. "/lowpix"

    location(workspace_location)
    startproject "lowpix"
    configurations { "debug", "release" }
    
    flags { "FatalCompileWarnings", "NoPCH", "StaticRuntime" }

    platforms { "x86", "x64" }
    filter "platforms:x86"
        architecture "x86"
    filter "platforms:x64"
        architecture "x86_64"
    filter {}

    filter { "language:C", "action:gmake" }
        buildoptions { "-std=gnu99" }

    filter "configurations:debug"
        targetsuffix "_d"
        defines { "DEBUG", "LP_DEBUG" }
        flags { "Symbols" }

    filter "configurations:release"
        defines { "NDEBUG", "LP_RELEASE" }
        optimize "On"
        floatingpoint "Fast"
        vectorextensions "SSE2"

    filter "action:vs*"
        defines { "WIN32", "_CRT_SECURE_NO_WARNINGS", "_CRT_SECURE_NO_DEPRECATE", "_CRT_NONSTDC_NO_DEPRECATE" }
        flags { "MultiProcessorCompile", "NoEditAndContinue", "NoIncrementalLink", "NoManifest", "NoMinimalRebuild" }

    project "glfw"
        kind "StaticLib"
        language "C"
        files { "src-lib/glfw/src/*.h", "src-lib/glfw/include/**.h",
            "src-lib/glfw/src/context.c",
            "src-lib/glfw/src/init.c",
            "src-lib/glfw/src/input.c",
            "src-lib/glfw/src/monitor.c",
            "src-lib/glfw/src/vulkan.c",
            "src-lib/glfw/src/window.c",
        }
        includedirs { "src-lib/glfw/src", "src-lib/glfw/include"}

        filter "system:linux"
            files { "src-lib/glfw/src/x11_*.c", "src-lib/glfw/src/glx_*.c", "src-lib/glfw/src/posix_*.c", "src-lib/glfw/src/linux_*.c", "src-lib/glfw/src/xkb_*.c" }
            defines { "_GLFW_X11", "_GLFW_GLX", "_GLFW_USE_LINUX_JOYSTICKS", "_GLFW_HAS_XRANDR", "_GLFW_HAS_PTHREAD" ,"_GLFW_HAS_SCHED_YIELD", "_GLFW_HAS_GLXGETPROCADDRESS" }
            buildoptions { "-pthread" }

        filter "system:windows"
            files { "src-lib/glfw/src/win32_*.c", "src-lib/glfw/src/wgl_*.c" }
            defines { "_GLFW_WIN32", "_GLFW_WGL", "_GLFW_USE_LINUX_JOYSTICKS", "_GLFW_HAS_XRANDR", "_GLFW_HAS_PTHREAD" ,"_GLFW_HAS_SCHED_YIELD", "_GLFW_HAS_GLXGETPROCADDRESS" }
            disablewarnings { "4100", "4152", "4204", "4457" }

        filter "system:macosx"
            files { "src-lib/glfw/src/cocoa_*.c", "src-lib/glfw/src/cocoa_*.h", "src-lib/glfw/src/cocoa_*.m" }
            defines { "_GLFW_COCOA" }
            buildoptions { " -fno-common" }
            linkoptions { "-framework OpenGL", "-framework Cocoa", "-framework IOKit" }

    project "lua"
        kind "StaticLib"
        language "C"

        includedirs { "src-lib/lua" }
        files { "src-lib/lua/**.h", "src-lib/lua/**.c" }
        removefiles { "src-lib/lua/lua.c", "src-lib/lua/luac.c", "src-lib/lua/print.c" }

        filter "action:vs*"
            disablewarnings { "4244", "4702" }

    project "liblowpix"
        kind "StaticLib"
        language "C"

        includedirs { "src/liblowpix/include" }
        files { "src/liblowpix/include/**.h", "src/liblowpix/src/**.c" }

    project "lowpix"
        kind "WindowedApp"
        language "C++"
        targetdir("build/bin")

        links { "liblowpix", "lua", "glfw" }

        includedirs { "src/lowpix/include", "src/liblowpix/include", "src-lib/lua", "src-lib/glfw/include" }
        files { "src/lowpix/**.h", "src/lowpix/**.inl", "src/lowpix/**.cpp", "src/lowpix/**.c" }
        defines { "GLEW_STATIC" }

        filter "system:windows"
            files { "src/lowpix/**.rc" }
            links { "opengl32" }

        filter "system:linux"
            links { "X11", "Xrandr", "rt", "GL", "GLU", "pthread", "dl", "Xinerama", "Xcursor" }

        filter "system:macosx"
            linkoptions { "-framework OpenGL", "-framework Cocoa", "-framework IOKit" }
            postbuildcommands {
                "{DELETE} -r lowpix.app",
                "{MKDIR} lowpix.app/Contents/MacOS",
				"{MKDIR} lowpix.app/Contents/Resources",
                "{COPY} %{cfg.buildtarget.abspath} lowpix.app/Contents/MacOS/lowpix",
				"{COPY} ../../../../src/lowpix/res/osx/icon.icns lowpix.app/Contents/Resources",
				"/usr/libexec/PlistBuddy -c 'Add :CFBundleName string lowpix' lowpix.app/Contents/Info.plist",
				"/usr/libexec/PlistBuddy -c 'Add :CFBundleExecutable string lowpix' lowpix.app/Contents/Info.plist",
				"/usr/libexec/PlistBuddy -c 'Add :CFBundleIconFile string lowpix.icns' lowpix.app/Contents/Info.plist",
				"/usr/libexec/PlistBuddy -c 'Add :CFBundleIdentifier string flush.lowpix' lowpix.app/Contents/Info.plist",
				"/usr/libexec/PlistBuddy -c 'Add :CFBundleShortVersionString string 0.2.1' lowpix.app/Contents/Info.plist",
				"/usr/libexec/PlistBuddy -c 'Add :NSHighResolutionCapable bool YES' lowpix.app/Contents/Info.plist"
            }                
            
        filter "action:vs*"
            disablewarnings { "4103", "4456", "4554", "4577", "6255", "6262", "28278" }
            buildoptions { "/volatile:iso" }
