/* RageDisplay_Vulkan_Helpers — Vulkan context, pipeline cache, render target, compiled geometry. */

#ifndef RAGE_DISPLAY_VULKAN_HELPERS_H
#define RAGE_DISPLAY_VULKAN_HELPERS_H

#include <vulkan/vulkan.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include "RageDisplay.h"
#include "RageTypes.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

// --------------------------------------------------------------------
// VulkanContext — all Vulkan instance/device/swapchain state
// --------------------------------------------------------------------
struct VulkanContext
{
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily = 0;
	uint32_t presentQueueFamily = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;

	// Swapchain
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
	VkExtent2D swapchainExtent = {0, 0};
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	// Depth buffer
	VkImage depthImage = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VmaAllocation depthAllocation = VK_NULL_HANDLE;
	VkFormat depthFormat = VK_FORMAT_D16_UNORM;

	// Render pass
	VkRenderPass renderPass = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> framebuffers;

	// Per-frame resources
	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
	struct FrameData
	{
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkSemaphore imageAvailable = VK_NULL_HANDLE;
		VkSemaphore renderFinished = VK_NULL_HANDLE;
		VkFence inFlight = VK_NULL_HANDLE;

		// Streaming vertex ring buffer
		VkBuffer vertexBuffer = VK_NULL_HANDLE;
		VmaAllocation vertexAllocation = VK_NULL_HANDLE;
		void *vertexMapped = nullptr;
		uint32_t vertexOffset = 0;
		uint32_t vertexCapacity = 0;

		// Staging buffer for texture uploads
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingAllocation = VK_NULL_HANDLE;
		void *stagingMapped = nullptr;
		uint32_t stagingOffset = 0;
		uint32_t stagingCapacity = 0;

		// Descriptor pool (reset each frame)
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	};
	FrameData frames[MAX_FRAMES_IN_FLIGHT];
	uint32_t currentFrame = 0;
	uint32_t currentImageIndex = 0;

	bool inRenderPass = false;
};

// --------------------------------------------------------------------
// Pipeline key — packed state for pipeline cache lookup
// --------------------------------------------------------------------
struct VulkanPipelineKey
{
	uint16_t shaderVariant;  // encodes vert+frag + specialization constants
	uint8_t blendMode;       // BlendMode enum
	uint8_t flags;           // bit 0: polygon mode (0=fill, 1=line)
	                         // bit 1: vertex format (0=sprite, 1=model)

	bool operator==(const VulkanPipelineKey &o) const
	{
		return shaderVariant == o.shaderVariant &&
		       blendMode == o.blendMode &&
		       flags == o.flags;
	}
};

struct VulkanPipelineKeyHash
{
	size_t operator()(const VulkanPipelineKey &k) const
	{
		uint32_t v = (uint32_t(k.shaderVariant) << 16) | (uint32_t(k.blendMode) << 8) | k.flags;
		return std::hash<uint32_t>()(v);
	}
};

// --------------------------------------------------------------------
// VulkanTexture — wraps a GPU texture image
// --------------------------------------------------------------------
struct VulkanTexture
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	int width = 0;
	int height = 0;
	bool hasMipmaps = false;
};

// --------------------------------------------------------------------
// VulkanRenderTarget — off-screen render target
// --------------------------------------------------------------------
struct VulkanRenderTarget
{
	VulkanTexture texture;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkImage depthImage = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VmaAllocation depthAllocation = VK_NULL_HANDLE;
	RenderTargetParam param;
};

// --------------------------------------------------------------------
// RageCompiledGeometryVulkan — pre-uploaded model mesh buffers
// --------------------------------------------------------------------
class RageCompiledGeometryVulkan : public RageCompiledGeometry
{
public:
	RageCompiledGeometryVulkan(VulkanContext *ctx);
	~RageCompiledGeometryVulkan();

	void Allocate(const std::vector<msMesh> &vMeshes) override;
	void Change(const std::vector<msMesh> &vMeshes) override;
	void Draw(int iMeshIndex) const override;

private:
	VulkanContext *m_ctx;

	VkBuffer m_VBOPos = VK_NULL_HANDLE;
	VkBuffer m_VBONormal = VK_NULL_HANDLE;
	VkBuffer m_VBOTex = VK_NULL_HANDLE;
	VkBuffer m_IBO = VK_NULL_HANDLE;
	VmaAllocation m_AllocPos = VK_NULL_HANDLE;
	VmaAllocation m_AllocNormal = VK_NULL_HANDLE;
	VmaAllocation m_AllocTex = VK_NULL_HANDLE;
	VmaAllocation m_AllocIBO = VK_NULL_HANDLE;

	int m_iTotalVertices = 0;
	int m_iTotalTriangles = 0;
};

// --------------------------------------------------------------------
// Utility functions
// --------------------------------------------------------------------
namespace VulkanUtil
{
	VkShaderModule CreateShaderModule(VkDevice device, const uint32_t *code, size_t size);

	void TransitionImageLayout(
		VkCommandBuffer cmd,
		VkImage image,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

	VkFormat RagePixelFormatToVkFormat(RagePixelFormat pf);
	bool NeedsCPUConversion(RagePixelFormat pf);

	// Returns the VkBlendFactor/VkBlendOp for a given BlendMode
	struct BlendState
	{
		VkBlendFactor srcColor, dstColor, srcAlpha, dstAlpha;
		VkBlendOp colorOp, alphaOp;
	};
	BlendState GetBlendState(BlendMode mode);
}

#endif
