add_executable(directx12_demo main.cpp)
target_link_libraries(directx12_demo PRIVATE window_winapi)
set_target_properties(directx12_demo PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
