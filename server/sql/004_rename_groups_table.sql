USE picaresque;

SET @has_old_groups = (
  SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = 'groups'
);

SET @has_user_groups = (
  SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = 'user_groups'
);

SET @rename_groups_sql = IF(
  @has_old_groups = 1 AND @has_user_groups = 0,
  'RENAME TABLE `groups` TO user_groups',
  'SELECT 1'
);

PREPARE rename_groups_stmt FROM @rename_groups_sql;
EXECUTE rename_groups_stmt;
DEALLOCATE PREPARE rename_groups_stmt;
