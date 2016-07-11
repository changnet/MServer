#ifndef __LACISM_H__
#define __LACISM_H__

class lacism
{
public:
    int32 scan(); /* 如果指定回调参数，则搜索到时调用回调函数 */
    int32 add_pattern();
    int32 load_from_file();
    int32 set_case_sensitive();
private:
    bool case_sensitive;
};

#endif /* __LACISM_H__ */
