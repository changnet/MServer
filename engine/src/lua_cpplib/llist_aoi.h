#pragma once

#include "../scene/list_aoi.h"
#include <lua.hpp>

/**
 * AOI十字链表实现
 */
class LListkAoi final: public ListAOI
{
public:
    ~LListkAoi() {}
    explicit LListkAoi(lua_State *L) { UNUSED(L); }

    /**
     * 是否使用y轴
     */
    int32_t use_y(lua_State *L);

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
     * @param src_y 长方体y轴起点坐标
     * @param src_z 长方体z轴起点坐标
     * @param dst_x 长方体x轴终点坐标
     * @param dst_y 长方体y轴终点坐标
     * @param dst_z 长方体z轴终点坐标
     */
    int32_t get_entity(lua_State *L);

    /**
     * 获取某个实体视野范围内的实体
     * @param id 实体id
     * @param mask 掩码，根据mask获取特定类型的实体(0xF表示所有)
     * @param tbl 获取到的实体存放在此table中，数量设置在n字段
     */
    int32_t get_visual_entity(lua_State *L);

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
     * @param mask 实体掩码
     * @param tbl [optional]视野内新增实体存放在此table中，数量设置在n字段
     */
    int32_t enter_entity(lua_State *L);

    /**
     * 实体更新，通常是位置变更
     * @param id 唯一实体id
     * @param x 实体像素坐标x
     * @param y 实体像素坐标y
     * @param tbl [optional]周围关注此实体更新事件的实体存放在此table中，数量设置在n字段
     * @param into [optional]视野内新增实体存放在此table中，数量设置在n字段
     * @param out [optional]视野内消失实体存放在此table中，数量设置在n字段
     */
    int32_t update_entity(lua_State *L);
};
