#pragma once

#include "../scene/grid_aoi.h"
#include <lua.hpp>

/**
 * AOI算法，目前用格子地图实现
 */
class LGridAoi : public GridAOI
{
public:
    ~LGridAoi();
    explicit LGridAoi(lua_State *L);

    /**
     * 设置地图大小
     * @param width 地图像素宽度
     * @param height 地图像素高度
     * @param pix_grid 单个格子边长(像素)
     */
    int32_t set_size(lua_State *L);

    /**
     * 设置实体视野大小，目前所有实体视野一样
     * @param width 视野宽度(像素)
     * @param height 视野高度(像素)
     */
    int32_t set_visual_range(lua_State *L);

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
     * @param mask [optional]掩码，根据mask获取特定类型的实体(0xF表示所有)
     */
    int32_t get_interest_me_entity(lua_State *L);

    /**
     * 获取某一范围内实体
     * 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
     * @param mask 掩码，根据mask获取特定类型的实体(0xF表示所有)
     * @param tbl 获取到的实体存放在此table中，数量设置在n字段
     * @param src_x 矩形左上角x坐标
     * @param src_y 矩形左上角y坐标
     * @param dst_x 矩形右下角x坐标
     * @param dst_y 矩形右下角y坐标
     */
    int32_t get_entity(lua_State *L);

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
     * @param tbl [optional]周围关注此实体进入事件的实体存放在此table中，数量设置在n字段
     */
    int32_t enter_entity(lua_State *L);

    /**
     * 实体更新，通常是位置变更
     * @param id 唯一实体id
     * @param x 实体像素坐标x
     * @param y 实体像素坐标y
     * @param tbl [optional]周围关注此实体更新事件的实体存放在此table中，数量设置在n字段
     * @param into [optional]周围关注此实体出现事件的实体存放在此table中，数量设置在n字段
     * @param out [optional]周围关注此实体消失事件的实体存放在此table中，数量设置在n字段
     */
    int32_t update_entity(lua_State *L);

    /**
     * 判断两个像素坐标是否在同一个格子中
     * @param src_x 源位置x像素坐标
     * @param src_y 源位置y像素坐标
     * @param dst_x 目标位置x像素坐标
     * @param dst_y 目标位置y像素坐标
     */
    int32_t is_same_pos(lua_State *L);
};
