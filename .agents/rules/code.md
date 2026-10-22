---
trigger: always_on
---

# 代码风格
* 文件必须使用utf8无BOM格式
* 使用中文，注释也用中文
* 函数名、变量名均为小写加下划线风格
* 类名用大驼峰风格
* 写适量的注释
* lua注释使用lua doc注释风格，例如
```lua
--@class Test
Test = {}

-- 示例函数
-- @param a number 参数1
-- @param b DataObj 参数1
-- @return boolean 返回是否成功
function Test.func(a, b)
end

```
* 与前端交互的函数使用c_开头，假如功能为get_reward时，则函数名为c_get_reward
* 一行代码长度尽量限制在80字符内，最大不超过120字符
* 出错禁止静默返回，必须写日志。除非这个出错是不可避免，打日志会频繁刷日志
* 禁止写任何兜底代码，除非确切会出现这种情况
* 来自客户端的参数，在入口处理做好校验。进入内部后，可作安全参数使用，不需要兜底
