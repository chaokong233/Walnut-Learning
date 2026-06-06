#pragma once

#include "Layer.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "imgui.h"
#include "vulkan/vulkan.h"
#include "myVulkan/myVulkanInclude.h"

#define VULKAN_RT 1

#ifdef VULKAN_RT
	const bool useVulkanRT = true;
#else
	const bool useVulkanRT = false;
#endif

class ImGui_ImplVulkanH_Window;

extern VkAllocationCallbacks*		g_Allocator;
extern VkInstance					g_Instance;
extern VkPhysicalDevice				g_PhysicalDevice;
extern VkDevice						g_Device;
extern uint32_t						g_QueueFamily;
extern VkQueue						g_Queue;
extern VkDebugReportCallbackEXT		g_DebugReport;
extern VkPipelineCache				g_PipelineCache;

extern vulkan::VulkanAllocator*		g_pVkMemoryAllocator;
extern vulkan::CommandPool*			g_pCommandPool;
extern vulkan::DescriptorAllocator* g_DescriptorAllocator;

extern ImGui_ImplVulkanH_Window		g_MainWindowData;
extern int							g_MinImageCount;
extern bool							g_SwapChainRebuild;
extern uint32_t						s_CurrentFrameIndex;

extern std::vector<std::function<void()>> s_VulkanRenderFuncQueue;

void check_vk_result(VkResult err);

struct GLFWwindow;

struct RayTracingScratchBuffer
{
	uint64_t deviceAddress = 0;
	std::shared_ptr<vulkan::VulkanMemoryResource> buffer;
};

// Ray tracing acceleration structure
struct AccelerationStructure {
	VkAccelerationStructureKHR handle;
	uint64_t deviceAddress = 0;
	std::shared_ptr<vulkan::VulkanMemoryResource> buffer;
};

namespace Walnut {
	struct ApplicationSpecification
	{
		std::string Name = "Walnut App";
		uint32_t Width = 1600;
		uint32_t Height = 900;
	};

	class Application
	{
	public:

		//
		Application(const ApplicationSpecification& applicationSpecification = ApplicationSpecification());
		~Application();

		static Application& Get();

		void Run();
		void SetMenubarCallback(const std::function<void()>& menubarCallback) { m_MenubarCallback = menubarCallback; }
		
		template<typename T>
		void PushLayer()
		{
			static_assert(std::is_base_of<Layer, T>::value, "Pushed type is not subclass of Layer!");
			m_LayerStack.emplace_back(std::make_shared<T>())->OnAttach();
		}

		void PushLayer(const std::shared_ptr<Layer>& layer) { m_LayerStack.emplace_back(layer); layer->OnAttach(); }

		void Close();

		float GetTime();
		GLFWwindow* GetWindowHandle() const { return m_WindowHandle; }

		static VkInstance GetInstance();
		static VkPhysicalDevice GetPhysicalDevice();
		static VkDevice GetDevice();

		static VkCommandBuffer GetCommandBuffer(bool begin);
		static void FlushCommandBuffer(VkCommandBuffer commandBuffer);

		static void SubmitResourceFree(std::function<void()>&& func);
	private:
		void Init();
		void Shutdown();
	private:
		ApplicationSpecification m_Specification;
		GLFWwindow* m_WindowHandle = nullptr;

		bool m_Running = false;

		float m_TimeStep = 0.0f;
		float m_FrameTime = 0.0f;
		float m_LastFrameTime = 0.0f;

		std::vector<std::shared_ptr<Layer>> m_LayerStack;
		std::function<void()> m_MenubarCallback;
	};

	// Implemented by CLIENT
	Application* CreateApplication(int argc, char** argv);
}