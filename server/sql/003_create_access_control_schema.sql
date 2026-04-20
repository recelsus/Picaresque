USE picaresque;

CREATE TABLE IF NOT EXISTS access_ip_rules (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  rule_id CHAR(36) NOT NULL,
  value_text VARCHAR(255) NOT NULL,
  address_family ENUM('ipv4', 'ipv6') NOT NULL,
  rule_type ENUM('single', 'cidr') NOT NULL,
  prefix_length TINYINT UNSIGNED NULL,
  effect ENUM('allow', 'deny') NOT NULL,
  surface ENUM('all', 'web', 'rest_api') NOT NULL DEFAULT 'all',
  enabled TINYINT(1) NOT NULL DEFAULT 1,
  note TEXT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_access_ip_rules_rule_id (rule_id),
  KEY idx_access_ip_rules_effect_surface_enabled (effect, surface, enabled),
  KEY idx_access_ip_rules_value_prefix_enabled (value_text, prefix_length, enabled)
);
