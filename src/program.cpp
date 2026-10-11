#include "program.hpp"

#include <GLFW/glfw3.h>
#include <map>
#include <print>
#include <ranges>

const std::vector<const char*> VALIDATION_LAYERS = {
	// NOLINT
	"VK_LAYER_KHRONOS_validation",
};
#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION{false};
#else
constexpr bool ENABLE_VALIDATION{true};
#endif // !NDEBUG

constexpr auto IsGraphicsQueue(const vk::QueueFamilyProperties& val) -> bool
{
	return (val.queueFlags & vk::QueueFlagBits::eGraphics)
		   != static_cast<vk::QueueFlags>(0);
};

void Program::Run()
{
	std::println("Hello, Vulkan!");
	this->Init();
	while (glfwWindowShouldClose(this->window) == 0)
	{
		glfwPollEvents();
		this->Update();
	}

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
};
void Program::Update() { }

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
	if (!layerProps.has_value())
	{
		std::println("ERROR: {}", vk::to_string(layerProps.result));
		std::exit(1);
	}
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
	auto extensionProps{context.enumerateInstanceExtensionProperties()};
	if (!extensionProps.has_value())
	{
		std::println(stderr, "ERROR: {}", vk::to_string(extensionProps.result));
		std::exit(1);
	}

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
		.enabledExtensionCount = glfwExtensionCount,
		.ppEnabledExtensionNames = glfwExtensions,
	};

	auto temp = this->context.createInstance(createInfo);
	if (!temp.has_value())
	{
		std::println(stderr, "ERROR: {}", vk::to_string(temp.result));
		std::exit(1);
	}
	else
	{
		this->instance = std::move(temp.value);
	}
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
		std::println(stderr, "ERROR: Failed to create window Surface");
		std::exit(1);
	}
	this->surface = vk::raii::SurfaceKHR(this->instance, _surface);
}

void Program::PickPhysicalDevice()
{
	auto devices{this->instance.enumeratePhysicalDevices()};
	if (!devices.has_value())
	{
		std::println(stderr, "ERROR: {}", vk::to_string(devices.result));
		std::exit(0);
	}
	if (devices->empty())
	{
		std::println(stderr, "ERROR: Failed to find GPUs with Vulkan support");
		std::exit(0);
	}

	std::multimap<int, vk::raii::PhysicalDevice> candidateGPUs;

	std::vector<const char*> requiredExtensions{vk::KHRSwapchainExtensionName};

	std::println(stdout, "Finding Suitable GPUs");

	for (auto& device : devices.value)
	{
		namespace r = std::ranges;
		auto deviceProps{device.getProperties()};
		std::println(stdout, "Found GPU: {}",
					 std::string(deviceProps.deviceName));
		// auto deviceFeats{device.getFeatures()};
		auto deviceFeats{
			device.getFeatures2<
				vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
				vk::PhysicalDeviceVulkan13Features,
				vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>(),
		};
		bool hasRequiredFeats
			= (deviceFeats.get<vk::PhysicalDeviceVulkan11Features>()
				   .shaderDrawParameters
			   != 0u)
			  && (deviceFeats.get<vk::PhysicalDeviceVulkan13Features>()
					  .dynamicRendering
				  != 0u)
			  && (deviceFeats
					  .get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
					  .extendedDynamicState
				  != 0u);

		auto queueFamilies{device.getQueueFamilyProperties()};
		auto deviceExtensions = device.enumerateDeviceExtensionProperties();
		auto extensionCheck = [&deviceExtensions](const auto& val) -> bool
		{
			auto cmp = [val](const auto& ext) -> bool
			{
				return std::strcmp(ext.extensionName, val) == 0;
			};
			return r::any_of(deviceExtensions.value, cmp);
		};
		uint32_t score{0};

		// if (deviceFeats.geometryShader == 0u)
		// 	continue;
		if (!hasRequiredFeats)
		{
			std::println(stdout, "	GPU failed because: Missing features");
			continue;
		}
		if (deviceProps.apiVersion < vk::ApiVersion13)
		{
			std::println(stdout, "	GPU failed because: Incorrect API Version");
			continue;
		}
		if (r::none_of(queueFamilies, IsGraphicsQueue))
		{
			std::println(stdout,
						 "	GPU failed because: Missing queue families");
			continue;
		}
		if (!r::all_of(requiredExtensions, extensionCheck))
		{
			std::println(stdout, "	GPU failed because: Missing extensions");
			continue;
		}

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
	if (!candidateGPUs.empty() && candidateGPUs.rbegin()->first > 0)
	{
		this->physicalDevice = candidateGPUs.rbegin()->second;
		std::println(
			stdout, "Selected GPU: {}",
			std::string(this->physicalDevice.getProperties().deviceName));
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
	auto queueFamProperties{this->physicalDevice.getQueueFamilyProperties()};
	auto graphicsQueueFamIt{
		r::find_if(queueFamProperties,
				   [](const auto& val) { return IsGraphicsQueue(val); }),
	};
	uint32_t graphicsIdx{~0u};
	for (const uint32_t i : r::views::iota(0u, queueFamProperties.size()))
	{
		const auto& queueFam{queueFamProperties[i]};
		auto supportsKHR{
			this->physicalDevice.getSurfaceSupportKHR(i, *this->surface),
		};
		if (!supportsKHR.has_value())
		{
			std::println(stderr, "ERROR: {}",
						 vk::to_string(supportsKHR.result));
			std::exit(1);
		}

		if (IsGraphicsQueue(queueFam) && (supportsKHR.value != 0))
		{
			graphicsIdx = i;
			break;
		}
	}
	if (graphicsIdx == !0u)
	{
		std::println(stderr,
					 "ERROR: Could not find suitable graphics/presnt queue");
		std::exit(1);
	}
	// auto graphicsIdx{static_cast<uint32_t>(
	// 	r::distance(queueFamProperties.begin(), graphicsQueueFamIt))};
	const float queuePrio{0.5f};
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		.queueFamilyIndex = graphicsIdx,
		.queueCount = 1,
		.pQueuePriorities = &queuePrio,
	};
	vk::StructureChain<vk::PhysicalDeviceFeatures2,
					   vk::PhysicalDeviceVulkan11Features,
					   vk::PhysicalDeviceVulkan13Features,
					   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		featureChain = {
			{},
			{.shaderDrawParameters = 1u},
			{.dynamicRendering = 1u},
			{.extendedDynamicState = 1u},
	};
	std::vector<const char*> requiredDeviceExtensions{
		vk::KHRSwapchainExtensionName,
	};
	vk::DeviceCreateInfo deviceCreateInfo{
		.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &deviceQueueCreateInfo,
		.enabledExtensionCount
		= static_cast<uint32_t>(requiredDeviceExtensions.size()),
		.ppEnabledExtensionNames = requiredDeviceExtensions.data(),
	};
	auto device = this->physicalDevice.createDevice(deviceCreateInfo);
	if (!device.has_value())
	{
		std::println(stderr, "ERROR: {}", vk::to_string(device.result));
		std::exit(1);
	}
	else
	{
		this->device = std::move(device.value);
		this->graphicsQueue = this->device.getQueue(graphicsIdx, 0);
	}
}
void Program::CreateSwapChain()
{
	namespace r = std::ranges;
	const auto surfaceCapabilities{
		this->physicalDevice.getSurfaceCapabilitiesKHR(*this->surface),
	};
	auto availableFormats{
		this->physicalDevice.getSurfaceFormatsKHR(*this->surface),
	};
	auto availablePresentModes{
		this->physicalDevice.getSurfacePresentModesKHR(*this->surface),
	};
	if (!surfaceCapabilities.has_value())
	{
		std::println(stderr, "ERROR: {}",
					 vk::to_string(surfaceCapabilities.result));
		std::exit(1);
	}
	if (!availableFormats.has_value())
	{
		std::println(stderr, "ERROR: {}",
					 vk::to_string(availableFormats.result));
		std::exit(1);
	}
	if (!availablePresentModes.has_value())
	{
		std::println(stderr, "ERROR: {}",
					 vk::to_string(availablePresentModes.result));
		std::exit(1);
	}

	auto formatCond = [](const auto& val) -> bool
	{
		return (val.format == vk::Format::eB8G8R8A8Srgb)
			   && (val.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
	};
	const auto formatIt{r::find_if(availableFormats.value, formatCond)};
	this->swapChainSurfaceFormat = formatIt != availableFormats->end()
									   ? *formatIt
									   : availableFormats.value.at(0);

	auto modeCond = [](const auto mode) -> bool
	{
		return mode == vk::PresentModeKHR::eMailbox;
	};
	auto presentMode{
		r::any_of(availablePresentModes.value, modeCond)
			? vk::PresentModeKHR::eMailbox
			: vk::PresentModeKHR::eFifo,
	};

	bool isExtentUnset = surfaceCapabilities->currentExtent.width
						 != std::numeric_limits<uint32_t>::max();
	if (isExtentUnset)
	{
		this->swapChainExtent = surfaceCapabilities->currentExtent;
	}
	else
	{
		int width;
		int height;
		glfwGetFramebufferSize(this->window, &width, &height);
		width = static_cast<int>(
			std::clamp<uint32_t>(static_cast<uint32_t>(width),
								 surfaceCapabilities->minImageExtent.width,
								 surfaceCapabilities->maxImageExtent.width));
		height = static_cast<int>(
			std::clamp<uint32_t>(static_cast<uint32_t>(height),
								 surfaceCapabilities->minImageExtent.height,
								 surfaceCapabilities->maxImageExtent.height));
		this->swapChainExtent = vk::Extent2D{
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height),
		};
	}

	auto minImgCount{
		std::max(3u, surfaceCapabilities->minImageCount),
	};
	auto isImgCountTooHigh{
		(0 < surfaceCapabilities->maxImageCount)
			&& (surfaceCapabilities->maxImageCount
				< surfaceCapabilities->minImageCount),
	};
	if (isImgCountTooHigh)
	{
		minImgCount = surfaceCapabilities->maxImageCount;
	}

	vk::SwapchainCreateInfoKHR createInfo{
		.surface = *this->surface,
		.minImageCount = minImgCount,
		.imageFormat = this->swapChainSurfaceFormat.format,
		.imageColorSpace = this->swapChainSurfaceFormat.colorSpace,
		.imageExtent = this->swapChainExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform = surfaceCapabilities->currentTransform,
		// Change for blending
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = presentMode,
		.clipped = 1u,
	};
	auto swapChain{this->device.createSwapchainKHR(createInfo)};
	if (!swapChain.has_value())
	{
		std::println(stderr, "ERROR: {}", vk::to_string(swapChain.result));
		std::exit(1);
	}
	else
	{
		this->swapChain = std::move(swapChain.value);
		auto images{this->swapChain.getImages()};
		if (!images.has_value())
		{
			std::println(stderr, "ERROR: {}", vk::to_string(images.result));
			std::exit(1);
		}
		else
		{
			this->swapChainImages = std::move(images.value);
		}
	}
}
