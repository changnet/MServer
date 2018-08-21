/* a star寻路算法
 * 2018-07-18 by xzc
 * http://theory.stanford.edu/~amitp/GameProgramming/
 * https://www.geeksforgeeks.org/a-search-algorithm/
 */

 #ifndef __A_STAR_H__
 #define __A_STAR_H__

class grid_map;
class a_star
{
public:
    // 路径辅助节点
    struct node
    {
        uint8_t mask; // 用于标识是否缓存数据，是否close，是否open
        int32_t g; // a*算法中f = g + h中的g，代表从起始位置到该格子的开销
        int32_t h; // a*算法中f = g + h中的h，代表该格子到目标节点预估的开销
        uint16_t x; // 该格子的x坐标
        uint16_t y; // 该格子的y坐标
        uint16_t px; // 该格子的父格子x坐标
        uint16_t py; // 该格子的父格子y坐标
    };
public:
    a_star();
    ~a_star();
    /* 搜索路径
     * @map：对应地图的地形数据
     * @x,y：起点坐标
     * @dx,dy：dest，终点坐标
     */
    bool search( const grid_map *map,int32_t x,int32_t y,int32_t dx,int32_t dy);
private:
    bool do_search(
        const grid_map *map,int32_t x,int32_t y,int32_t dx,int32_t dy);
    node *new_node(uint16_t x,uint16_t y,uint16_t px = 0,uint16_t py = 0)
    {
        if ( _pool_idx >= _pool_max ) return NULL;

        node *nd = _node_pool[_pool_idx ++];
        nd->x = x;
        nd->y = y;
        nd->px = px;
        nd->px = py;
        return nd;
    }
// heuristic Manhattan distance
private:
    node **_node_set; // 记录当前寻路格子集合
    node *_node_pool; // 格子对象内存池

    int32_t _set_max;  // 当前集合大小
    int32_t _pool_max; // 内存池格子数量
    int32_t _pool_idx; // 内存池当前已用数量
};

#endif /* __A_STAR_H__ */
