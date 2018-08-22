#include "a_star.h"

// 默认格子集合大小，128*128有点大，占128k内存了
// 如果有超级大地图，那么可能要考虑用hash_map，虽然慢一点，至少不会爆内存
#define DEFAULT_SET 128*128
#define DEFAULT_POOL 1024  // 默认分配格子内存池数量

/* 定义一个格子的距离,整数计算效率比浮点高，
 * 根据勾股定理，要定一个误差较小的整数对角距离,边长为10，那边沿对角走则为14
 */
#define D 10   // 格子边长
#define DD 14  // 格子对角边长


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
#define IS_CLOSE(x,y)
#define FIND_OPEN(x,y)
#define PUSH_OPEN_SET(x,y)
#define PUSH_CLOSE_SET(X,Y)

    // 地图以左上角为坐标原点，分别向8个方向移动时的向量
    const static int16_t offset [][2] =
    {
        {0,-1},{1,0},{ 0,1},{-1, 0}, // 北-东-南-西
        {1,-1},{1,1},{-1,1},{-1,-1}  // 东北-东南-西南-西北
    };

    struct node *parent = new_node(x,y);
    while ( parent )
    {
        PUSH_CLOSE_SET(parent);

        /* 查找相邻的8个点,这里允许沿对角行走
         * TODO:对角的两个格子均可行走，则可以沿对角行走。部分游戏要相邻格子也可行走
         * 才可以，这个看策划具体设定
         */
        for ( int32_t dir = 0;dir < 8;dir ++ )
        {
            int32_t x = parent->x + offset[dir][0];
            int32_t y = parent->y + offset[dir][1];

            // 不可行走或者已经close的格子，忽略
            if ( map->get_pass_cost(x,y) < 0 || IS_CLOSE(x,y) ) continue;

            /* 计算起点到当前点的消耗
             * 前4个方向(北-东-南-西)都是直走，假设边长为10，那边沿对角走则为14
             * 复杂的游戏，可能每个格子的速度不一样(即get_pass_cost值不一样)，有的
             * 是飞行，有的是行走，这里先假设都是一样的
             */
            int32_t g = parent->g + (dir < 4 ? D : DD);
            int32_t h = diagonal( x,y,dx,dy );

            struct node *child = FIND_OPEN( x,y )
            if ( child )
            {
                // 发现更优路径，更新路径
                if ( g + h < child->g + child->h )
                {
                    child->g = g;
                    child->h = h;
                    child->px = parent->x;
                    child->py = parent->y;
                }
            }
            else
            {
                child = new_node(x,y,parent->x,parent->y);
                child->g = g;
                child->h = h;
                PUSH_OPEN_SET( child );
            }
        }

        parent = NULL;
    }

#undef PUSH_OPEN_SET
#undef PUSH_CLOSE_SET
}

node *a_star::new_node(uint16_t x,uint16_t y,uint16_t px,uint16_t py)
{
    if ( _pool_idx >= _pool_max ) return NULL;
    node *nd = _node_pool[_pool_idx ++];
    nd->x = x;
    nd->y = y;
    nd->px = px;
    nd->px = py;

    nd->g = 0;
    nd->h = 0;
    nd->mask = 0;

    return nd;
}

/* 曼哈顿距离，不会算对角距离，
 * 适用只能往东南西北4个方向，不能走对角的游戏
 */
int32_t a_star::manhattan(int32_t x,int32_t y,int32_t gx,int32_t gy)
{
    int32_t dx = abs(x - dx);
    int32_t dy = abs(y - dy);

    return D * (dx + dy);
}

/* 对角距离
 * 适用东南西北，以及东北-东南-西南-西北(沿45度角走)的游戏
 */
int32_t a_star::diagonal(int32_t x,int32_t y,int32_t gx,int32_t gy)
{
    int32_t dx = abs(x - gx);
    int32_t dy = abs(y - gy);
    // DD是斜边长，2*D是两直角边总和，min(dx,dy)就是需要走45度的格子数
    // D * (dx + dy)是先假设所有格子直走，然后加上5度走多出的距离
    return D * (dx + dy) + (DD - 2 * D) * min(dx, dy);
}

/* 欧几里得距离
 * 适用可以沿任意角度行走的游戏。但是f = g + h中，g的值是一步步算出来的，因此g值
 * 要么是直线，要么是45度角的消耗，因此会导致f值不准确。不过这里的h <= n，还是可以
 * 得到最小路径，只是算法效率受影响
 */
int32_t a_star::euclidean(int32_t x,int32_t y,int32_t gx,int32_t gy)
{
    int32_t dx = abs(x - x);
    int32_t dy = abs(y - y);
    // 这个算法有sqrt，会慢一点，不过现在的游戏大多数是可以任意角度行走的
    return D * sqrt(dx * dx + dy * dy);
}
