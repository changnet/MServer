# 关于AI设计
这里的AI是服务端用的AI，通常就是怪物用。当然这里顺便也用来做测试。以后可能还会用于机器人。

# 为什么不用行为树(Behavior tree)
行为树有几点要素：
1. action:动作，如移动、攻击
2. condition:条件，比如什么时候可以攻击
3. selector(Sequence、Parallel...):选择器，选择哪个动作来执行

对于action，这里还是保留了行为树的模式，细分各个action，低耦合，方便重用。

对于condition，哪种怪物攻击哪些目标等等，显然这个得设计一种格式让策划配置，详见aiconfig.

而selector，是完全改掉了。  
在大部分游戏中，AI其实都比较简单。比如一个arpg游戏，无非就主动怪、被动怪、巡逻、镖车...
这都算不上复杂，如果要设计一个行为树流程无论是程序还是策划，用UI工具还是手写配置，都相对麻烦，
而且这些流程并不多，即使算上特殊的boss，最终可能也只有10多个流程。那么，程序直接写逻辑就简单
得多么。比如一个主动怪，他的逻辑就可以这么写：
```lua
function routine( time )
    if status == IDLE then
        check_target() -- 空闲搜索可攻击目标
    end

    if status == CHASE then
        check_chase() -- 追击目标
    end

    if status == ATTACK then
        do_attack() -- 攻击目标
    end

    if status == RETURN then
        go_home() -- 回归
    end
end
```
而对于其他怪物，可能更简单，比如镖车就一直移动，直到目的地就可以了。
