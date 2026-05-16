#pragma once

#include <vector>
#include <bitset>

/**
 * @brief 用LCRS(Left-Child Right-Sibling)实现的字典树，
 * 支持敏感词检测与替换
 */
class DictTree
{
public:
    DictTree();
    ~DictTree();

    /**
     * 添加一个关键字到字典树
     * @param word 关键字，以'\0'结尾的字符串
     */
    void add_word(const char *word);
    /**
     * 从文件加载关键字，每行一个
     * @param path 文件路径
     * @return 加载成功的词汇数量
     */
    int32_t load_from_file(const char *path);
    /**
     * 检查文本中是否包含关键字
     * @param text 待检测的文本
     * @return 是否包含关键字
     */
    bool contain(const char *text) const;
    /**
     * 替换文本中的关键字为指定字符（如*）。
     * 未匹配到关键字时直接返回原文本指针，不做拷贝
     * @param text 待替换的文本
     * @param replace_char 替换字符
     * @return 替换后的文本
     */
    const char *replace(const char *text, char replace_char) const;

    /**
     * 设置是否忽略某个字符
     * @param c 字符
     * @param ignore 是否忽略
     */
    void set_ignore_char(char c, bool ignore);
    /**
     * 批量设置忽略字符
     * @param chars 需要忽略的字符集合
     */
    void set_ignore_chars(const char *chars);

private:
    // LCRS节点
    struct DictNode
    {
        int32_t first_child  = -1;
        int32_t next_sibling = -1;
        uint8_t ch           = 0;
        bool is_end          = false;
    };

    int32_t new_node(uint8_t ch);
    int32_t add_child(int32_t node_index, uint8_t ch);
    int32_t get_child(int32_t node_index, uint8_t ch) const;

    /**
     * 从pos位置尝试匹配关键字（最长匹配）
     * @param pos 起始位置
     * @return 匹配结束位置(含)，未匹配返回nullptr
     */
    const char *match_at(const char *pos) const;
    /**
     * 从p位置开始执行匹配替换，
     * 结果追加到replace_buf_
     * @param p 起始位置
     * @param replace_char 替换字符
     */
    void match_replace(const char *p, char replace_char) const;

    int32_t root_ = -1;
    std::vector<DictNode> pool_;
    std::bitset<256> ignore_map_;
    mutable std::vector<char> replace_buf_; // 替换缓冲区
};
