-- system libraries
sys = {}

sys.separator = package.config:sub(1,1) -- 路径分割斜杠，windows=\，linux=/

-- 一些通用又不好归到某个模块的函数，可以放这里

-- 获取文件路径的上一层路径(不包含最后的斜杠，如/etc/log的上一层为/etc)
function sys.path_up(path)
    return string.match(path, "(.*)[/\\]")
end