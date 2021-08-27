
DROP TABLE IF EXISTS `money`;
CREATE TABLE `money` (
  `auto_id` int(11) NOT NULL AUTO_INCREMENT COMMENT '自增id',
  `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid',
  `id` int(11) DEFAULT '0' COMMENT '货币类型',
  `op_val` int(11) DEFAULT '0' COMMENT '操作数量，>0获取，<0消耗',
  `new_val` int(11) DEFAULT '0' COMMENT '操作后剩余数量',
  `op_type` int(11) DEFAULT '0' COMMENT '操作类型(详见后端日志定义)',
  `op_time` int(11) DEFAULT '0' COMMENT '操作时间',
  `ext` varchar(32) DEFAULT '0' COMMENT '额外数据(比如记录邮件附件的具体来源)',
  PRIMARY KEY (`auto_id`),
  INDEX `index_id` (`id`),
  INDEX `index_pid` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='货币日志';

DROP TABLE IF EXISTS `login_logout`;
CREATE TABLE `login_logout` (
  `auto_id` int(11) NOT NULL AUTO_INCREMENT COMMENT '自增id',
  `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid',
  `op_type` int(11) DEFAULT '0' COMMENT '操作类型(详见后端日志定义)',
  `op_time` int(11) DEFAULT '0' COMMENT '操作时间',
  PRIMARY KEY (`auto_id`),
  INDEX `index_pid` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='登录退出日志';

DROP TABLE IF EXISTS `item`;
CREATE TABLE `item` (
  `auto_id` int(11) NOT NULL AUTO_INCREMENT COMMENT '自增id',
  `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid',
  `item_id` int(11) DEFAULT '0' COMMENT '道具id',
  `op_val` int(11) DEFAULT '0' COMMENT '操作数量，>0获取，<0消耗',
  `new_val` int(11) DEFAULT '0' COMMENT '操作后剩余数量',
  `op_type` int(11) DEFAULT '0' COMMENT '操作类型(详见后端日志定义)',
  `op_time` int(11) DEFAULT '0' COMMENT '操作时间',
  `ext` varchar(32) DEFAULT '0' COMMENT '额外数据(比如记录邮件附件的具体来源)',
  PRIMARY KEY (`auto_id`),
  INDEX `index_id` (`pid`),
  INDEX `index_item_id` (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='道具日志';
