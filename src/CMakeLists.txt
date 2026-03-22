cmake_minimum_required(VERSION 3.30)

project(Anima)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(imgui CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(Anima WIN32
    # Entry point
    ../apps/main.cpp

    # Core
    core/Engine.cpp

    # Drawing system
    core/Canvas.cpp
    core/Viewport.cpp
    core/InputHandler.cpp

    # UI
    ui/Styles.cpp

    # Resources
    resources/resources.rc

    # imgui backends
    ../extern/imgui_impl_sdl2.cpp
    ../extern/imgui_impl_opengl3.cpp
)
target_include_directories(Anima PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../extern
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_link_libraries(Anima PRIVATE
    imgui::imgui
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    SDL2::SDL2
    glm::glm
    nlohmann_json::nlohmann_json
    opengl32
)