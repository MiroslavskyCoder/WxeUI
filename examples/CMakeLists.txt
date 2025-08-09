# Examples CMakeLists.txt

# DirectX 12 Demo
if(ENABLE_DIRECTX12)
    add_subdirectory(directx12_demo)
endif()

# DirectX 11 Demo
if(ENABLE_DIRECTX11)
    add_subdirectory(directx11_demo)
endif()

# Vulkan Demo
if(ENABLE_VULKAN)
    add_subdirectory(vulkan_demo)
endif()

# ANGLE Demo
if(ENABLE_ANGLE)
    add_subdirectory(angle_demo)
endif()

# API Comparison
if(BUILD_PERFORMANCE_TESTS)
    add_subdirectory(api_comparison)
endif()

# Basic window (already exists)
add_subdirectory(basic_window)
add_subdirectory(layer_system)
