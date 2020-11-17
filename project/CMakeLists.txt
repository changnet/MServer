cmake_minimum_required(VERSION 3.5)

project(master)

# 设置的参数能被子目录继承，子目录可以选择覆盖
set(LIBRARY_OUTPUT_PATH "../server/bin/.build")
set(EXECUTABLE_OUTPUT_PATH "../")

# 头文件目录
include_directories(
    ../engine/deps/pbc
    ../engine/deps/lua_parson
    ../engine/deps/lua_rapidxml
    ../engine/deps/http-parser
    ../engine/deps/aho-corasick
    ../engine/deps/lua_flatbuffers
    ../engine/deps/lua_bson
    ../engine/deps/websocket-parser

    /usr/local/include/libmongoc-1.0
    /usr/local/include/libbson-1.0
    /usr/include/mysql
)

# 库文件目录
link_directories(
    ${LIBRARY_OUTPUT_PATH}
)

set(CAMKE_C_FLAGS "-O2 -g")
set(CMAKE_CXX_FLAGS "-O2 -g -std=c++11")

file(GLOB_RECURSE SRC_LIST "../engine/src/*.cpp")

add_executable(master ${SRC_LIST})

# 库文件，必须 在add_executable之后
# 默认情况下，加.a后缀的是static链接，其他是dynamic链接
# 静态链接是为了方便部署，运维安装完系统后，不用额外安装的库静态链接，如 pthread、rt
# 其他的需要静态链接
target_link_libraries(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/pbc/build/libpbc.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/lua_parson/liblua_parson.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/lua_rapidxml/liblua_rapidxml.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/http-parser/libhttp_parser.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/aho-corasick/libacism.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/lua_flatbuffers/liblua_flatbuffers.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/lua_bson/liblua_bson.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/deps/websocket-parser/libwebsocket_parser.a

    flatbuffers.a
    uuid.a
    mariadbclient.a
    lua.a
    mongoc-static-1.0.a bson-static-1.0.a sasl2.a
    ssl
    crypto
    dl rt pthread resolv z
)
