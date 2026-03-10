-- data worker的入口文件，它需要加载的文件比较少，不适用module_loader

require "engine.preloader"

local require = require_worker

require("data.data_mgr", W_DATA)
require("data.data_cache", W_DATA)
require("mysql.mysql", W_MYSQL)
require("mysql.mysql", W_MYSQL)
require("mongodb.mongodb", W_MONGODB)
