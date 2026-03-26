/* RageDisplay_Vulkan_Helpers — Utility implementations. */

#include "global.h"
#include "RageDisplay_Vulkan_Helpers.h"
#include "RageLog.h"
#include "RageSurface.h"

// VMA implementation (exactly one translation unit)
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

// ====================================================================
// VulkanUtil
// ====================================================================

VkShaderModule VulkanUtil::CreateShaderModule(VkDevice device, const uint32_t *code, size_t size)
{
	VkShaderModuleCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = size;
	ci.pCode = code;

	VkShaderModule mod = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
	{
		LOG->Warn("VulkanUtil::CreateShaderModule failed");
		return VK_NULL_HANDLE;
	}
	return mod;
}

void VulkanUtil::TransitionImageLayout(
	VkCommandBuffer cmd,
	VkImage image,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	VkImageAspectFlags aspectMask)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspectMask;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}

	vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
		0, nullptr, 0, nullptr, 1, &barrier);
}

VkFormat VulkanUtil::RagePixelFormatToVkFormat(RagePixelFormat pf)
{
	switch (pf)
	{
	case RagePixelFormat_RGBA8:  return VK_FORMAT_R8G8B8A8_UNORM;
	case RagePixelFormat_BGRA8:  return VK_FORMAT_B8G8R8A8_UNORM;
	case RagePixelFormat_RGBA4:  return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
	case RagePixelFormat_RGB5A1: return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
	case RagePixelFormat_RGB5:   return VK_FORMAT_R5G6B5_UNORM_PACK16;
	case RagePixelFormat_RGB8:   return VK_FORMAT_R8G8B8A8_UNORM; // converted on CPU
	case RagePixelFormat_PAL:    return VK_FORMAT_R8G8B8A8_UNORM; // converted on CPU
	case RagePixelFormat_BGR8:   return VK_FORMAT_B8G8R8A8_UNORM; // converted on CPU
	case RagePixelFormat_A1BGR5: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
	case RagePixelFormat_X1RGB5: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
	default:                     return VK_FORMAT_R8G8B8A8_UNORM;
	}
}

bool VulkanUtil::NeedsCPUConversion(RagePixelFormat pf)
{
	return pf == RagePixelFormat_RGB8 || pf == RagePixelFormat_PAL || pf == RagePixelFormat_BGR8;
}

VulkanUtil::BlendState VulkanUtil::GetBlendState(BlendMode mode)
{
	// Default: normal blending
	BlendState s;
	s.colorOp = VK_BLEND_OP_ADD;
	s.alphaOp = VK_BLEND_OP_ADD;
	s.srcAlpha = VK_BLEND_FACTOR_ONE;
	s.dstAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	switch (mode)
	{
	case BLEND_NORMAL:
		s.srcColor = VK_BLEND_FACTOR_SRC_ALPHA;
		s.dstColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case BLEND_ADD:
		s.srcColor = VK_BLEND_FACTOR_SRC_ALPHA;
		s.dstColor = VK_BLEND_FACTOR_ONE;
		break;
	case BLEND_SUBTRACT:
		s.srcColor = VK_BLEND_FACTOR_SRC_ALPHA;
		s.dstColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		s.colorOp = VK_BLEND_OP_REVERSE_SUBTRACT;
		break;
	case BLEND_MODULATE:
		s.srcColor = VK_BLEND_FACTOR_ZERO;
		s.dstColor = VK_BLEND_FACTOR_SRC_COLOR;
		break;
	case BLEND_COPY_SRC:
		s.srcColor = VK_BLEND_FACTOR_ONE;
		s.dstColor = VK_BLEND_FACTOR_ZERO;
		s.srcAlpha = VK_BLEND_FACTOR_ONE;
		s.dstAlpha = VK_BLEND_FACTOR_ZERO;
		break;
	case BLEND_ALPHA_MASK:
		s.srcColor = VK_BLEND_FACTOR_ZERO;
		s.dstColor = VK_BLEND_FACTOR_ONE;
		s.srcAlpha = VK_BLEND_FACTOR_ZERO;
		s.dstAlpha = VK_BLEND_FACTOR_SRC_ALPHA;
		break;
	case BLEND_ALPHA_KNOCK_OUT:
		s.srcColor = VK_BLEND_FACTOR_ZERO;
		s.dstColor = VK_BLEND_FACTOR_ONE;
		s.srcAlpha = VK_BLEND_FACTOR_ZERO;
		s.dstAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case BLEND_ALPHA_MULTIPLY:
		s.srcColor = VK_BLEND_FACTOR_SRC_ALPHA;
		s.dstColor = VK_BLEND_FACTOR_ZERO;
		break;
	case BLEND_WEIGHTED_MULTIPLY:
		s.srcColor = VK_BLEND_FACTOR_SRC_ALPHA;
		s.dstColor = VK_BLEND_FACTOR_SRC_COLOR;
		break;
	case BLEND_INVERT_DEST:
		s.srcColor = VK_BLEND_FACTOR_ONE;
		s.dstColor = VK_BLEND_FACTOR_ONE;
		s.colorOp = VK_BLEND_OP_SUBTRACT;
		break;
	case BLEND_NO_EFFECT:
		s.srcColor = VK_BLEND_FACTOR_ZERO;
		s.dstColor = VK_BLEND_FACTOR_ONE;
		s.srcAlpha = VK_BLEND_FACTOR_ZERO;
		s.dstAlpha = VK_BLEND_FACTOR_ONE;
		break;
	default:
		s.srcColor = VK_BLEND_FACTOR_SRC_ALPHA;
		s.dstColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	}
	return s;
}

// ====================================================================
// RageCompiledGeometryVulkan
// ====================================================================

RageCompiledGeometryVulkan::RageCompiledGeometryVulkan(VulkanContext *ctx)
	: m_ctx(ctx)
{
}

RageCompiledGeometryVulkan::~RageCompiledGeometryVulkan()
{
	if (m_ctx && m_ctx->allocator)
	{
		if (m_VBOPos) vmaDestroyBuffer(m_ctx->allocator, m_VBOPos, m_AllocPos);
		if (m_VBONormal) vmaDestroyBuffer(m_ctx->allocator, m_VBONormal, m_AllocNormal);
		if (m_VBOTex) vmaDestroyBuffer(m_ctx->allocator, m_VBOTex, m_AllocTex);
		if (m_IBO) vmaDestroyBuffer(m_ctx->allocator, m_IBO, m_AllocIBO);
	}
}

void RageCompiledGeometryVulkan::Allocate(const std::vector<msMesh> &vMeshes)
{
	m_iTotalVertices = GetTotalVertices();
	m_iTotalTriangles = GetTotalTriangles();

	if (m_iTotalVertices == 0 || m_iTotalTriangles == 0)
		return;

	VkBufferCreateInfo bufCI = {};
	bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocCI = {};
	allocCI.usage = VMA_MEMORY_USAGE_AUTO;
	allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	// Position VBO
	bufCI.size = m_iTotalVertices * sizeof(RageVector3);
	vmaCreateBuffer(m_ctx->allocator, &bufCI, &allocCI, &m_VBOPos, &m_AllocPos, nullptr);

	// Normal VBO
	vmaCreateBuffer(m_ctx->allocator, &bufCI, &allocCI, &m_VBONormal, &m_AllocNormal, nullptr);

	// TexCoord VBO
	bufCI.size = m_iTotalVertices * sizeof(RageVector2);
	vmaCreateBuffer(m_ctx->allocator, &bufCI, &allocCI, &m_VBOTex, &m_AllocTex, nullptr);

	// Index buffer
	bufCI.size = m_iTotalTriangles * sizeof(msTriangle);
	bufCI.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vmaCreateBuffer(m_ctx->allocator, &bufCI, &allocCI, &m_IBO, &m_AllocIBO, nullptr);
}

void RageCompiledGeometryVulkan::Change(const std::vector<msMesh> &vMeshes)
{
	// Upload vertex data for each mesh
	for (size_t i = 0; i < vMeshes.size(); i++)
	{
		const MeshInfo &mi = m_vMeshInfo[i];
		const msMesh &mesh = vMeshes[i];

		// Map and upload positions
		{
			void *data;
			vmaMapMemory(m_ctx->allocator, m_AllocPos, &data);
			RageVector3 *dst = reinterpret_cast<RageVector3 *>(data) + mi.iVertexStart;
			for (int v = 0; v < mi.iVertexCount; v++)
				dst[v] = mesh.Vertices[v].p;
			vmaUnmapMemory(m_ctx->allocator, m_AllocPos);
		}

		// Normals
		{
			void *data;
			vmaMapMemory(m_ctx->allocator, m_AllocNormal, &data);
			RageVector3 *dst = reinterpret_cast<RageVector3 *>(data) + mi.iVertexStart;
			for (int v = 0; v < mi.iVertexCount; v++)
				dst[v] = mesh.Vertices[v].n;
			vmaUnmapMemory(m_ctx->allocator, m_AllocNormal);
		}

		// TexCoords
		{
			void *data;
			vmaMapMemory(m_ctx->allocator, m_AllocTex, &data);
			RageVector2 *dst = reinterpret_cast<RageVector2 *>(data) + mi.iVertexStart;
			for (int v = 0; v < mi.iVertexCount; v++)
				dst[v] = mesh.Vertices[v].t;
			vmaUnmapMemory(m_ctx->allocator, m_AllocTex);
		}

		// Indices (remap to global vertex offsets)
		{
			void *data;
			vmaMapMemory(m_ctx->allocator, m_AllocIBO, &data);
			msTriangle *dst = reinterpret_cast<msTriangle *>(data) + mi.iTriangleStart;
			for (int t = 0; t < mi.iTriangleCount; t++)
			{
				for (int j = 0; j < 3; j++)
					dst[t].nVertexIndices[j] = mesh.Triangles[t].nVertexIndices[j] + mi.iVertexStart;
			}
			vmaUnmapMemory(m_ctx->allocator, m_AllocIBO);
		}
	}
}

void RageCompiledGeometryVulkan::Draw(int iMeshIndex) const
{
	const MeshInfo &mi = m_vMeshInfo[iMeshIndex];
	if (mi.iTriangleCount == 0)
		return;

	auto &frame = m_ctx->frames[m_ctx->currentFrame];
	VkCommandBuffer cmd = frame.commandBuffer;

	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(cmd, 0, 1, &m_VBOPos, offsets);
	vkCmdBindVertexBuffers(cmd, 1, 1, &m_VBONormal, offsets);
	vkCmdBindVertexBuffers(cmd, 2, 1, &m_VBOTex, offsets);
	vkCmdBindIndexBuffer(cmd, m_IBO, 0, VK_INDEX_TYPE_UINT16);

	vkCmdDrawIndexed(cmd, mi.iTriangleCount * 3, 1,
		mi.iTriangleStart * 3, 0, 0);
}
