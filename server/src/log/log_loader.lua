-- log_loader.lua
require "engine.preloader"

local require = require_worker

require("log.log_mgr", W_LOG)
