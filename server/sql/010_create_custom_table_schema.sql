USE picaresque;

CREATE TABLE IF NOT EXISTS custom_tables (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  table_id CHAR(36) NOT NULL,
  table_name VARCHAR(255) NOT NULL,
  created_by_user_id CHAR(36) NOT NULL,
  updated_by_user_id CHAR(36) NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_custom_tables_table_id (table_id),
  KEY idx_custom_tables_created_by_user_id (created_by_user_id),
  CONSTRAINT fk_custom_tables_created_by_user
    FOREIGN KEY (created_by_user_id) REFERENCES users(user_id),
  CONSTRAINT fk_custom_tables_updated_by_user
    FOREIGN KEY (updated_by_user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS custom_table_columns (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  column_id CHAR(36) NOT NULL,
  table_id CHAR(36) NOT NULL,
  column_name VARCHAR(128) NOT NULL,
  column_type VARCHAR(32) NOT NULL,
  is_required TINYINT(1) NOT NULL DEFAULT 0,
  position_index INT UNSIGNED NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_custom_table_columns_column_id (column_id),
  UNIQUE KEY uk_custom_table_columns_table_name (table_id, column_name),
  KEY idx_custom_table_columns_table_id (table_id),
  CONSTRAINT chk_custom_table_columns_type
    CHECK (column_type IN ('varchar', 'long_text', 'integer', 'decimal', 'boolean', 'date', 'time', 'datetime', 'json')),
  CONSTRAINT fk_custom_table_columns_table
    FOREIGN KEY (table_id) REFERENCES custom_tables(table_id)
);

CREATE TABLE IF NOT EXISTS custom_table_required_permissions (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  table_id CHAR(36) NOT NULL,
  group_id VARCHAR(128) NOT NULL,
  read_level TINYINT UNSIGNED NOT NULL,
  write_level TINYINT UNSIGNED NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_custom_table_required_permissions_table_group (table_id, group_id),
  CONSTRAINT chk_custom_table_required_permissions_read_range CHECK (read_level BETWEEN 1 AND 99),
  CONSTRAINT chk_custom_table_required_permissions_write_range CHECK (write_level BETWEEN 1 AND 99),
  CONSTRAINT chk_custom_table_required_permissions_order CHECK (read_level >= write_level),
  CONSTRAINT fk_custom_table_required_permissions_table
    FOREIGN KEY (table_id) REFERENCES custom_tables(table_id)
);

CREATE TABLE IF NOT EXISTS custom_table_rows (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  row_id CHAR(36) NOT NULL,
  table_id CHAR(36) NOT NULL,
  created_by_user_id CHAR(36) NOT NULL,
  updated_by_user_id CHAR(36) NOT NULL,
  values_json LONGTEXT NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_custom_table_rows_row_id (row_id),
  KEY idx_custom_table_rows_table_id (table_id),
  CONSTRAINT fk_custom_table_rows_table
    FOREIGN KEY (table_id) REFERENCES custom_tables(table_id),
  CONSTRAINT fk_custom_table_rows_created_by_user
    FOREIGN KEY (created_by_user_id) REFERENCES users(user_id),
  CONSTRAINT fk_custom_table_rows_updated_by_user
    FOREIGN KEY (updated_by_user_id) REFERENCES users(user_id)
);
