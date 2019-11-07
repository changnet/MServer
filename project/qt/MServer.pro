TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    ../../master/cpp_src/ev/ev.cpp \
    ../../master/cpp_src/ev/ev_watcher.cpp \
    ../../master/cpp_src/global/global.cpp \
    ../../master/cpp_src/log/log.cpp \
    ../../master/cpp_src/lua_cpplib/lacism.cpp \
    ../../master/cpp_src/lua_cpplib/laoi.cpp \
    ../../master/cpp_src/lua_cpplib/lastar.cpp \
    ../../master/cpp_src/lua_cpplib/lev.cpp \
    ../../master/cpp_src/lua_cpplib/llog.cpp \
    ../../master/cpp_src/lua_cpplib/lmap.cpp \
    ../../master/cpp_src/lua_cpplib/lmongo.cpp \
    ../../master/cpp_src/lua_cpplib/lnetwork_mgr.cpp \
    ../../master/cpp_src/lua_cpplib/lrank.cpp \
    ../../master/cpp_src/lua_cpplib/lsql.cpp \
    ../../master/cpp_src/lua_cpplib/lstate.cpp \
    ../../master/cpp_src/lua_cpplib/lstatistic.cpp \
    ../../master/cpp_src/lua_cpplib/ltimer.cpp \
    ../../master/cpp_src/lua_cpplib/lutil.cpp \
    ../../master/cpp_src/mongo/mongo.cpp \
    ../../master/cpp_src/mysql/sql.cpp \
    ../../master/cpp_src/net/codec/bson_codec.cpp \
    ../../master/cpp_src/net/codec/codec_mgr.cpp \
    ../../master/cpp_src/net/codec/flatbuffers_codec.cpp \
    ../../master/cpp_src/net/codec/protobuf_codec.cpp \
    ../../master/cpp_src/net/io/io.cpp \
    ../../master/cpp_src/net/io/ssl_io.cpp \
    ../../master/cpp_src/net/io/ssl_mgr.cpp \
    ../../master/cpp_src/net/packet/http_packet.cpp \
    ../../master/cpp_src/net/packet/stream_packet.cpp \
    ../../master/cpp_src/net/packet/websocket_packet.cpp \
    ../../master/cpp_src/net/packet/ws_stream_packet.cpp \
    ../../master/cpp_src/net/buffer.cpp \
    ../../master/cpp_src/net/socket.cpp \
    ../../master/cpp_src/scene/a_star.cpp \
    ../../master/cpp_src/scene/grid_aoi.cpp \
    ../../master/cpp_src/scene/grid_map.cpp \
    ../../master/cpp_src/system/static_global.cpp \
    ../../master/cpp_src/system/statistic.cpp \
    ../../master/cpp_src/thread/thread.cpp \
    ../../master/cpp_src/thread/thread_mgr.cpp \
    ../../master/cpp_src/util/rank.cpp \
    ../../master/cpp_src/main.cpp \
    ../../master/cpp_src/log/async_log.cpp \
    ../../master/cpp_src/global/dbg_mem.cpp

HEADERS += \
    ../../master/cpp_src/ev/ev.h \
    ../../master/cpp_src/ev/ev_watcher.h \
    ../../master/cpp_src/global/assert.h \
    ../../master/cpp_src/global/global.h \
    ../../master/cpp_src/global/map.h \
    ../../master/cpp_src/global/types.h \
    ../../master/cpp_src/log/log.h \
    ../../master/cpp_src/lua_cpplib/lacism.h \
    ../../master/cpp_src/lua_cpplib/laoi.h \
    ../../master/cpp_src/lua_cpplib/lastar.h \
    ../../master/cpp_src/lua_cpplib/lclass.h \
    ../../master/cpp_src/lua_cpplib/lev.h \
    ../../master/cpp_src/lua_cpplib/llog.h \
    ../../master/cpp_src/lua_cpplib/lmap.h \
    ../../master/cpp_src/lua_cpplib/lmongo.h \
    ../../master/cpp_src/lua_cpplib/lnetwork_mgr.h \
    ../../master/cpp_src/lua_cpplib/lrank.h \
    ../../master/cpp_src/lua_cpplib/lsql.h \
    ../../master/cpp_src/lua_cpplib/lstate.h \
    ../../master/cpp_src/lua_cpplib/lstatistic.h \
    ../../master/cpp_src/lua_cpplib/ltimer.h \
    ../../master/cpp_src/lua_cpplib/ltools.h \
    ../../master/cpp_src/lua_cpplib/lutil.h \
    ../../master/cpp_src/mongo/mongo.h \
    ../../master/cpp_src/mysql/sql.h \
    ../../master/cpp_src/mysql/sql_def.h \
    ../../master/cpp_src/net/codec/bson_codec.h \
    ../../master/cpp_src/net/codec/codec.h \
    ../../master/cpp_src/net/codec/codec_mgr.h \
    ../../master/cpp_src/net/codec/flatbuffers_codec.h \
    ../../master/cpp_src/net/codec/protobuf_codec.h \
    ../../master/cpp_src/net/io/io.h \
    ../../master/cpp_src/net/io/ssl_io.h \
    ../../master/cpp_src/net/io/ssl_mgr.h \
    ../../master/cpp_src/net/packet/http_packet.h \
    ../../master/cpp_src/net/packet/packet.h \
    ../../master/cpp_src/net/packet/stream_packet.h \
    ../../master/cpp_src/net/packet/websocket_packet.h \
    ../../master/cpp_src/net/packet/ws_stream_packet.h \
    ../../master/cpp_src/net/buffer.h \
    ../../master/cpp_src/net/net_header.h \
    ../../master/cpp_src/net/socket.h \
    ../../master/cpp_src/pool/object_pool.h \
    ../../master/cpp_src/pool/ordered_pool.h \
    ../../master/cpp_src/pool/pool.h \
    ../../master/cpp_src/scene/a_star.h \
    ../../master/cpp_src/scene/grid_aoi.h \
    ../../master/cpp_src/scene/grid_map.h \
    ../../master/cpp_src/scene/scene_include.h \
    ../../master/cpp_src/system/static_global.h \
    ../../master/cpp_src/system/statistic.h \
    ../../master/cpp_src/thread/auto_mutex.h \
    ../../master/cpp_src/thread/thread.h \
    ../../master/cpp_src/thread/thread_mgr.h \
    ../../master/cpp_src/util/rank.h \
    ../../master/cpp_src/config.h \
    ../../master/cpp_src/log/async_log.h \
    ../../master/cpp_src/global/dbg_mem.h


unix:!macx: LIBS += -L$$PWD/../../master/cpp_src/deps/aho-corasick/ -lacism

INCLUDEPATH += $$PWD/../../master/cpp_src/deps/aho-corasick
DEPENDPATH += $$PWD/../../master/cpp_src/deps/aho-corasick

unix:!macx: PRE_TARGETDEPS += $$PWD/../../master/cpp_src/deps/aho-corasick/libacism.a

unix:!macx: LIBS += -L$$PWD/../../master/cpp_src/deps/http-parser/ -lhttp_parser

INCLUDEPATH += $$PWD/../../master/cpp_src/deps/http-parser
DEPENDPATH += $$PWD/../../master/cpp_src/deps/http-parser

unix:!macx: PRE_TARGETDEPS += $$PWD/../../master/cpp_src/deps/http-parser/libhttp_parser.a

unix:!macx: LIBS += -L$$PWD/../../master/cpp_src/deps/lua_bson/ -llua_bson

INCLUDEPATH += $$PWD/../../master/cpp_src/deps/lua_bson
DEPENDPATH += $$PWD/../../master/cpp_src/deps/lua_bson

unix:!macx: PRE_TARGETDEPS += $$PWD/../../master/cpp_src/deps/lua_bson/liblua_bson.a

unix:!macx: LIBS += -L$$PWD/../../master/cpp_src/deps/lua_flatbuffers/ -llua_flatbuffers

INCLUDEPATH += $$PWD/../../master/cpp_src/deps/lua_flatbuffers
DEPENDPATH += $$PWD/../../master/cpp_src/deps/lua_flatbuffers

unix:!macx: PRE_TARGETDEPS += $$PWD/../../master/cpp_src/deps/lua_flatbuffers/liblua_flatbuffers.a

unix:!macx: LIBS += -L$$PWD/../../master/cpp_src/deps/lua_parson/ -llua_parson

INCLUDEPATH += $$PWD/../../master/cpp_src/deps/lua_parson
DEPENDPATH += $$PWD/../../master/cpp_src/deps/lua_parson

unix:!macx: PRE_TARGETDEPS += $$PWD/../../master/cpp_src/deps/lua_parson/liblua_parson.a

unix:!macx: LIBS += -L$$PWD/../../master/cpp_src/deps/lua_rapidxml/ -llua_rapidxml

INCLUDEPATH += $$PWD/../../master/cpp_src/deps/lua_rapidxml
DEPENDPATH += $$PWD/../../master/cpp_src/deps/lua_rapidxml

unix:!macx: PRE_TARGETDEPS += $$PWD/../../master/cpp_src/deps/lua_rapidxml/liblua_rapidxml.a

unix:!macx: LIBS += -L$$PWD/../../master/cpp_src/deps/pbc/build/ -lpbc

INCLUDEPATH += $$PWD/../../master/cpp_src/deps/pbc
DEPENDPATH += $$PWD/../../master/cpp_src/deps/pbc

unix:!macx: LIBS += -L$$PWD/../../master/cpp_src/deps/websocket-parser/ -lwebsocket_parser

INCLUDEPATH += $$PWD/../../master/cpp_src/deps/websocket-parser
DEPENDPATH += $$PWD/../../master/cpp_src/deps/websocket-parser

unix:!macx: PRE_TARGETDEPS += $$PWD/../../master/cpp_src/deps/websocket-parser/libwebsocket_parser.a
