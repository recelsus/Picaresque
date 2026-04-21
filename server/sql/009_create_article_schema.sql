USE picaresque;

CREATE TABLE IF NOT EXISTS articles (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  article_id CHAR(36) NOT NULL,
  title VARCHAR(255) NOT NULL,
  body LONGTEXT NOT NULL,
  created_by_user_id CHAR(36) NOT NULL,
  updated_by_user_id CHAR(36) NOT NULL,
  is_locked TINYINT(1) NOT NULL DEFAULT 0,
  locked_by_user_id CHAR(36) NULL,
  locked_at DATETIME(3) NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_articles_article_id (article_id),
  KEY idx_articles_created_by_user_id (created_by_user_id),
  CONSTRAINT fk_articles_created_by_user
    FOREIGN KEY (created_by_user_id) REFERENCES users(user_id),
  CONSTRAINT fk_articles_updated_by_user
    FOREIGN KEY (updated_by_user_id) REFERENCES users(user_id),
  CONSTRAINT fk_articles_locked_by_user
    FOREIGN KEY (locked_by_user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS article_required_permissions (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  article_id CHAR(36) NOT NULL,
  group_id VARCHAR(128) NOT NULL,
  read_level TINYINT UNSIGNED NOT NULL,
  write_level TINYINT UNSIGNED NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_article_required_permissions_article_group (article_id, group_id),
  CONSTRAINT chk_article_required_permissions_read_range CHECK (read_level BETWEEN 1 AND 99),
  CONSTRAINT chk_article_required_permissions_write_range CHECK (write_level BETWEEN 1 AND 99),
  CONSTRAINT chk_article_required_permissions_order CHECK (read_level >= write_level),
  CONSTRAINT fk_article_required_permissions_article
    FOREIGN KEY (article_id) REFERENCES articles(article_id)
);
