cmake_minimum_required(VERSION 3.5)

# pbc
file(GLOB SRC_LIST "pbc/src/*.c")
add_library(pbc STATIC ${SRC_LIST})
target_include_directories(pbc PRIVATE
    pbc
)
if(WINDOWS)
    # 强制以C++编译，这是pbc VC工程的设定。里面有些bool等类型，C编译不过
    target_compile_options(pbc PRIVATE /TP /wd4146 /wd4273 /wd4244 /wd4018 /wd4267)
endif()

# llhttp
file(GLOB SRC_LIST "llhttp/src/*.c")
add_library(llhttp STATIC ${SRC_LIST})
# 清空编译参数，不然的话有太多的警告。llhttp的CMakeList.txt也没有额外参数
set_target_properties(llhttp PROPERTIES COMPILE_OPTIONS "")
# 使用c99标准编译，参照llhttp的CMakeList.txt
set_property(TARGET llhttp PROPERTY C_STANDARD 99)

target_include_directories(llhttp PRIVATE
    llhttp/include
)

# https://cmake.org/cmake/help/latest/command/target_link_libraries.html#libraries-for-both-a-target-and-its-dependents
# Library dependencies are transitive by default with this signature. 
# When this target is linked into another target then the libraries linked to
# this target will appear on the link line for the other target too.
# target_link_libraries(lua_flatbuffers flatbuffers)

# lua 5.5.1
# 源码直接放在deps/lua由cmake编译，不再依赖vcpkg或者系统安装的lua(官方源码包见deps/README.md)
# windows编成lua.dll：msvc的exe默认不导出符号，server/c_module下的外部lua c模块需要靠它解析
# linux编成静态库：master链接时加了-Wl,-E导出符号，外部so模块在加载时一样能解析到lua_*
set(LUA_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/lua/src)
file(GLOB LUA_SRC_LIST "${LUA_SRC_DIR}/*.c")
# lua.c是解释器的main，luac.c是编译器的main，都不能编进库
list(REMOVE_ITEM LUA_SRC_LIST "${LUA_SRC_DIR}/lua.c" "${LUA_SRC_DIR}/luac.c")

if(WINDOWS)
    add_library(lua SHARED ${LUA_SRC_LIST})
    # 官方luaconf.h只有定义了LUA_BUILD_AS_DLL才会dllexport，而core的.c自带LUA_CORE、
    # lib的.c自带LUA_LIB，所以这里只需要补LUA_BUILD_AS_DLL
    # (vcpkg的lua端口是把这个判断改成了#if 1，所以它编出来的一定是dll且无法改)
    target_compile_definitions(lua PRIVATE LUA_BUILD_AS_DLL)
    # dll要输出到server/var，install(CODE)才会把它拷到server/bin
    set_target_properties(lua PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
        ARCHIVE_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
    )
else()
    add_library(lua STATIC ${LUA_SRC_LIST})
    # LUA_USE_LINUX提供io.popen等posix扩展，官方make PLAT=linux加的就是这个宏
    target_compile_definitions(lua PUBLIC LUA_USE_LINUX)
    # dlopen在loadlib.c里用，math在lmathlib.c里用
    target_link_libraries(lua PUBLIC ${CMAKE_DL_LIBS} m)
endif()

# 第三方代码，清掉项目自己的编译参数(linux下-Wconversion -Wextra -pedantic会刷屏)
set_target_properties(lua PROPERTIES COMPILE_OPTIONS "")
# 头文件目录用PUBLIC，链接lua的target(lua_parson、cjson、master)自动继承
target_include_directories(lua PUBLIC ${LUA_SRC_DIR})

# lua解释器，install到server/bin
add_executable(lua_interp ${LUA_SRC_DIR}/lua.c)
set_target_properties(lua_interp PROPERTIES OUTPUT_NAME lua COMPILE_OPTIONS "")
target_link_libraries(lua_interp PRIVATE lua)
install(TARGETS lua_interp DESTINATION "${CMAKE_INSTALL_PREFIX}")
# lua.dll不需要单独install，它在server/var里，会被install(CODE)的*.dll一起拷到server/bin

#lua_parson
add_library(lua_parson STATIC
    lua_parson/lparson.c
    lua_parson/parson/parson.c
)
target_include_directories(lua_parson PRIVATE
    lua_parson/parson
)
target_link_libraries(lua_parson PRIVATE lua)

# lua_rapidxml
add_library(lua_rapidxml STATIC lua_rapidxml/lrapidxml.cpp)
target_include_directories(lua_rapidxml PRIVATE
    lua_rapidxml/rapidxml
)
target_link_libraries(lua_rapidxml PRIVATE lua)

# lua-cjson
# 编成lua的c模块，install到server/c_module。它自带的CMakeLists.txt没有被使用
# (那个是上游的，里面是find_package(Lua)那一套)，这里和其它deps一样直接编
add_library(cjson MODULE
    lua-cjson/lua_cjson.c
    lua-cjson/strbuf.c
    lua-cjson/fpconv.c
)
# 模块文件必须是cjson.dll / cjson.so，不能带lib前缀
set_target_properties(cjson PROPERTIES PREFIX "")
set_target_properties(cjson PROPERTIES COMPILE_OPTIONS "")
target_include_directories(cjson PRIVATE ${LUA_SRC_DIR})
if(WINDOWS)
    # windows不允许模块有未定义的符号，必须链接lua.dll
    # linux靠master的-Wl,-E在dlopen时解析，不需要链接
    target_link_libraries(cjson PRIVATE lua)
    target_compile_definitions(cjson PRIVATE
        _CRT_SECURE_NO_WARNINGS
        # windows的sprintf/strtod对nan、inf的处理不一样，上游的Makefile也是关掉的
        DISABLE_INVALID_NUMBERS
    )
endif()
# 构建产物放server/var/c_module：既不与源码混在一起，也不会被install(CODE)的*.dll拷进server/bin
set_target_properties(cjson PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}/c_module"
    RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}/c_module"
    ARCHIVE_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}/c_module"
)
install(TARGETS cjson
    LIBRARY DESTINATION "${C_MODULE_PATH}"
    RUNTIME DESTINATION "${C_MODULE_PATH}"
)

# websocket-parser
add_library(websocket_parser STATIC
    websocket-parser/websocket_parser.c
)

# kcp —— 直接在这里编译，不要 add_subdirectory(kcp)
# deps/kcp/CMakeLists.txt 里是 cmake_minimum_required(VERSION 4.0)，
# 而deps是3.5，add_subdirectory进去会直接fatal error
if(ENABLE_KCP)
    add_library(kcp STATIC kcp/ikcp.c)
    # 清空编译参数：linux下 -Wall/-pedantic 会让第三方代码刷屏
    # （注意 -Wextra/-Wconversion 是 add_subdirectory 之后才加的，这里必须显式清）
    set_target_properties(kcp PROPERTIES COMPILE_OPTIONS "")
    target_include_directories(kcp PUBLIC kcp)
endif()
