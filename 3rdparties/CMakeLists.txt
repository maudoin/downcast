project(3rdparties LANGUAGES C CXX)

add_library(${PROJECT_NAME}
    imgui_app.cpp
    imgui.h
    sqlite3.c
    sqlite3.h
    httplib.h
    nanojpeg.c
    picopng.cpp
    imfilebrowser.h
    rapidxml-1.13/rapidxml.hpp
    rapidxml-1.13/rapidxml_iterators.hpp
    rapidxml-1.13/rapidxml_print.hpp
    rapidxml-1.13/rapidxml_utils.hpp
)
target_include_directories(${PROJECT_NAME}
    PUBLIC
    .
    rapidxml-1.13
)
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    OpenSSL::SSL -static
    OpenSSL::Crypto -static
    Threads::Threads -static
    d3d11.lib#sokol/imgui_app
    Imm32.lib#d3d(sokol/imgui_app)
    wsock32.lib#crypto static
    ws2_32.lib#httplib
    Crypt32.lib#httplib
)

add_subdirectory(googletest-release-1.12.1)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
