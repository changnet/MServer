#include "dict_tree.hpp"
#include <fstream>
#include <cctype>
#include <cstring>
#include <string>

DictTree::DictTree() {
    pool_.reserve(10000); // 预留较大空间，避免频繁分配及失效
    root_ = new_node(0);
}

DictTree::~DictTree() {
}

int32_t DictTree::new_node(uint8_t ch) {
    int32_t index = static_cast<int32_t>(pool_.size());
    DictNode node;
    node.ch = ch;
    pool_.push_back(node);
    return index;
}

int32_t DictTree::find_or_add_child(int32_t node_index, uint8_t ch) {
    int32_t first_child = pool_[node_index].first_child;
    if (first_child == -1) {
        int32_t new_child = new_node(ch);
        pool_[node_index].first_child = new_child;
        return new_child;
    }

    int32_t curr = first_child;
    int32_t last = curr;
    while (curr != -1) {
        if (pool_[curr].ch == ch) {
            return curr;
        }
        last = curr;
        curr = pool_[curr].next_sibling;
    }

    int32_t new_child = new_node(ch);
    pool_[last].next_sibling = new_child;
    return new_child;
}

int32_t DictTree::get_child(int32_t node_index, uint8_t ch) const {
    if (node_index == -1 || node_index >= static_cast<int32_t>(pool_.size())) return -1;
    
    int32_t curr = pool_[node_index].first_child;
    while (curr != -1) {
        if (pool_[curr].ch == ch) {
            return curr;
        }
        curr = pool_[curr].next_sibling;
    }
    return -1;
}

void DictTree::add_word(const char* word) {
    int32_t curr = root_;
    for (const char* p = word; *p != '\0'; ++p) {
        uint8_t uid = static_cast<uint8_t>(std::tolower(*p));
        curr = find_or_add_child(curr, uid);
    }
    pool_[curr].is_end = true;
}

bool DictTree::load_from_file(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back(); // 处理 \r\n
        }
        if (!line.empty()) {
            add_word(line.c_str());
        }
    }
    
    return true;
}

bool DictTree::contain(const char* text) const {
    for (const char* p = text; *p != '\0'; ++p) {
        int32_t curr = root_;
        for (const char* q = p; *q != '\0'; ++q) {
            uint8_t ch = static_cast<uint8_t>(*q);
            
            if (ignore_map_[ch]) {
                continue;
            }
            
            ch = std::tolower(ch);
            curr = get_child(curr, ch);
            if (curr == -1) {
                break;
            }
            
            if (pool_[curr].is_end) {
                return true;
            }
        }
    }
    return false;
}

const char* DictTree::replace(const char* text, char replace_char) const {
    thread_local static std::string result;
    result.clear();
    
    const char* p = text;
    while (*p != '\0') {
        int32_t curr = root_;
        const char* match_end = p;
        bool found = false;
        
        for (const char* q = p; *q != '\0'; ++q) {
            uint8_t raw_ch = static_cast<uint8_t>(*q);
            if (ignore_map_[raw_ch]) {
                continue;
            }
            
            uint8_t ch = std::tolower(raw_ch);
            curr = get_child(curr, ch);
            
            if (curr == -1) {
                break;
            }
            
            if (pool_[curr].is_end) {
                found = true;
                match_end = q; // 记录最长匹配位置（如果要做最长匹配的话，继续搜索）
            }
        }
        
        if (found) {
            // 被匹配的区间[p, match_end] 全部替换为指定字符
            for (const char* k = p; k <= match_end; ++k) {
                // 如果只替换英文字符，或每个byte替换一次可能会导致特殊符号格式问题
                // 这里按字节直接替换
                result += replace_char;
            }
            p = match_end + 1;
        } else {
            result += *p;
            p++;
        }
    }
    
    return result.c_str();
}

void DictTree::set_ignore_char(char c, bool ignore) {
    ignore_map_[static_cast<uint8_t>(c)] = ignore;
}

void DictTree::set_ignore_chars(const char* chars) {
    for (const char* p = chars; *p != '\0'; ++p) {
        set_ignore_char(*p, true);
    }
}
