#ifndef __LMAP_MGR_H__
#define __LMAP_MGR_H__

#include <lua.hpp>
#include "../scene/a_star.h"
#include "../scene/map_mgr.h"

class lmap_mgr : public map_mgr
{
public:
    static void uninstance();
    static class lmap_mgr *instance();

    ~lmap_mgr();
    explicit lmap_mgr( lua_State *L );

    int32 load_path( lua_State *L ); // 加载该目录下所有地图文件
    int32 load_file( lua_State *L ); // 加载单个文件
    int32 remove_map( lua_State *L ); // 删除一个地图数据
    int32 create_map( lua_State *L ); // 创建一个地图
    int32 fill_map( lua_State *L );   // 填充地图数据

    int32 find_path( lua_State *L); // 寻路
private:
    class a_star _a_star;
    static class lmap_mgr *_lmap_mgr;
};

#endif /* __LMAP_MGR_H__ */
