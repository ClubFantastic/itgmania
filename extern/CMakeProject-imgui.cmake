set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
set(IMGUI_SRC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
)
if(HAS_GL3)
    list(APPEND IMGUI_SRC "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")
else()
    list(APPEND IMGUI_SRC "${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp")
endif()
add_library("imgui" STATIC ${IMGUI_SRC})
target_include_directories("imgui" PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
if(EMSCRIPTEN)
    target_compile_options("imgui" PRIVATE "SHELL:-sUSE_SDL=3")
    target_link_options("imgui" PRIVATE "SHELL:-sUSE_SDL=3")
else()
    target_link_libraries("imgui" SDL3::SDL3)
endif()
if(HAS_GL3)
    target_compile_definitions("imgui" PUBLIC HAS_GL3)
endif()
set_property(TARGET "imgui" PROPERTY FOLDER "External Libraries")
disable_project_warnings("imgui")
