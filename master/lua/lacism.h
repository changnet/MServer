#ifndef __LACISM_H__
#define __LACISM_H__

/* 暂时不提供忽略大小写
 * 原因是原acism库不支持这一功能，一旦修改，代码库不好管理
 * 另外游戏中过滤的很多是中文，大小写无意义.每个字符都转换也消耗性能
 * 如果有少部分大小写都要过滤，请把大小写都加入到关键字列表
 * 如果要修改
 * 1)在加载文件时把内存中的字符都转换为小写
 * 2)在acism_more加个默认参数，是否case_sensitive
 * 3)根据参数将_SYMBOL sym = ps.symv[(uint8_t)*cp++];中的*cp++转换为小写即可
 */

#include <msutil.h>
#include <acism.h>

#include "../global/global.h"

class lacism
{
public:
    int32 scan(); /* 如果指定回调参数，则搜索到时调用回调函数 */
    int32 load_from_file();
};

#endif /* __LACISM_H__ */
