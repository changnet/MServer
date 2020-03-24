-- 一些常用的几何、图形函数
graph = {}

-- 平面点的旋转 graph.rotate(1, 0, 45) = (0.7, 0.7)
-- @x: x轴坐标
-- @y: y轴坐标
-- @alpha: 角度
function graph.rotate(x, y, alpha)
    -- https://www.cnblogs.com/orange1438/p/4583825.html

    -- 角度转换为弧度
    local radian = alpha * 3.1459 / 180
    local cos, sin = math.cos(radian), math.sin(radian)

    -- 如果是格子坐标，需要取整
    return x * cos - y * sin, x * sin + y * cos
end

-- 点是否在圆内
-- @x: x轴坐标
-- @y: y轴坐标
-- @center_x: 圆心x轴坐标
-- @center_x: 圆心y轴坐标
-- @radius: 半径
function graph.within_circle(x, y, center_x, center_y, radius)
    return (x - center_x)^2 + (y - center_y)^2 <= radius^2
end

-- 以坐标原点为中心，从src到dst是否按顺时间旋转
function graph.is_clockwise(x, y, dst_x, dst_y)
    -- https://stackoverflow.com/questions/13652518/efficiently-find-points-inside-a-circle-sector
    -- https://www.cnblogs.com/newbeeyu/p/5859382.html
    -- 1 做“start arm” OA向量 "逆时针方向"转过90度 的法向量(normal vector)，则利用两个垂直向量的 点乘 为 0 的性质 则该法向量可以表示为n1(-y1, x1)
    -- 2 n1 和 OP 两向量做点积 n1*OP = |n1|*|OP|*cosa (a为n1和OP间的夹角) ,如果点积的结果大于0，则表明0 < a < 90, 而OA和n1 夹角= 90. 这时可以证明0A向量到OP向量是顺时针旋转
    -- 反之点积结果小于0，表示0A向量到OP向量是逆时针旋转. 这里给出点积的百科:  http://baike.baidu.com/view/2744555.htm
    return -x * dst_y + y * dst_x > 0
end

-- 知道扇形的开始、结束边界，判断点是否在扇形
-- @x: x轴坐标
-- @y: y轴坐标
-- @beg_x: 扇形开始边界x轴坐标
-- @beg_x: 扇形开始边界y轴坐标
-- @end_x: 扇形结束边界x轴坐标
-- @end_x: 扇形结束边界y轴坐标
-- @center_x: 圆心x轴坐标
-- @center_x: 圆心y轴坐标
-- @radius: 半径
function graph.within_circle_sector(
    x, y, beg_x, beg_y, end_x, end_y, center_x, center_y, radius)
    -- https://stackoverflow.com/questions/13652518/efficiently-find-points-inside-a-circle-sector
    -- https://www.cnblogs.com/newbeeyu/p/5859382.html

    -- 转换为以圆心为坐标原点
    local zero_x, zero_y = x - center_x, y - center_y

    -- 以圆心为中心，逆时针旋转，第一个扇形边界为开始边界beg_x, beg_y
    -- 继续旋转，遇到目标点x,y，再旋转，遇到结束边界end_x, end_y
    -- 1. 开始边界必须在目标点逆时针方向
    -- 2. 结束边界必须在目标点顺时针方向
    -- 3. 目标点与圆心的距离在半径范围内

    if graph.is_clockwise(beg_x, beg_y, zero_x, zero_y) then return false end
    if not graph.is_clockwise(end_x, end_y, zero_x, zero_y) then return false end

    return graph.within_circle(zero_x, zero_y, 0, 0, radius)
end
