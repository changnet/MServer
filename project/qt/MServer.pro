CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += /usr/lib/gcc/x86_64-linux-gnu/9/include
INCLUDEPATH += ../../engine/include

SOURCES += \
    ../../engine/src/ev/ev.cpp \
    ../../engine/src/ev/ev_watcher.cpp \
    ../../engine/src/global/global.cpp \
    ../../engine/src/log/log.cpp \
    ../../engine/src/lua_cpplib/lacism.cpp \
    ../../engine/src/lua_cpplib/laoi.cpp \
    ../../engine/src/lua_cpplib/lastar.cpp \
    ../../engine/src/lua_cpplib/lev.cpp \
    ../../engine/src/lua_cpplib/llog.cpp \
    ../../engine/src/lua_cpplib/lmap.cpp \
    ../../engine/src/lua_cpplib/lmongo.cpp \
    ../../engine/src/lua_cpplib/lnetwork_mgr.cpp \
    ../../engine/src/lua_cpplib/lrank.cpp \
    ../../engine/src/lua_cpplib/lsql.cpp \
    ../../engine/src/lua_cpplib/lstate.cpp \
    ../../engine/src/lua_cpplib/lstatistic.cpp \
    ../../engine/src/lua_cpplib/ltimer.cpp \
    ../../engine/src/lua_cpplib/lutil.cpp \
    ../../engine/src/mongo/mongo.cpp \
    ../../engine/src/mysql/sql.cpp \
    ../../engine/src/net/codec/bson_codec.cpp \
    ../../engine/src/net/codec/codec_mgr.cpp \
    ../../engine/src/net/codec/flatbuffers_codec.cpp \
    ../../engine/src/net/codec/protobuf_codec.cpp \
    ../../engine/src/net/io/io.cpp \
    ../../engine/src/net/io/ssl_io.cpp \
    ../../engine/src/net/io/ssl_mgr.cpp \
    ../../engine/src/net/packet/http_packet.cpp \
    ../../engine/src/net/packet/stream_packet.cpp \
    ../../engine/src/net/packet/websocket_packet.cpp \
    ../../engine/src/net/packet/ws_stream_packet.cpp \
    ../../engine/src/net/buffer.cpp \
    ../../engine/src/net/socket.cpp \
    ../../engine/src/scene/a_star.cpp \
    ../../engine/src/scene/grid_aoi.cpp \
    ../../engine/src/scene/grid_map.cpp \
    ../../engine/src/system/static_global.cpp \
    ../../engine/src/system/statistic.cpp \
    ../../engine/src/thread/thread.cpp \
    ../../engine/src/thread/thread_mgr.cpp \
    ../../engine/src/util/rank.cpp \
    ../../engine/src/main.cpp \
    ../../engine/src/log/async_log.cpp \
    ../../engine/src/global/dbg_mem.cpp

HEADERS += \
    ../../engine/src/ev/ev.h \
    ../../engine/src/ev/ev_watcher.h \
    ../../engine/src/global/assert.h \
    ../../engine/src/global/global.h \
    ../../engine/src/global/map.h \
    ../../engine/src/global/types.h \
    ../../engine/src/log/log.h \
    ../../engine/src/lua_cpplib/lacism.h \
    ../../engine/src/lua_cpplib/laoi.h \
    ../../engine/src/lua_cpplib/lastar.h \
    ../../engine/src/lua_cpplib/lclass.h \
    ../../engine/src/lua_cpplib/lev.h \
    ../../engine/src/lua_cpplib/llog.h \
    ../../engine/src/lua_cpplib/lmap.h \
    ../../engine/src/lua_cpplib/lmongo.h \
    ../../engine/src/lua_cpplib/lnetwork_mgr.h \
    ../../engine/src/lua_cpplib/lrank.h \
    ../../engine/src/lua_cpplib/lsql.h \
    ../../engine/src/lua_cpplib/lstate.h \
    ../../engine/src/lua_cpplib/lstatistic.h \
    ../../engine/src/lua_cpplib/ltimer.h \
    ../../engine/src/lua_cpplib/ltools.h \
    ../../engine/src/lua_cpplib/lutil.h \
    ../../engine/src/mongo/mongo.h \
    ../../engine/src/mysql/sql.h \
    ../../engine/src/mysql/sql_def.h \
    ../../engine/src/net/codec/bson_codec.h \
    ../../engine/src/net/codec/codec.h \
    ../../engine/src/net/codec/codec_mgr.h \
    ../../engine/src/net/codec/flatbuffers_codec.h \
    ../../engine/src/net/codec/protobuf_codec.h \
    ../../engine/src/net/io/io.h \
    ../../engine/src/net/io/ssl_io.h \
    ../../engine/src/net/io/ssl_mgr.h \
    ../../engine/src/net/packet/http_packet.h \
    ../../engine/src/net/packet/packet.h \
    ../../engine/src/net/packet/stream_packet.h \
    ../../engine/src/net/packet/websocket_packet.h \
    ../../engine/src/net/packet/ws_stream_packet.h \
    ../../engine/src/net/buffer.h \
    ../../engine/src/net/net_header.h \
    ../../engine/src/net/socket.h \
    ../../engine/src/pool/object_pool.h \
    ../../engine/src/pool/ordered_pool.h \
    ../../engine/src/pool/pool.h \
    ../../engine/src/scene/a_star.h \
    ../../engine/src/scene/grid_aoi.h \
    ../../engine/src/scene/grid_map.h \
    ../../engine/src/scene/scene_include.h \
    ../../engine/src/system/static_global.h \
    ../../engine/src/system/statistic.h \
    ../../engine/src/thread/auto_mutex.h \
    ../../engine/src/thread/thread.h \
    ../../engine/src/thread/thread_mgr.h \
    ../../engine/src/util/rank.h \
    ../../engine/src/config.h \
    ../../engine/src/log/async_log.h \
    ../../engine/src/global/dbg_mem.h


unix:!macx: LIBS += -L$$PWD/../../engine/deps/aho-corasick/ -lacism

INCLUDEPATH += $$PWD/../../engine/deps/aho-corasick
DEPENDPATH += $$PWD/../../engine/deps/aho-corasick

unix:!macx: PRE_TARGETDEPS += $$PWD/../../engine/deps/aho-corasick/libacism.a

unix:!macx: LIBS += -L$$PWD/../../engine/deps/http-parser/ -lhttp_parser

INCLUDEPATH += $$PWD/../../engine/deps/http-parser
DEPENDPATH += $$PWD/../../engine/deps/http-parser

unix:!macx: PRE_TARGETDEPS += $$PWD/../../engine/deps/http-parser/libhttp_parser.a

unix:!macx: LIBS += -L$$PWD/../../engine/deps/lua_bson/ -llua_bson

INCLUDEPATH += $$PWD/../../engine/deps/lua_bson
DEPENDPATH += $$PWD/../../engine/deps/lua_bson

unix:!macx: PRE_TARGETDEPS += $$PWD/../../engine/deps/lua_bson/liblua_bson.a

unix:!macx: LIBS += -L$$PWD/../../engine/deps/lua_flatbuffers/ -llua_flatbuffers

INCLUDEPATH += $$PWD/../../engine/deps/lua_flatbuffers
DEPENDPATH += $$PWD/../../engine/deps/lua_flatbuffers

unix:!macx: PRE_TARGETDEPS += $$PWD/../../engine/deps/lua_flatbuffers/liblua_flatbuffers.a

unix:!macx: LIBS += -L$$PWD/../../engine/deps/lua_parson/ -llua_parson

INCLUDEPATH += $$PWD/../../engine/deps/lua_parson
DEPENDPATH += $$PWD/../../engine/deps/lua_parson

unix:!macx: PRE_TARGETDEPS += $$PWD/../../engine/deps/lua_parson/liblua_parson.a

unix:!macx: LIBS += -L$$PWD/../../engine/deps/lua_rapidxml/ -llua_rapidxml

INCLUDEPATH += $$PWD/../../engine/deps/lua_rapidxml
DEPENDPATH += $$PWD/../../engine/deps/lua_rapidxml

unix:!macx: PRE_TARGETDEPS += $$PWD/../../engine/deps/lua_rapidxml/liblua_rapidxml.a

unix:!macx: LIBS += -L$$PWD/../../engine/deps/pbc/build/ -lpbc

INCLUDEPATH += $$PWD/../../engine/deps/pbc
DEPENDPATH += $$PWD/../../engine/deps/pbc

unix:!macx: LIBS += -L$$PWD/../../engine/deps/websocket-parser/ -lwebsocket_parser

INCLUDEPATH += $$PWD/../../engine/deps/websocket-parser
DEPENDPATH += $$PWD/../../engine/deps/websocket-parser

unix:!macx: PRE_TARGETDEPS += $$PWD/../../engine/deps/websocket-parser/libwebsocket_parser.a
