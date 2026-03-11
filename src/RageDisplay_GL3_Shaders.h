/* Embedded GLSL 3.30 shader source strings for RageDisplay_GL3. */

#ifndef RAGE_DISPLAY_GL3_SHADERS_H
#define RAGE_DISPLAY_GL3_SHADERS_H

namespace GL3Shaders
{
	// Core sprite shaders (unlit)
	extern const char *g_SpriteVertSrc;
	extern const char *g_SpriteFragSrc;

	// Lit sprite shaders (with Phong lighting)
	extern const char *g_LitSpriteVertSrc;
	extern const char *g_LitSpriteFragSrc;

	// Effect fragment shaders (all use sprite vertex shader)
	extern const char *g_EffectFragUnpremultiply;
	extern const char *g_EffectFragColorBurn;
	extern const char *g_EffectFragColorDodge;
	extern const char *g_EffectFragVividLight;
	extern const char *g_EffectFragHardMix;
	extern const char *g_EffectFragOverlay;
	extern const char *g_EffectFragScreen;
	extern const char *g_EffectFragYUYV422;
	extern const char *g_EffectFragDistanceField;
}

#endif
