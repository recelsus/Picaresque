CREATE TABLE IF NOT EXISTS embedded_queries (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  query_id VARCHAR(64) NOT NULL,
  article_id VARCHAR(64) NOT NULL,
  fragment_kind VARCHAR(16) NOT NULL,
  location_index BIGINT UNSIGNED NOT NULL,
  sql_body TEXT NOT NULL,
  table_id CHAR(36) NOT NULL,
  created_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  UNIQUE KEY uq_embedded_queries_query_id (query_id),
  KEY idx_embedded_queries_article_id (article_id),
  CONSTRAINT fk_embedded_queries_article
    FOREIGN KEY (article_id) REFERENCES articles(article_id)
    ON DELETE CASCADE,
  CONSTRAINT fk_embedded_queries_table
    FOREIGN KEY (table_id) REFERENCES custom_tables(table_id)
    ON DELETE RESTRICT
);
