-- data worker的入口文件，它需要加载的文件比较少，不适用module_loader

require "engine.preloader"

local require = require_worker

require("data.data_mgr", W.DATA)
require("data.data_cache", W.DATA)
require("mysql.mysql", W.MYSQL)
require("mongodb.mongodb", W.MONGODB)
