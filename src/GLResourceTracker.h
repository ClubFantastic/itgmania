// GLResourceTracker - lightweight GL resource allocation/deallocation tracker.
//
// Wraps glGen/glDelete calls with counters and periodic logging to identify
// GL resource leaks (textures, buffers, VAOs, FBOs, renderbuffers, shaders).
//
// Include this header and call GL_TRACK_* macros instead of raw GL calls.
// Call GLResourceTracker::LogStats() periodically (e.g. once per frame).
//
// Enable at build time: -DWITH_GL_RESOURCE_TRACKING=ON (CMake) or
// define GL_RESOURCE_TRACKING before including this header.

#ifndef GL_RESOURCE_TRACKER_H
#define GL_RESOURCE_TRACKER_H

#include "RageDisplay_OGL_Helpers.h"
#include "RageLog.h"

#include <atomic>
#include <cstdio>

#if defined(GL_RESOURCE_TRACKING)

class GLResourceTracker
{
public:
	// Resource type counters
	struct Stats {
		std::atomic<int> textures{0};
		std::atomic<int> buffers{0};
		std::atomic<int> vaos{0};
		std::atomic<int> fbos{0};
		std::atomic<int> renderbuffers{0};
		std::atomic<int> shaders{0};
		std::atomic<int> programs{0};

		// Lifetime totals
		std::atomic<int> totalTexturesCreated{0};
		std::atomic<int> totalTexturesDeleted{0};
		std::atomic<int> totalBuffersCreated{0};
		std::atomic<int> totalBuffersDeleted{0};
	};

	static Stats& Get()
	{
		static Stats s;
		return s;
	}

	// Log current resource counts.  If delta-only, only log when counts changed.
	static void LogStats(bool deltaOnly = true)
	{
		static int prevTex = 0, prevBuf = 0, prevVao = 0, prevFbo = 0, prevRb = 0;
		static int prevShd = 0, prevPrg = 0;
		static int frameCounter = 0;
		++frameCounter;

		auto &s = Get();
		int tex = s.textures.load();
		int buf = s.buffers.load();
		int vao = s.vaos.load();
		int fbo = s.fbos.load();
		int rb  = s.renderbuffers.load();
		int shd = s.shaders.load();
		int prg = s.programs.load();

		if (deltaOnly &&
			tex == prevTex && buf == prevBuf && vao == prevVao &&
			fbo == prevFbo && rb == prevRb && shd == prevShd && prg == prevPrg)
			return;

		int dTex = tex - prevTex;
		int dBuf = buf - prevBuf;
		int dVao = vao - prevVao;
		int dFbo = fbo - prevFbo;
		int dRb  = rb  - prevRb;

		LOG->Info("GLTrack [frame %d]: tex=%d(%+d) buf=%d(%+d) vao=%d(%+d) fbo=%d(%+d) rb=%d(%+d) shd=%d prg=%d | lifetime tex: +%d/-%d buf: +%d/-%d",
			frameCounter,
			tex, dTex, buf, dBuf, vao, dVao, fbo, dFbo, rb, dRb, shd, prg,
			s.totalTexturesCreated.load(), s.totalTexturesDeleted.load(),
			s.totalBuffersCreated.load(), s.totalBuffersDeleted.load());

		prevTex = tex; prevBuf = buf; prevVao = vao;
		prevFbo = fbo; prevRb  = rb;  prevShd = shd; prevPrg = prg;
	}

	// Force-log regardless of delta
	static void DumpStats()
	{
		LogStats(false);
	}

	// --- Wrapped GL calls ---

	static void GenTextures(GLsizei n, GLuint *textures, const char *caller)
	{
		glGenTextures(n, textures);
		auto &s = Get();
		s.textures.fetch_add(n);
		s.totalTexturesCreated.fetch_add(n);
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: GenTexture id=%u (%s)", textures[i], caller);
	}

	static void DeleteTextures(GLsizei n, const GLuint *textures, const char *caller)
	{
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: DeleteTexture id=%u (%s)", textures[i], caller);
		glDeleteTextures(n, textures);
		auto &s = Get();
		s.textures.fetch_sub(n);
		s.totalTexturesDeleted.fetch_add(n);
	}

	static void GenBuffers(GLsizei n, GLuint *buffers, const char *caller)
	{
		glGenBuffers(n, buffers);
		Get().buffers.fetch_add(n);
		Get().totalBuffersCreated.fetch_add(n);
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: GenBuffer id=%u (%s)", buffers[i], caller);
	}

	static void DeleteBuffers(GLsizei n, const GLuint *buffers, const char *caller)
	{
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: DeleteBuffer id=%u (%s)", buffers[i], caller);
		glDeleteBuffers(n, buffers);
		Get().buffers.fetch_sub(n);
		Get().totalBuffersDeleted.fetch_add(n);
	}

	// ARB extension variants (legacy OGL renderer)
	static void GenBuffersARB(GLsizei n, GLuint *buffers, const char *caller)
	{
		glGenBuffersARB(n, buffers);
		Get().buffers.fetch_add(n);
		Get().totalBuffersCreated.fetch_add(n);
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: GenBufferARB id=%u (%s)", buffers[i], caller);
	}

	static void DeleteBuffersARB(GLsizei n, const GLuint *buffers, const char *caller)
	{
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: DeleteBufferARB id=%u (%s)", buffers[i], caller);
		glDeleteBuffersARB(n, buffers);
		Get().buffers.fetch_sub(n);
		Get().totalBuffersDeleted.fetch_add(n);
	}

	static void GenVertexArrays(GLsizei n, GLuint *arrays, const char *caller)
	{
		glGenVertexArrays(n, arrays);
		Get().vaos.fetch_add(n);
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: GenVAO id=%u (%s)", arrays[i], caller);
	}

	static void DeleteVertexArrays(GLsizei n, const GLuint *arrays, const char *caller)
	{
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: DeleteVAO id=%u (%s)", arrays[i], caller);
		glDeleteVertexArrays(n, arrays);
		Get().vaos.fetch_sub(n);
	}

	static void GenFramebuffers(GLsizei n, GLuint *fbos, const char *caller)
	{
		glGenFramebuffers(n, fbos);
		Get().fbos.fetch_add(n);
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: GenFBO id=%u (%s)", fbos[i], caller);
	}

	static void DeleteFramebuffers(GLsizei n, const GLuint *fbos, const char *caller)
	{
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: DeleteFBO id=%u (%s)", fbos[i], caller);
		glDeleteFramebuffers(n, fbos);
		Get().fbos.fetch_sub(n);
	}

	// EXT variants (legacy OGL renderer)
	static void GenFramebuffersEXT(GLsizei n, GLuint *fbos, const char *caller)
	{
		glGenFramebuffersEXT(n, fbos);
		Get().fbos.fetch_add(n);
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: GenFBO_EXT id=%u (%s)", fbos[i], caller);
	}

	static void DeleteFramebuffersEXT(GLsizei n, const GLuint *fbos, const char *caller)
	{
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: DeleteFBO_EXT id=%u (%s)", fbos[i], caller);
		glDeleteFramebuffersEXT(n, fbos);
		Get().fbos.fetch_sub(n);
	}

	static void GenRenderbuffers(GLsizei n, GLuint *rbs, const char *caller)
	{
		glGenRenderbuffers(n, rbs);
		Get().renderbuffers.fetch_add(n);
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: GenRenderbuffer id=%u (%s)", rbs[i], caller);
	}

	static void DeleteRenderbuffers(GLsizei n, const GLuint *rbs, const char *caller)
	{
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: DeleteRenderbuffer id=%u (%s)", rbs[i], caller);
		glDeleteRenderbuffers(n, rbs);
		Get().renderbuffers.fetch_sub(n);
	}

	// EXT variants (legacy OGL renderer)
	static void GenRenderbuffersEXT(GLsizei n, GLuint *rbs, const char *caller)
	{
		glGenRenderbuffersEXT(n, rbs);
		Get().renderbuffers.fetch_add(n);
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: GenRenderbuffer_EXT id=%u (%s)", rbs[i], caller);
	}

	static void DeleteRenderbuffersEXT(GLsizei n, const GLuint *rbs, const char *caller)
	{
		for (GLsizei i = 0; i < n; ++i)
			LOG->Trace("GLTrack: DeleteRenderbuffer_EXT id=%u (%s)", rbs[i], caller);
		glDeleteRenderbuffersEXT(n, rbs);
		Get().renderbuffers.fetch_sub(n);
	}

	static GLuint CreateShader(GLenum type, const char *caller)
	{
		GLuint s = glCreateShader(type);
		Get().shaders.fetch_add(1);
		LOG->Trace("GLTrack: CreateShader id=%u type=0x%x (%s)", s, type, caller);
		return s;
	}

	static void DeleteShader(GLuint shader, const char *caller)
	{
		LOG->Trace("GLTrack: DeleteShader id=%u (%s)", shader, caller);
		glDeleteShader(shader);
		Get().shaders.fetch_sub(1);
	}

	static GLuint CreateProgram(const char *caller)
	{
		GLuint p = glCreateProgram();
		Get().programs.fetch_add(1);
		LOG->Trace("GLTrack: CreateProgram id=%u (%s)", p, caller);
		return p;
	}

	static void DeleteProgram(GLuint program, const char *caller)
	{
		LOG->Trace("GLTrack: DeleteProgram id=%u (%s)", program, caller);
		glDeleteProgram(program);
		Get().programs.fetch_sub(1);
	}
};

// Convenience macros that auto-fill caller name
#define GL_TRACK_GEN_TEXTURES(n, t)       GLResourceTracker::GenTextures(n, t, __func__)
#define GL_TRACK_DELETE_TEXTURES(n, t)     GLResourceTracker::DeleteTextures(n, t, __func__)
#define GL_TRACK_GEN_BUFFERS(n, b)         GLResourceTracker::GenBuffers(n, b, __func__)
#define GL_TRACK_DELETE_BUFFERS(n, b)       GLResourceTracker::DeleteBuffers(n, b, __func__)
#define GL_TRACK_GEN_VAOS(n, a)            GLResourceTracker::GenVertexArrays(n, a, __func__)
#define GL_TRACK_DELETE_VAOS(n, a)          GLResourceTracker::DeleteVertexArrays(n, a, __func__)
#define GL_TRACK_GEN_FBOS(n, f)            GLResourceTracker::GenFramebuffers(n, f, __func__)
#define GL_TRACK_DELETE_FBOS(n, f)          GLResourceTracker::DeleteFramebuffers(n, f, __func__)
#define GL_TRACK_GEN_RENDERBUFFERS(n, r)   GLResourceTracker::GenRenderbuffers(n, r, __func__)
#define GL_TRACK_DELETE_RENDERBUFFERS(n, r) GLResourceTracker::DeleteRenderbuffers(n, r, __func__)
#define GL_TRACK_CREATE_SHADER(type)       GLResourceTracker::CreateShader(type, __func__)
#define GL_TRACK_DELETE_SHADER(s)          GLResourceTracker::DeleteShader(s, __func__)
#define GL_TRACK_CREATE_PROGRAM()          GLResourceTracker::CreateProgram(__func__)
#define GL_TRACK_DELETE_PROGRAM(p)         GLResourceTracker::DeleteProgram(p, __func__)
#define GL_TRACK_LOG_STATS()               GLResourceTracker::LogStats()
#define GL_TRACK_DUMP_STATS()              GLResourceTracker::DumpStats()
// ARB/EXT variants for legacy OGL renderer
#define GL_TRACK_GEN_BUFFERS_ARB(n, b)         GLResourceTracker::GenBuffersARB(n, b, __func__)
#define GL_TRACK_DELETE_BUFFERS_ARB(n, b)       GLResourceTracker::DeleteBuffersARB(n, b, __func__)
#define GL_TRACK_GEN_FBOS_EXT(n, f)            GLResourceTracker::GenFramebuffersEXT(n, f, __func__)
#define GL_TRACK_DELETE_FBOS_EXT(n, f)          GLResourceTracker::DeleteFramebuffersEXT(n, f, __func__)
#define GL_TRACK_GEN_RENDERBUFFERS_EXT(n, r)   GLResourceTracker::GenRenderbuffersEXT(n, r, __func__)
#define GL_TRACK_DELETE_RENDERBUFFERS_EXT(n, r) GLResourceTracker::DeleteRenderbuffersEXT(n, r, __func__)

#else // GL_RESOURCE_TRACKING not defined — zero-cost pass-through

#define GL_TRACK_GEN_TEXTURES(n, t)        glGenTextures(n, t)
#define GL_TRACK_DELETE_TEXTURES(n, t)      glDeleteTextures(n, t)
#define GL_TRACK_GEN_BUFFERS(n, b)          glGenBuffers(n, b)
#define GL_TRACK_DELETE_BUFFERS(n, b)        glDeleteBuffers(n, b)
#define GL_TRACK_GEN_VAOS(n, a)             glGenVertexArrays(n, a)
#define GL_TRACK_DELETE_VAOS(n, a)           glDeleteVertexArrays(n, a)
#define GL_TRACK_GEN_FBOS(n, f)             glGenFramebuffers(n, f)
#define GL_TRACK_DELETE_FBOS(n, f)           glDeleteFramebuffers(n, f)
#define GL_TRACK_GEN_RENDERBUFFERS(n, r)    glGenRenderbuffers(n, r)
#define GL_TRACK_DELETE_RENDERBUFFERS(n, r)  glDeleteRenderbuffers(n, r)
#define GL_TRACK_CREATE_SHADER(type)        glCreateShader(type)
#define GL_TRACK_DELETE_SHADER(s)           glDeleteShader(s)
#define GL_TRACK_CREATE_PROGRAM()           glCreateProgram()
#define GL_TRACK_DELETE_PROGRAM(p)          glDeleteProgram(p)
#define GL_TRACK_LOG_STATS()                ((void)0)
#define GL_TRACK_DUMP_STATS()               ((void)0)
// ARB/EXT variants
#define GL_TRACK_GEN_BUFFERS_ARB(n, b)         glGenBuffersARB(n, b)
#define GL_TRACK_DELETE_BUFFERS_ARB(n, b)       glDeleteBuffersARB(n, b)
#define GL_TRACK_GEN_FBOS_EXT(n, f)            glGenFramebuffersEXT(n, f)
#define GL_TRACK_DELETE_FBOS_EXT(n, f)          glDeleteFramebuffersEXT(n, f)
#define GL_TRACK_GEN_RENDERBUFFERS_EXT(n, r)   glGenRenderbuffersEXT(n, r)
#define GL_TRACK_DELETE_RENDERBUFFERS_EXT(n, r) glDeleteRenderbuffersEXT(n, r)

#endif // GL_RESOURCE_TRACKING

#endif // GL_RESOURCE_TRACKER_H
