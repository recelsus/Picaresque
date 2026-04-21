USE picaresque;

SET @has_scope_name = (
  SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = 'user_scoped_permissions'
    AND COLUMN_NAME = 'scope_name'
);

SET @has_group_id = (
  SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = 'user_scoped_permissions'
    AND COLUMN_NAME = 'group_id'
);

SET @rename_scope_sql = IF(
  @has_scope_name = 1 AND @has_group_id = 0,
  'ALTER TABLE user_scoped_permissions RENAME COLUMN scope_name TO group_id',
  'SELECT 1'
);

PREPARE rename_scope_stmt FROM @rename_scope_sql;
EXECUTE rename_scope_stmt;
DEALLOCATE PREPARE rename_scope_stmt;

SET @add_new_unique_sql = IF(
  NOT EXISTS (
    SELECT 1
    FROM INFORMATION_SCHEMA.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'user_scoped_permissions'
      AND INDEX_NAME = 'uk_user_scoped_permissions_user_group'
  ),
  'ALTER TABLE user_scoped_permissions ADD UNIQUE KEY uk_user_scoped_permissions_user_group (user_id, group_id)',
  'SELECT 1'
);

PREPARE add_new_unique_stmt FROM @add_new_unique_sql;
EXECUTE add_new_unique_stmt;
DEALLOCATE PREPARE add_new_unique_stmt;

SET @drop_old_unique_sql = IF(
  EXISTS (
    SELECT 1
    FROM INFORMATION_SCHEMA.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'user_scoped_permissions'
      AND INDEX_NAME = 'uk_user_scoped_permissions_user_scope'
  ),
  'ALTER TABLE user_scoped_permissions DROP INDEX uk_user_scoped_permissions_user_scope',
  'SELECT 1'
);

PREPARE drop_old_unique_stmt FROM @drop_old_unique_sql;
EXECUTE drop_old_unique_stmt;
DEALLOCATE PREPARE drop_old_unique_stmt;
