/* RageDisplay_GL3: OpenGL 3.3 core profile renderer. */

#ifndef RAGE_DISPLAY_GL3_H
#define RAGE_DISPLAY_GL3_H

#include "RageDisplay.h"
#include "RageDisplay_OGL_Helpers.h"

#include <cstdint>
#include <map>

class RageDisplay_GL3: public RageDisplay
{
public:
	RageDisplay_GL3();
	virtual ~RageDisplay_GL3();
	virtual std::string Init( const VideoModeParams &p, bool bAllowUnacceleratedRenderer );

	virtual std::string GetApiDescription() const { return "OpenGL 3.3"; }
	virtual void GetDisplaySpecs(DisplaySpecs &out) const;
	void ResolutionChanged();
	const RagePixelFormatDesc *GetPixelFormatDesc(RagePixelFormat pf) const;

	bool SupportsThreadedRendering();
	void BeginConcurrentRenderingMainThread();
	void EndConcurrentRenderingMainThread();
	void BeginConcurrentRendering();
	void EndConcurrentRendering();

	bool BeginFrame();
	void EndFrame();
	ActualVideoModeParams GetActualVideoModeParams() const;
	void SetBlendMode( BlendMode mode );
	bool SupportsTextureFormat( RagePixelFormat pixfmt, bool realtime=false );
	bool SupportsPerVertexMatrixScale();
	uintptr_t CreateTexture(
		RagePixelFormat pixfmt,
		RageSurface* img,
		bool bGenerateMipMaps );
	void UpdateTexture(
		uintptr_t iTexHandle,
		RageSurface* img,
		int xoffset, int yoffset, int width, int height
		);
	void DeleteTexture( uintptr_t iTexHandle );
	RageSurface *GetTexture( uintptr_t iTexture );

	void ClearAllTextures();
	int GetNumTextureUnits();
	void SetTexture( TextureUnit tu, uintptr_t iTexture );
	void SetTextureMode( TextureUnit tu, TextureMode tm );
	void SetTextureWrapping( TextureUnit tu, bool b );
	int GetMaxTextureSize() const;
	void SetTextureFiltering( TextureUnit tu, bool b );
	void SetEffectMode( EffectMode effect );
	bool IsEffectModeSupported( EffectMode effect );
	bool SupportsRenderToTexture() const;
	bool SupportsFullscreenBorderlessWindow() const;
	uintptr_t CreateRenderTarget( const RenderTargetParam &param, int &iTextureWidthOut, int &iTextureHeightOut );
	uintptr_t GetRenderTarget();
	void SetRenderTarget( uintptr_t iHandle, bool bPreserveTexture );
	bool IsZWriteEnabled() const;
	bool IsZTestEnabled() const;
	void SetZWrite( bool b );
	void SetZBias( float f );
	void SetZTestMode( ZTestMode mode );
	void ClearZBuffer();
	void SetCullMode( CullMode mode );
	void SetAlphaTest( bool b );
	void SetMaterial(
		const RageColor &emissive,
		const RageColor &ambient,
		const RageColor &diffuse,
		const RageColor &specular,
		float shininess
		);
	void SetLighting( bool b );
	void SetLightOff( int index );
	void SetLightDirectional(
		int index,
		const RageColor &ambient,
		const RageColor &diffuse,
		const RageColor &specular,
		const RageVector3 &dir );

	void SetSphereEnvironmentMapping( TextureUnit tu, bool b );
	void SetCelShaded( int stage );

	RageCompiledGeometry* CreateCompiledGeometry();
	void DeleteCompiledGeometry( RageCompiledGeometry* p );

	virtual void SetPolygonMode( PolygonMode pm );
	virtual void SetLineWidth( float fWidth );

	std::string GetTextureDiagnostics( uintptr_t id ) const;

protected:
	void DrawQuadsInternal( const RageSpriteVertex v[], int iNumVerts );
	void DrawQuadStripInternal( const RageSpriteVertex v[], int iNumVerts );
	void DrawFanInternal( const RageSpriteVertex v[], int iNumVerts );
	void DrawStripInternal( const RageSpriteVertex v[], int iNumVerts );
	void DrawTrianglesInternal( const RageSpriteVertex v[], int iNumVerts );
	void DrawCompiledGeometryInternal( const RageCompiledGeometry *p, int iMeshIndex );
	void DrawLineStripInternal( const RageSpriteVertex v[], int iNumVerts, float LineWidth );
	void DrawSymmetricQuadStripInternal( const RageSpriteVertex v[], int iNumVerts );

	std::string TryVideoMode( const VideoModeParams &p, bool &bNewDeviceOut );
	RageSurface* CreateScreenshot();
	RagePixelFormat GetImgPixelFormat( RageSurface* &img, bool &FreeImg, int width, int height, bool bPalettedTexture );

	void SendCurrentMatrices();

private:
	void InitShaderPrograms();
	void DestroyShaderPrograms();
	void UploadVertices( const RageSpriteVertex v[], int iNumVerts );
	void SetSpriteUniforms();
	void InvalidateMatrixCache();

	// Shader programs
	GLuint m_SpriteProgram;
	GLuint m_LitSpriteProgram;
	GLuint m_CurrentProgram;

	// Uniform locations for sprite program
	GLint m_uProjection;
	GLint m_uModelView;
	GLint m_uTextureMatrix;
	GLint m_uTexture0;
	GLint m_uTextureMode;
	GLint m_uAlphaTestEnabled;
	GLint m_uTextureEnabled;

	// Uniform locations for lit sprite program
	GLint m_uLitProjection;
	GLint m_uLitModelView;
	GLint m_uLitTextureMatrix;
	GLint m_uLitTexture0;
	GLint m_uLitTextureMode;
	GLint m_uLitAlphaTestEnabled;
	GLint m_uLitTextureEnabled;
	GLint m_uLightEnabled;
	GLint m_uLightDir;
	GLint m_uLightAmbient;
	GLint m_uLightDiffuse;
	GLint m_uLightSpecular;
	GLint m_uMatEmissive;
	GLint m_uMatAmbient;
	GLint m_uMatDiffuse;
	GLint m_uMatSpecular;
	GLint m_uMatShininess;

	// Effect shader programs
	GLuint m_EffectPrograms[NUM_EffectMode];

	// VAO/VBO
	GLuint m_VAO;
	GLuint m_VBO;
	GLuint m_QuadIBO;
	int m_iVBOSize;

	// Quad index buffer
	int m_iQuadIBOSize;
	void EnsureQuadIBO( int iNumQuads );

	// Symmetric quad strip IBO (reused across calls)
	GLuint m_SymQuadIBO;
	int m_iSymQuadIBOSize;

	// Current state (for uniforms)
	TextureMode m_CurTextureMode;
	bool m_bAlphaTestEnabled;
	bool m_bTextureEnabled[NUM_TextureUnit];
	bool m_bLightingEnabled;
	bool m_bSphereMapping;
	int m_iCelShadedStage;
	bool m_bInvertY;

	// GL state cache — skip redundant state changes
	BlendMode m_CachedBlendMode;
	bool m_bCachedZWrite;
	ZTestMode m_CachedZTestMode;
	float m_fCachedZBias;
	CullMode m_CachedCullMode;

	// Matrix cache — dirty flags to avoid redundant uniform uploads
	bool m_bMatrixDirty;
	// Cache the last uploaded matrices so we can detect changes
	RageMatrix m_CachedProjection;
	RageMatrix m_CachedModelView;
	RageMatrix m_CachedTextureMatrix;
	GLuint m_CachedMatrixProgram; // which program the cached matrices were uploaded to

	// Uniform cache — dirty flag to avoid redundant uniform uploads
	bool m_bUniformsDirty;
	// Cached uniform values (last uploaded)
	TextureMode m_CachedTextureMode;
	bool m_bCachedAlphaTestEnabled;
	bool m_bCachedTextureEnabled;
	bool m_bCachedLightingEnabled;
	GLuint m_CachedUniformProgram;
	// Cached lighting uniforms
	bool m_bLightUniformsDirty;

	// Material state
	RageColor m_MatEmissive, m_MatAmbient, m_MatDiffuse, m_MatSpecular;
	float m_fMatShininess;
	bool m_bMaterialLightingFallback; // when lighting off, use diffuse+emissive as vertex color

	// Light state
	struct LightState {
		bool enabled;
		RageColor ambient, diffuse, specular;
		RageVector3 dir;
	};
	LightState m_Lights[8];

	// Render targets
	std::map<uintptr_t, RenderTarget *> m_mapRenderTargets;
	RenderTarget *m_pCurrentRenderTarget;

	// Texture tracking for per-unit state
	uintptr_t m_iCurrentTextures[NUM_TextureUnit];
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
