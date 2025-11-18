-- data worker的入口文件，它需要加载的文件比较少，不适用module_loader

require "engine.preloader"

local require = require_worker

require("mysql.mysql", W_MYSQL)
require("mongodb.mongodb", W_MONGODB)
