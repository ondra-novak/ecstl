add_executable(compile_test compile_test.cpp 
 constexpr_tests.cpp
)
add_executable(hello_world hello_world.cpp)
add_executable(hello_world_c hello_world.c)
target_link_libraries(hello_world_c ecsc)
add_executable(deleter_test deleter_test.c)
target_link_libraries(deleter_test ecsc)
add_executable(view_test view.c)
target_link_libraries(view_test ecsc)


add_executable(signals signals.cpp)