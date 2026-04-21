USE picaresque;

UPDATE users
SET password_hash = SHA2(CONCAT('password:', password_hash), 256)
WHERE password_hash NOT REGEXP '^[0-9a-f]{64}$';

DELETE user_scoped_permissions
FROM user_scoped_permissions
INNER JOIN users ON users.user_id = user_scoped_permissions.user_id
WHERE users.role = 'admin'
  AND user_scoped_permissions.group_id = '*';
