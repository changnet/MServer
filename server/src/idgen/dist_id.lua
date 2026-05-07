-- distubuted id generator
-- xzc

-- 生成一个分布式的唯一id，字符串格式
DistId = oo.class("DistId")

--[[
如果是集群，节点数量有限，还可以将就把addr和自增拼在一起限制在64位内形成一个整数

但如果是小服模式，有多节点同时还要考虑跨服，想要id唯一就只能用字符串了

服务器id + 节点addr + 时间戳 + 自增序列
如果确定了服务器架构（比如是集群还是小服，哪些业务会用这个id），可以根据业务截取部分位
来构建int64，但这里主要是为了提供一个通用分布式id
]]

-- 64字符(url safe)，标准url后面是+/
local BASE64_CHARS = {
    "0","1","2","3","4","5","6","7","8","9",
    "A","B","C","D","E","F","G","H","I","J",
    "K","L","M","N","O","P","Q","R","S","T",
    "U","V","W","X","Y","Z",
    "a","b","c","d","e","f","g","h","i","j",
    "k","l","m","n","o","p","q","r","s","t",
    "u","v","w","x","y","z","-","_",
}
local CHARS = {}

-- 预先生成固定部分，自增时只填充变化的部分，减少计算量
-- @param sid 服务器id
-- @param addr 节点地址
-- @param tm 时间戳，单位秒
local function prepare(self, sid, addr, tm)
        --[[
    服务器id + 节点addr + 时间戳 + 自增序列

    如果是简单的两个数字拼接，那数字会很大，生成的字符串比较长。这里把不太可能出现的位
    全部放到高位，这样大部分情况下的字符串会比较短。

    时间戳最大32位，8年会占28位（注意调整game_time起始时间点）
    节点addr最大32位，实际由index和type构成，单个节点数量<=7，type数量<=15，所以占7位
    服务器id最大20位，10240占14位，即服务器id<10240时生成的字符串会比较短
    自增序列最大15位，32768/s的生成速度可以满足绝大部分业务需求了

    这样定下来，绝大部分情况下生成的字符在一个int64范围内

    高位：time（4位）+ addr（25） + 服务器id（6）
    低位：服务器id（14位） + addr（7位）+ 时间（28位） + 自增（15位）

    这样最小(服务器id为1)时占50位，字符长为9
    time最大时占17个字符
    ]]

    if not addr then addr = LOCAL_ADDR end
    if not tm then tm = time.game_time() end
    if not sid then sid = Engine.get_server_id() end

    local wtype, index, main = Engine.unmake_address(addr)

    -- low part bits: sid_lo(14) | addr_lo(7) | seq(15) | time_lo(28)
    local sid_lo = sid & 0x3FFF -- 14 bits
    local addr_lo = ((index & 0x7) << 4) | (wtype & 0xF) -- 7 bits
    local time_lo = tm & 0x0FFFFFFF -- 28 bits

    local lo = (sid_lo << 50) | (addr_lo << 43) | (time_lo << 15)

    -- high part bits: time_hi(4) + addr_hi(25) + sid_hi(6)
    local time_hi = (tm >> 28) & 0x0F -- upper 4 bits of a 32-bit time
    wtype = wtype >> 4 -- 8位用掉4位
    index = index >> 3 -- 14位用掉3位
    local addr_hi = ((index << 5) | (wtype << 1) | main) & 0x01FFFFFF -- 25 bits
    local sid_hi = (sid >> 14) & 0x3F -- 6 bits

    local hi = (time_hi << (25 + 6)) | (addr_hi << 6) | sid_hi

    self.hi = hi
    self.lo = lo
    self.seed = 0
    self.time = tm
    self.sid = sid
    self.addr = addr
end

function DistId:__init(sid, addr, time)
    prepare(self, sid, addr, time)

    -- Both right and left shifts fill the vacant bits with zeros.
    -- 其他语言java、c++是会区分有符号和无符号的右移的，lua没有，
    -- 所以要保证hi的最高位不能是1，否则右移时会一直补1，导致死循环
    assert((-1 >> 1) > 0)
end

-- 自增seed，并生成hi和lo
local function make_id(self)


    local seed = self.seed + 1
    if seed > 0x7FFF then-- 15 bits
        prepare(self, self.sid, self.addr, self.time + 1)
        seed = 1
    end

    self.seed = seed

    local lo = self.lo | seed
    return self.hi, lo
end

-- 把两个64位整数编码成一个base64字符串，长度最多22字符
-- @param hi 高64位整数
-- @param lo 低64位整数
local function encode64(hi, lo)
    -- 进行base64编译，要把hi和lo合并成一个128位整数来编码
    -- 但lua没有128位整数，所以从hi的低位开始编码
    -- 每次取6位，然后把hi的位数补到lo，直到hi和lo都为0
    -- 注意这种算法是不是编码前面的0，这是故意的，减少字符串长度（标准base64是会编码0）
    local n = 0
    while hi ~= 0 or lo ~= 0 do
        n = n + 1
        -- 0x3f是63，base64的掩码，取6位。和lo%64等同只是效率更高
        CHARS[n] = BASE64_CHARS[(lo & 0x3F) + 1]
        -- 低6位已经用掉，右移6位移除
        -- 把高位的低6位合并到lo的高6位
        lo = (lo >> 6) | ((hi & 0x3F) << 58)
        -- 高位的6位也用掉了，右移6位移除
        hi = hi >> 6
    end

    if 0 == n then return "0" end

    return table.concat(CHARS, "", 1, n)
end

-- 生成一个唯一id
function DistId:next_id()
    local hi, lo = make_id(self)
    return encode64(hi, lo)
end

return DistId
