cmake_minimum_required(VERSION 3.5)

project(MServer)

# 设置编译参数
# -pipe 使用pipe而不是临时文件，对代码无影响
# -Wall This enables all the warnings
# -g gdb debug option
# -s:直接生成与运用strip同样效果的可执行文件（删除了所有符号信息）
# -Wl,rpath 指定运行时库文件(so)路径，便携程序可用
# -w 关闭所有告警
# -rdynamic 却是一个 连接选项 ，它将指示连接器把所有符号（而不仅仅只是程序已使用到的外
#    部符号）都添加到动态符号表（即.dynsym表）里，以便那些通过 dlopen() 或 backtrace()
#    (这一系列函数使用.dynsym表内符号)这样的函数使用。
# 如果不知道mongo库文件位置，可用echo $(pkg-config --cflags --libs libmongoc-1.0)查看
# mysql库根据安装方法不同，位置不本样，可用mysql_config查看
# (新版本-lmysqlclient_r与-lmysqlclient是一样的)
# -std=C++98 -std=c++03 -std=c++0x(c++ tr1) -std=c++11 -std=C++14

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(LIB_PREFIX "D:/cygwin64/")
    message("Using CYGWIN LIBRARY ${LIB_PREFIX}")
endif()

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Debug )
endif()
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "-O0 -g -pipe -Wall -pedantic")
    set(CMAKE_CXX_FLAGS "-O0 -g -pipe -Wall -pedantic -std=c++11")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_FLAGS "-O2 -g -pipe -Wall -DNDEBUG -pedantic")
    set(CMAKE_CXX_FLAGS "-O2 -g -pipe -Wall -pedantic -DNDEBUG -std=c++11")
else()
    message(FATAL_ERROR "Unknow CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
endif()

message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
message("   CMAKE_C_FLAGS = ${CMAKE_C_FLAGS}")
message("   CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
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
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_LIST_DIR}/../server/bin/.libs")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_LIST_DIR}/../server/bin")

# pbc
file(GLOB SRC_LIST "${DEPS_PATH}/pbc/src/*.c")
add_library(pbc STATIC ${SRC_LIST})
target_include_directories(pbc PRIVATE
    ${DEPS_PATH}/pbc
)
# http-parser
add_library(http_parser STATIC ${DEPS_PATH}/http-parser/http_parser.c)

# lua_bson
add_library(lua_bson STATIC ${DEPS_PATH}/lua_bson/lbson.c)
target_include_directories(lua_bson PRIVATE
    /usr/local/include/libbson-1.0
)
# target_link_libraries(lua_bson bson-static-1.0)

# lua_flatbuffers
file(GLOB SRC_LIST "${DEPS_PATH}/lua_flatbuffers/*.cpp")
list(REMOVE_ITEM SRC_LIST "${CMAKE_CURRENT_LIST_DIR}/${DEPS_PATH}/lua_flatbuffers/cpp_test.cpp")
add_library(lua_flatbuffers STATIC ${SRC_LIST})

# https://cmake.org/cmake/help/latest/command/target_link_libraries.html#libraries-for-both-a-target-and-its-dependents
# Library dependencies are transitive by default with this signature. 
# When this target is linked into another target then the libraries linked to
# this target will appear on the link line for the other target too.
# target_link_libraries(lua_flatbuffers flatbuffers)

#lua_parson
add_library(lua_parson STATIC
    ${DEPS_PATH}/lua_parson/lparson.c
    ${DEPS_PATH}/lua_parson/parson/parson.c
)
target_include_directories(lua_parson PRIVATE
    ${DEPS_PATH}/lua_parson/parson
)

# lua_rapidxml
add_library(lua_rapidxml STATIC ${DEPS_PATH}/lua_rapidxml/lrapidxml.cpp)
target_include_directories(lua_rapidxml PRIVATE
    ${DEPS_PATH}/lua_rapidxml/rapidxml
)

# websocket-parser
add_library(websocket_parser STATIC
    ${DEPS_PATH}/websocket-parser/websocket_parser.c
)

# aho-corasick
add_library(acism STATIC
    ${DEPS_PATH}/aho-corasick/acism.c
    ${DEPS_PATH}/aho-corasick/msutil.c
    ${DEPS_PATH}/aho-corasick/acism_create.c
    ${DEPS_PATH}/aho-corasick/acism_dump.c
    ${DEPS_PATH}/aho-corasick/acism_file.c
)


# master

# 把库的符号包含到生成的exe中，lua直接require so库时才可以找到符号
# 新版本用 target_link_options
# -E等同于 --export-dynamic 或者 -rdynamic
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-E")

# deps库的目录(cmake 3.13后请使用 target_link_directories)
link_directories(
    ${LIBRARY_OUTPUT_PATH}
)
file(GLOB_RECURSE SRC_LIST "${SRC_ROOT_PATH}/src/*.cpp")
add_executable(master ${SRC_LIST})

target_include_directories(master PRIVATE
    ${DEPS_PATH}/pbc
    ${DEPS_PATH}/lua_parson
    ${DEPS_PATH}/lua_rapidxml
    ${DEPS_PATH}/http-parser
    ${DEPS_PATH}/aho-corasick
    ${DEPS_PATH}/lua_flatbuffers
    ${DEPS_PATH}/lua_bson
    ${DEPS_PATH}/websocket-parser

    ${LIB_PREFIX}/usr/include
    ${LIB_PREFIX}/usr/local/include
    ${LIB_PREFIX}/usr/include/mysql
    ${LIB_PREFIX}/usr/local/include/libmongoc-1.0
    ${LIB_PREFIX}/usr/local/include/libbson-1.0
)

# 库文件，必须 在add_executable之后
# 默认情况下，加.a后缀的是static链接，其他是dynamic链接
# 静态链接是为了方便部署，运维安装完系统后，不用额外安装的库静态链接，如 pthread、rt
# 其他的需要静态链接
target_link_libraries(master
    # 需要编译的库，可以写成acism或者acism.a。前者与上面的add_library同名，cmake会
    # 自动解决依赖关系，后者表示静态链接，但不会产生依赖关系
    pbc
    acism
    lua_parson
    lua_rapidxml
    http_parser
    lua_flatbuffers
    lua_bson
    websocket_parser

    lua.a
    uuid.a
    flatbuffers.a
    mariadbclient.a
    mongoc-static-1.0.a
    bson-static-1.0.a

    # sasl2.a # mongodb使用LDAP认证才需要sasl2，暂时不使用

    # mariadb使用了gnutls
    ssl gnutls crypto dl rt pthread resolv z
)
