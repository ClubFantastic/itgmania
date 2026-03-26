/* RageDisplay_Vulkan: Vulkan renderer backend. */

#ifndef RAGE_DISPLAY_VULKAN_H
#define RAGE_DISPLAY_VULKAN_H

#include "RageDisplay.h"
#include "RageDisplay_Vulkan_Helpers.h"

#include <cstdint>
#include <unordered_map>

class RageDisplay_Vulkan : public RageDisplay
{
public:
	RageDisplay_Vulkan();
	virtual ~RageDisplay_Vulkan();
	virtual std::string Init(const VideoModeParams &p, bool bAllowUnacceleratedRenderer);

	virtual std::string GetApiDescription() const { return "Vulkan"; }
	virtual void GetDisplaySpecs(DisplaySpecs &out) const;
	void ResolutionChanged();
	const RagePixelFormatDesc *GetPixelFormatDesc(RagePixelFormat pf) const;

	bool BeginFrame();
	void EndFrame();
	ActualVideoModeParams GetActualVideoModeParams() const;
	void SetBlendMode(BlendMode mode);
	bool SupportsTextureFormat(RagePixelFormat pixfmt, bool realtime = false);
	bool SupportsPerVertexMatrixScale();
	uintptr_t CreateTexture(
		RagePixelFormat pixfmt,
		RageSurface *img,
		bool bGenerateMipMaps);
	void UpdateTexture(
		uintptr_t iTexHandle,
		RageSurface *img,
		int xoffset, int yoffset, int width, int height);
	void DeleteTexture(uintptr_t iTexHandle);

	void ClearAllTextures();
	int GetNumTextureUnits();
	void SetTexture(TextureUnit tu, uintptr_t iTexture);
	void SetTextureMode(TextureUnit tu, TextureMode tm);
	void SetTextureWrapping(TextureUnit tu, bool b);
	int GetMaxTextureSize() const;
	void SetTextureFiltering(TextureUnit tu, bool b);
	void SetEffectMode(EffectMode effect);
	bool IsEffectModeSupported(EffectMode effect);
	bool SupportsRenderToTexture() const;
	bool SupportsFullscreenBorderlessWindow() const;
	uintptr_t CreateRenderTarget(const RenderTargetParam &param, int &iTextureWidthOut, int &iTextureHeightOut);
	uintptr_t GetRenderTarget();
	void SetRenderTarget(uintptr_t iHandle, bool bPreserveTexture);
	bool IsZWriteEnabled() const;
	bool IsZTestEnabled() const;
	void SetZWrite(bool b);
	void SetZBias(float f);
	void SetZTestMode(ZTestMode mode);
	void ClearZBuffer();
	void SetCullMode(CullMode mode);
	void SetAlphaTest(bool b);
	void SetMaterial(
		const RageColor &emissive,
		const RageColor &ambient,
		const RageColor &diffuse,
		const RageColor &specular,
		float shininess);
	void SetLighting(bool b);
	void SetLightOff(int index);
	void SetLightDirectional(
		int index,
		const RageColor &ambient,
		const RageColor &diffuse,
		const RageColor &specular,
		const RageVector3 &dir);

	void SetSphereEnvironmentMapping(TextureUnit tu, bool b);
	void SetCelShaded(int stage);

	RageCompiledGeometry *CreateCompiledGeometry();
	void DeleteCompiledGeometry(RageCompiledGeometry *p);

	virtual void SetPolygonMode(PolygonMode pm);
	virtual void SetLineWidth(float fWidth);

protected:
	void DrawQuadsInternal(const RageSpriteVertex v[], int iNumVerts);
	void DrawQuadStripInternal(const RageSpriteVertex v[], int iNumVerts);
	void DrawFanInternal(const RageSpriteVertex v[], int iNumVerts);
	void DrawStripInternal(const RageSpriteVertex v[], int iNumVerts);
	void DrawTrianglesInternal(const RageSpriteVertex v[], int iNumVerts);
	void DrawCompiledGeometryInternal(const RageCompiledGeometry *p, int iMeshIndex);
	void DrawLineStripInternal(const RageSpriteVertex v[], int iNumVerts, float LineWidth);
	void DrawSymmetricQuadStripInternal(const RageSpriteVertex v[], int iNumVerts);

	std::string TryVideoMode(const VideoModeParams &p, bool &bNewDeviceOut);
	RageSurface *CreateScreenshot();

private:
	// Initialization helpers
	bool CreateInstance();
	bool SelectPhysicalDevice();
	bool CreateLogicalDevice();
	bool CreateSwapchain();
	void DestroySwapchain();
	bool CreateRenderPass();
	bool CreateFramebuffers();
	bool CreateDepthBuffer();
	bool CreateFrameResources();
	void DestroyFrameResources();
	bool CreatePipelineLayout();
	bool CreateShaderModules();
	void DestroyShaderModules();
	bool CreateSamplers();

	// Per-frame helpers
	void UploadVertices(const RageSpriteVertex v[], int iNumVerts);
	void EnsureQuadIBO(int iNumQuads);
	void BindPipelineForCurrentState();
	void FlushPushConstants();
	void BindCurrentTexture();
	void BeginRenderPass();
	void EndRenderPass();
	VulkanPipelineKey MakeCurrentPipelineKey() const;
	VkPipeline GetOrCreatePipeline(const VulkanPipelineKey &key);
	uint16_t GetCurrentShaderVariant() const;

	// Texture helpers
	uintptr_t AllocTextureHandle();
	void UploadTextureData(VulkanTexture &tex, RageSurface *img,
		int xoffset, int yoffset, int width, int height);

	// Context
	VulkanContext m_ctx;

	// Pipeline cache
	std::unordered_map<VulkanPipelineKey, VkPipeline, VulkanPipelineKeyHash> m_pipelineCache;
	VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	VkPipelineCache m_vkPipelineCache = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descSetLayoutPerFrame = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descSetLayoutTexture = VK_NULL_HANDLE;

	// Shader modules
	VkShaderModule m_spriteVert = VK_NULL_HANDLE;
	VkShaderModule m_spriteFrag = VK_NULL_HANDLE;
	VkShaderModule m_litSpriteVert = VK_NULL_HANDLE;
	VkShaderModule m_litSpriteFrag = VK_NULL_HANDLE;
	VkShaderModule m_effectFrags[NUM_EffectMode] = {};

	// Samplers (wrap x filter = 4 combos)
	VkSampler m_samplers[4] = {};  // [wrap*2 + filter]
	VkSampler GetSampler(bool wrap, bool linear) const;

	// Constant white color buffer for models (no per-vertex color)
	VkBuffer m_whiteColorBuf = VK_NULL_HANDLE;
	VmaAllocation m_whiteColorAlloc = VK_NULL_HANDLE;

	// Quad index buffer
	VkBuffer m_quadIBO = VK_NULL_HANDLE;
	VmaAllocation m_quadIBOAlloc = VK_NULL_HANDLE;
	int m_iQuadIBOSize = 0;

	// UBO for lighting/material (per-frame ring)
	struct PerFrameUBOData
	{
		float modelView[16];
		float lightDir[4];
		float lightAmbient[4];
		float lightDiffuse[4];
		float lightSpecular[4];
		float matEmissive[4];
		float matAmbient[4];
		float matDiffuse[4];
		float matSpecular[4];
		float matShininess;
		float _pad[3];
	};
	VkBuffer m_uboBuffer = VK_NULL_HANDLE;
	VmaAllocation m_uboAlloc = VK_NULL_HANDLE;
	void *m_uboMapped = nullptr;
	uint32_t m_uboOffset = 0;
	static constexpr uint32_t UBO_RING_SIZE = 256; // max UBO sub-allocations per frame
	uint32_t m_uboAlignment = 256;

	// Dummy texture for when no texture is bound
	VulkanTexture m_dummyTexture;
	VkDescriptorSet m_dummyDescriptorSet = VK_NULL_HANDLE;

	// Current render state
	BlendMode m_curBlendMode = BLEND_NORMAL;
	ZTestMode m_curZTestMode = ZTEST_OFF;
	bool m_bZWrite = false;
	float m_fZBias = 0.0f;
	CullMode m_curCullMode = CULL_BACK;
	EffectMode m_curEffectMode = EffectMode_Normal;
	bool m_bAlphaTestEnabled = false;
	bool m_bLightingEnabled = false;
	TextureMode m_curTextureMode = TextureMode_Modulate;
	PolygonMode m_curPolygonMode = POLYGON_FILL;
	float m_fLineWidth = 1.0f;

	// Texture state
	uintptr_t m_iCurrentTextures[NUM_TextureUnit] = {};
	bool m_bTextureEnabled[NUM_TextureUnit] = {};
	bool m_bTextureWrapping[NUM_TextureUnit] = {};
	bool m_bTextureFiltering[NUM_TextureUnit] = {};
	std::unordered_map<uintptr_t, VulkanTexture> m_textures;
	uintptr_t m_nextTextureHandle = 1;

	// Lighting/material state
	struct LightState
	{
		bool enabled = false;
		RageColor ambient, diffuse, specular;
		RageVector3 dir;
	};
	LightState m_Lights[8];
	RageColor m_MatEmissive, m_MatAmbient, m_MatDiffuse, m_MatSpecular;
	float m_fMatShininess = 0.0f;

	// Render targets
	std::unordered_map<uintptr_t, VulkanRenderTarget> m_renderTargets;
	uintptr_t m_currentRT = 0;

	// Push constant data
	struct PushConstants
	{
		float mvp[16];
		float texMatrix[16];
	};

	// Track whether we've bound a pipeline this draw
	bool m_bPipelineBound = false;
	bool m_bDrawingModel = false;
	VulkanPipelineKey m_lastBoundKey = {};

	// Batched texture upload state
	VkCommandPool m_uploadPool = VK_NULL_HANDLE;
	VkCommandBuffer m_uploadCmdBuf = VK_NULL_HANDLE;
	VkFence m_uploadFence = VK_NULL_HANDLE;
	bool m_uploadCmd = false;
	std::vector<std::pair<VkBuffer, VmaAllocation>> m_pendingStagingBuffers;
	void FlushTextureUploads();
};

#endif

/*
 * Copyright (c) 2025 ITGmania Contributors
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
