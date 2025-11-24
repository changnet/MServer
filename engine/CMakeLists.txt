cmake_minimum_required(VERSION 3.5)

# 检测是否有vcpkg环境
# https://vcpkg.readthedocs.io/en/latest/users/integration/
# The CMAKE_TOOLCHAIN_FILE setting simply must be set before the project() directive is first called
# 在project之前 CMAKE_SYSTEM_NAME 是无效的
if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    set(WINDOWS TRUE)
    set(VCPKG_PATH "vcpkg_win_x64")
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
    set(LINUX TRUE)
    set(VCPKG_PATH "vcpkg_linux_x64")
endif()

# EXISTS需要绝对路径
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${VCPKG_PATH})
    message(STATUS "using vcpkg root ${VCPKG_PATH}")
    set(CMAKE_TOOLCHAIN_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/${VCPKG_PATH}/vcpkg-export-20250514-100442/scripts/buildsystems/vcpkg.cmake
        CACHE STRING "Vcpkg toolchain file")
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
endif()


project(MServer)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Debug )
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# win和linux下编译选项详见：https://github.com/Samsung/netcoredbg/blob/master/compileoptions.cmake
if(WINDOWS)
    # 优先查找.lib类型的静态库
    # set(CMAKE_FIND_LIBRARY_SUFFIXES .lib ${CMAKE_FIND_LIBRARY_SUFFIXES})

    # 以下有些参数是默认的，不需要重复加，通过编译日志可以看出有哪些参数
    #https://docs.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library?view=msvc-160
    # use the multithread, static (debug) version of the run-time library LIBCMTD.lib
    # add_compile_options($<$<OR:$<CONFIG:Release>,$<CONFIG:Relwithdebinfo>>:/MT>)
    # add_compile_options($<$<OR:$<CONFIG:Debug>,$<CONFIG:Checked>>:/MTd>)

    # add_compile_options(/W3) # set warning level to 3
    # add_compile_options(/WX) # treat warnings as errors
    add_compile_options(/Zc:forScope) # C++ language conformance: enforce Standard C++ for scoping rules
    add_compile_options(/source-charset:utf-8) # Force MSVC to compile source as UTF-8.
    add_compile_options($<$<OR:$<CONFIG:Debug>,$<CONFIG:Relwithdebinfo>>:/Zi>) # enable debugging information

    # https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
    add_compile_options(/Zc:__cplusplus) # 使用新的__cplusplus值，否则在win下这个值一直是199711L

    # 优化选项
    # https://docs.microsoft.com/en-us/cpp/build/reference/o1-o2-minimize-size-maximize-speed?view=msvc-160
    add_compile_options($<$<OR:$<CONFIG:Release>,$<CONFIG:Relwithdebinfo>>:/O2>)
    add_compile_options($<$<OR:$<OR:$<CONFIG:Release>,$<CONFIG:Relwithdebinfo>>,$<CONFIG:Checked>>:/O1>)

    # VS 2019以后支持并且需要在bat脚本设置环境变量：set ASAN_OPTIONS=detect_leaks=1
    # 如果从VS以debug模式拉起，则无问题。如果从cmd直接运行exe，则会缺少dll并且没有任何提示
    # 需要从目录C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.37.32822\bin\Hostx64\x64
    # 拷贝clang_rt.asan_dbg_dynamic-x86_64.dll and clang_rt.asan_dynamic-x86_64.dll到bin目录
    # add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
elseif(LINUX)
    # 优先查找.a类型的静态库
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a .so)
    add_compile_options(-pipe -Wall -pedantic)
    # -fsanitize=address -static-libasan -g
    # 这个选项会导致linux下占用20G VIRT内存 https://github.com/google/sanitizers/issues/704
    add_compile_options($<$<CONFIG:DEBUG>:-fsanitize=address>)
    add_link_options($<$<CONFIG:DEBUG>:-fsanitize=address>)
endif()

message(STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_FIND_LIBRARY_SUFFIXES = ${CMAKE_FIND_LIBRARY_SUFFIXES}")
message(STATUS "CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "CMAKE_C_FLAGS = ${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
# message("   CMAKE_C_FLAGS_DEBUG = ${CMAKE_C_FLAGS_DEBUG}")
# message("   CMAKE_C_FLAGS_RELEASE = ${CMAKE_C_FLAGS_RELEASE}")
# message("   CMAKE_C_FLAGS_RELWITHDEBINFO = ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
# message("   CMAKE_C_FLAGS_MINSIZEREL = ${CMAKE_C_FLAGS_MINSIZEREL}")
# message("   CMAKE_CXX_FLAGS_DEBUG = ${CMAKE_CXX_FLAGS_DEBUG}")
# message("   CMAKE_CXX_FLAGS_RELEASE = ${CMAKE_CXX_FLAGS_RELEASE}")
# message("   CMAKE_CXX_FLAGS_RELWITHDEBINFO = ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
# message("   CMAKE_CXX_FLAGS_MINSIZEREL = ${CMAKE_CXX_FLAGS_MINSIZEREL}")

# 设置的参数能被子目录继承，子目录可以选择覆盖
set(SRC_ROOT_PATH ".")
set(DEPS_PATH "${SRC_ROOT_PATH}/deps")
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_LIST_DIR}/../server/var/libs")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_LIST_DIR}/../server/var")
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../server/bin")

string(LENGTH "${CMAKE_SOURCE_DIR}/" SRC_PATH_SIZE)
add_definitions("-DSRC_PATH_SIZE=${SRC_PATH_SIZE}")

if(WINDOWS)
    find_package(unofficial-libmariadb CONFIG REQUIRED)
elseif(LINUX)
    # 通过apt安装暂时没有通过find_package查找mariadb的方式
    # find_package(libmariadb REQUIRED)
endif()

find_package(OpenSSL REQUIRED)
find_package(Lua 5.4 REQUIRED)
find_package(mongoc-1.0 CONFIG REQUIRED)

add_subdirectory(deps)

# master

# 自己写的代码，额外加一些检测。第三方库不需要这么严格的检测
if(LINUX)
	add_compile_options(-Wextra -Wconversion -Wformat=1 -Wno-unused-parameter -Wstrict-aliasing)
	# 把库的符号包含到生成的exe中，lua直接require so库时才可以找到符号
	# 新版本用 target_link_options
	# -E等同于 --export-dynamic 或者 -rdynamic
	set(CMAKE_EXE_LINKER_FLAGS "-Wl,-E")
endif()

# deps库的目录(cmake 3.13后请使用 target_link_directories)
link_directories(
    ${LIBRARY_OUTPUT_PATH}
)
file(GLOB_RECURSE SRC_LIST "${SRC_ROOT_PATH}/src/*.cpp")
add_executable(master ${SRC_LIST})

# 这个文件里使用_TIMESTAMP_这个宏来标记编译时间，所以每次都需要更新一下才会重新编译
# add_custom_command可以更新时间戳，但没法保证在构建master前执行
add_custom_target(update_build_time
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_LIST_DIR}/src/system/env.cpp
    COMMENT "update build timestamp"
)
add_dependencies(master update_build_time)

target_include_directories(master PRIVATE
    ${SRC_ROOT_PATH}/src # 本身的源码目录添加为include，这样不同目录的文件相互引用时，不需要使用../global这种路径
    ${DEPS_PATH}/pbc
    ${DEPS_PATH}/llhttp/include
    ${DEPS_PATH}/lua_parson
    ${DEPS_PATH}/lua_rapidxml
    ${DEPS_PATH}/aho-corasick
    ${DEPS_PATH}/websocket-parser
)

# 库文件，必须 在add_executable之后
# 默认情况下，加.a后缀的是static链接，其他是dynamic链接
# 静态链接是为了方便部署，运维安装完系统后，不用额外安装的库静态链接，如 pthread、rt
# 其他的需要静态链接
target_link_libraries(master PRIVATE
    # 需要编译的库，可以写成acism或者acism.a。前者与上面的add_library同名，cmake会
    # 自动解决依赖关系，后者表示静态链接，但不会产生依赖关系
    pbc
    acism
    llhttp
    lua_parson
    lua_rapidxml
    websocket_parser
)

if(WINDOWS)
    target_include_directories(master PRIVATE
        # "C:\\Program Files (x86)\\Visual Leak Detector\\include"
    )
    target_link_libraries(master PRIVATE
        Rpcrt4 # uuidCreate
        Dbghelp # MiniDumpWriteDump
        ${LUA_LIBRARIES}
        OpenSSL::SSL OpenSSL::Crypto
        unofficial::libmariadb
        mongo::mongoc_shared
        # "C:\\Program Files (x86)\\Visual Leak Detector\\lib\\Win64\\vld.lib"
    )
elseif(UNIX)
    # 用system指定这些目录为系统目录，去除bson中的一些编译警告
    target_include_directories(master SYSTEM PRIVATE
        /usr/include/mariadb
    )
    target_link_libraries(master PRIVATE
        lua.a
        uuid.a
        mariadbclient.a
        mongo::mongoc_static

        # sasl2.a # mongodb使用LDAP认证才需要sasl2，暂时不使用

        ssl crypto dl rt pthread resolv z stdc++fs
    )
endif()

install(TARGETS master DESTINATION "${CMAKE_INSTALL_PREFIX}")

# 使用GLOB只是cmake运行时生成列表，那时dll未生成，会无效
# 其他方式，比如直接install RUNTIME或者TARGET_RUNTIME_DLLS只能安装cmake中依赖的dll
# 所以这里直接用code来执行安装
install(CODE "
    file(GLOB INSTALL_FILES
        \"${EXECUTABLE_OUTPUT_PATH}/*.dll\"
        \"${EXECUTABLE_OUTPUT_PATH}/*.pdb\"
    )

    file(MAKE_DIRECTORY \"${CMAKE_INSTALL_PREFIX}\")

    # 这里不能用install，因为install只在cmake执行时生成
    foreach(INSTALL_FILE \${INSTALL_FILES})
        get_filename_component(FILENAME \${INSTALL_FILE} NAME)
        file(COPY \${INSTALL_FILE} DESTINATION \"${CMAKE_INSTALL_PREFIX}\")
        message(STATUS \"Installed: \${FILENAME}\")
    endforeach()
")
