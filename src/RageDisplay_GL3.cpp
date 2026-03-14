/* RageDisplay_GL3: OpenGL 3.3 core profile renderer. */

#include "global.h"

#include "RageDisplay_GL3.h"
#include "RageDisplay_GL3_Helpers.h"
#include "RageDisplay_GL3_Shaders.h"
#include "RageDisplay_OGL_Helpers.h"
using namespace RageDisplay_Legacy_Helpers;

#include "RageFile.h"
#include "RageSurface.h"
#include "RageSurfaceUtils.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "RageTextureManager.h"
#include "RageMath.h"
#include "RageTypes.h"
#include "EnumHelper.h"
#include "DisplaySpec.h"
#include "LocalizedString.h"

#include "arch/LowLevelWindow/LowLevelWindow.h"
#if defined(HAS_SDL3)
#include "arch/LowLevelWindow/LowLevelWindow_SDL.h"
#include "ImGuiManager.h"
#endif

#include "TracyHelper.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

static LowLevelWindow *g_pWind;

static RageDisplay::RagePixelFormatDesc PIXEL_FORMAT_DESC[NUM_RagePixelFormat] = {
	{
		/* R8G8B8A8 */
		32,
		{ 0xFF000000,
		  0x00FF0000,
		  0x0000FF00,
		  0x000000FF }
	}, {
		/* B8G8R8A8 */
		32,
		{ 0x0000FF00,
		  0x00FF0000,
		  0xFF000000,
		  0x000000FF }
	}, {
		/* R4G4B4A4 */
		16,
		{ 0xF000,
		  0x0F00,
		  0x00F0,
		  0x000F },
	}, {
		/* R5G5B5A1 */
		16,
		{ 0xF800,
		  0x07C0,
		  0x003E,
		  0x0001 },
	}, {
		/* R5G5B5X1 */
		16,
		{ 0xF800,
		  0x07C0,
		  0x003E,
		  0x0000 },
	}, {
		/* R8G8B8 */
		24,
		{ 0xFF0000,
		  0x00FF00,
		  0x0000FF,
		  0x000000 }
	}, {
		/* Paletted */
		8,
		{ 0,0,0,0 } /* N/A */
	}, {
		/* B8G8R8 */
		24,
		{ 0x0000FF,
		  0x00FF00,
		  0xFF0000,
		  0x000000 }
	}, {
		/* A1R5G5B5 */
		16,
		{ 0x7C00,
		  0x03E0,
		  0x001F,
		  0x8000 },
	}, {
		/* X1R5G5B5 */
		16,
		{ 0x7C00,
		  0x03E0,
		  0x001F,
		  0x0000 },
	}
};

/* GL pixel format mapping — same as legacy but without paletted texture support */
struct GLPixFmtInfo_t {
	GLenum internalfmt;
	GLenum format;
	GLenum type;
}
#ifdef EMSCRIPTEN
/* GLES3 has no GL_BGRA, GL_BGR, GL_RGB5, or GL_UNSIGNED_SHORT_1_5_5_5_REV.
 * Map BGR(A) formats to RGB(A) — the game will swizzle on CPU before upload.
 * GL_RGB5 is replaced with GL_RGB5_A1 (closest available). */
const g_GL3PixFmtInfo[NUM_RagePixelFormat] = {
	{ GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE },                           // RGBA8
	{ GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE },                           // BGRA8 → upload as RGBA (CPU swizzle)
	{ GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4 },                  // RGBA4
	{ GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1 },                // RGB5A1
	{ GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1 },                // RGB5 → use RGB5_A1
	{ GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE },                              // RGB8
	{ GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE },                           // PAL — will convert on CPU
	{ GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE },                              // BGR8 → upload as RGB (CPU swizzle)
	{ GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1 },                // A1BGR5 → use RGBA order
	{ GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1 },                // X1RGB5 → use RGBA order
};
#else
const g_GL3PixFmtInfo[NUM_RagePixelFormat] = {
	{ GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE },                           // RGBA8
	{ GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE },                           // BGRA8
	{ GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4 },                  // RGBA4
	{ GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1 },                // RGB5A1
	{ GL_RGB5, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1 },                   // RGB5
	{ GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE },                              // RGB8
	{ GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE },                           // PAL — will convert on CPU
	{ GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE },                              // BGR8
	{ GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV },           // A1BGR5
	{ GL_RGB5, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV },              // X1RGB5
};
#endif

static void FixLittleEndian()
{
	if constexpr (!Endian::little) {
		return;
	}

	static bool bInitialized = false;
	if (bInitialized)
		return;
	bInitialized = true;

	for( int i = 0; i < NUM_RagePixelFormat; ++i )
	{
		RageDisplay::RagePixelFormatDesc &pf = PIXEL_FORMAT_DESC[i];
		if (g_GL3PixFmtInfo[i].type != GL_UNSIGNED_BYTE || pf.bpp == 8)
			continue;

		for( int mask = 0; mask < 4; ++mask)
		{
			int m = pf.masks[mask];
			switch( pf.bpp )
			{
			case 24: m = Swap24(m); break;
			case 32: m = Swap32(m); break;
			default:
				 FAIL_M(ssprintf("Unsupported BPP value: %i", pf.bpp));
			}
			pf.masks[mask] = m;
		}
	}
}

// ============================================================
// Helpers implementation
// ============================================================

namespace RageDisplay_GL3_Helpers
{

void Init()
{
	// Initialize the GL enum → string map (shared with legacy)
	RageDisplay_Legacy_Helpers::Init();
}

GLuint CompileShader( GLenum type, const char *source )
{
	GLuint shader = glCreateShader( type );

	/* Prepend the correct GLSL version header.  The shader body strings
	 * omit #version so they work on both desktop GL and WebGL2/GLES3. */
#ifdef EMSCRIPTEN
	const char *version = "#version 300 es\nprecision mediump float;\n";
#else
	const char *version = "#version 330 core\n";
#endif
	const char *sources[2] = { version, source };
	glShaderSource( shader, 2, sources, nullptr );
	glCompileShader( shader );

	GLint status;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &status );
	if (!status)
	{
		GLchar info[1024];
		glGetShaderInfoLog( shader, sizeof(info), nullptr, info );
		LOG->Warn( "GL3 shader compile error: %s", info );
		glDeleteShader( shader );
		return 0;
	}
	return shader;
}

GLuint LinkProgram( GLuint vert, GLuint frag )
{
	GLuint prog = glCreateProgram();
	glAttachShader( prog, vert );
	glAttachShader( prog, frag );
	glLinkProgram( prog );

	GLint status;
	glGetProgramiv( prog, GL_LINK_STATUS, &status );
	if (!status)
	{
		GLchar info[1024];
		glGetProgramInfoLog( prog, sizeof(info), nullptr, info );
		LOG->Warn( "GL3 program link error: %s", info );
		glDeleteProgram( prog );
		return 0;
	}
	return prog;
}

GLuint BuildProgram( const char *vertSrc, const char *fragSrc )
{
	GLuint vert = CompileShader( GL_VERTEX_SHADER, vertSrc );
	if (!vert) return 0;
	GLuint frag = CompileShader( GL_FRAGMENT_SHADER, fragSrc );
	if (!frag) { glDeleteShader(vert); return 0; }
	GLuint prog = LinkProgram( vert, frag );
	glDeleteShader( vert );
	glDeleteShader( frag );
	return prog;
}

} // namespace RageDisplay_GL3_Helpers

// ============================================================
// FBO Render Target (core profile)
// ============================================================

RenderTarget_FBO_GL3::RenderTarget_FBO_GL3()
{
	m_iFrameBuffer = 0;
	m_iTexHandle = 0;
	m_iDepthBuffer = 0;
}

RenderTarget_FBO_GL3::~RenderTarget_FBO_GL3()
{
	if (m_iDepthBuffer)
		glDeleteRenderbuffers( 1, &m_iDepthBuffer );
	if (m_iFrameBuffer)
		glDeleteFramebuffers( 1, &m_iFrameBuffer );
	if (m_iTexHandle)
	{
		GLuint tex = static_cast<GLuint>(m_iTexHandle);
		glDeleteTextures( 1, &tex );
	}
}

void RenderTarget_FBO_GL3::Create( const RenderTargetParam &param, int &iTextureWidthOut, int &iTextureHeightOut )
{
	m_Param = param;

	int iTextureWidth = power_of_two( param.iWidth );
	int iTextureHeight = power_of_two( param.iHeight );
	iTextureWidthOut = iTextureWidth;
	iTextureHeightOut = iTextureHeight;

	GLuint tex;
	glGenTextures( 1, &tex );
	m_iTexHandle = tex;

	glBindTexture( GL_TEXTURE_2D, tex );
	GLenum type = param.bWithAlpha ? GL_RGBA : GL_RGB;
	GLenum internalformat;
	if (param.bFloat)
		internalformat = param.bWithAlpha ? GL_RGBA16F : GL_RGB16F;
	else
		internalformat = param.bWithAlpha ? GL_RGBA8 : GL_RGB8;

	glTexImage2D( GL_TEXTURE_2D, 0, internalformat,
		iTextureWidth, iTextureHeight, 0, type, GL_UNSIGNED_BYTE, nullptr );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	glGenFramebuffers( 1, &m_iFrameBuffer );
	glBindFramebuffer( GL_FRAMEBUFFER, m_iFrameBuffer );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );

	if (param.bWithDepthBuffer)
	{
		glGenRenderbuffers( 1, &m_iDepthBuffer );
		glBindRenderbuffer( GL_RENDERBUFFER, m_iDepthBuffer );
		glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, iTextureWidth, iTextureHeight );
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_iDepthBuffer );
	}

	GLenum fbStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	ASSERT_M( fbStatus == GL_FRAMEBUFFER_COMPLETE,
		ssprintf("GL3 FBO incomplete: 0x%x", fbStatus) );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void RenderTarget_FBO_GL3::StartRenderingTo()
{
	glBindFramebuffer( GL_FRAMEBUFFER, m_iFrameBuffer );
}

void RenderTarget_FBO_GL3::FinishRenderingTo()
{
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

// ============================================================
// Compiled Geometry (GL3 — VAO/VBO)
// ============================================================

class RageCompiledGeometryGL3 : public RageCompiledGeometry
{
public:
	RageCompiledGeometryGL3()
	{
		m_VAO = 0;
		m_VBOPos = 0;
		m_VBONormal = 0;
		m_VBOTex = 0;
		m_VBOTextureMatrixScale = 0;
		m_IBO = 0;
	}
	~RageCompiledGeometryGL3()
	{
		if (m_VAO) glDeleteVertexArrays( 1, &m_VAO );
		if (m_VBOPos) glDeleteBuffers( 1, &m_VBOPos );
		if (m_VBONormal) glDeleteBuffers( 1, &m_VBONormal );
		if (m_VBOTex) glDeleteBuffers( 1, &m_VBOTex );
		if (m_VBOTextureMatrixScale) glDeleteBuffers( 1, &m_VBOTextureMatrixScale );
		if (m_IBO) glDeleteBuffers( 1, &m_IBO );
	}

	void Allocate( const std::vector<msMesh> &vMeshes )
	{
		// Always allocate at least 1 entry
		size_t totalVerts = std::max( GetTotalVertices(), (size_t)1 );
		size_t totalTris = std::max( GetTotalTriangles(), (size_t)1 );

		glGenVertexArrays( 1, &m_VAO );
		glBindVertexArray( m_VAO );

		glGenBuffers( 1, &m_VBOPos );
		glBindBuffer( GL_ARRAY_BUFFER, m_VBOPos );
		glBufferData( GL_ARRAY_BUFFER, totalVerts * sizeof(RageVector3), nullptr, GL_DYNAMIC_DRAW );
		glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
		glEnableVertexAttribArray( 0 );

		glGenBuffers( 1, &m_VBONormal );
		glBindBuffer( GL_ARRAY_BUFFER, m_VBONormal );
		glBufferData( GL_ARRAY_BUFFER, totalVerts * sizeof(RageVector3), nullptr, GL_DYNAMIC_DRAW );
		glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
		glEnableVertexAttribArray( 1 );

		// No per-vertex color for model geometry — set a default white
		glVertexAttrib4f( 2, 1.0f, 1.0f, 1.0f, 1.0f );

		glGenBuffers( 1, &m_VBOTex );
		glBindBuffer( GL_ARRAY_BUFFER, m_VBOTex );
		glBufferData( GL_ARRAY_BUFFER, totalVerts * sizeof(RageVector2), nullptr, GL_DYNAMIC_DRAW );
		glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, 0, nullptr );
		glEnableVertexAttribArray( 3 );

		// TextureMatrixScale (for scrolling textures on models)
		glGenBuffers( 1, &m_VBOTextureMatrixScale );
		glBindBuffer( GL_ARRAY_BUFFER, m_VBOTextureMatrixScale );
		glBufferData( GL_ARRAY_BUFFER, totalVerts * sizeof(RageVector2), nullptr, GL_DYNAMIC_DRAW );
		// Not bound to an attrib yet — will be used if texture matrix scaling is needed

		glGenBuffers( 1, &m_IBO );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_IBO );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER, totalTris * sizeof(msTriangle), nullptr, GL_DYNAMIC_DRAW );

		glBindVertexArray( 0 );
	}

	void Change( const std::vector<msMesh> &vMeshes )
	{
		for (size_t i = 0; i < vMeshes.size(); ++i)
		{
			const MeshInfo &meshInfo = m_vMeshInfo[i];
			const msMesh &mesh = vMeshes[i];

			// Upload positions
			std::vector<RageVector3> positions( mesh.Vertices.size() );
			std::vector<RageVector3> normals( mesh.Vertices.size() );
			std::vector<RageVector2> texcoords( mesh.Vertices.size() );
			std::vector<RageVector2> texMatScale( mesh.Vertices.size() );

			for (size_t j = 0; j < mesh.Vertices.size(); ++j)
			{
				positions[j] = mesh.Vertices[j].p;
				normals[j] = mesh.Vertices[j].n;
				texcoords[j] = mesh.Vertices[j].t;
				texMatScale[j] = mesh.Vertices[j].TextureMatrixScale;
			}

			glBindBuffer( GL_ARRAY_BUFFER, m_VBOPos );
			glBufferSubData( GL_ARRAY_BUFFER, meshInfo.iVertexStart * sizeof(RageVector3),
				meshInfo.iVertexCount * sizeof(RageVector3), positions.data() );

			if (m_bNeedsNormals)
			{
				glBindBuffer( GL_ARRAY_BUFFER, m_VBONormal );
				glBufferSubData( GL_ARRAY_BUFFER, meshInfo.iVertexStart * sizeof(RageVector3),
					meshInfo.iVertexCount * sizeof(RageVector3), normals.data() );
			}

			glBindBuffer( GL_ARRAY_BUFFER, m_VBOTex );
			glBufferSubData( GL_ARRAY_BUFFER, meshInfo.iVertexStart * sizeof(RageVector2),
				meshInfo.iVertexCount * sizeof(RageVector2), texcoords.data() );

			glBindBuffer( GL_ARRAY_BUFFER, m_VBOTextureMatrixScale );
			glBufferSubData( GL_ARRAY_BUFFER, meshInfo.iVertexStart * sizeof(RageVector2),
				meshInfo.iVertexCount * sizeof(RageVector2), texMatScale.data() );

			// Remap triangle indices: mesh-local vertex indices must be offset
			// by iVertexStart to reference the correct position in the global VBO.
			std::vector<msTriangle> remappedTris( mesh.Triangles.size() );
			for (size_t j = 0; j < mesh.Triangles.size(); ++j)
				for (int k = 0; k < 3; ++k)
					remappedTris[j].nVertexIndices[k] = static_cast<uint16_t>(
						meshInfo.iVertexStart + mesh.Triangles[j].nVertexIndices[k] );

			glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_IBO );
			glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, meshInfo.iTriangleStart * sizeof(msTriangle),
				meshInfo.iTriangleCount * sizeof(msTriangle), remappedTris.data() );
		}
	}

	void Draw( int iMeshIndex ) const
	{
		const MeshInfo &meshInfo = m_vMeshInfo[iMeshIndex];
		glBindVertexArray( m_VAO );
		glDrawRangeElements(
			GL_TRIANGLES,
			meshInfo.iVertexStart,
			meshInfo.iVertexStart + meshInfo.iVertexCount - 1,
			meshInfo.iTriangleCount * 3,
			GL_UNSIGNED_SHORT,
			reinterpret_cast<void*>(meshInfo.iTriangleStart * sizeof(msTriangle)) );
		glBindVertexArray( 0 );
	}

private:
	GLuint m_VAO;
	GLuint m_VBOPos, m_VBONormal, m_VBOTex, m_VBOTextureMatrixScale;
	GLuint m_IBO;
};

// ============================================================
// RageDisplay_GL3 implementation
// ============================================================

RageDisplay_GL3::RageDisplay_GL3()
{
	LOG->Trace( "RageDisplay_GL3::RageDisplay_GL3()" );
	LOG->MapLog("renderer", "Current renderer: OpenGL 3.3");

	FixLittleEndian();
	RageDisplay_GL3_Helpers::Init();

	g_pWind = nullptr;
	m_SpriteProgram = 0;
	m_LitSpriteProgram = 0;
	m_CurrentProgram = 0;
	m_VAO = 0;
	m_VBO = 0;
	m_QuadIBO = 0;
	m_iVBOSize = 0;
	m_iQuadIBOSize = 0;
	m_SymQuadIBO = 0;
	m_iSymQuadIBOSize = 0;
	m_CurTextureMode = TextureMode_Modulate;
	m_bAlphaTestEnabled = false;
	m_bLightingEnabled = false;
	m_bSphereMapping = false;
	m_iCelShadedStage = 0;
	m_bInvertY = false;
	m_bMaterialLightingFallback = false;
	m_fMatShininess = 0;
	m_pCurrentRenderTarget = nullptr;
	m_bMatrixDirty = true;
	m_CachedMatrixProgram = 0;
	m_bUniformsDirty = true;
	m_CachedTextureMode = TextureMode_Modulate;
	m_bCachedAlphaTestEnabled = false;
	m_bCachedTextureEnabled = false;
	m_bCachedLightingEnabled = false;
	m_CachedUniformProgram = 0;
	m_bLightUniformsDirty = true;

	for (int i = 0; i < NUM_TextureUnit; ++i)
	{
		m_bTextureEnabled[i] = false;
		m_iCurrentTextures[i] = 0;
	}
	for (int i = 0; i < 8; ++i)
		m_Lights[i].enabled = false;

	memset(m_EffectPrograms, 0, sizeof(m_EffectPrograms));

	m_CachedBlendMode = BLEND_NORMAL;
	m_bCachedZWrite = true;
	m_CachedZTestMode = ZTEST_OFF;
	m_fCachedZBias = 0.0f;
	m_CachedCullMode = CULL_NONE;

}

RageDisplay_GL3::~RageDisplay_GL3()
{
	DestroyShaderPrograms();

	if (m_VAO) glDeleteVertexArrays( 1, &m_VAO );
	if (m_VBO) glDeleteBuffers( 1, &m_VBO );
	if (m_QuadIBO) glDeleteBuffers( 1, &m_QuadIBO );
	if (m_SymQuadIBO) glDeleteBuffers( 1, &m_SymQuadIBO );

	for (auto &pair : m_mapRenderTargets)
		delete pair.second;
	m_mapRenderTargets.clear();

	delete g_pWind;
	g_pWind = nullptr;
}

void RageDisplay_GL3::InitShaderPrograms()
{
	using namespace RageDisplay_GL3_Helpers;
	using namespace GL3Shaders;

	m_SpriteProgram = BuildProgram( g_SpriteVertSrc, g_SpriteFragSrc );
	if (!m_SpriteProgram)
	{
		LOG->Warn("GL3: Failed to build sprite shader program!");
		return;
	}
	m_uProjection = glGetUniformLocation( m_SpriteProgram, "u_Projection" );
	m_uModelView = glGetUniformLocation( m_SpriteProgram, "u_ModelView" );
	m_uTextureMatrix = glGetUniformLocation( m_SpriteProgram, "u_TextureMatrix" );
	m_uTexture0 = glGetUniformLocation( m_SpriteProgram, "u_Texture0" );
	m_uTextureMode = glGetUniformLocation( m_SpriteProgram, "u_TextureMode" );
	m_uAlphaTestEnabled = glGetUniformLocation( m_SpriteProgram, "u_AlphaTestEnabled" );
	m_uTextureEnabled = glGetUniformLocation( m_SpriteProgram, "u_TextureEnabled" );

	m_LitSpriteProgram = BuildProgram( g_LitSpriteVertSrc, g_LitSpriteFragSrc );
	if (!m_LitSpriteProgram)
	{
		LOG->Warn("GL3: Failed to build lit sprite shader program!");
		return;
	}
	m_uLitProjection = glGetUniformLocation( m_LitSpriteProgram, "u_Projection" );
	m_uLitModelView = glGetUniformLocation( m_LitSpriteProgram, "u_ModelView" );
	m_uLitTextureMatrix = glGetUniformLocation( m_LitSpriteProgram, "u_TextureMatrix" );
	m_uLitTexture0 = glGetUniformLocation( m_LitSpriteProgram, "u_Texture0" );
	m_uLitTextureMode = glGetUniformLocation( m_LitSpriteProgram, "u_TextureMode" );
	m_uLitAlphaTestEnabled = glGetUniformLocation( m_LitSpriteProgram, "u_AlphaTestEnabled" );
	m_uLitTextureEnabled = glGetUniformLocation( m_LitSpriteProgram, "u_TextureEnabled" );
	m_uLightEnabled = glGetUniformLocation( m_LitSpriteProgram, "u_LightEnabled" );
	m_uLightDir = glGetUniformLocation( m_LitSpriteProgram, "u_LightDir" );
	m_uLightAmbient = glGetUniformLocation( m_LitSpriteProgram, "u_LightAmbient" );
	m_uLightDiffuse = glGetUniformLocation( m_LitSpriteProgram, "u_LightDiffuse" );
	m_uLightSpecular = glGetUniformLocation( m_LitSpriteProgram, "u_LightSpecular" );
	m_uMatEmissive = glGetUniformLocation( m_LitSpriteProgram, "u_MatEmissive" );
	m_uMatAmbient = glGetUniformLocation( m_LitSpriteProgram, "u_MatAmbient" );
	m_uMatDiffuse = glGetUniformLocation( m_LitSpriteProgram, "u_MatDiffuse" );
	m_uMatSpecular = glGetUniformLocation( m_LitSpriteProgram, "u_MatSpecular" );
	m_uMatShininess = glGetUniformLocation( m_LitSpriteProgram, "u_MatShininess" );

	// Build effect programs (all use sprite vertex shader)
	struct { EffectMode mode; const char *fragSrc; } effects[] = {
		{ EffectMode_Unpremultiply, g_EffectFragUnpremultiply },
		{ EffectMode_ColorBurn, g_EffectFragColorBurn },
		{ EffectMode_ColorDodge, g_EffectFragColorDodge },
		{ EffectMode_VividLight, g_EffectFragVividLight },
		{ EffectMode_HardMix, g_EffectFragHardMix },
		{ EffectMode_Overlay, g_EffectFragOverlay },
		{ EffectMode_Screen, g_EffectFragScreen },
		{ EffectMode_YUYV422, g_EffectFragYUYV422 },
		{ EffectMode_DistanceField, g_EffectFragDistanceField },
	};
	for (auto &e : effects)
	{
		m_EffectPrograms[e.mode] = BuildProgram( g_SpriteVertSrc, e.fragSrc );
		if (!m_EffectPrograms[e.mode])
			LOG->Warn("GL3: Failed to build effect shader for mode %d", e.mode);
	}

	// Set default program
	m_CurrentProgram = m_SpriteProgram;
	glUseProgram( m_SpriteProgram );
	glUniform1i( m_uTexture0, 0 );
}

void RageDisplay_GL3::DestroyShaderPrograms()
{
	if (m_SpriteProgram) glDeleteProgram( m_SpriteProgram );
	if (m_LitSpriteProgram) glDeleteProgram( m_LitSpriteProgram );
	for (int i = 0; i < NUM_EffectMode; ++i)
		if (m_EffectPrograms[i]) glDeleteProgram( m_EffectPrograms[i] );

	m_SpriteProgram = 0;
	m_LitSpriteProgram = 0;
	m_CurrentProgram = 0;
	memset(m_EffectPrograms, 0, sizeof(m_EffectPrograms));
}

static LocalizedString OBTAIN_AN_UPDATED_VIDEO_DRIVER_GL3 ( "RageDisplay_GL3", "Obtain an updated driver from your video card manufacturer." );

std::string RageDisplay_GL3::Init( const VideoModeParams &p, bool bAllowUnacceleratedRenderer )
{
	g_pWind = LowLevelWindow::Create();

	bool bIgnore = false;
	std::string sError = SetVideoMode( p, bIgnore );
	if (sError != "")
		return sError;

	// Log driver details
	g_pWind->LogDebugInformation();
	LOG->Info("GL3 Vendor: %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
	LOG->Info("GL3 Renderer: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
	LOG->Info("GL3 Version: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
	LOG->Info("GL3 GLSL Version: %s", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
	LOG->Info("GL3 Max texture size: %i", GetMaxTextureSize());

	// Check for minimum GL version support
	GLint major = 0, minor = 0;
#ifdef EMSCRIPTEN
	// GL_MAJOR_VERSION/GL_MINOR_VERSION may not work on WebGL2/GLES3.
	// Parse from GL_VERSION string instead ("OpenGL ES 3.0 ..." or "WebGL 2.0 ...").
	{
		const char *ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		if (ver) {
			// Try "OpenGL ES X.Y" first, then "WebGL X.Y"
			if (sscanf(ver, "OpenGL ES %d.%d", &major, &minor) != 2)
				sscanf(ver, "WebGL %d.%d", &major, &minor);
			// WebGL 2.0 == GLES 3.0
			if (major == 2 && strstr(ver, "WebGL") != nullptr)
				major = 3;
		}
	}
	if (major < 3)
	{
		return ssprintf( "OpenGL ES 3.0 required but only %d.%d available. %s",
			major, minor, OBTAIN_AN_UPDATED_VIDEO_DRIVER_GL3.GetValue().c_str() );
	}
#else
	glGetIntegerv( GL_MAJOR_VERSION, &major );
	glGetIntegerv( GL_MINOR_VERSION, &minor );
	if (major < 3 || (major == 3 && minor < 3))
	{
		return ssprintf( "OpenGL 3.3 required but only %d.%d available. %s",
			major, minor, OBTAIN_AN_UPDATED_VIDEO_DRIVER_GL3.GetValue().c_str() );
	}
#endif
	LOG->Info("GL3 context version: %d.%d", major, minor);

	// Build shaders
	InitShaderPrograms();
	if (!m_SpriteProgram)
		return "GL3: Failed to compile required shaders.";

	// Create VAO and VBO for dynamic sprite vertex data
	glGenVertexArrays( 1, &m_VAO );
	glBindVertexArray( m_VAO );

	glGenBuffers( 1, &m_VBO );
	glBindBuffer( GL_ARRAY_BUFFER, m_VBO );
	m_iVBOSize = 4096; // Pre-allocate for ~1024 quads to avoid runtime reallocation
	glBufferData( GL_ARRAY_BUFFER, m_iVBOSize * sizeof(RageSpriteVertex), nullptr, GL_STREAM_DRAW );

	// Set up vertex attribute layout matching RageSpriteVertex
	// layout: vec3 p (12B) + vec3 n (12B) + RageVColor c (4B) + vec2 t (8B) = 36B stride
	const GLsizei stride = sizeof(RageSpriteVertex);
	// a_Position (location 0)
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, stride,
		reinterpret_cast<void*>(offsetof(RageSpriteVertex, p)) );
	glEnableVertexAttribArray( 0 );
	// a_Normal (location 1)
	glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, stride,
		reinterpret_cast<void*>(offsetof(RageSpriteVertex, n)) );
	glEnableVertexAttribArray( 1 );
	// a_Color (location 2) — BGRA unsigned bytes, normalized
	glVertexAttribPointer( 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
		reinterpret_cast<void*>(offsetof(RageSpriteVertex, c)) );
	glEnableVertexAttribArray( 2 );
	// a_TexCoord (location 3)
	glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, stride,
		reinterpret_cast<void*>(offsetof(RageSpriteVertex, t)) );
	glEnableVertexAttribArray( 3 );

	// Create quad index buffer (will grow as needed)
	glGenBuffers( 1, &m_QuadIBO );
	m_iQuadIBOSize = 0;
	EnsureQuadIBO( 1024 ); // pre-allocate for 1024 quads

	// Create symmetric quad strip IBO (reused across calls)
	glGenBuffers( 1, &m_SymQuadIBO );
	m_iSymQuadIBOSize = 0;

	glBindVertexArray( 0 );

	// Set some default GL state
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_ALWAYS );

	return std::string();
}

void RageDisplay_GL3::ResolutionChanged()
{
	if (BeginFrame())
		EndFrame();
	RageDisplay::ResolutionChanged();
}

std::string RageDisplay_GL3::TryVideoMode( const VideoModeParams &p, bool &bNewDeviceOut )
{
	std::string err;
	err = g_pWind->TryVideoMode( p, bNewDeviceOut );
	if (err != "")
		return err;

	if (bNewDeviceOut)
	{
#if defined(HAS_SDL3)
		LowLevelWindow_SDL *pSDLWind = dynamic_cast<LowLevelWindow_SDL *>(g_pWind);
		if (pSDLWind)
			ImGuiManager::Initialize( pSDLWind->GetWindow(), pSDLWind->GetGLContext() );
#endif
	}

	return std::string();
}

void RageDisplay_GL3::GetDisplaySpecs(DisplaySpecs &out) const
{
	out.clear();
	g_pWind->GetDisplaySpecs(out);
}

ActualVideoModeParams RageDisplay_GL3::GetActualVideoModeParams() const
{
	return g_pWind->GetActualVideoModeParams();
}

const RageDisplay::RagePixelFormatDesc *RageDisplay_GL3::GetPixelFormatDesc(RagePixelFormat pf) const
{
	ASSERT( pf < NUM_RagePixelFormat );
	return &PIXEL_FORMAT_DESC[pf];
}

// ============================================================
// Frame begin/end
// ============================================================

bool RageDisplay_GL3::BeginFrame()
{
	ZoneScopedN("GL3::BeginFrame");
	int fWidth = g_pWind->GetActualVideoModeParams().windowWidth;
	int fHeight = g_pWind->GetActualVideoModeParams().windowHeight;

	glViewport( 0, 0, fWidth, fHeight );
	glClearColor( 0, 0, 0, 0 );

	// Invalidate all caches — ImGui or other subsystems may have changed GL state
	InvalidateMatrixCache();
	// Force GL state cache to unknown so next Set* call applies the state
	m_CachedBlendMode = (BlendMode)-1;
	m_bCachedZWrite = !true; // force next SetZWrite to apply
	m_CachedZTestMode = (ZTestMode)-1;
	m_fCachedZBias = -999.0f;
	m_CachedCullMode = (CullMode)-1;

	SetZWrite( true );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	// Restore GL state that may have been changed by ImGui or other subsystems
	glUseProgram( m_CurrentProgram );
	glBindVertexArray( m_VAO );
	glDisable( GL_SCISSOR_TEST );

	return RageDisplay::BeginFrame();
}

void RageDisplay_GL3::EndFrame()
{
	ZoneScopedN("GL3::EndFrame");

	FrameLimitBeforeVsync( g_pWind->GetActualVideoModeParams().rate );
	g_pWind->SwapBuffers();
	FrameLimitAfterVsync();

	// When vsync is on, SwapBuffers already blocks until the next display
	// refresh, so glFinish() is redundant and just adds a full pipeline stall.
	// When vsync is off, glFinish() prevents the driver from queuing multiple
	// frames, keeping the engine state close to what's on screen.
	if (!g_pWind->GetActualVideoModeParams().vsync)
		glFinish();

	g_pWind->Update();

	FrameMark;
	RageDisplay::EndFrame();
}

// ============================================================
// Matrix upload
// ============================================================

void RageDisplay_GL3::InvalidateMatrixCache()
{
	m_bMatrixDirty = true;
	m_bUniformsDirty = true;
	m_bLightUniformsDirty = true;
	m_CachedMatrixProgram = 0;
	m_CachedUniformProgram = 0;
}

void RageDisplay_GL3::SendCurrentMatrices()
{
	RageMatrix projection;
	RageMatrixMultiply( &projection, GetCentering(), GetProjectionTop() );

	if (m_bInvertY)
	{
		RageMatrix flip;
		RageMatrixScale( &flip, +1, -1, +1 );
		RageMatrixMultiply( &projection, &flip, &projection );
	}

	RageMatrix modelView;
	RageMatrixMultiply( &modelView, GetViewTop(), GetWorldTop() );

	const RageMatrix *texMatrix = GetTextureTop();

	// Check if anything actually changed since last upload
	bool bProgramChanged = (m_CurrentProgram != m_CachedMatrixProgram);
	bool bProjectionChanged = bProgramChanged || memcmp( &projection, &m_CachedProjection, sizeof(RageMatrix) ) != 0;
	bool bModelViewChanged = bProgramChanged || memcmp( &modelView, &m_CachedModelView, sizeof(RageMatrix) ) != 0;
	bool bTexMatChanged = bProgramChanged || memcmp( texMatrix, &m_CachedTextureMatrix, sizeof(RageMatrix) ) != 0;

	if (!bProjectionChanged && !bModelViewChanged && !bTexMatChanged)
		return;

	if (m_CurrentProgram == m_SpriteProgram)
	{
		if (bProjectionChanged)
			glUniformMatrix4fv( m_uProjection, 1, GL_FALSE, (const float*)&projection );
		if (bModelViewChanged)
			glUniformMatrix4fv( m_uModelView, 1, GL_FALSE, (const float*)&modelView );
		if (bTexMatChanged)
			glUniformMatrix4fv( m_uTextureMatrix, 1, GL_FALSE, (const float*)texMatrix );
	}
	else if (m_CurrentProgram == m_LitSpriteProgram)
	{
		if (bProjectionChanged)
			glUniformMatrix4fv( m_uLitProjection, 1, GL_FALSE, (const float*)&projection );
		if (bModelViewChanged)
			glUniformMatrix4fv( m_uLitModelView, 1, GL_FALSE, (const float*)&modelView );
		if (bTexMatChanged)
			glUniformMatrix4fv( m_uLitTextureMatrix, 1, GL_FALSE, (const float*)texMatrix );
	}
	else
	{
		// Effect program — uses same uniform names as sprite
		GLint loc;
		if (bProjectionChanged)
		{
			loc = glGetUniformLocation( m_CurrentProgram, "u_Projection" );
			if (loc >= 0) glUniformMatrix4fv( loc, 1, GL_FALSE, (const float*)&projection );
		}
		if (bModelViewChanged)
		{
			loc = glGetUniformLocation( m_CurrentProgram, "u_ModelView" );
			if (loc >= 0) glUniformMatrix4fv( loc, 1, GL_FALSE, (const float*)&modelView );
		}
		if (bTexMatChanged)
		{
			loc = glGetUniformLocation( m_CurrentProgram, "u_TextureMatrix" );
			if (loc >= 0) glUniformMatrix4fv( loc, 1, GL_FALSE, (const float*)texMatrix );
		}
	}

	// Update cache
	m_CachedProjection = projection;
	m_CachedModelView = modelView;
	m_CachedTextureMatrix = *texMatrix;
	m_CachedMatrixProgram = m_CurrentProgram;
}

void RageDisplay_GL3::SetSpriteUniforms()
{
	bool bProgramChanged = (m_CurrentProgram != m_CachedUniformProgram);
	bool bTexModeChanged = bProgramChanged || m_CurTextureMode != m_CachedTextureMode;
	bool bAlphaTestChanged = bProgramChanged || m_bAlphaTestEnabled != m_bCachedAlphaTestEnabled;
	bool bTexEnabledChanged = bProgramChanged || m_bTextureEnabled[0] != m_bCachedTextureEnabled;

	if (m_CurrentProgram == m_SpriteProgram)
	{
		if (bTexModeChanged)
			glUniform1i( m_uTextureMode, (int)m_CurTextureMode );
		if (bAlphaTestChanged)
			glUniform1i( m_uAlphaTestEnabled, m_bAlphaTestEnabled ? 1 : 0 );
		if (bTexEnabledChanged)
			glUniform1i( m_uTextureEnabled, m_bTextureEnabled[0] ? 1 : 0 );
	}
	else if (m_CurrentProgram == m_LitSpriteProgram)
	{
		bool bLightChanged = bProgramChanged || m_bLightingEnabled != m_bCachedLightingEnabled;

		if (bTexModeChanged)
			glUniform1i( m_uLitTextureMode, (int)m_CurTextureMode );
		if (bAlphaTestChanged)
			glUniform1i( m_uLitAlphaTestEnabled, m_bAlphaTestEnabled ? 1 : 0 );
		if (bTexEnabledChanged)
			glUniform1i( m_uLitTextureEnabled, m_bTextureEnabled[0] ? 1 : 0 );
		if (bLightChanged)
			glUniform1i( m_uLightEnabled, m_bLightingEnabled ? 1 : 0 );

		if (m_bLightingEnabled && (m_bLightUniformsDirty || bProgramChanged))
		{
			// Use light 0 (primary directional light)
			if (m_Lights[0].enabled)
			{
				glUniform4f( m_uLightDir, m_Lights[0].dir.x, m_Lights[0].dir.y, m_Lights[0].dir.z, 0 );
				glUniform4fv( m_uLightAmbient, 1, (const float*)&m_Lights[0].ambient );
				glUniform4fv( m_uLightDiffuse, 1, (const float*)&m_Lights[0].diffuse );
				glUniform4fv( m_uLightSpecular, 1, (const float*)&m_Lights[0].specular );
			}
			else
			{
				glUniform4f( m_uLightDir, 0, 0, -1, 0 );
				glUniform4f( m_uLightAmbient, 0.2f, 0.2f, 0.2f, 1 );
				glUniform4f( m_uLightDiffuse, 1, 1, 1, 1 );
				glUniform4f( m_uLightSpecular, 0, 0, 0, 1 );
			}
			glUniform4fv( m_uMatEmissive, 1, (const float*)&m_MatEmissive );
			glUniform4fv( m_uMatAmbient, 1, (const float*)&m_MatAmbient );
			glUniform4fv( m_uMatDiffuse, 1, (const float*)&m_MatDiffuse );
			glUniform4fv( m_uMatSpecular, 1, (const float*)&m_MatSpecular );
			glUniform1f( m_uMatShininess, m_fMatShininess );
			m_bLightUniformsDirty = false;
		}

		m_bCachedLightingEnabled = m_bLightingEnabled;
	}
	else
	{
		if (bProgramChanged)
		{
			// Effect program — only upload on program change
			GLint loc;
			loc = glGetUniformLocation( m_CurrentProgram, "u_TextureEnabled" );
			if (loc >= 0) glUniform1i( loc, m_bTextureEnabled[0] ? 1 : 0 );
			loc = glGetUniformLocation( m_CurrentProgram, "u_Texture0" );
			if (loc >= 0) glUniform1i( loc, 0 );
		}
	}

	// Update cache
	m_CachedTextureMode = m_CurTextureMode;
	m_bCachedAlphaTestEnabled = m_bAlphaTestEnabled;
	m_bCachedTextureEnabled = m_bTextureEnabled[0];
	m_CachedUniformProgram = m_CurrentProgram;
}

// ============================================================
// Vertex upload
// ============================================================

void RageDisplay_GL3::UploadVertices( const RageSpriteVertex v[], int iNumVerts )
{
	glBindVertexArray( m_VAO );
	glBindBuffer( GL_ARRAY_BUFFER, m_VBO );

	if (iNumVerts > m_iVBOSize)
	{
		m_iVBOSize = iNumVerts * 2;
		glBufferData( GL_ARRAY_BUFFER, m_iVBOSize * sizeof(RageSpriteVertex), nullptr, GL_STREAM_DRAW );
	}

	glBufferSubData( GL_ARRAY_BUFFER, 0, iNumVerts * sizeof(RageSpriteVertex), v );
}

void RageDisplay_GL3::EnsureQuadIBO( int iNumQuads )
{
	if (iNumQuads <= m_iQuadIBOSize)
		return;

	m_iQuadIBOSize = std::max( iNumQuads, m_iQuadIBOSize * 2 );

	std::vector<uint16_t> indices( m_iQuadIBOSize * 6 );
	for (int i = 0; i < m_iQuadIBOSize; ++i)
	{
		// Quad vertices: 0,1,2,3 → two triangles: 0,1,2 and 0,2,3
		uint16_t base = static_cast<uint16_t>(i * 4);
		indices[i*6+0] = base+0;
		indices[i*6+1] = base+1;
		indices[i*6+2] = base+2;
		indices[i*6+3] = base+0;
		indices[i*6+4] = base+2;
		indices[i*6+5] = base+3;
	}

	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_QuadIBO );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint16_t), indices.data(), GL_STATIC_DRAW );
}

// ============================================================
// Draw methods
// ============================================================

void RageDisplay_GL3::DrawQuadsInternal( const RageSpriteVertex v[], int iNumVerts )
{
	ZoneScopedN("GL3::DrawQuads");
	glUseProgram( m_CurrentProgram );
	SendCurrentMatrices();
	SetSpriteUniforms();
	UploadVertices( v, iNumVerts );

	int iNumQuads = iNumVerts / 4;
	EnsureQuadIBO( iNumQuads );

	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_QuadIBO );
	glDrawElements( GL_TRIANGLES, iNumQuads * 6, GL_UNSIGNED_SHORT, nullptr );
}

void RageDisplay_GL3::DrawQuadStripInternal( const RageSpriteVertex v[], int iNumVerts )
{
	ZoneScopedN("GL3::DrawQuadStrip");

	glUseProgram( m_CurrentProgram );
	SendCurrentMatrices();
	SetSpriteUniforms();
	UploadVertices( v, iNumVerts );

	glDrawArrays( GL_TRIANGLE_STRIP, 0, iNumVerts );
}

void RageDisplay_GL3::DrawFanInternal( const RageSpriteVertex v[], int iNumVerts )
{
	ZoneScopedN("GL3::DrawFan");

	glUseProgram( m_CurrentProgram );
	SendCurrentMatrices();
	SetSpriteUniforms();
	UploadVertices( v, iNumVerts );

	glDrawArrays( GL_TRIANGLE_FAN, 0, iNumVerts );
}

void RageDisplay_GL3::DrawStripInternal( const RageSpriteVertex v[], int iNumVerts )
{
	ZoneScopedN("GL3::DrawStrip");

	glUseProgram( m_CurrentProgram );
	SendCurrentMatrices();
	SetSpriteUniforms();
	UploadVertices( v, iNumVerts );

	glDrawArrays( GL_TRIANGLE_STRIP, 0, iNumVerts );
}

void RageDisplay_GL3::DrawTrianglesInternal( const RageSpriteVertex v[], int iNumVerts )
{
	ZoneScopedN("GL3::DrawTriangles");

	glUseProgram( m_CurrentProgram );
	SendCurrentMatrices();
	SetSpriteUniforms();
	UploadVertices( v, iNumVerts );

	glDrawArrays( GL_TRIANGLES, 0, iNumVerts );
}

void RageDisplay_GL3::DrawCompiledGeometryInternal( const RageCompiledGeometry *p, int iMeshIndex )
{
	ZoneScopedN("GL3::DrawCompiledGeometry");

	glUseProgram( m_CurrentProgram );
	SendCurrentMatrices();
	SetSpriteUniforms();

	// Models don't have per-vertex colors (attrib 2 is disabled in their VAO).
	// When lighting is off, the legacy renderer applies the material color via
	// glColor4fv(diffuse+emissive+ambient) as a fallback vertex color.
	// Replicate that here by setting the current vertex attrib value.
	if (!m_bLightingEnabled)
	{
		RageColor c = m_MatDiffuse;
		c.r += m_MatEmissive.r + m_MatAmbient.r;
		c.g += m_MatEmissive.g + m_MatAmbient.g;
		c.b += m_MatEmissive.b + m_MatAmbient.b;
		c.r = std::min( c.r, 1.0f );
		c.g = std::min( c.g, 1.0f );
		c.b = std::min( c.b, 1.0f );
		// The shader swizzles a_Color.zyxw (BGRA→RGBA), so pass in BGRA order
		glVertexAttrib4f( 2, c.b, c.g, c.r, c.a );
	}

	p->Draw( iMeshIndex );
}

void RageDisplay_GL3::DrawLineStripInternal( const RageSpriteVertex v[], int iNumVerts, float fLineWidth )
{

	if (!GetActualVideoModeParams().bSmoothLines)
	{
		RageDisplay::DrawLineStripInternal(v, iNumVerts, fLineWidth );
		return;
	}

	glUseProgram( m_CurrentProgram );
	SendCurrentMatrices();
	SetSpriteUniforms();
	UploadVertices( v, iNumVerts );

#ifndef EMSCRIPTEN
	glEnable( GL_LINE_SMOOTH );
#endif
	{
		const RageMatrix* pMat = GetProjectionTop();
		float fW = 2 / pMat->m[0][0];
		float fH = -2 / pMat->m[1][1];
		float fWidthVal = float(g_pWind->GetActualVideoModeParams().width) / fW;
		float fHeightVal = float(g_pWind->GetActualVideoModeParams().height) / fH;
		fLineWidth *= (fWidthVal + fHeightVal) / 2;
	}
	glLineWidth( fLineWidth );

	glDrawArrays( GL_LINE_STRIP, 0, iNumVerts );

#ifndef EMSCRIPTEN
	glDisable( GL_LINE_SMOOTH );
#endif
}

void RageDisplay_GL3::DrawSymmetricQuadStripInternal( const RageSpriteVertex v[], int iNumVerts )
{
	ZoneScopedN("GL3::DrawSymmetricQuadStrip");

	int iNumPieces = (iNumVerts-3)/3;
	int iNumTriangles = iNumPieces*4;
	int iNumIndices = iNumTriangles*3;

	// Grow the persistent IBO if needed
	if (iNumPieces > m_iSymQuadIBOSize)
	{
		int iNewSize = std::max( iNumPieces, m_iSymQuadIBOSize * 2 );
		iNewSize = std::max( iNewSize, 64 ); // minimum allocation

		std::vector<uint16_t> vIndices( iNewSize * 12 );
		for( uint16_t i = 0; i < (uint16_t)iNewSize; i++ )
		{
			vIndices[i*12+0] = i*3+1;
			vIndices[i*12+1] = i*3+3;
			vIndices[i*12+2] = i*3+0;
			vIndices[i*12+3] = i*3+1;
			vIndices[i*12+4] = i*3+4;
			vIndices[i*12+5] = i*3+3;
			vIndices[i*12+6] = i*3+1;
			vIndices[i*12+7] = i*3+5;
			vIndices[i*12+8] = i*3+4;
			vIndices[i*12+9] = i*3+1;
			vIndices[i*12+10] = i*3+2;
			vIndices[i*12+11] = i*3+5;
		}

		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_SymQuadIBO );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER, vIndices.size() * sizeof(uint16_t), vIndices.data(), GL_STATIC_DRAW );
		m_iSymQuadIBOSize = iNewSize;
	}

	glUseProgram( m_CurrentProgram );
	SendCurrentMatrices();
	SetSpriteUniforms();
	UploadVertices( v, iNumVerts );

	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_SymQuadIBO );
	glDrawElements( GL_TRIANGLES, iNumIndices, GL_UNSIGNED_SHORT, nullptr );
}

// ============================================================
// Texture management
// ============================================================

void RageDisplay_GL3::ClearAllTextures()
{
	FOREACH_ENUM( TextureUnit, i )
		SetTexture( i, 0 );
}

int RageDisplay_GL3::GetNumTextureUnits()
{
	GLint units;
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
	return std::min( units, (GLint)NUM_TextureUnit );
}

void RageDisplay_GL3::SetTexture( TextureUnit tu, uintptr_t iTexture )
{
	// Skip if the texture is already bound to this unit
	if (m_iCurrentTextures[tu] == iTexture)
		return;

	glActiveTexture( GL_TEXTURE0 + tu );

	if (iTexture)
	{
		glBindTexture( GL_TEXTURE_2D, static_cast<GLuint>(iTexture) );
		m_bTextureEnabled[tu] = true;
	}
	else
	{
		glBindTexture( GL_TEXTURE_2D, 0 );
		m_bTextureEnabled[tu] = false;
	}
	m_iCurrentTextures[tu] = iTexture;

	glActiveTexture( GL_TEXTURE0 );
}

void RageDisplay_GL3::SetTextureMode( TextureUnit tu, TextureMode tm )
{
	// Only texture unit 0 mode is used in the shader
	if (tu == TextureUnit_1)
		m_CurTextureMode = tm;
}

void RageDisplay_GL3::SetTextureFiltering( TextureUnit tu, bool b )
{
	glActiveTexture( GL_TEXTURE0 + tu );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, b ? GL_LINEAR : GL_NEAREST );

	GLint iMinFilter;
	if (b)
	{
#ifdef EMSCRIPTEN
		/* GLES3 has no glGetTexLevelParameteriv, so we can't check if mipmaps
		 * exist.  Using mipmap filtering on a texture without mipmaps makes it
		 * incomplete (renders black) in GLES3.  Default to GL_LINEAR which
		 * always works.  Textures with mipmaps will just skip trilinear. */
		iMinFilter = GL_LINEAR;
#else
		GLint iWidth1 = -1, iWidth2 = -1;
		glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iWidth1 );
		glGetTexLevelParameteriv( GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &iWidth2 );
		if (iWidth1 > 1 && iWidth2 != 0)
		{
			if (g_pWind->GetActualVideoModeParams().bTrilinearFiltering)
				iMinFilter = GL_LINEAR_MIPMAP_LINEAR;
			else
				iMinFilter = GL_LINEAR_MIPMAP_NEAREST;
		}
		else
		{
			iMinFilter = GL_LINEAR;
		}
#endif
	}
	else
	{
		iMinFilter = GL_NEAREST;
	}
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, iMinFilter );

	glActiveTexture( GL_TEXTURE0 );
}

void RageDisplay_GL3::SetTextureWrapping( TextureUnit tu, bool b )
{
	glActiveTexture( GL_TEXTURE0 + tu );
	GLenum mode = b ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode );
	glActiveTexture( GL_TEXTURE0 );
}

int RageDisplay_GL3::GetMaxTextureSize() const
{
	GLint size;
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &size );
	return size;
}

RagePixelFormat RageDisplay_GL3::GetImgPixelFormat( RageSurface* &img, bool &bFreeImg, int width, int height, bool bPalettedTexture )
{
	RagePixelFormat pixfmt = FindPixelFormat( img->format->BitsPerPixel, img->format->Rmask, img->format->Gmask, img->format->Bmask, img->format->Amask );

	bool bSupported = true;
	// GL3 core doesn't support paletted textures — convert to RGBA
	if (img->fmt.BytesPerPixel == 1)
		bSupported = false;

	if (pixfmt == RagePixelFormat_Invalid)
		bSupported = false;

	if (!bSupported)
	{
		// Convert to RGBA8
		pixfmt = RagePixelFormat_RGBA8;
		const RagePixelFormatDesc *pfd = GetPixelFormatDesc(pixfmt);
		RageSurface *imgconv = CreateSurface( width, height, pfd->bpp,
			pfd->masks[0], pfd->masks[1], pfd->masks[2], pfd->masks[3] );
		RageSurfaceUtils::Blit( img, imgconv, width, height );
		img = imgconv;
		bFreeImg = true;
	}
	else
	{
		bFreeImg = false;
	}
	return pixfmt;
}

uintptr_t RageDisplay_GL3::CreateTexture(
	RagePixelFormat pixfmt,
	RageSurface* pImg,
	bool bGenerateMipMaps )
{
	ASSERT( pixfmt < NUM_RagePixelFormat );

	bool bFreeImg;
	RagePixelFormat SurfacePixFmt = GetImgPixelFormat( pImg, bFreeImg, pImg->w, pImg->h, false );
	ASSERT( SurfacePixFmt != RagePixelFormat_Invalid );

	// For paletted source, always upload as RGBA8
	GLenum glTexFormat = (pixfmt == RagePixelFormat_PAL) ?
		GL_RGBA8 : g_GL3PixFmtInfo[pixfmt].internalfmt;
	GLenum glImageFormat = g_GL3PixFmtInfo[SurfacePixFmt].format;
	GLenum glImageType = g_GL3PixFmtInfo[SurfacePixFmt].type;

	glActiveTexture( GL_TEXTURE0 );

	uintptr_t iTexHandle;
	glGenTextures( 1, reinterpret_cast<GLuint*>(&iTexHandle) );
	ASSERT( iTexHandle != 0 );

	glBindTexture( GL_TEXTURE_2D, static_cast<GLuint>(iTexHandle) );

#ifndef EMSCRIPTEN
	if (g_pWind->GetActualVideoModeParams().bAnisotropicFiltering)
	{
		GLfloat fLargestSupportedAnisotropy;
		glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargestSupportedAnisotropy );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargestSupportedAnisotropy );
	}
#endif

	SetTextureFiltering( TextureUnit_1, true );
	SetTextureWrapping( TextureUnit_1, false );

	glPixelStorei( GL_UNPACK_ROW_LENGTH, pImg->pitch / pImg->format->BytesPerPixel );

	glTexImage2D( GL_TEXTURE_2D, 0, glTexFormat,
		power_of_two(pImg->w), power_of_two(pImg->h), 0,
		glImageFormat, glImageType, nullptr );
	if (pImg->pixels)
		glTexSubImage2D( GL_TEXTURE_2D, 0,
			0, 0, pImg->w, pImg->h,
			glImageFormat, glImageType, pImg->pixels );

	if (bGenerateMipMaps)
		glGenerateMipmap( GL_TEXTURE_2D );

	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glFlush();

	if (bFreeImg)
		delete pImg;
	return iTexHandle;
}

void RageDisplay_GL3::UpdateTexture(
	uintptr_t iTexHandle,
	RageSurface* pImg,
	int iXOffset, int iYOffset, int iWidth, int iHeight )
{
	glBindTexture( GL_TEXTURE_2D, static_cast<GLuint>(iTexHandle) );

	bool bFreeImg;
	RagePixelFormat SurfacePixFmt = GetImgPixelFormat( pImg, bFreeImg, iWidth, iHeight, false );

	glPixelStorei( GL_UNPACK_ROW_LENGTH, pImg->pitch / pImg->format->BytesPerPixel );

	GLenum glImageFormat = g_GL3PixFmtInfo[SurfacePixFmt].format;
	GLenum glImageType = g_GL3PixFmtInfo[SurfacePixFmt].type;

	glTexSubImage2D( GL_TEXTURE_2D, 0,
		iXOffset, iYOffset,
		iWidth, iHeight,
		glImageFormat, glImageType, pImg->pixels );

	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

	if (bFreeImg)
		delete pImg;
}

void RageDisplay_GL3::DeleteTexture( uintptr_t iTexture )
{
	if (iTexture == 0)
		return;

	if (m_mapRenderTargets.find(iTexture) != m_mapRenderTargets.end())
	{
		delete m_mapRenderTargets[iTexture];
		m_mapRenderTargets.erase( iTexture );
		return;
	}

	glDeleteTextures( 1, reinterpret_cast<GLuint*>(&iTexture) );
}

RageSurface *RageDisplay_GL3::GetTexture( uintptr_t iTexture )
{
	if (iTexture == 0)
		return nullptr;

#ifdef EMSCRIPTEN
	/* GLES3 has neither glGetTexLevelParameteriv nor glGetTexImage.
	 * Use a framebuffer readback: attach the texture to an FBO and
	 * glReadPixels from it.  We don't know the texture dimensions, so
	 * query the currently bound FBO size via the attachment params. */
	GLuint fbo;
	glGenFramebuffers( 1, &fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		static_cast<GLuint>(iTexture), 0 );

	GLenum fbStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glDeleteFramebuffers( 1, &fbo );
		LOG->Warn( "GL3::GetTexture: FBO incomplete (0x%x), cannot read back texture", fbStatus );
		return nullptr;
	}

	/* Query the attachment dimensions via glGetFramebufferAttachmentParameteriv */
	GLint iWidth = 0, iHeight = 0;
	glGetFramebufferAttachmentParameteriv( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &iWidth ); // dummy — see below

	/* There's no direct way to query attached texture size through the FBO in
	 * GLES3.  Fall back to RGBA readback of the implementation-chosen size.
	 * The caller will need to know the size.  For now, use GL_RGBA/UNSIGNED_BYTE
	 * and query via the viewport as an approximation — but really, this path
	 * is rarely used on Emscripten (screenshots use CreateScreenshot instead). */
	GLint viewport[4];
	glGetIntegerv( GL_VIEWPORT, viewport );
	iWidth = viewport[2];
	iHeight = viewport[3];

	const RagePixelFormatDesc &desc = PIXEL_FORMAT_DESC[RagePixelFormat_RGBA8];
	RageSurface *pImage = CreateSurface( iWidth, iHeight, desc.bpp,
		desc.masks[0], desc.masks[1], desc.masks[2], desc.masks[3] );
	glReadPixels( 0, 0, iWidth, iHeight, GL_RGBA, GL_UNSIGNED_BYTE, pImage->pixels );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glDeleteFramebuffers( 1, &fbo );
	return pImage;
#else
	glBindTexture( GL_TEXTURE_2D, static_cast<GLuint>(iTexture) );
	GLint iHeight, iWidth, iAlphaBits;
	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &iHeight );
	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iWidth );
	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE, &iAlphaBits );
	int iFormat = iAlphaBits ? RagePixelFormat_RGBA8 : RagePixelFormat_RGB8;

	const RagePixelFormatDesc &desc = PIXEL_FORMAT_DESC[iFormat];
	RageSurface *pImage = CreateSurface( iWidth, iHeight, desc.bpp,
		desc.masks[0], desc.masks[1], desc.masks[2], desc.masks[3] );
	glGetTexImage( GL_TEXTURE_2D, 0, g_GL3PixFmtInfo[iFormat].format,
		GL_UNSIGNED_BYTE, pImage->pixels );
	return pImage;
#endif
}

bool RageDisplay_GL3::SupportsTextureFormat( RagePixelFormat pixfmt, bool bRealtime )
{
	// All formats are supported — on GLES3 the BGR(A) formats are mapped to
	// RGB(A) equivalents in g_GL3PixFmtInfo and the game swizzles on CPU.
	return true;
}

// ============================================================
// Render state
// ============================================================

void RageDisplay_GL3::SetBlendMode( BlendMode mode )
{
	if (mode == m_CachedBlendMode)
		return;

	m_CachedBlendMode = mode;
	glEnable( GL_BLEND );

	if (mode == BLEND_INVERT_DEST)
		glBlendEquation( GL_FUNC_SUBTRACT );
	else if (mode == BLEND_SUBTRACT)
		glBlendEquation( GL_FUNC_REVERSE_SUBTRACT );
	else
		glBlendEquation( GL_FUNC_ADD );

	int iSourceRGB, iDestRGB;
	int iSourceAlpha = GL_ONE, iDestAlpha = GL_ONE_MINUS_SRC_ALPHA;
	switch( mode )
	{
	case BLEND_NORMAL:
		iSourceRGB = GL_SRC_ALPHA; iDestRGB = GL_ONE_MINUS_SRC_ALPHA;
		break;
	case BLEND_ADD:
		iSourceRGB = GL_SRC_ALPHA; iDestRGB = GL_ONE;
		break;
	case BLEND_SUBTRACT:
		iSourceRGB = GL_SRC_ALPHA; iDestRGB = GL_ONE_MINUS_SRC_ALPHA;
		break;
	case BLEND_MODULATE:
		iSourceRGB = GL_ZERO; iDestRGB = GL_SRC_COLOR;
		break;
	case BLEND_COPY_SRC:
		iSourceRGB = GL_ONE; iDestRGB = GL_ZERO;
		iSourceAlpha = GL_ONE; iDestAlpha = GL_ZERO;
		break;
	case BLEND_ALPHA_MASK:
		iSourceRGB = GL_ZERO; iDestRGB = GL_ONE;
		iSourceAlpha = GL_ZERO; iDestAlpha = GL_SRC_ALPHA;
		break;
	case BLEND_ALPHA_KNOCK_OUT:
		iSourceRGB = GL_ZERO; iDestRGB = GL_ONE;
		iSourceAlpha = GL_ZERO; iDestAlpha = GL_ONE_MINUS_SRC_ALPHA;
		break;
	case BLEND_ALPHA_MULTIPLY:
		iSourceRGB = GL_SRC_ALPHA; iDestRGB = GL_ZERO;
		break;
	case BLEND_WEIGHTED_MULTIPLY:
		iSourceRGB = GL_SRC_ALPHA; iDestRGB = GL_SRC_COLOR;
		break;
	case BLEND_INVERT_DEST:
		iSourceRGB = GL_ONE; iDestRGB = GL_ONE;
		break;
	case BLEND_NO_EFFECT:
		iSourceRGB = GL_ZERO; iDestRGB = GL_ONE;
		iSourceAlpha = GL_ZERO; iDestAlpha = GL_ONE;
		break;
	default:
		FAIL_M(ssprintf("Invalid BlendMode: %i", mode));
	}

	glBlendFuncSeparate( iSourceRGB, iDestRGB, iSourceAlpha, iDestAlpha );
}

bool RageDisplay_GL3::IsZWriteEnabled() const
{
	GLboolean b;
	glGetBooleanv( GL_DEPTH_WRITEMASK, &b );
	return b != GL_FALSE;
}

bool RageDisplay_GL3::IsZTestEnabled() const
{
	GLenum a;
	glGetIntegerv( GL_DEPTH_FUNC, (GLint*)&a );
	return a != GL_ALWAYS;
}

void RageDisplay_GL3::ClearZBuffer()
{

	bool write = IsZWriteEnabled();
	SetZWrite( true );
	glClear( GL_DEPTH_BUFFER_BIT );
	SetZWrite( write );
}

void RageDisplay_GL3::SetZWrite( bool b )
{
	if (b == m_bCachedZWrite)
		return;

	m_bCachedZWrite = b;
	glDepthMask( b );
}

void RageDisplay_GL3::SetZBias( float f )
{
	if (f == m_fCachedZBias)
		return;

	m_fCachedZBias = f;
	float fNear = SCALE( f, 0.0f, 1.0f, 0.05f, 0.0f );
	float fFar = SCALE( f, 0.0f, 1.0f, 1.0f, 0.95f );
#ifdef EMSCRIPTEN
	glDepthRangef( fNear, fFar );
#else
	glDepthRange( fNear, fFar );
#endif
}

void RageDisplay_GL3::SetZTestMode( ZTestMode mode )
{
	if (mode == m_CachedZTestMode)
		return;

	m_CachedZTestMode = mode;
	glEnable( GL_DEPTH_TEST );
	switch( mode )
	{
	case ZTEST_OFF:			glDepthFunc( GL_ALWAYS );	break;
	case ZTEST_WRITE_ON_PASS:	glDepthFunc( GL_LEQUAL );	break;
	case ZTEST_WRITE_ON_FAIL:	glDepthFunc( GL_GREATER );	break;
	default:
		FAIL_M(ssprintf("Invalid ZTestMode: %i", mode));
	}
}

void RageDisplay_GL3::SetCullMode( CullMode mode )
{
	if (mode == m_CachedCullMode)
		return;

	m_CachedCullMode = mode;
	if (mode != CULL_NONE)
		glEnable( GL_CULL_FACE );
	switch( mode )
	{
	case CULL_BACK:		glCullFace( GL_BACK );		break;
	case CULL_FRONT:	glCullFace( GL_FRONT );		break;
	case CULL_NONE:		glDisable( GL_CULL_FACE );	break;
	default:
		FAIL_M(ssprintf("Invalid CullMode: %i", mode));
	}
}

void RageDisplay_GL3::SetAlphaTest( bool b )
{
	m_bAlphaTestEnabled = b;
	// Alpha test is handled in the fragment shader via uniform
}

void RageDisplay_GL3::SetMaterial(
	const RageColor &emissive,
	const RageColor &ambient,
	const RageColor &diffuse,
	const RageColor &specular,
	float shininess )
{
	m_MatEmissive = emissive;
	m_MatAmbient = ambient;
	m_MatDiffuse = diffuse;
	m_MatSpecular = specular;
	m_fMatShininess = shininess;
	m_bLightUniformsDirty = true;
}

void RageDisplay_GL3::SetLighting( bool b )
{
	m_bLightingEnabled = b;
	if (b)
	{
		if (m_CurrentProgram != m_LitSpriteProgram && m_LitSpriteProgram)
		{
			m_CurrentProgram = m_LitSpriteProgram;
			glUseProgram( m_LitSpriteProgram );
			glUniform1i( m_uLitTexture0, 0 );
		}
	}
	else
	{
		if (m_CurrentProgram == m_LitSpriteProgram)
		{
			m_CurrentProgram = m_SpriteProgram;
			glUseProgram( m_SpriteProgram );
			glUniform1i( m_uTexture0, 0 );
		}
	}
}

void RageDisplay_GL3::SetLightOff( int index )
{
	if (index >= 0 && index < 8)
	{
		m_Lights[index].enabled = false;
		m_bLightUniformsDirty = true;
	}
}

void RageDisplay_GL3::SetLightDirectional(
	int index,
	const RageColor &ambient,
	const RageColor &diffuse,
	const RageColor &specular,
	const RageVector3 &dir )
{
	if (index < 0 || index >= 8)
		return;
	m_Lights[index].enabled = true;
	m_Lights[index].ambient = ambient;
	m_Lights[index].diffuse = diffuse;
	m_Lights[index].specular = specular;
	m_Lights[index].dir = dir;
	m_bLightUniformsDirty = true;
}

void RageDisplay_GL3::SetSphereEnvironmentMapping( TextureUnit tu, bool b )
{
	// Sphere mapping would require a vertex shader variant; stub for now
	m_bSphereMapping = b;
}

void RageDisplay_GL3::SetCelShaded( int stage )
{
	m_iCelShadedStage = stage;
	// Cel shading would require dedicated shaders; stub for now
}

void RageDisplay_GL3::SetEffectMode( EffectMode effect )
{
	GLuint prog;
	if (effect == EffectMode_Normal)
	{
		prog = m_bLightingEnabled ? m_LitSpriteProgram : m_SpriteProgram;
	}
	else
	{
		prog = m_EffectPrograms[effect];
		if (!prog)
			prog = m_SpriteProgram; // fallback
	}

	if (prog != m_CurrentProgram)
	{
		m_CurrentProgram = prog;
		glUseProgram( prog );
		// Set texture sampler uniform
		GLint loc = glGetUniformLocation( prog, "u_Texture0" );
		if (loc >= 0) glUniform1i( loc, 0 );
	}
}

bool RageDisplay_GL3::IsEffectModeSupported( EffectMode effect )
{
	if (effect == EffectMode_Normal)
		return true;
	return m_EffectPrograms[effect] != 0;
}

void RageDisplay_GL3::SetPolygonMode( PolygonMode pm )
{
#ifndef EMSCRIPTEN
	GLenum m;
	switch (pm)
	{
	case POLYGON_FILL:	m = GL_FILL; break;
	case POLYGON_LINE:	m = GL_LINE; break;
	default:
		FAIL_M(ssprintf("Invalid PolygonMode: %i", pm));
	}
	glPolygonMode( GL_FRONT_AND_BACK, m );
#endif
	// GLES3 only supports filled polygons; wireframe mode is unavailable.
}

void RageDisplay_GL3::SetLineWidth( float fWidth )
{
	glLineWidth( fWidth );
}

// ============================================================
// Render targets
// ============================================================

bool RageDisplay_GL3::SupportsRenderToTexture() const
{
	return true; // FBOs are core in GL 3.3
}

bool RageDisplay_GL3::SupportsFullscreenBorderlessWindow() const
{
	return g_pWind->SupportsFullscreenBorderlessWindow();
}

uintptr_t RageDisplay_GL3::CreateRenderTarget( const RenderTargetParam &param, int &iTextureWidthOut, int &iTextureHeightOut )
{
	RenderTarget_FBO_GL3 *pTarget = new RenderTarget_FBO_GL3;
	pTarget->Create( param, iTextureWidthOut, iTextureHeightOut );

	uintptr_t iTexture = pTarget->GetTexture();
	ASSERT( m_mapRenderTargets.find(iTexture) == m_mapRenderTargets.end() );
	m_mapRenderTargets[iTexture] = pTarget;
	return iTexture;
}

uintptr_t RageDisplay_GL3::GetRenderTarget()
{
	for (auto it = m_mapRenderTargets.begin(); it != m_mapRenderTargets.end(); ++it)
		if (it->second == m_pCurrentRenderTarget)
			return it->first;
	return 0;
}

void RageDisplay_GL3::SetRenderTarget( uintptr_t iTexture, bool bPreserveTexture )
{

	InvalidateMatrixCache(); // Projection/viewport changes

	if (iTexture == 0)
	{
		m_bInvertY = false;
		glFrontFace( GL_CCW );

		DISPLAY->CameraPopMatrix();

		int fWidth = g_pWind->GetActualVideoModeParams().windowWidth;
		int fHeight = g_pWind->GetActualVideoModeParams().windowHeight;
		glViewport( 0, 0, fWidth, fHeight );

		if (m_pCurrentRenderTarget)
			m_pCurrentRenderTarget->FinishRenderingTo();
		m_pCurrentRenderTarget = nullptr;
		return;
	}

	if (m_pCurrentRenderTarget != nullptr)
		SetRenderTarget(0, true);

	ASSERT(m_mapRenderTargets.find(iTexture) != m_mapRenderTargets.end());
	RenderTarget *pTarget = m_mapRenderTargets[iTexture];
	pTarget->StartRenderingTo();
	m_pCurrentRenderTarget = pTarget;

	// Set up for rendering to texture
	m_bInvertY = pTarget->InvertY();
	if (m_bInvertY)
		glFrontFace( GL_CW );

	// Set up viewport and projection for the render target
	int iWidth = pTarget->GetParam().iWidth;
	int iHeight = pTarget->GetParam().iHeight;
	glViewport( 0, 0, iWidth, iHeight );

	DISPLAY->CameraPushMatrix();
	DISPLAY->LoadMenuPerspective( 0, (float)iWidth, (float)iHeight, (float)iWidth/2, (float)iHeight/2 );

	if (!bPreserveTexture)
	{
		glClearColor( 0, 0, 0, 0 );
		SetZWrite( true );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
}

// ============================================================
// Compiled geometry
// ============================================================

RageCompiledGeometry* RageDisplay_GL3::CreateCompiledGeometry()
{
	return new RageCompiledGeometryGL3;
}

void RageDisplay_GL3::DeleteCompiledGeometry( RageCompiledGeometry* p )
{
	delete p;
}

bool RageDisplay_GL3::SupportsPerVertexMatrixScale()
{
	return false; // Not yet implemented for GL3
}

// ============================================================
// Threading
// ============================================================

bool RageDisplay_GL3::SupportsThreadedRendering()
{
	return g_pWind->SupportsThreadedRendering();
}

void RageDisplay_GL3::BeginConcurrentRenderingMainThread()
{
	g_pWind->BeginConcurrentRenderingMainThread();
}

void RageDisplay_GL3::EndConcurrentRenderingMainThread()
{
	g_pWind->EndConcurrentRenderingMainThread();
}

void RageDisplay_GL3::BeginConcurrentRendering()
{
	RageDisplay::BeginConcurrentRendering();
	g_pWind->BeginConcurrentRendering();
}

void RageDisplay_GL3::EndConcurrentRendering()
{
	g_pWind->EndConcurrentRendering();
}

// ============================================================
// Screenshot
// ============================================================

RageSurface* RageDisplay_GL3::CreateScreenshot()
{
	int width = g_pWind->GetActualVideoModeParams().width;
	int height = g_pWind->GetActualVideoModeParams().height;

	const RagePixelFormatDesc &desc = PIXEL_FORMAT_DESC[RagePixelFormat_RGBA8];
	RageSurface *image = CreateSurface( width, height, desc.bpp,
		desc.masks[0], desc.masks[1], desc.masks[2], 0 );

#ifndef EMSCRIPTEN
	glReadBuffer( GL_FRONT );
#endif
	// GLES3 default framebuffer only supports GL_BACK for glReadBuffer.
	glReadPixels( 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels );
	RageSurfaceUtils::FlipVertically( image );

	return image;
}

std::string RageDisplay_GL3::GetTextureDiagnostics( uintptr_t id ) const
{
	return std::string();
}

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
