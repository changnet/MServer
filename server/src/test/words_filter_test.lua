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

        -- scan返回匹配字符串尾位置，一般一个中文字符占3
        -- AB匹配到B时，返回的是B的字符串尾，即2
        -- “出售枪支”匹配到“枪支”时返回枪支的末尾位置
        Test.equal(acism:scan("好好学习，天天向上"), 0)
        Test.equal(acism:scan("!!!!!!!!AAAAAA"), 9)
        Test.equal(acism:scan("出售枪支是非法的"), 12)

        -- 替换
        local ch = "*"
        Test.equal(acism:replace("A", ch), "*")
        Test.equal(acism:replace("枪支", ch), "******")
        Test.equal(acism:replace("出售枪支是非法的", ch),
                "出售******是非法的")
        Test.equal(acism:replace("匹配不到我", ch), "匹配不到我")
        Test.equal(acism:replace("这枪支是A货", ch), "这******是*货")
        Test.equal(acism:replace("AAAAAAAA", ch), "********")
    end)

    Test.it(string.format("acism scan perf test: %d words", #PERF_WORDS),
         function()
        for _, words in pairs(PERF_WORDS) do acism:scan(words) end
    end)

    Test.it(string.format("acism replace perf test: %d words", #PERF_WORDS),
         function()
        for _, words in pairs(PERF_WORDS) do acism:replace(words, "*") end
    end)
end)

Test.describe("dicttree words filter test", function()
    Test.it("dicttree base test", function()
        local filter = engine.DictTree()

        -- 逻辑添加关键字
        filter:add_word("傻逼")
        filter:add_word("SB")
        filter:add_word("赌博")
        filter:add_word("代练")
        filter:add_word("fucK")

        -- 测试忽略字符
        filter:set_ignore_chars("- *")

        assert(filter:contain("你真是个傻逼"), "contain failed: 你真是个傻逼")
        assert(filter:contain("你真是个sb"), "case insensitive contain failed")
        assert(filter:contain("fUcK you"), "case insensitive contain failed")
        assert(filter:contain("代--练"), "ignore chars contain failed")
        assert(not filter:contain("正常聊天"), "false positive contain failed")

        local replaced1 = filter:replace("你真是个傻逼啊", string.byte("*"))
        assert(not string.find(replaced1, "傻逼"), "replace failed: 傻逼")

        local replaced2 = filter:replace("你真是个s-b啊", string.byte("*"))
        assert(not string.find(string.lower(replaced2), "sb"), "replace failed: s-b")
    end)
    Test.it(string.format("acism scan perf test: %d words", #PERF_WORDS),
         function()
        local filter = engine.DictTree()
        filter:load_from_file(words_path)
        -- for _, words in pairs(PERF_WORDS) do acism:scan(words) end
    end)
end)
