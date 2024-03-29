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

#lua_parson
add_library(lua_parson STATIC
    lua_parson/lparson.c
    lua_parson/parson/parson.c
)
target_include_directories(lua_parson PRIVATE
    lua_parson/parson
    ${LUA_INCLUDE_DIR}
)

# lua_rapidxml
add_library(lua_rapidxml STATIC lua_rapidxml/lrapidxml.cpp)
target_include_directories(lua_rapidxml PRIVATE
    lua_rapidxml/rapidxml
    ${LUA_INCLUDE_DIR}
)

# websocket-parser
add_library(websocket_parser STATIC
    websocket-parser/websocket_parser.c
)

# aho-corasick
add_library(acism STATIC
    aho-corasick/acism.c
    aho-corasick/msutil.c
    aho-corasick/acism_create.c
    aho-corasick/acism_dump.c
    aho-corasick/acism_file.c
)
if(WINDOWS)
    target_compile_definitions(acism PRIVATE _CRT_SECURE_NO_WARNINGS )
    target_compile_options(acism PRIVATE /wd4013 /wd4477 /wd4273 /wd4244 /wd4267)
endif()
