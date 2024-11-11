-- 一些常用的几何、图形函数
graph = {}

local deg = math.deg -- 弧度转角度，相当于 180 / math.pi
local rad = math.rad -- 角度转弧度，相当于 math.pi / 180

local max = math.max
local min = math.min

-- 平面点的旋转 graph.rotate(1, 0, 45) = (0.7, 0.7)
-- @param x x轴坐标
-- @param y y轴坐标
-- @param alpha 角度
function graph.rotate(x, y, alpha)
    -- https://www.cnblogs.com/orange1438/p/4583825.html
    --[[
        旋转矩阵公式
        # 原坐标(x, y)旋转后的新坐标(x', y')计算公式
        # θ = 45度 = π/4
        x' = x * cos(θ) + y * sin(θ)
        y' = -x * sin(θ) + y * cos(θ)
        # 由于是45度，cos(45°) = sin(45°) = 1/√2 ≈ 0.707
        x' = x * 0.707 + y * 0.707
        y' = -x * 0.707 + y * 0.707
    ]]

    -- 角度转换为弧度cos单位为弧度，上面的cos(θ) = cos(π/4) = 0.707
    local radian = rad(alpha)
    local cos, sin = math.cos(radian), math.sin(radian)

    -- 如果是格子坐标，需要取整
    return x * cos - y * sin, x * sin + y * cos
end

-- 点是否在圆内
-- @param x x轴坐标
-- @param y y轴坐标
-- @param center_x 圆心x轴坐标
-- @param center_x 圆心y轴坐标
-- @param radius 半径
function graph.within_circle(x, y, center_x, center_y, radius)
    return (x - center_x) ^ 2 + (y - center_y) ^ 2 <= radius ^ 2
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

-- 知道扇形的开始、结束边界坐标，判断点是否在扇形
-- @param x x轴坐标
-- @param y y轴坐标
-- @param beg_x 扇形开始边界x轴坐标
-- @param beg_x 扇形开始边界y轴坐标
-- @param end_x 扇形结束边界x轴坐标
-- @param end_x 扇形结束边界y轴坐标
-- @param center_x 圆心x轴坐标
-- @param center_x 圆心y轴坐标
-- @param radius 半径
function graph.within_circle_sector(x, y, beg_x, beg_y, end_x, end_y, center_x,
                                    center_y, radius)
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

-- 以(x,y)为屏幕坐标系原点，计算点(dst_x, dst_y)与x轴形成的角度(-180, 180]
-- @param positive 是否转换为正数角[0，360)
function graph.angle(x, y, dst_x, dst_y, positive)
    -- atan2(y, x)返回左上角屏幕坐标系中，(0,0)与(x,y)形成的线段与x轴逆时针形成的弧度
    -- 即x轴与y轴与线段形成的三角形中，y对应的角的弧度

    -- atan2(x, y)返回左上角屏幕坐标系中，(0,0)与(x,y)形成的线段与y轴逆时针形成的弧度

    --[[
        坐标系为左上角屏幕坐标系

                 |
                 |
      (-90,180)  |   (0,-90)
                 |
      ------------------------>  X
                 |
       (90,180)  |  (0,90)
                 |
                 |
                 |
                 v
                 Y
    ]]

    -- 180 / PI是弧度转换为角度，相当于math.deg
    local angle = deg(math.atan(dst_y - y, dst_x - x))
    if not positive or angle >= 0 then return angle end

    return angle + 360
end

-- 判断一个角度是否在指定范围内
-- @param beg_angle 逆时针方向，起始角度
-- @param end_angle 逆时针方向，结束角度
function graph.within_angle_range(angle, beg_angle, end_angle)
    -- 负角度全部转换为[0,360)
    if angle < 0 then angle = angle + 360 end
    if beg_angle < 0 then beg_angle = beg_angle + 360 end
    if end_angle < 0 then end_angle = end_angle + 360 end

    -- 因为规定了逆时针方向，如果起始角度大于结束角度
    -- 则是扇形被0度分成2部分的情况，特殊处理一下
    if beg_angle > end_angle then
        return angle >= beg_angle or angle <= end_angle
    end

    return angle >= beg_angle and angle <= end_angle
end

-- 根据一个坐标范围，计算中点
function graph.range_to_mid(min_range, max_range)
    -- 例如：x轴上(90,180)的中点是135，同样适用于y轴。同时计算x，y则是线段中点
    return (min_range + max_range) / 2
end

-- 根据中点，计算最大、最小值
function graph.mid_to_range(mid, range)
    -- 例如：x轴中点是135，范围大小是90，则最小、最大值为(90,180)
    -- 同时计算x、y轴，则可以根据线段中点计算出两头的坐标
    local mid_range = range / 2
    return mid - mid_range, mid + mid_range
end

-- 已知扇形角度，判断点是否在扇形内
-- @param x x轴坐标
-- @param y y轴坐标
-- @param center_x 圆心x轴坐标
-- @param center_x 圆心y轴坐标
-- @param radius 半径
-- @param beg_angle 开始的角度
-- @param end_angle 结束的角度
function graph.within_circle_range(x, y, center_x, center_y, radius, beg_angle,
                                   end_angle)
    -- 假如激光炮可移动角度是90度，求开炮时命中的目标
    -- 那么以炮为中心，射程为半径
    -- 以开炮目标坐标计算出初始角度，结合移动角度计算出角度范围
    -- 以半径为正方形的半长，从AOI选出所有实体
    -- 用此函数判断每个实体是否在扇形内即可得到命中目标

    -- 判断是否在圆内
    if not graph.within_circle(x, y, center_x, center_y, radius) then
        return false
    end
    -- 计算目标和圆心形成的角度
    local angle = graph.angel(x, y, center_x, center_y)
    -- 判断角度是否在预期角度范围内
    return angle >= beg_angle and angle <= end_angle
end

-- 计算水平(没有旋转角度)两个矩形的重叠区域
-- @param src 源矩形，xl、yl(是左上角坐标，left)，xr、yr是右下角下标，right
-- @param dst 目标矩形左上角和右下角坐标
-- @return 是否重叠，左上角坐标，右下角坐标
function graph.rectangle_intersection(src_xl, src_yl, src_xr, src_yr, dst_xl,
                                      dst_yl, dst_xr, dst_yr)

    local xl = max(src_xl, dst_xl)
    local xr = min(src_xr, dst_xr)
    local yl = max(src_yl, dst_yl)
    local yr = min(src_yr, dst_yr)

    -- 没有重叠
    if xl > xr or yl > yr then return false end

    return true, xl, yl, xr, yr
end

-- 判断点是否在多边形内
-- @vert_x: 多边形各个点的x坐标
-- @vert_y：多边形各个点的y坐标，顺序和x坐标对应
function graph.within_polygon(vert_x, vert_y, x, y)
    -- https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html
    -- https://stackoverflow.com/questions/217578/how-can-i-determine-whether-a-2d-point-is-within-a-polygon

    --[[
        I run a semi-infinite ray horizontally (increasing x, fixed y) out from
    the test point, and count how many edges it crosses. At each crossing, the
    ray switches between inside and outside. This is called the Jordan curve theorem.

    The variable c is switching from 0 to 1 and 1 to 0 each time the horizontal
    ray crosses any edge. So basically it's keeping track of whether the number
    of edges crossed are even or odd. 0 means even and 1 means odd.

    int i, j, c = 0;
    for (i = 0, j = nvert-1; i < nvert; j = i++) {
      if ( ((verty[i]>testy) != (verty[j]>testy)) &&
       (testx < (vertx[j]-vertx[i]) * (testy-verty[i]) / (verty[j]-verty[i]) + vertx[i]) )
         c = !c;
    }
    return c;
    ]]

    assert(#vert_x == #vert_y)
    local within = false

    local j = #vert_y
    for i = 0, #vert_x do
        if (vert_y[i] > y) ~= (vert_y[j] > y) and
            (x < (vert_x[j] - vert_x[i]) * (y - vert_y[i]) /
                (vert_y[j] - vert_y[i]) + vert_x[i]) then

            j = i
            within = not within
        end
    end

    return within
end
