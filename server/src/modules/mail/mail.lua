-- mail.lua
-- 2018-05-20
-- xzc

-- 邮件模块
Mail = {}

---@class MailObj 单个邮件结构
---@field id number 唯一id
---@field cid number 配置id（如果存在配置，title和text可能为nil，需要前端读配置）
---@field text string 邮件内容
---@field title string 标题
---@field atts table 邮件附件(attachments)，格式为通用奖励格式
---@field sender string 发件人
---@field time number 发送时间戳
---@field read number 是否已读
---@field att_stat number 是否已读
---@field op number 日志操作，用于跟踪附件资源产出。参考log_header
---@field log_str string 日志字符串，记录一些额外信息，方便日志分析

function Mail.send()
end

function Mail.send_player(player)
end

return Mail
