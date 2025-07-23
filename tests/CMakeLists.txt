# 创建测试可执行目标
add_executable(test_example test_example.cpp)

# 链接ThreadPool头文件库
target_link_libraries(test_example PRIVATE lock_free_queue)

# 可选：设置测试可执行文件的输出目录
set_target_properties(test_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/build/tests
)

# 添加测试
add_test(NAME LockFreeQueue_Example COMMAND test_example)


# 自定义测试文件
# 创建测试可执行目标
add_executable(test_arr01 test_arr01.cpp)

# 链接ThreadPool头文件库
target_link_libraries(test_arr01 PRIVATE lock_free_queue)

# 可选：设置测试可执行文件的输出目录
set_target_properties(test_arr01 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/build/tests
)

# 添加测试
add_test(NAME LockFreeQueueArrBased_BasicTest01 COMMAND test_arr01)