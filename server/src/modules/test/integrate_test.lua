-- 集成测试、玩家测试
local IntegrateTest = {}

--[[
src/test中的测试是单元测试，和业务无关

这里的测试则是业务相关，在服务器各个组件启动完成后，可通过玩家执行测试，方便测试系统集成
后的功能。

这里的代码只有执行test这个gm才会加载，正式服不会加载

如果是某个模块的测试，一般放在该模块里，通过该模块的gm来执行即可，没必要放这里

放这里的多数是一些多模块交互的测试，或者一些零碎的测试，一般比较少
]]

local function set_or_validate_ikey(stg, log_str)
    local ikey = {
        [1] = "a",
        [2] = "b",
        [99] = "c",
    }

    if table.empty(stg) then
        for k, v in pairs(ikey) do
            stg[k] = v
        end
        print("set ikey for", log_str)
    else
        if table.equal(stg, ikey) then
            print("validate ikey success for", log_str)
        else
            eprint("validate ikey failed for", log_str, table.dump(stg))
        end
    end
end

-- GM: @test ikey
-- 测试各种存储的ikey是否能正确恢复
function run_ikey_test(player)
    print("run ikey test", player)

    if type(player) == "number" then
        player = PlayerMgr.get_player(player)
    end

    if type(player) == "table" then
        local stg = Player.get_storage(player, "ikeytest")
        if not stg then
            stg = {}
            Player.set_storage(player, "ikeytest", stg)
        end
        set_or_validate_ikey(stg, "player")
    end

    local gstg = storage("ikeytest")
    if gstg then
        set_or_validate_ikey(gstg, "global")
    else
        dprint("no global storage, global test ignore")
    end

    local json_stg = {}
    set_or_validate_ikey(json_stg, "json")
    local json_str = Json.encode(json_stg)
    local json_stg2 = sys.restore_json(json_str)
    set_or_validate_ikey(json_stg2, "json")
end

-- GM: @test ikey
-- 测试各种存储的ikey是否能正确恢复
function IntegrateTest.ikey(player)
    run_ikey_test(player)

    -- player没有全局数据，到game再执行一次测试
    if LOCAL_TYPE ~= W.GAME then
        local pid = type(player) == "table" and player.pid or player
        Send[GAME_ADDR].Misc.eval_file("test.integrate_test", "ikey", pid)
    end
end

-- 死循环测试：能否中断并查出死循环的调用栈
function IntegrateTest.deadloop()
    -- print("run deadloop test")
    -- while true do end
end

-- ping测试，只测试client-->gateway-->player返回
function IntegrateTest.ping1(roleObj, id)
    local pkt = {
        channel = 0xFFFF,
        context = string.format("%d", id),
    }
    NetMsg.send(roleObj, M.ChatMsg, pkt)
end

-- ping测试，只测试client-->gateway-->player-->game返回，和ping1对比
function IntegrateTest.ping2(roleObj, id)
    Call[GAME_ADDR].Misc.dummy()

    local pkt = {
        channel = 0xFFFF,
        context = string.format("%d", id),
    }
    NetMsg.send(roleObj, M.ChatMsg, pkt)
end

-- GM: @test xxx
function IntegrateTest.run(player, cmd, ...)
    assert("run" ~= cmd, "test cmd should not be run")
    local func = IntegrateTest[cmd]
    if func then
        func(player, ...)
    else
        print("unknown test cmd", cmd)
    end
end

return IntegrateTest
