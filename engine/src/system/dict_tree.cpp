#include "dict_tree.hpp"
#include <fstream>
#include <cctype>
#include <cstring>
#include <string>

DictTree::DictTree()
{
    pool_.reserve(10240); // 预留较大空间，避免频繁分配及失效
    root_ = new_node(0);
}

DictTree::~DictTree()
{
}

int32_t DictTree::new_node(uint8_t ch)
{
    int32_t index = static_cast<int32_t>(pool_.size());
    pool_.push_back({-1, -1, ch, false});
    return index;
}

int32_t DictTree::add_child(int32_t node_index, uint8_t ch)
{
    int32_t first_child = pool_[node_index].first_child;
    if (first_child == -1)
    {
        int32_t new_child             = new_node(ch);
        pool_[node_index].first_child = new_child;
        return new_child;
    }

    int32_t curr = first_child;
    int32_t last = curr;
    while (curr != -1)
    {
        if (pool_[curr].ch == ch)
        {
            return curr;
        }
        last = curr;
        curr = pool_[curr].next_sibling;
    }

    int32_t new_child        = new_node(ch);
    pool_[last].next_sibling = new_child;
    return new_child;
}

int32_t DictTree::get_child(int32_t node_index, uint8_t ch) const
{
    if (node_index == -1 || node_index >= static_cast<int32_t>(pool_.size()))
        return -1;

    int32_t curr = pool_[node_index].first_child;
    while (curr != -1)
    {
        if (pool_[curr].ch == ch)
        {
            return curr;
        }
        curr = pool_[curr].next_sibling;
    }
    return -1;
}

void DictTree::add_word(const char *word)
{
    int32_t curr = root_;
    for (const char *p = word; *p != '\0'; ++p)
    {
        uint8_t uid = static_cast<uint8_t>(std::tolower(*p));
        curr        = add_child(curr, uid);
    }
    pool_[curr].is_end = true;
}

int32_t DictTree::load_from_file(const char *path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return -1;
    }

    int32_t size = 0;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back(); // 处理 \r\n
        }
        if (!line.empty())
        {
            size++;
            add_word(line.c_str());
        }
    }

    file.close();

    return size;
}

const char *DictTree::match_at(const char *pos) const
{
    int32_t curr          = root_;
    const char *match_end = nullptr;

    for (const char *q = pos; *q != '\0'; ++q)
    {
        uint8_t raw_ch = static_cast<uint8_t>(*q);
        if (ignore_map_[raw_ch])
        {
            continue;
        }

        uint8_t ch = static_cast<uint8_t>(std::tolower(raw_ch));
        curr       = get_child(curr, ch);
        if (curr == -1)
        {
            break;
        }

        if (pool_[curr].is_end)
        {
            match_end = q; // 记录最长匹配位置
        }
    }

    return match_end;
}

bool DictTree::contain(const char *text) const
{
    for (const char *p = text; *p != '\0'; ++p)
    {
        if (match_at(p))
        {
            return true;
        }
    }
    return false;
}

void DictTree::match_replace(const char *p, char replace_char) const
{
    while (*p != '\0')
    {
        const char *match_end = match_at(p);
        if (match_end)
        {
            // 将匹配区间替换为指定字符
            for (const char *k = p; k <= match_end; ++k)
            {
                replace_buf_.push_back(replace_char);
            }
            p = match_end + 1;
        }
        else
        {
            replace_buf_.push_back(*p);
            ++p;
        }
    }
    replace_buf_.push_back('\0');
}

const char *DictTree::replace(const char *text, char replace_char) const
{
    // 第一阶段：只扫描不拷贝，找到第一个匹配位置
    const char *p = text;
    while (*p != '\0')
    {
        const char *match_end = match_at(p);
        if (match_end)
        {
            // 首次匹配，初始化缓冲区
            replace_buf_.clear();

            // 将匹配前的文本批量拷贝到缓冲区
            if (p > text)
            {
                replace_buf_.insert(replace_buf_.end(), text, p);
            }

            // 替换匹配区间
            for (const char *k = p; k <= match_end; ++k)
            {
                replace_buf_.push_back(replace_char);
            }

            // 第二阶段：处理剩余文本
            match_replace(match_end + 1, replace_char);
            return replace_buf_.data();
        }
        ++p;
    }

    // 无匹配，直接返回原文本
    return text;
}

void DictTree::set_ignore_char(char c, bool ignore)
{
    ignore_map_[static_cast<uint8_t>(c)] = ignore;
}

void DictTree::set_ignore_chars(const char *chars)
{
    for (const char *p = chars; *p != '\0'; ++p)
    {
        set_ignore_char(*p, true);
    }
}
