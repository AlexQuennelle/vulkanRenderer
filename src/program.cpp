#include "program.hpp"

#include <fstream>
#include <map>
#include <print>
#include <ranges>
#include <vector>

#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION{false};
#else
constexpr bool ENABLE_VALIDATION{true};
#endif // !NDEBUG

constexpr std::string SHADER_EXTENSION{".spv"};

const std::vector<const char*> VALIDATION_LAYERS = {
	"VK_LAYER_KHRONOS_validation",
};
const std::vector<const char*> REQUIRED_DEVICE_EXTENSIONS{
	vk::KHRSwapchainExtensionName,
};

template <typename T> constexpr void CheckError(T& object)
{
	if (!object.has_value())
	{
		std::println(stderr, "ERROR: {}", vk::to_string(object.result));
		std::exit(1);
	}
}

inline auto LoadShader(const std::string& filename) -> std::vector<char>
{
	std::ifstream file{filename + SHADER_EXTENSION,
					   std::ios::ate | std::ios::binary};
	if (!file.is_open())
	{
		std::println(stderr, "ERROR: Failed to load shader {}: {}",
					 filename + SHADER_EXTENSION, std::strerror(errno));
		std::exit(1);
	}

	std::vector<char> data(static_cast<size_t>(file.tellg()));
	file.seekg(0, std::ios::beg);
	file.read(data.data(), static_cast<std::streamsize>(data.size()));
	file.close();

	return data;
}

void Program::Run()
{
	std::println("Hello, Vulkan!");
	this->Init();
	while (glfwWindowShouldClose(this->window) == 0)
	{
		glfwPollEvents();
		this->Update();
		this->Draw();
	}
	this->device.waitIdle();

	// Cleanup
	glfwDestroyWindow(this->window);
	glfwTerminate();
}
void Program::Init()
{
	// Init Window
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	this->window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
									"Hello, Vulkan!", nullptr, nullptr);

	// Init Vulkan
	this->CreateVKInstance();
	this->CreateSurface();
	this->PickPhysicalDevice();
	this->CreateLogicalDevice();
	this->CreateSwapChain();
	this->CreateImageViews();
	this->CreateGraphicsPipeline();
	this->CreateCommandPool();
	this->CreateCommandBuffer();
	this->CreateSyncObjects();
}
void Program::Update() { }
void Program::Draw()
{
	auto fenceResult{
		this->device.waitForFences(*this->drawFence, vk::True, UINT64_MAX),
	};
	if (fenceResult != vk::Result::eSuccess)
	{
		std::println(stderr, "ERROR: Failed to wait for fence");
		std::exit(1);
	}
	this->device.resetFences(*this->drawFence);

	auto [result, imageIdx] = this->swapChain.acquireNextImage(
		UINT64_MAX, *this->presentCompleteSemaphore, nullptr);
	this->RecordCommandBuffer(imageIdx);
	this->graphicsQueue.waitIdle();

	vk::PipelineStageFlags waitDestStageMask{
		vk::PipelineStageFlagBits::eColorAttachmentOutput};
	const vk::SubmitInfo submitInfo{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*this->presentCompleteSemaphore,
		.pWaitDstStageMask = &waitDestStageMask,
		.commandBufferCount = 1,
		.pCommandBuffers = &*this->commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*this->renderFinishedSemaphore,
	};
	this->graphicsQueue.submit(submitInfo, *this->drawFence);

	const vk::PresentInfoKHR presentInfoKHR{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*this->renderFinishedSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &*this->swapChain,
		.pImageIndices = &imageIdx,
	};
	result = this->graphicsQueue.presentKHR(presentInfoKHR);
	switch (result)
	{
	case vk::Result::eSuccess:
		break;
	case vk::Result::eSuboptimalKHR:
		std::println(
			stdout,
			"vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !");
		break;
	default:
		break; // an unexpected result is returned!
	}
}

void Program::CreateVKInstance()
{
	constexpr vk::ApplicationInfo appInfo{
		.pApplicationName = NAME,
		.applicationVersion = VK_MAKE_VERSION(0, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = vk::ApiVersion14,
	};

	std::vector<const char*> requiredLayers;
	if (ENABLE_VALIDATION)
	{
		requiredLayers.assign(VALIDATION_LAYERS.begin(),
							  VALIDATION_LAYERS.end());
	}

	auto layerProps{this->context.enumerateInstanceLayerProperties()};
	CheckError(layerProps);
	for (const auto* requiredLayer : requiredLayers)
	{
		auto comp = [requiredLayer](const auto& layerProp) -> bool
		{
			return std::strcmp(layerProp.layerName, requiredLayer) == 0;
		};
		namespace r = std::ranges;
		if (r::none_of(layerProps.value, comp))
		{
			std::println("ERROR: Required layer not supported: {}",
						 std::string(requiredLayer));
			std::exit(1);
		}
	}

	uint32_t glfwExtensionCount{0};
	const char** glfwExtensions{
		glfwGetRequiredInstanceExtensions(&glfwExtensionCount),
	};
	auto extensionProps{this->context.enumerateInstanceExtensionProperties()};
	CheckError(extensionProps);
	std::span<const char*> glfwExtensionRange{glfwExtensions,
											  glfwExtensionCount};
	for (const auto* const glfwExtension : glfwExtensionRange)
	{
		auto comp = [glfwExtension](const auto& extensionProp) -> bool
		{
			return std::strcmp(extensionProp.extensionName, glfwExtension) == 0;
		};
		namespace r = std::ranges;
		if (r::none_of(extensionProps.value, comp))
		{
			std::println(stderr,
						 "ERROR: Required GLFW extension not supported: {}",
						 glfwExtension);
			std::exit(1);
		}
	}
	std::println("Available Extensions:");
	for (auto& extension : extensionProps.value)
	{
		std::println("\t{}", std::string(extension.extensionName));
	}

	vk::InstanceCreateInfo createInfo{
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
		.ppEnabledLayerNames = requiredLayers.data(),
		.enabledExtensionCount = glfwExtensionCount,
		.ppEnabledExtensionNames = glfwExtensions,
	};
	auto temp{this->context.createInstance(createInfo)};
	CheckError(temp);
	this->instance = std::move(temp.value);
}
void Program::CreateSurface()
{
	VkSurfaceKHR _surface{nullptr};
	auto surfaceResult{
		glfwCreateWindowSurface(*this->instance, this->window, nullptr,
								&_surface),
	};
	if (surfaceResult != 0)
	{
		std::println(stderr, "ERROR: failed to create window surface!");
		std::exit(1);
	}
	this->surface = vk::raii::SurfaceKHR(this->instance, _surface);
}
void Program::PickPhysicalDevice()
{
	auto devices{this->instance.enumeratePhysicalDevices()};
	CheckError(devices);
	if (devices.value.empty())
	{
		std::println(stderr, "ERROR: Failed to find GPUs with Vulkan support");
		std::exit(1);
	}

	std::multimap<int, vk::raii::PhysicalDevice> candidateGPUs;

	for (auto& device : devices.value)
	{
		namespace r = std::ranges;
		auto deviceProps{device.getProperties()};
		std::println(stdout, "Found GPU: {}",
					 std::string(deviceProps.deviceName));

		if (deviceProps.apiVersion < vk::ApiVersion13)
		{
			std::println(stdout, "	GPU failed because: Incorrect API version");
			continue;
		}
		else
		{
			std::println(stdout, "	GPU has suitable API version");
		}

		bool hasSuitableQueue{false};
		auto queueFamilies{device.getQueueFamilyProperties()};
		for (const auto i : r::views::iota(0u, queueFamilies.size()))
		{
			bool graphics{
				(queueFamilies.at(i).queueFlags & vk::QueueFlagBits::eGraphics)
					!= static_cast<vk::QueueFlags>(0),
			};
			if (!graphics)
				continue;
			if (device.getSurfaceSupportKHR(i, *this->surface).value
				== vk::False)
				continue;

			hasSuitableQueue = true;
			break;
		}
		if (!hasSuitableQueue)
		{
			std::println(stdout,
						 "	GPU failed because: Missing queue families");
			continue;
		}
		else
		{
			std::println(stdout, "	GPU found suitable queue families");
		}

		auto deviceExtensions{device.enumerateDeviceExtensionProperties()};
		auto extensionCheck = [&deviceExtensions](const auto& val) -> bool
		{
			auto cmp = [val](const auto& ext) -> bool
			{
				return std::strcmp(ext.extensionName, val) == 0;
			};
			return r::any_of(deviceExtensions.value, cmp);
		};
		if (!r::all_of(REQUIRED_DEVICE_EXTENSIONS, extensionCheck))
		{
			std::println(stdout, "	GPU failed because: Missing extensions");
			continue;
		}
		else
		{
			std::println(stdout, "	GPU supports all extensions");
		}

		auto feats{
			device.getFeatures2<
				vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
				vk::PhysicalDeviceVulkan13Features,
				vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>(),
		};
		bool hasRequiredFeats
			= (feats.get<vk::PhysicalDeviceVulkan11Features>()
				   .shaderDrawParameters
			   == vk::True)
			  && (feats.get<vk::PhysicalDeviceVulkan13Features>()
					  .dynamicRendering
				  == vk::True)
			  && (feats.get<vk::PhysicalDeviceVulkan13Features>()
					  .synchronization2
				  == vk::True)
			  && (feats.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
					  .extendedDynamicState
				  == vk::True);
		if (!hasRequiredFeats)
		{
			std::println(stdout, "	GPU failed because: Missing features");
			continue;
		}
		else
		{
			std::println(stdout, "	GPU has all required features");
		}

		uint32_t score{0};
		switch (deviceProps.deviceType)
		{
			using enum vk::PhysicalDeviceType;
		case eDiscreteGpu:
			score += 1000;
			break;
		case eIntegratedGpu:
			score += 500;
			break;
		case eCpu:
			score += 10;
			break;
		default:
			break;
		}
		score += deviceProps.limits.maxImageDimension2D;

		candidateGPUs.insert({score, device});
	}
	std::println();
	if (!candidateGPUs.empty() && candidateGPUs.rbegin()->first > 0)
	{
		this->physicalDevice = candidateGPUs.rbegin()->second;
		std::println(
			stdout, "Selected GPU: {}",
			std::string(this->physicalDevice.getProperties().deviceName));
		std::println();
	}
	else
	{
		std::println(stderr, "ERROR: Failed to find suitable GPU");
		std::exit(1);
	}
}
void Program::CreateLogicalDevice()
{
	namespace r = std::ranges;
	auto queueFamilyProps{this->physicalDevice.getQueueFamilyProperties()};

	for (const auto i : r::views::iota(0u, queueFamilyProps.size()))
	{
		bool graphics{
			(queueFamilyProps.at(i).queueFlags & vk::QueueFlagBits::eGraphics)
				!= static_cast<vk::QueueFlags>(0),
		};
		if (!graphics)
			continue;
		auto present
			= this->physicalDevice.getSurfaceSupportKHR(i, *this->surface);
		if (present.value == vk::False)
			continue;

		this->graphicsIdx = i;
		break;
	}
	if (this->graphicsIdx == ~0u)
	{
		std::println(stderr,
					 "ERROR: Could not find suitable graphics/presnt queue");
		std::exit(1);
	}

	vk::StructureChain<vk::PhysicalDeviceFeatures2,
					   vk::PhysicalDeviceVulkan11Features,
					   vk::PhysicalDeviceVulkan13Features,
					   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		featureChain = {
			{},
			{.shaderDrawParameters = vk::True},
			{.synchronization2 = vk::True, .dynamicRendering = vk::True},
			{.extendedDynamicState = vk::True},
	};

	const float queuePrio{0.5f};
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		.queueFamilyIndex = this->graphicsIdx,
		.queueCount = 1,
		.pQueuePriorities = &queuePrio,
	};
	vk::DeviceCreateInfo deviceCreateInfo{
		.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &deviceQueueCreateInfo,
		.enabledExtensionCount
		= static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size()),
		.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data(),
	};

	auto device{this->physicalDevice.createDevice(deviceCreateInfo)};
	CheckError(device);
	this->device = std::move(device.value);
	this->graphicsQueue = this->device.getQueue(this->graphicsIdx, 0);
}
void Program::CreateSwapChain()
{
	namespace r = std::ranges;
	auto surfaceCapabilities{
		this->physicalDevice.getSurfaceCapabilitiesKHR(*this->surface),
	};

	bool isExtentUnset = surfaceCapabilities.value.currentExtent.width
						 != std::numeric_limits<uint32_t>::max();
	if (isExtentUnset)
	{
		this->swapChainExtent = surfaceCapabilities.value.currentExtent;
	}
	else
	{
		int width{};
		int height{};
		glfwGetFramebufferSize(this->window, &width, &height);
		width = static_cast<int>(std::clamp<uint32_t>(
			static_cast<uint32_t>(width),
			surfaceCapabilities.value.minImageExtent.width,
			surfaceCapabilities.value.maxImageExtent.width));
		height = static_cast<int>(std::clamp<uint32_t>(
			static_cast<uint32_t>(height),
			surfaceCapabilities.value.minImageExtent.height,
			surfaceCapabilities.value.maxImageExtent.height));
		this->swapChainExtent = vk::Extent2D{
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height),
		};
	}

	auto minImgCount{
		std::max(3u, surfaceCapabilities.value.minImageCount),
	};
	auto isImgCountTooHigh{
		(0 < surfaceCapabilities.value.maxImageCount)
			&& (surfaceCapabilities.value.maxImageCount
				< surfaceCapabilities.value.minImageCount),
	};
	if (isImgCountTooHigh)
	{
		minImgCount = surfaceCapabilities.value.maxImageCount;
	}

	auto availableFormats{
		this->physicalDevice.getSurfaceFormatsKHR(*this->surface),
	};
	CheckError(availableFormats);
	auto formatCond = [](const auto& val) -> bool
	{
		return (val.format == vk::Format::eB8G8R8A8Srgb)
			   && (val.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
	};
	const auto formatIt{r::find_if(availableFormats.value, formatCond)};
	this->swapChainSurfaceFormat = formatIt != availableFormats.value.end()
									   ? *formatIt
									   : availableFormats.value.at(0);

	auto availablePresentModes{
		physicalDevice.getSurfacePresentModesKHR(*surface),
	};
	CheckError(availablePresentModes);
	auto modeCond = [](const auto mode) -> bool
	{
		return mode == vk::PresentModeKHR::eMailbox;
	};
	auto presentMode{
		r::any_of(availablePresentModes.value, modeCond)
			? vk::PresentModeKHR::eMailbox
			: vk::PresentModeKHR::eFifo,
	};

	vk::SwapchainCreateInfoKHR createInfo{
		.surface = *this->surface,
		.minImageCount = minImgCount,
		.imageFormat = this->swapChainSurfaceFormat.format,
		.imageColorSpace = this->swapChainSurfaceFormat.colorSpace,
		.imageExtent = this->swapChainExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform = surfaceCapabilities.value.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = presentMode,
		.clipped = vk::True,
	};

	auto swapChain{this->device.createSwapchainKHR(createInfo)};
	CheckError(swapChain);
	this->swapChain = std::move(swapChain.value);

	auto images{this->swapChain.getImages()};
	CheckError(images);
	this->swapChainImages = std::move(images.value);
}
void Program::CreateImageViews()
{
	vk::ImageViewCreateInfo imageViewCreateInfo{
		.viewType = vk::ImageViewType::e2D,
		.format = this->swapChainSurfaceFormat.format,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	for (auto& image : this->swapChainImages)
	{
		imageViewCreateInfo.image = image;
		auto imgView{this->device.createImageView(imageViewCreateInfo)};
		CheckError(imgView);
		this->swapChainImageViews.emplace_back(std::move(imgView.value));
	}
}
void Program::CreateGraphicsPipeline()
{
	auto shaderCode{LoadShader(SHADERS "tringle")};
	vk::ShaderModuleCreateInfo shaderCreateInfo{
		.codeSize = shaderCode.size() * sizeof(char),
		.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data()), // GROSS
	};
	auto shaderModule{device.createShaderModule(shaderCreateInfo)};
	CheckError(shaderModule);

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = shaderModule.value,
		.pName = "vertMain",
	};
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = shaderModule.value,
		.pName = "fragMain",
	};
	std::array<vk::PipelineShaderStageCreateInfo, 2> stages{{
		vertShaderStageInfo,
		fragShaderStageInfo,
	}};

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
		.topology = vk::PrimitiveTopology::eTriangleList,
	};

	vk::PipelineViewportStateCreateInfo viewportState{
		.viewportCount = 1,
		.scissorCount = 1,
	};

	vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eClockwise,
		.depthBiasEnable = vk::False,
		.lineWidth = 1.0f,
	};

	vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
	};

	vk::PipelineColorBlendAttachmentState colorBlendAttachment{
		.blendEnable = vk::True,
		.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
		.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
		.colorBlendOp = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eZero,
		.alphaBlendOp = vk::BlendOp::eAdd,
		.colorWriteMask = vk::ColorComponentFlagBits::eR
						  | vk::ColorComponentFlagBits::eG
						  | vk::ColorComponentFlagBits::eB
						  | vk::ColorComponentFlagBits::eA,
	};

	vk::PipelineColorBlendStateCreateInfo colorBlending{
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
	};

	std::vector<vk::DynamicState> dynamicStates{
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	};
	vk::PipelineDynamicStateCreateInfo dynamicState{
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data(),
	};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		.setLayoutCount = 0,
		.pushConstantRangeCount = 0,
	};
	auto pipelineLayout{this->device.createPipelineLayout(pipelineLayoutInfo)};
	CheckError(pipelineLayout);
	this->pipelineLayout = std::move(pipelineLayout.value);

	vk::StructureChain<vk::GraphicsPipelineCreateInfo,
					   vk::PipelineRenderingCreateInfo>
		pipelineCreateInfoChain{
			{
				.stageCount = 2,
				.pStages = stages.data(),
				.pVertexInputState = &vertexInputInfo,
				.pInputAssemblyState = &inputAssembly,
				.pViewportState = &viewportState,
				.pRasterizationState = &rasterizer,
				.pMultisampleState = &multisampling,
				.pColorBlendState = &colorBlending,
				.pDynamicState = &dynamicState,
				.layout = this->pipelineLayout,
				.renderPass = nullptr,
			},
			{
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &this->swapChainSurfaceFormat.format,
			},
	};

	auto pipeline{
		this->device.createGraphicsPipeline(
			nullptr,
			pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()),
	};
	CheckError(pipeline);
	this->graphicsPipeline = std::move(pipeline.value);
}
void Program::CreateCommandPool()
{
	vk::CommandPoolCreateInfo poolInfo{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = this->graphicsIdx,
	};
	auto commandPool{this->device.createCommandPool(poolInfo)};
	CheckError(commandPool);
	this->commandPool = std::move(commandPool.value);
}
void Program::CreateCommandBuffer()
{
	vk::CommandBufferAllocateInfo allocInfo{
		.commandPool = this->commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};
	auto buffers{this->device.allocateCommandBuffers(allocInfo)};
	CheckError(buffers);
	if (buffers.value.empty())
	{
		std::println(stderr, "ERROR: No command buffers allocated");
		std::exit(1);
	}
	this->commandBuffer = std::move(buffers.value.front());
}
void Program::RecordCommandBuffer(uint32_t imgIdx)
{
	commandBuffer.begin({});

	this->TransitionImageLayout(
		imgIdx, vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal, //
		{},										  // blank
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput);

	vk::ClearValue clearColor{
		vk::ClearColorValue(0.133f, 0.302f, 0.855f, 1.0f)};
	vk::RenderingAttachmentInfo attachmentInfo{
		.imageView = this->swapChainImageViews.at(imgIdx),
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = clearColor,
	};
	vk::RenderingInfo renderingInfo{
		.renderArea = {
			.offset = {.x = 0, .y = 0},
			.extent = this->swapChainExtent,
		},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &attachmentInfo,
	};

	this->commandBuffer.beginRendering(renderingInfo);
	this->commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
									 *this->graphicsPipeline);
	this->commandBuffer.setViewport(
		0, vk::Viewport(
			   0.0f, 0.0f, static_cast<float>(this->swapChainExtent.width),
			   static_cast<float>(this->swapChainExtent.height), 0.0f, 1.0f));
	this->commandBuffer.setScissor(
		0, vk::Rect2D(vk::Offset2D(0, 0), this->swapChainExtent));
	this->commandBuffer.draw(3, 1, 0, 0);

	this->commandBuffer.endRendering();

	this->TransitionImageLayout(
		imgIdx, vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits2::eColorAttachmentWrite, //
		{},											// blank
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eBottomOfPipe);
	this->commandBuffer.end();
}
void Program::TransitionImageLayout(uint32_t imgIdx, vk::ImageLayout oldLayout,
									vk::ImageLayout newLayout,
									vk::AccessFlags2 srcAccess,
									vk::AccessFlags2 dstAccess,
									vk::PipelineStageFlags2 srcStage,
									vk::PipelineStageFlags2 dstStage)
{
	vk::ImageMemoryBarrier2 barrier{
		.srcStageMask = srcStage,
		.srcAccessMask = srcAccess,
		.dstStageMask = dstStage,
		.dstAccessMask = dstAccess,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = this->swapChainImages.at(imgIdx),
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	vk::DependencyInfo dependencyInfo{
		.dependencyFlags = {},
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier,
	};
	this->commandBuffer.pipelineBarrier2(dependencyInfo);
}
void Program::CreateSyncObjects()
{
	auto present{this->device.createSemaphore({})};
	CheckError(present);
	this->presentCompleteSemaphore = std::move(present.value);

	auto render{this->device.createSemaphore({})};
	CheckError(render);
	this->renderFinishedSemaphore = std::move(render.value);

	auto draw{
		this->device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled}),
	};
	CheckError(draw);
	this->drawFence = std::move(draw.value);
}
