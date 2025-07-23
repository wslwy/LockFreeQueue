# CMakeList.txt: lock_free_queue 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("ThreadPoolExample")

# 指定最低C++版本要求（全局作用）
set(CMAKE_CXX_STANDARD 17)  # 可以是14, 17, 20等
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 必须满足此标准
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器特有扩展（提高可移植性）

# 设置全局输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "out/build/bin")

# 添加头文件库目标（重要！模板类需要这个）
# 注意：这里不能使用${PROJECT_NAME}作为目标名
add_library(lock_free_queue INTERFACE)

# 设置公共头文件目录（INTERFACE表示头文件属性）
target_include_directories(lock_free_queue INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 启用测试模块
enable_testing()
add_subdirectory(tests)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET lock_free_queue PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。