#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;

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

	vk::raii::Queue graphicsQueue{nullptr};
	uint32_t graphicsIdx{~0u};
	vk::raii::SwapchainKHR swapChain{nullptr};
	std::vector<vk::Image> swapChainImages;
	std::vector<vk::ImageView> swapChainImageViews;
	vk::SurfaceFormatKHR swapChainSurfaceFormat;
	vk::Extent2D swapChainExtent;

	vk::raii::PipelineLayout pipelineLayout{nullptr};
	vk::raii::Pipeline graphicsPipeline{nullptr};
	vk::raii::CommandPool commandPool{nullptr};
	vk::raii::CommandBuffer commandBuffer{nullptr};

	vk::raii::Semaphore presentCompleteS{nullptr};
	vk::raii::Semaphore renderFinishedS{nullptr};
	vk::raii::Fence drawFence{nullptr};
};
