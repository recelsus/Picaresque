USE picaresque;

CREATE TABLE IF NOT EXISTS user_web_sessions (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  session_id CHAR(36) NOT NULL,
  user_id CHAR(36) NOT NULL,
  token_prefix VARCHAR(16) NOT NULL,
  token_hash CHAR(64) NOT NULL,
  enabled TINYINT(1) NOT NULL DEFAULT 1,
  expires_at DATETIME(3) NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_user_web_sessions_session_id (session_id),
  UNIQUE KEY uk_user_web_sessions_token_hash (token_hash),
  KEY idx_user_web_sessions_user_id (user_id),
  CONSTRAINT fk_user_web_sessions_user
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);
