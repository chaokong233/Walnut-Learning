project "RayTracing"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   targetdir "bin/%{cfg.buildcfg}"
   staticruntime "off"

   files { "src/**.h", "src/**.cpp" }

   includedirs
   {
      "../vendor/imgui",
      "../vendor/glfw/include",
      "../vendor/oidn/include/OpenImageDenoise",
      "../vendor/myVulkan",
      "../vendor/VMA",
      "../Walnut/src",
      "./",
      "../vendor/assimpInclude",
      "../vendor/nlohmann",
      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.glm}",
   }

    links
    {
        "Walnut",

        "%{Library.odin}",
        "%{Library.odin_core}"
    }

   targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

   postbuildcommands
   {
      "if exist \"%{cfg.targetdir}/config/scene.json\" del \"%{cfg.targetdir}/config/scene.json\"",
      "{COPYDIR} \"%{prj.location}/config\" \"%{cfg.targetdir}/config\"",
      "{COPYDIR} \"%{prj.location}/assets\" \"%{cfg.targetdir}/assets\"",
      "{COPYDIR} \"%{wks.location}/Walnut/src/Walnut/shaders\" \"%{cfg.targetdir}/shaders\""
   }

   filter "system:windows"
      systemversion "latest"
      defines { "WL_PLATFORM_WINDOWS" }

   filter "configurations:Debug"
      defines { "WL_DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "WL_RELEASE" }
      runtime "Release"
      optimize "On"
      symbols "On"

   filter "configurations:Dist"
      kind "WindowedApp"
      defines { "WL_DIST" }
      runtime "Release"
      optimize "On"
      symbols "Off"
