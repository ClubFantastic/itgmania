/* Helpers for RageDisplay_GL3: shader compilation, FBO render target. */

#ifndef RAGE_DISPLAY_GL3_HELPERS_H
#define RAGE_DISPLAY_GL3_HELPERS_H

#include "RageDisplay.h"
#include "RageDisplay_OGL_Helpers.h"

#include <cstdint>

namespace RageDisplay_GL3_Helpers
{
	void Init();
	GLuint CompileShader( GLenum type, const char *source );
	GLuint LinkProgram( GLuint vert, GLuint frag );
	GLuint BuildProgram( const char *vertSrc, const char *fragSrc );
}

/* Core-profile FBO render target (no EXT suffix). */
class RenderTarget_FBO_GL3 : public RenderTarget
{
public:
	RenderTarget_FBO_GL3();
	~RenderTarget_FBO_GL3();
	void Create( const RenderTargetParam &param, int &iTextureWidthOut, int &iTextureHeightOut );
	uintptr_t GetTexture() const { return m_iTexHandle; }
	void StartRenderingTo();
	void FinishRenderingTo();
	virtual bool InvertY() const { return true; }

private:
	GLuint m_iFrameBuffer;
	uintptr_t m_iTexHandle;
	GLuint m_iDepthBuffer;
};

#endif
