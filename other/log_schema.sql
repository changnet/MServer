
DROP TABLE IF EXISTS `gold`;
CREATE TABLE `gold` (
  `id` int(11) NOT NULL AUTO_INCREMENT COMMENT '自增id',
  `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid',
  `op_val` int(11) DEFAULT '0' COMMENT '操作数量，>0获取，<0消耗',
  `new_val` int(11) DEFAULT '0' COMMENT '操作后剩余数量',
  `op_type` int(11) DEFAULT '0' COMMENT '操作类型(详见后端日志定义)',
  `op_time` int(11) DEFAULT '0' COMMENT '操作时间',
  `ext` varchar(32) DEFAULT '0' COMMENT '额外数据(比如记录邮件附件的具体来源)',
  PRIMARY KEY (`id`,`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='元宝日志';

DROP TABLE IF EXISTS `login_logout`;
CREATE TABLE `login_logout` (
  `id` int(11) NOT NULL AUTO_INCREMENT COMMENT '自增id',
  `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid',
  `op_type` int(11) DEFAULT '0' COMMENT '操作类型(详见后端日志定义)',
  `op_time` int(11) DEFAULT '0' COMMENT '操作时间',
  PRIMARY KEY (`id`,`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='登录退出日志';

DROP TABLE IF EXISTS `item`;
CREATE TABLE `item` (
  `id` int(11) NOT NULL AUTO_INCREMENT COMMENT '自增id',
  `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid',
  `item_id` int(11) DEFAULT '0' COMMENT '道具id',
  `op_val` int(11) DEFAULT '0' COMMENT '操作数量，>0获取，<0消耗',
  `new_val` int(11) DEFAULT '0' COMMENT '操作后剩余数量',
  `op_type` int(11) DEFAULT '0' COMMENT '操作类型(详见后端日志定义)',
  `op_time` int(11) DEFAULT '0' COMMENT '操作时间',
  `ext` varchar(32) DEFAULT '0' COMMENT '额外数据(比如记录邮件附件的具体来源)',
  PRIMARY KEY (`id`,`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='道具日志';
