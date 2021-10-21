-- 屏蔽字过滤测试
local Acism = require "engine.Acism"

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

t_describe("acism words filter test", function()
    local acism = Acism()
    make_perf_words()

    t_it("acism base test", function()
        local i = acism:load_from_file(words_path, false)
        t_print(i, "band wods loaded!")

        -- scan返回匹配字符串尾位置，一般一个中文字符占3
        -- AB匹配到B时，返回的是B的字符串尾，即2
        -- “出售枪支”匹配到“枪支”时返回枪支的末尾位置
        t_equal(acism:scan("好好学习，天天向上"), 0)
        t_equal(acism:scan("!!!!!!!!AAAAAA"), 9)
        t_equal(acism:scan("出售枪支是非法的"), 12)

        -- 替换
        local ch = "*"
        t_equal(acism:replace("A", ch), "*")
        t_equal(acism:replace("枪支", ch), "******")
        t_equal(acism:replace("出售枪支是非法的", ch),
                "出售******是非法的")
        t_equal(acism:replace("匹配不到我", ch), "匹配不到我")
        t_equal(acism:replace("这枪支是A货", ch), "这******是*货")
        t_equal(acism:replace("AAAAAAAA", ch), "********")
    end)

    t_it(string.format("acism scan perf test: %d words", #PERF_WORDS),
         function()
        for _, words in pairs(PERF_WORDS) do acism:scan(words) end
    end)

    t_it(string.format("acism replace perf test: %d words", #PERF_WORDS),
         function()
        for _, words in pairs(PERF_WORDS) do acism:replace(words, "*") end
    end)
end)
