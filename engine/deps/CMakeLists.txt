cmake_minimum_required(VERSION 3.5)

# pbc
file(GLOB SRC_LIST "pbc/src/*.c")
add_library(pbc STATIC ${SRC_LIST})
target_include_directories(pbc PRIVATE
    pbc
)
# http-parser
add_library(http_parser STATIC http-parser/http_parser.c)

# lua_bson
find_package(libbson-1.0 CONFIG REQUIRED)
add_library(lua_bson STATIC lua_bson/lbson.c)
target_include_directories(lua_bson PRIVATE
    ${BSON_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIR}
)
# target_link_libraries(lua_bson bson-static-1.0)

# lua_flatbuffers
file(GLOB SRC_LIST "lua_flatbuffers/*.cpp")
list(REMOVE_ITEM SRC_LIST "${CMAKE_CURRENT_LIST_DIR}/lua_flatbuffers/cpp_test.cpp")
add_library(lua_flatbuffers STATIC ${SRC_LIST})
target_include_directories(lua_flatbuffers PRIVATE
    lua_parson/parson
    ${LUA_INCLUDE_DIR}
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
