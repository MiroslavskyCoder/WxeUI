cmake_minimum_required(VERSION 3.20)
project(window_winapi VERSION 1.0.0 LANGUAGES CXX)

# Настройки C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Профили сборки
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "Performance")


if(WIN32)
    set(CMAKE_SYSTEM_VERSION 10.0)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-DWINVER=0x0A00)  # Windows 10
    add_definitions(-D_WIN32_WINNT=0x0A00)
endif()

find_package(wil CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
find_package(directxmath CONFIG)
find_package(d3d12-memory-allocator CONFIG) 
find_package(directxtk12 CONFIG REQUIRED)
find_package(directxmesh CONFIG REQUIRED) 
find_package(directxmath CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED) 
find_package(directx-headers CONFIG REQUIRED)
find_package(D3D12MemoryAllocator CONFIG REQUIRED)
find_path(D3DX12_INCLUDE_DIRS "d3dx12.h")
find_package(directx-dxc CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibXml2 REQUIRED)

file(GLOB_RECURSE WINDOW_WINAPI_SOURCES src/*.cc)

add_executable(EXV2 ${WINDOW_WINAPI_SOURCES}) 

target_link_libraries(EXV2 PRIVATE WIL::WIL)
target_link_libraries(EXV2 PRIVATE unofficial::skia::skia)
target_link_libraries(EXV2 PRIVATE Microsoft::DirectXTK12)
target_link_libraries(EXV2 PRIVATE Microsoft::DirectXMesh)
target_link_libraries(EXV2 PRIVATE Microsoft::DirectXMesh::Utilities)
target_link_libraries(EXV2 PRIVATE Microsoft::DirectXMath) 
target_link_libraries(EXV2 PRIVATE Microsoft::DirectX-Headers)
target_link_libraries(EXV2 PRIVATE GPUOpen::D3D12MemoryAllocator) 
target_include_directories(EXV2 PRIVATE ${D3DX12_INCLUDE_DIRS}) 
target_link_libraries(EXV2 PRIVATE Microsoft::DirectXShaderCompiler)
target_link_libraries(EXV2 PRIVATE Microsoft::CppWinRT)
target_link_libraries(EXV2 PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(EXV2 PRIVATE LibXml2::LibXml2)
target_link_libraries(EXV2 PRIVATE ZLIB::ZLIB)
target_link_libraries(EXV2 PRIVATE zstd::libzstd)

if(TARGET Microsoft::DirectX12-Agility)
    file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/D3D12")
    add_custom_command(TARGET EXV2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PROPERTY:Microsoft::DirectX12-Core,IMPORTED_LOCATION_RELEASE> $<TARGET_FILE_DIR:EXV2>/D3D12
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PROPERTY:Microsoft::DirectX12-Layers,IMPORTED_LOCATION_DEBUG> $<TARGET_FILE_DIR:EXV2>/D3D12
        COMMAND_EXPAND_LISTS
    )
endif()
target_include_directories(EXV2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Базовые библиотеки
target_link_libraries(EXV2 PUBLIC
    skia::skia
    user32.lib
    kernel32.lib
    gdi32.lib
    winspool.lib
    shell32.lib
    ole32.lib
    oleaut32.lib
    uuid.lib
    comdlg32.lib
    advapi32.lib
    shcore.lib
) 
# Компиляционные опции
target_compile_options(EXV2 PRIVATE
    /W4
    /WX
    /permissive-
    /Zc:__cplusplus
    /DDPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
)
