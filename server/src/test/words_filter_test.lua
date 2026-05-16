-- 屏蔽字过滤测试

local words_path = "../config/banned_words.txt" -- 字库文件

local PERF_WORDS = {}
local function make_perf_words()
    local str =
        "这里是一些很随意的单词组合，字库里大多是英文，几乎所有的字母都有，" ..
            "因此只要包含英文，命中率还是会很高的。尽量产生一些随机的字符吧" ..
            "ABCDEFGHIJKLMNOPQXYZ123456789!@#$%^&*"

    local codes = {}
    for _, c in utf8.codes(str) do table.insert(codes, utf8.char(c)) end

    local words = {}
    for _ = 1, 5000 do
        local idx = math.random(1, #codes)
        for cnt = 1, 20 do
            words[cnt] = codes[idx]

            idx = idx + 1
            if idx >= #codes then idx = 1 end
        end
        table.insert(PERF_WORDS, table.concat(words, ""))
    end
end


Test.describe("acism words filter test", function()
    local Acism = require "engine.Acism"
    local acism = Acism()
    make_perf_words()

    Test.it("acism base test", function()
        local i = acism:load_from_file(words_path, false)
        Test.print(i, "band wods loaded!")
    end)

    Test.it(string.format("acism scan perf test: %d words", #PERF_WORDS),
         function()
            local t0 = Engine.steady_clock()
        for _, words in pairs(PERF_WORDS) do acism:scan(words) end
        local t1 = Engine.steady_clock()
        -- scan 5000 words 2 ms
        Test.print(string.format("scan %d words %d ms", #PERF_WORDS, t1 - t0))
    end)
end)

Test.describe("dicttree words filter test", function()
    Test.it("dicttree base test", function()
        local DictTree = require "engine.DictTree"
        local filter = DictTree()

        -- 逻辑添加关键字
        filter:add_word("傻逼")
        filter:add_word("SB")
        filter:add_word("赌博")
        filter:add_word("代练")
        filter:add_word("fucK")

        -- 测试忽略字符
        filter:set_ignore_chars("- *")

        Test.equal(true, filter:contain("你真是个傻逼"))
        Test.equal(true, filter:contain("你真是个sb"))
        Test.equal(true, filter:contain("fUcK you"))
        Test.equal(true, filter:contain("代--练"))
        Test.equal(false, filter:contain("正常聊天"))

        local replaced1 = filter:replace("你真是个傻逼啊", "*")
        Test.equal(replaced1, "你真是个******啊")

        local replaced2 = filter:replace("你真是个s-b啊", "*")
        Test.equal(replaced2, "你真是个***啊")
    end)
    Test.it(string.format("dicttree perf test: %d words", #PERF_WORDS),
         function()
        local DictTree = require "engine.DictTree"
        local filter = DictTree()
        local t0 = Engine.steady_clock()
        local size = filter:load_from_file(words_path)
        local t1 = Engine.steady_clock()
        for _, words in pairs(PERF_WORDS) do filter:contain(words) end
        local t2 = Engine.steady_clock()

        -- DEBUG:load 479830 words 559 ms, contain 89 ms
        -- RELEASE:load 479830 words 145 ms, contain 17 ms
        -- 作为对比：https://github.com/changnet/aho-corasick这个AC自动机只需要2ms,差距巨大
        -- 但执行5000次扫描，字典树只需要17ms也够用了，并且比较方便地做ignore_chars
        -- AC自动机做这个需要复制一遍字符串去掉ignore_chars或者改掉算法中的源码
        Test.print(string.format(
            "load %d words %d ms, contain %d ms", size, t1 - t0, t2 - t1))
    end)
end)
