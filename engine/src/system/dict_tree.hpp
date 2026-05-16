#pragma once

#include <vector>
#include <bitset>

/**
 * @brief 用LCRS实现的字典树
 */
class DictTree
{
public:
    DictTree();
    ~DictTree();

    // 逻辑添加关键字
    void add_word(const char *word);
    // 从文件加载，每行一个关键字
    bool load_from_file(const char *path);
    // 检查是否包含关键字
    bool contain(const char *text) const;
    // 替换文本中的关键字为指定字符（如*）
    const char *replace(const char *text, char replace_char) const;

    // 设置是否忽略某个字符
    void set_ignore_char(char c, bool ignore);
    // 批量设置忽略字符
    void set_ignore_chars(const char *chars);

private:
    // LCRS结构
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

    int32_t root_ = -1;
    std::vector<DictNode> pool_;
    std::bitset<256> ignore_map_;
};
