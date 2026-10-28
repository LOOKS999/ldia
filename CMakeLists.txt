cmake_minimum_required(VERSION 3.10)
project(Lida LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Lida WIN32
    src/main.cpp
    resource.rc
)

target_link_libraries(Lida PRIVATE
    user32
    gdi32
    shell32
    advapi32
    comdlg32
    ole32
)

if(MSVC)
    set_target_properties(Lida PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()
