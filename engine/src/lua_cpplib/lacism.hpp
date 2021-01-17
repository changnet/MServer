#pragma once

#include <lua.hpp>

extern "C"
{
// 使用4byte可以支持10M的字库，8字节则可以支持300M，如果启用，则编译acism时也需要加这个定义
// #define ACISM_SIZE 8

#include <msutil.h>
#include <acism.h>
}


#include "../global/global.hpp"

/**
 * AC自动机，用于关键字过滤
 */
class LAcism
{
public:
    ~LAcism();
    explicit LAcism(lua_State *L);

    /**
     * 搜索字符串中是否包含关键字
     * @param text 需要扫描的字符串
     * @return 首次出现关键字的位置，未扫描到则返回0
     */
    int32_t scan(lua_State *L);

    /**
     * 搜索字符串中是否包含关键字并替换出现的关键字
     * @param text 需要扫描的字符串
     * @param repl 替换字符串，出现的关键字将会被替换为此字符串
     * @return 替换后的字符串
     */
    int32_t replace(lua_State *L);

    /**
     * 从指定文件加载关键字，文件以\n换行
     * @param path 包含关键字的文件路径，文件中每一行表示一个关键字
     * @param ci case_sensitive，是否区分大小写
     * @return 加载的关键字数量
     */
    int32_t load_from_file(lua_State *L);
private:
    ACISM *_psp; /// AC自动机的数据结构
    MEMBUF _patt; /// pattern，字库内容
    MEMREF *_pattv; /// 把_patt拆分成字库数组
    int32_t _case_sensitive; /// 是否区分大小写
};
