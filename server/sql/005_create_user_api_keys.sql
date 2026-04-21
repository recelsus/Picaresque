USE picaresque;

CREATE TABLE IF NOT EXISTS user_api_keys (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  user_id CHAR(36) NOT NULL,
  key_prefix VARCHAR(32) NOT NULL,
  key_hash CHAR(64) NOT NULL,
  enabled TINYINT(1) NOT NULL DEFAULT 1,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_user_api_keys_user_id (user_id),
  UNIQUE KEY uk_user_api_keys_key_hash (key_hash),
  CONSTRAINT fk_user_api_keys_user
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);
