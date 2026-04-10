-- WalnutExternal.lua

VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["glm"] = "../vendor/glm"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"

odin_dir = "$(SolutionDir)vendor/oidn/lib/OpenImageDenoise.lib";
odin_core_dir = "$(SolutionDir)vendor/oidn/lib/OpenImageDenoise_core.lib"
Library["odin"] = odin_dir;
Library["odin_core"] = odin_core_dir;

group "Dependencies"
   include "vendor/imgui"
   include "vendor/glfw"
group ""

group "Core"
   include "Walnut"
group ""