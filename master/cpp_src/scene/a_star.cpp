#include "a_star.h"

#define DEFAULT_SET 128*128 // 默认格子集合大小，128*128有点大，占128k内存了
#define DEFAULT_POOL 1024  // 默认分配格子内存池数量

a_star::a_star()
{
    _node_set = NULL;  // 记录当前寻路格子数据
    _node_pool = NULL; // 格子对象内存池

    _set_max  = 0;
    _pool_max = 0; // 内存池格子数量
    _pool_idx = 0; // 内存池当前已用数量
}

a_star::~a_star()
{
    delete []_node_set;
    delete []_node_pool;

    _node_set = NULL;
    _node_pool = NULL;

    _set_max  = 0;
    _pool_max = 0; // 内存池格子数量
    _pool_idx = 0; // 内存池当前已用数量
}

/* 搜索路径
 * @map：对应地图的地形数据
 * @x,y：起点坐标
 * @dx,dy：dest，终点坐标
 */
bool a_star::search(
    const grid_map *map,int32_t x,int32_t y,int32_t dx,int32_t dy)
{
    uint16_t width = map->get_width();
    uint16_t height = map->get_height();

    if ( x > width || y > height || dx > width || dy > height ) return false;

    // 分配格子集合，每次寻路时只根据当前地图只增不减
    if ( _set_max < width*height )
    {
        delete []_node_set;

        _set_max = width*height;
        _set_max = _set_max > DEFAULT_SET ? _set_max : DEFAULT_SET;

        _node_set = new node*[DEFAULT_SET];
    }

    /* 格子内存池
     * 现在内存池默认分配1024个，如果用完了那就算找不到路径
     * 如果以后确实有更高的需求，加个参数，用完后传入一个更大的数量在这里重新分配重新
     * 查找。
     * 注意内存池的对象是引用到node_set里的，必须要重新调用search函数
     */
    if ( _pool_max < DEFAULT_POOL )
    {
        delete []_node_pool;

        _pool_max = DEFAULT_POOL;
        _node_pool = new struct node[DEFAULT_POOL];
    }

    // 清空寻路缓存
    _pool_idx = 0;
    memset( _node_set,NULL,sizeof(void *)*width*height );

    return do_search( map,x,y,dx,dy );
}

bool a_star::do_search(
    const grid_map *map,int32_t x,int32_t y,int32_t dx,int32_t dy)
{
#define IS_OPEN(x,y)
#define IS_CLOSE(x,y)
#define PUSH_OPEN_SET(x,y)
#define PUSH_CLOSE_SET(X,Y)

    node *nd = new_node(x,y);

    PUSH_OPEN_SET(nd);
    PUSH_CLOSE_SET(nd);

    while (nd)
    {

    }

#undef PUSH_OPEN_SET
#undef PUSH_CLOSE_SET
}
