#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

constexpr uint32_t WINDOW_WIDTH = 600;
constexpr uint32_t WINDOW_HEIGHT = 600;

class Program
{
	public:
	Program() = default;

	void Run();

	private:
	void Init();
	void Update();
	void Draw();

	void CreateVKInstance();
	void CreateSurface();
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateGraphicsPipeline();
	void CreateCommandPool();
	void CreateCommandBuffer();
	void RecordCommandBuffer(uint32_t imgIdx);
	void TransitionImageLayout(uint32_t imgIdx, vk::ImageLayout oldLayout,
							   vk::ImageLayout newLayout,
							   vk::AccessFlags2 srcAccess,
							   vk::AccessFlags2 dstAccess,
							   vk::PipelineStageFlags2 srcStage,
							   vk::PipelineStageFlags2 dstStage);
	void CreateSyncObjects();

	GLFWwindow* window{nullptr};
	vk::raii::Context context;
	vk::raii::Instance instance{nullptr};

	vk::raii::SurfaceKHR surface{nullptr};
	vk::raii::PhysicalDevice physicalDevice{nullptr};
	vk::raii::Device device{nullptr};

	uint32_t graphicsIdx{~0u};
	vk::raii::Queue graphicsQueue{nullptr};
	vk::raii::SwapchainKHR swapChain{nullptr};
	std::vector<vk::Image> swapChainImages;
	vk::SurfaceFormatKHR swapChainSurfaceFormat;
	vk::Extent2D swapChainExtent;
	std::vector<vk::raii::ImageView> swapChainImageViews;

	vk::raii::PipelineLayout pipelineLayout{nullptr};
	vk::raii::Pipeline graphicsPipeline{nullptr};

	vk::raii::CommandPool commandPool{nullptr};
	vk::raii::CommandBuffer commandBuffer{nullptr};

	vk::raii::Semaphore presentCompleteSemaphore{nullptr};
	vk::raii::Semaphore renderFinishedSemaphore{nullptr};
	vk::raii::Fence drawFence{nullptr};
};
