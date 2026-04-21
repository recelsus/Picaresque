CREATE DATABASE IF NOT EXISTS picaresque
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE picaresque;

CREATE TABLE IF NOT EXISTS users (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  user_id CHAR(36) NOT NULL,
  login_id VARCHAR(64) NOT NULL,
  user_name VARCHAR(128) NOT NULL,
  email VARCHAR(255) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  role ENUM('admin', 'owner', 'member') NOT NULL DEFAULT 'member',
  is_active TINYINT(1) NOT NULL DEFAULT 1,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_users_user_id (user_id),
  UNIQUE KEY uk_users_login_id (login_id),
  UNIQUE KEY uk_users_email (email)
);

CREATE TABLE IF NOT EXISTS user_groups (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  group_id CHAR(36) NOT NULL,
  group_name VARCHAR(128) NOT NULL,
  description TEXT NULL,
  created_by_user_id CHAR(36) NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_user_groups_group_id (group_id),
  UNIQUE KEY uk_user_groups_group_name (group_name),
  CONSTRAINT fk_user_groups_created_by_user
    FOREIGN KEY (created_by_user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS group_owners (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  group_id CHAR(36) NOT NULL,
  user_id CHAR(36) NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_group_owners_group_user (group_id, user_id),
  CONSTRAINT fk_group_owners_group
    FOREIGN KEY (group_id) REFERENCES user_groups(group_id),
  CONSTRAINT fk_group_owners_user
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS group_memberships (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  group_id CHAR(36) NOT NULL,
  user_id CHAR(36) NOT NULL,
  membership_status ENUM('invited', 'active', 'suspended') NOT NULL DEFAULT 'invited',
  joined_at DATETIME(3) NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_group_memberships_group_user (group_id, user_id),
  KEY idx_group_memberships_user_id (user_id),
  CONSTRAINT fk_group_memberships_group
    FOREIGN KEY (group_id) REFERENCES user_groups(group_id),
  CONSTRAINT fk_group_memberships_user
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS user_scoped_permissions (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  user_id CHAR(36) NOT NULL,
  group_id VARCHAR(128) NOT NULL,
  read_level TINYINT UNSIGNED NOT NULL,
  write_level TINYINT UNSIGNED NOT NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_user_scoped_permissions_user_group (user_id, group_id),
  CONSTRAINT chk_user_scoped_permissions_read_range CHECK (read_level BETWEEN 1 AND 99),
  CONSTRAINT chk_user_scoped_permissions_write_range CHECK (write_level BETWEEN 1 AND 99),
  CONSTRAINT chk_user_scoped_permissions_order CHECK (read_level >= write_level),
  CONSTRAINT fk_user_scoped_permissions_user
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS group_invitations (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  invitation_id CHAR(36) NOT NULL,
  group_id CHAR(36) NOT NULL,
  invited_user_id CHAR(36) NOT NULL,
  invited_by_user_id CHAR(36) NOT NULL,
  invitation_status ENUM('pending', 'accepted', 'declined', 'expired') NOT NULL DEFAULT 'pending',
  expires_at DATETIME(3) NULL,
  accepted_at DATETIME(3) NULL,
  created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  UNIQUE KEY uk_group_invitations_invitation_id (invitation_id),
  KEY idx_group_invitations_invited_user (invited_user_id),
  CONSTRAINT fk_group_invitations_group
    FOREIGN KEY (group_id) REFERENCES user_groups(group_id),
  CONSTRAINT fk_group_invitations_invited_user
    FOREIGN KEY (invited_user_id) REFERENCES users(user_id),
  CONSTRAINT fk_group_invitations_invited_by_user
    FOREIGN KEY (invited_by_user_id) REFERENCES users(user_id)
);
