# MySQL 初期化手順

## 対象

このディレクトリには、Picaresque の server 側で利用する
MySQL の初期化 SQL を配置する。

現時点では user / group / permission 系の最小スキーマのみを含む。

## ファイル

- `001_create_user_permission_schema.sql`
  - database 作成
  - user / group / invitation / permission テーブル作成
- `002_seed_user_permission_data.sql`
  - 現時点では投入なし
  - 初回セットアップ方式を採用するため空のまま維持
- `003_create_access_control_schema.sql`
  - access_ip_rules
  - IP による入口制御用スキーマ

## 実行順

```bash
mysql -u root -p < server/sql/001_create_user_permission_schema.sql
mysql -u root -p < server/sql/003_create_access_control_schema.sql
```

必要に応じて `002_seed_user_permission_data.sql` は開発用データ投入用に拡張する。

## 補足

- `user_scoped_permissions` の `scope_name='*'` は DB 上は保持可能だが、
  実際の付与制御はアプリケーション側で admin 限定にする
- `1-99` を有効値とし、`0` は許可しない
- `read_level >= write_level` を DB 制約でも保持する
- 初期 admin は SQL seed ではなく API セットアップで作成する
- access 制御は `permission` とは別テーブルで管理する
- API キーは今後 `user/auth` 側のテーブルとして追加する
