# ITGmania Emscripten/WebAssembly Port

## Status (2026-03-14)

**The game boots, renders with WebGL2, plays audio, loads themes (Simply Love), shows the song wheel, and can enter gameplay.** Major systems working: GL3 renderer, SDL audio, ImGui overlay, theme loading, HTTP-based lazy file loading.

## Architecture

### Build System
- **Branch:** `cf-web`
- **Emscripten SDK:** `~/emsdk` (version 5.0.3)
- **Configure:** `source ~/emsdk/emsdk_env.sh && emcmake cmake -S . -B build-emscripten -DWITH_SDL3=ON -DWITH_GL3=ON -DCMAKE_C_FLAGS="-pthread" -DCMAKE_CXX_FLAGS="-pthread"`
- **Build:** `cmake --build build-emscripten -j$(nproc)`
- **Output:** `itgmania.html`, `itgmania.js`, `itgmania.wasm` (in project root)

### Web Serving
- **Directory:** `web-serve/` with symlinks to build output and game data
- **Server:** `web-serve/server.py` — Python HTTP server with COOP/COEP headers (required for SharedArrayBuffer/pthreads)
- **Start:** `cd web-serve && python3 server.py 8080`
- **URL:** `http://localhost:8080/itgmania.html`

### Game Data (Manifest System)
- **Script:** `Docs/Devdocs/generate-web-manifest.sh` generates `gamedata-manifest.txt`
- **Format:** `size\tpath` per line, `-1` for directories
- **Includes:** Characters, Data, NoteSkins, Songs, Themes
- **Symlinks in `web-serve/gamedata/`:** Characters, Data, NoteSkins, Songs, Themes → `../../`
- **Regenerate after adding songs/data:** `bash Docs/Devdocs/generate-web-manifest.sh . gamedata-manifest.txt`

### Key Emscripten Link Flags (`src/CMakeLists.txt`)
- `-sUSE_SDL=3` — SDL3 from Emscripten ports
- `-sASYNCIFY -sASYNCIFY_STACK_SIZE=65536` — allows synchronous XHR (for lazy file loading)
- `-sFULL_ES3=1 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2` — WebGL2/GLES3
- `-sPTHREAD_POOL_SIZE=8 -pthread` — web workers for threads
- `-sALLOW_MEMORY_GROWTH=1` — dynamic heap
- `-sEXPORTED_RUNTIME_METHODS=callMain` — deferred main() call
- `--shell-file src/emscripten_shell.html` — custom HTML shell with "Load Game" button
- `-sASSERTIONS=2 -g` — debug mode (remove for release)

## Files Created/Modified

### New Files
| File | Purpose |
|------|---------|
| `src/RageFileDriverHTTP.h/.cpp` | Lazy HTTP file driver — fetches game data on demand via XHR. Bypasses FilenameDB; uses simple `std::map` for manifest lookups |
| `src/arch/ArchHooks/ArchHooks_Emscripten.h/.cpp` | Platform hooks — timing via `emscripten_get_now()`, audio context resume on user gesture, HTTP VFS mount |
| `src/emscripten_shell.html` | Custom HTML shell with "Load Game" button (satisfies browser autoplay policy) |
| `web-serve/server.py` | Dev server with COOP/COEP headers for SharedArrayBuffer |
| `Docs/Devdocs/generate-web-manifest.sh` | Generates file manifest for HTTP driver |

### Modified Files (CMake)
| File | Changes |
|------|---------|
| `StepmaniaCore.cmake` | Skip `find_package(SDL3)` on Emscripten (provided by ports) |
| `src/CMakeLists.txt` | Emscripten link/compile flags, skip SSE2, skip glew/hidapi, custom shell |
| `src/CMakeData-arch.cmake` | Skip HID lights drivers on Emscripten |
| `src/CMakeData-os.cmake` | Skip HidDevice, add SpecialDirs for Emscripten |
| `src/CMakeData-rage.cmake` | Exclude legacy `RageDisplay_OGL.cpp`, add HTTP driver |
| `extern/CMakeLists.txt` | Skip hidapi, glew, libusb on Emscripten |
| `extern/CMakeProject-imgui.cmake` | Use `-sUSE_SDL=3` instead of `SDL3::SDL3` target |
| `extern/CMakeProject-ixwebsocket.cmake` | Define `PLATFORM_NAME` for Emscripten |
| `extern/CMakeProject-libjpeg-turbo.cmake` | Pass toolchain file, disable SIMD |

### Modified Files (Source)
| File | Changes |
|------|---------|
| `src/RageDisplay_GL3.cpp` | GLES3 compat: pixel formats, version check (parse GL_VERSION string), skip GL_LINE_SMOOTH/glPolygonMode/anisotropic, FBO-based GetTexture, glDepthRangef |
| `src/RageDisplay_GL3_Shaders.cpp` | Removed `#version` lines (prepended at runtime by CompileShader) |
| `src/RageDisplay_OGL_Helpers.h` | `<GLES3/gl3.h>` on Emscripten instead of `<GL/glew.h>` |
| `src/RageDisplay_OGL_Helpers.cpp` | Guard desktop-only GL constants |
| `src/RageDisplay.cpp` | Skip `FrameLimitBeforeVsync` on Emscripten (browser handles vsync) |
| `src/StepMania.cpp` | Don't default to `SUPPORT_OPENGL` when `SUPPORT_GL3` defined; add "OpenGL ES" video card defaults with `gl3` renderer |
| `src/GameLoop.cpp` | `emscripten_set_main_loop(RunOneFrame, 0, 1)` |
| `src/ImGuiManager.cpp` | Use `#version 300 es` for ImGui shaders on Emscripten |
| `src/arch/arch_default.h` | Emscripten section: SDL3 input, SDL sound, Null movie, GL3 renderer |
| `src/arch/Threads/Threads_Pthreads.cpp` | Skip `pthread_setname_np`, Emscripten monotonic clock path |
| `src/archutils/Common/PthreadHelpers.cpp` | Emscripten section (no `pthread_kill`/signals) |
| `src/arch/Sound/RageSoundDriver_SDL.cpp` | Use device native sample rate on Emscripten to avoid resampling artifacts |
| `src/CharacterManager.cpp` | Skip "Characters/default missing" crash on Emscripten |
| `src/GameState.cpp` | Skip null character assert on Emscripten |
| `src/Background.cpp` | Gracefully handle missing background effects instead of crashing |

## Known Issues / TODO

### FilenameDB Data Loss (Partially Worked Around)
The `FilenameDB` (used by `RageFileDriver` base class) intermittently loses directory data — probably a subtle caching/threading issue. **Workaround:** The HTTP driver now bypasses `FilenameDB` entirely and implements `GetDirListing`, `GetFileType`, and `GetFileSizeInBytes` directly from a simple `std::map`. This fixed theme/song loading. However, some code paths may still use `FilenameDB` indirectly.

### Background Effects Missing
`BackgroundImpl::Layer::CreateBackground` can't find background effect Lua files (e.g., `StretchNormal.lua`). These are in `BGAnimations/` which may need to be added to the manifest. **Current fix:** gracefully skip with blank Actor instead of crashing. **Proper fix:** add `BGAnimations/` to the manifest script, or investigate why `BackgroundUtil::GetBackgroundEffects` fails.

### Characters Not Loading
The `Characters/` directory is in the manifest but `GetDirListing` returns empty. This was an issue with the old `FilenameDB`-based driver. With the new direct-map driver it may work, but hasn't been verified. Characters are skipped on Emscripten anyway (nobody uses them).

### Performance
- **ASYNCIFY** adds significant overhead (~2.5x wasm size, runtime overhead for stack unwinding). Consider migrating to fully async file loading or preloading to eliminate ASYNCIFY dependency.
- **`-pthread + ALLOW_MEMORY_GROWTH`** generates a warning about slow non-wasm code. Consider using a fixed memory size for release builds.
- **Debug flags** (`-g`, `-sASSERTIONS=2`) should be removed for release.

### Missing Features
- **Movie textures:** FFmpeg is not available; `MovieTexture_Null` is used. Video backgrounds won't play.
- **File writing:** The HTTP driver is read-only. Save data (profiles, scores, preferences) needs IndexedDB backing via Emscripten's IDBFS or a custom solution.
- **Song loading:** Songs appear in the wheel but the loading/VFS path needs more testing. The manifest must be regenerated when songs are added.
- **Input:** Only SDL3 input (keyboard/mouse via browser events). No dance pad support in browser (would need WebHID or WebUSB).
- **Networking:** IXWebSocket compiles but websocket behavior in browser is untested.

### Build Gotchas
- **libjpeg-turbo:** Built as ExternalProject — must pass the Emscripten toolchain file and disable SIMD (`-DWITH_SIMD=OFF`)
- **Global `-pthread`:** Must pass `-DCMAKE_C_FLAGS="-pthread" -DCMAKE_CXX_FLAGS="-pthread"` at configure time so ALL libraries (lua, ogg, vorbis, etc.) compile with atomics support
- **HTML shell:** Emscripten replaces `{{{ SCRIPT }}}` with a `<script>` tag — can't place it inside JavaScript code
- **`callMain`:** Must be exported via `-sEXPORTED_RUNTIME_METHODS=callMain` for the deferred-start shell
- **COOP/COEP headers:** Required for SharedArrayBuffer (pthreads). Use `server.py`, not plain `python3 -m http.server`

## Quick Start (Fresh Machine)

```bash
# 1. Install emsdk
git clone https://github.com/nicholasgasior/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest

# 2. Configure
cd /path/to/itgmania
source ~/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-emscripten -DWITH_SDL3=ON -DWITH_GL3=ON \
  -DCMAKE_C_FLAGS="-pthread" -DCMAKE_CXX_FLAGS="-pthread"

# 3. Build
cmake --build build-emscripten -j$(nproc)

# 4. Generate manifest (include Songs if you have them)
bash Docs/Devdocs/generate-web-manifest.sh . gamedata-manifest.txt

# 5. Set up web-serve directory
mkdir -p web-serve/gamedata
ln -sf ../itgmania.html ../itgmania.js ../itgmania.wasm web-serve/
ln -sf ../../gamedata-manifest.txt web-serve/gamedata/
for d in Characters Data NoteSkins Songs Themes; do
  ln -sf ../../$d web-serve/gamedata/$d
done

# 6. Serve
cd web-serve && python3 server.py 8080
# Open http://localhost:8080/itgmania.html
```
