/*

使用PARTITION BY HASH(MONTH(time)) PARTITIONS 12;可以不用定时分区，但保存的日志时间超过一年分区效果就不大。最大的问题是
hash分区无法减少select * from xxx where time between "2026-01-01" and "2026-02-01"这种时间查询的搜索范围，都是全表搜索

而使用range分区，MySQL本身有针对时间的特殊处理，会把搜索范围限制在分区内，但要定期维护分区

所在这里的日志表名都以log_开头，后面会启用一个事件定时创建这些表的分区，并删除超过12个月的分区

*/
DROP TABLE IF EXISTS `log_misc`;
CREATE TABLE IF NOT EXISTS `log_misc` (
    `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    `op` varchar(512) DEFAULT null COMMENT '操作日志标记',
    `val` varchar(512) DEFAULT NULL COMMENT '操作的变量',
    `val1` varchar(512) DEFAULT NULL COMMENT '额外数据1',
    `val2` varchar(512) DEFAULT NULL COMMENT '额外数据2',
    `vals` TEXT DEFAULT NULL COMMENT '额外数据,玩家信息等',
    `time` DATETIME NOT NULL COMMENT '操作时间',
    INDEX `index_op` (`op`),
    INDEX `index_pid` (`pid`)
)
ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='杂项操作日志'
PARTITION BY RANGE (TO_DAYS(`time`)) (
    PARTITION pmax VALUES LESS THAN (MAXVALUE)
);

DROP TABLE IF EXISTS `log_money`;
CREATE TABLE IF NOT EXISTS `log_money` (
    `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    `log_id` int(11) DEFAULT '0' COMMENT '操作日志id(详见后端日志定义)',
    `id` int(11) DEFAULT '0' COMMENT '资源id',
    `change` bigint(64) DEFAULT '0' COMMENT '资源变化数量，负数表示扣除',
	`new_num` bigint(64) DEFAULT '0' COMMENT '变化后的数量',
    `val1` varchar(512) DEFAULT NULL COMMENT '额外数据1',
    `val2` varchar(512) DEFAULT NULL COMMENT '额外数据2',
    `vals` TEXT DEFAULT NULL COMMENT '额外数据,玩家信息等',
    `time` DATETIME NOT NULL COMMENT '操作时间',
    INDEX `index_op` (`log_id`),
    INDEX `index_id` (`id`),
    INDEX `index_pid` (`pid`)
)
ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='虚拟货币日志'
PARTITION BY RANGE (TO_DAYS(`time`)) (
    PARTITION pmax VALUES LESS THAN (MAXVALUE)
);

DROP TABLE IF EXISTS `log_item`;
CREATE TABLE IF NOT EXISTS `log_item` (
    `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    `log_id` int(11) DEFAULT '0' COMMENT '操作日志id(详见后端日志定义)',
    `id` int(11) DEFAULT '0' COMMENT '道具id',
    `change` bigint(64) DEFAULT '0' COMMENT '变化数量，负数表示扣除',
	`new_num` bigint(64) DEFAULT '0' COMMENT '变化后的数量',
    `val1` varchar(512) DEFAULT NULL COMMENT '额外数据1',
    `val2` varchar(512) DEFAULT NULL COMMENT '额外数据2',
    `vals` TEXT DEFAULT NULL COMMENT '额外数据,玩家信息等',
    `time` DATETIME NOT NULL COMMENT '操作时间',
    INDEX `index_op` (`log_id`),
    INDEX `index_id` (`id`),
    INDEX `index_pid` (`pid`)
)
ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='道具日志'
PARTITION BY RANGE (TO_DAYS(`time`)) (
    PARTITION pmax VALUES LESS THAN (MAXVALUE)
);

DROP TABLE IF EXISTS `log_stat`;
CREATE TABLE IF NOT EXISTS `log_stat` (
    `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    `stat` int(11) DEFAULT '0' COMMENT '状态定义(详见后端定义)',
    `val` varchar(2048) DEFAULT NULL COMMENT '额外数据1',
    `time` DATETIME NOT NULL COMMENT '操作时间',
    PRIMARY KEY (`pid`, `stat`, `time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='状态日志，覆盖而不是新增'
PARTITION BY RANGE (TO_DAYS(`time`)) (
    PARTITION pmax VALUES LESS THAN (MAXVALUE)
);

DELIMITER $$
DROP PROCEDURE IF EXISTS log_partition_maintainer_proc$$
CREATE PROCEDURE log_partition_maintainer_proc()
BEGIN
    DECLARE done INT DEFAULT FALSE;
    DECLARE tbl VARCHAR(64);
    DECLARE cur CURSOR FOR
        SELECT TABLE_NAME FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME LIKE 'log\_%';
    DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE;

    OPEN cur;
    read_loop: LOOP
        FETCH cur INTO tbl;
        IF done THEN
            LEAVE read_loop;
        END IF;

        -- drop partitions older than 12 months
                SET @older_than = TO_DAYS(DATE_SUB(CURDATE(), INTERVAL 12 MONTH));
                SELECT GROUP_CONCAT(CONCAT('`',PARTITION_NAME,'`') SEPARATOR ',') INTO @drop_parts
                FROM information_schema.PARTITIONS
                WHERE TABLE_SCHEMA = DATABASE()
                    AND TABLE_NAME = tbl
                    AND PARTITION_NAME IS NOT NULL
                    AND PARTITION_METHOD = 'RANGE'
                    AND PARTITION_NAME <> 'pmax'
                    AND PARTITION_DESCRIPTION <> 'MAXVALUE'
                    AND CAST(PARTITION_DESCRIPTION AS SIGNED) < @older_than;

        IF @drop_parts IS NOT NULL THEN
            SET @sql = CONCAT('ALTER TABLE `', tbl, '` DROP PARTITION ', @drop_parts);
            PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
        END IF;

        -- add partitions for next 12 months if missing
        SET @m = 0;
        WHILE @m < 12 DO
            SET @first_of_month = DATE_FORMAT(DATE_ADD(CURDATE(), INTERVAL @m MONTH), '%Y-%m-01');
            SET @pname = DATE_FORMAT(DATE_ADD(CURDATE(), INTERVAL @m MONTH), 'p%Y%m');
            SET @less = TO_DAYS(DATE_ADD(@first_of_month, INTERVAL 1 MONTH));
            SELECT COUNT(*) INTO @cnt FROM information_schema.PARTITIONS
            WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = tbl AND PARTITION_NAME = @pname;
            IF @cnt = 0 THEN
                SET @sql = CONCAT('ALTER TABLE `', tbl, '` REORGANIZE PARTITION pmax INTO (PARTITION ', @pname, ' VALUES LESS THAN (', @less, '), PARTITION pmax VALUES LESS THAN (MAXVALUE))');
                PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
            END IF;
            SET @m = @m + 1;
        END WHILE;

    END LOOP;
    CLOSE cur;
END$$

CREATE EVENT IF NOT EXISTS log_partition_maintainer
ON SCHEDULE EVERY 1 MONTH
STARTS (CURRENT_TIMESTAMP + INTERVAL 1 MONTH)
ON COMPLETION PRESERVE
ENABLE
DO CALL log_partition_maintainer_proc()$$
DELIMITER ;

-- Run once now to create current partitions when executing this SQL file
CALL log_partition_maintainer_proc();

