#pragma once

/* 暂时不提供忽略大小写
 * 原因是原acism库不支持这一功能，一旦修改，代码库不好管理
 * 另外游戏中过滤的很多是中文，大小写无意义.每个字符都转换也消耗性能
 * 如果有少部分大小写都要过滤，请把大小写都加入到关键字列表
 * 如果要修改
 * 1)在加载文件时把内存中的字符都转换为小写
 * 2)在acism_more加个默认参数，是否case_sensitive
 * 3)根据参数将_SYMBOL sym = ps.symv[(uint8_t)*cp++];中的*cp++转换为小写即可
 */

#include <lua.hpp>

#ifdef __cplusplus
extern "C"
{
#endif

//#include <msutil.h>
#include <acism.h>

#ifdef __cplusplus
}
#endif

#include "../global/global.h"

typedef struct
{
    char *ptr;
    size_t len;
} MemBuf;

typedef struct
{
    int32_t rpl_len;  /* rpl的长度 */
    size_t text_pos;  /* 已替换部分相对于原字符串位置 */
    size_t word_len;  /* word的长度 */
    size_t rpl_size;  /* rpl的总长度 */
    char *rpl_text;   /* 替换后的字符串 */
    const char *word; /* 替换为此字符串 */
    const char *text; /* 原字符串 */

    void reserved(size_t bytes)
    {
        size_t new_size = rpl_size ? rpl_size : ACISM_REPLACE_DEFAULT;
        while (new_size < bytes)
        {
            new_size *= 2;
        }

        const char *tmp = rpl_text;
        rpl_text        = new char[new_size];
        if (tmp)
        {
            memcpy(rpl_text, tmp, rpl_len);
            delete[] tmp;
        }
        rpl_size = new_size;
    }
} MemRpl;

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
     * 从指定文件加载关键字
     * @param path 包含关键字的文件路径，文件中每一行表示一个关键字
     * @param ci case_sensitive，是否区分大小写
     * @return 加载的关键字数量
     */
    int32_t load_from_file(lua_State *L);

    int32_t do_replace(int32_t strnum, int32_t textpos);

    static int32_t on_match(int32_t strnum, int32_t textpos, MEMREF const *pattv);
    static int32_t on_replace(int32_t strnum, int32_t textpos, void *context);

private:
    /* 这几个函数在acism.h或msutil.h中都有类似的函数
     * 重写原因如下:
     * 1.原来的函数不支持大小写检测,slurp无法处理空文件...
     * 2.在原代码上修改导致第三方库难升级维护
     * 3.msutil根本就不在libacism.a中，需要改动makefile
     * 4.大小写的处理是在acism.c上修改的，原因是_acism.h中的写法在C++中编译不过
     */
    int32_t acism_slurp(const char *path);
    MEMREF *acism_refsplit(char *text, char sep, int *pcount);

private:
    ACISM *_psp;
    MemBuf _patt;
    int32_t _loaded;
    MEMREF *_pattv;
    MemRpl _memrpl;
    int32_t _case_sensitive;
};
