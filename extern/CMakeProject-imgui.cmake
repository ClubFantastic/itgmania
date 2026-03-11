set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
set(IMGUI_SRC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp"
)
add_library("imgui" STATIC ${IMGUI_SRC})
target_include_directories("imgui" PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
target_link_libraries("imgui" SDL3::SDL3)
set_property(TARGET "imgui" PROPERTY FOLDER "External Libraries")
disable_project_warnings("imgui")
