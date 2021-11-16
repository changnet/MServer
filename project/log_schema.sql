
DROP TABLE IF EXISTS `misc`;
CREATE TABLE IF NOT EXISTS `misc` (
    `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    `op` int(11) DEFAULT '0' COMMENT '操作日志id(详见后端日志定义)',
    `val` varchar(512) DEFAULT NULL COMMENT '操作的变量',
    `val1` varchar(512) DEFAULT NULL COMMENT '额外数据1',
    `val2` varchar(512) DEFAULT NULL COMMENT '额外数据2',
    `vals` varchar(512) DEFAULT NULL COMMENT '额外数据,剩下的额外数据都自动拼接到这里',
    `time` DATETIME NOT NULL COMMENT '操作时间',
    INDEX `index_id` (`op`),
    INDEX `index_pid` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='杂项操作日志'
PARTITION BY HASH(MONTH(time)) PARTITIONS 12;

DROP TABLE IF EXISTS `res`;
CREATE TABLE IF NOT EXISTS `res` (
    `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    `op` int(11) DEFAULT '0' COMMENT '操作日志id(详见后端日志定义)',
    `id` int(11) DEFAULT '0' COMMENT '资源id',
    `change` int(11) NOT NULL COMMENT '资源变化数量',
    `val1` varchar(512) DEFAULT NULL COMMENT '额外数据1',
    `val2` varchar(512) DEFAULT NULL COMMENT '额外数据2',
    `vals` varchar(512) DEFAULT NULL COMMENT '额外数据,剩下的额外数据都自动拼接到这里',
    `time` DATETIME NOT NULL COMMENT '操作时间',
    INDEX `index_id` (`op`),
    INDEX `index_id` (`id`),
    INDEX `index_pid` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='资源变化日志'
PARTITION BY HASH(MONTH(time)) PARTITIONS 12;

DROP TABLE IF EXISTS `stat`;
CREATE TABLE IF NOT EXISTS `stat` (
    `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    `stat` int(11) DEFAULT '0' COMMENT '状态定义(详见后端定义)',
    `val` varchar(2048) DEFAULT NULL COMMENT '额外数据1',
    `time` DATETIME NOT NULL COMMENT '操作时间',
    PRIMARY KEY (`pid`, `stat`),
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='当前状态日志';
