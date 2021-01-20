#pragma once

#ifdef USE_ORTH_LIST_AOI
    #include "../scene/orth_list_aoi.hpp"
using ListAOI = OrthListAOI;
#else
    #include "../scene/skip_list_aoi.hpp"
using ListAOI = SkipListAOI;
#endif
#include <lua.hpp>

/**
 * AOI链表实现
 */
class LListAoi final : public ListAOI
{
public:
    ~LListAoi() {}
    explicit LListAoi(lua_State *L) { UNUSED(L); }

    /// 是否使用y轴，仅十字链表可用
    int32_t use_y(lua_State *L);

    /// 设置索引参数，仅跳表可用
    int32_t set_index(lua_State *L);

    /**
     * 获取所有实体
     * @param mask 掩码，根据mask获取特定类型的实体(0xF表示所有)
     * @param tbl 获取到的实体存放在此table中，数量设置在n字段
     */
    int32_t get_all_entity(lua_State *L);

    /**
     * 获取周围关注自己的实体列表,常用于自己释放技能、扣血、特效等广播给周围的人
     * @param id 自己的唯一实体id
     * @param tbl 获取到的实体存放在此table中，数量设置在n字段
     */
    int32_t get_interest_me_entity(lua_State *L);

    /**
     * 获取某一范围内实体
     * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
     * @param mask 掩码，根据mask获取特定类型的实体(0xF表示所有)
     * @param tbl 获取到的实体存放在此table中，数量设置在n字段
     * @param src_x 长方体x轴起点坐标
     * @param dst_x 长方体x轴终点坐标
     * @param src_y 长方体y轴起点坐标
     * @param dst_y 长方体y轴终点坐标
     * @param src_z 长方体z轴起点坐标
     * @param dst_z 长方体z轴终点坐标
     */
    int32_t get_entity(lua_State *L);

    /**
     * 获取某个实体视野范围内的实体
     * @param id 实体id
     * @param mask 掩码，根据mask获取特定类型的实体(0xF表示所有)
     * @param tbl 获取到的实体存放在此table中，数量设置在n字段
     * @param visual [optional]使用特定的视野而不是实体本身的视野
     */
    int32_t get_visual_entity(lua_State *L);

    /**
     * 更新实体视野范围
     * @param id 实体的唯一id
     * @param visual 新视野大小
     * @param list_me_in 该列表中实体因视野扩大出现在我的视野范围
     * @param list_me_out 该列表中实体因视野缩小从我的视野范围消失
     */
    int32_t update_visual(lua_State *L);

    /**
     * 实体退出场景
     * @param id 唯一实体id
     * @param tbl [optional]interest_me存放在此table中，数量设置在n字段
     */
    int32_t exit_entity(lua_State *L);

    /**
     * 实体进入场景
     * @param id 唯一实体id
     * @param x 实体像素坐标x
     * @param y 实体像素坐标y
     * @param z 实体像素坐标z
     * @param visual 视野大小
     * @param mask 实体掩码
     * @param list_me_in [optional]该列表中实体出现在我的视野范围
     * @param list_other_in [optional]我出现在该列表中实体的视野范围内
     */
    int32_t enter_entity(lua_State *L);

    /**
     * 实体更新，通常是位置变更
     * @param id 唯一实体id
     * @param x 实体像素坐标x
     * @param y 实体像素坐标y
     * @param z 实体像素坐标z
     * @param list_me_in [optional]该列表中实体出现在我的视野范围
     * @param list_other_in [optional]我出现在该列表中实体的视野范围内
     * @param list_me_out [optional]该列表中实体从我的视野范围消失
     * @param list_other_out [optional]我从该列表中实体的视野范围内消失
     */
    int32_t update_entity(lua_State *L);

    /**
     * 打印整个链表，用于调试
     * @param dump 是否打印链表数据
     */
    int32_t valid_dump(lua_State *L);
};
