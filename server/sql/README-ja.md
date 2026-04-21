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
- `004_rename_groups_table.sql`
  - 旧 `groups` テーブルを `user_groups` へ移行
  - MySQL 予約語衝突の解消用
- `005_create_user_api_keys.sql`
  - `user_api_keys`
  - user ごとの API キー保存
  - 1 user につき 1 key
  - 平文は保存せず hash と prefix のみ保存

## 実行方法

通常は DB コンテナ起動時に自動適用せず、
アプリケーション側の migration runner から適用する。

```bash
cd server
./build/picaresque_migrate
```

上記コマンドは `schema_migrations` テーブルを利用して、
未適用の `.sql` のみをファイル名順で適用する。

必要に応じて `002_seed_user_permission_data.sql` は開発用データ投入用に拡張する。

## 補足

- `user_scoped_permissions` の `scope_name='*'` は DB 上は保持可能だが、
  実際の付与制御はアプリケーション側で admin 限定にする
- `1-99` を有効値とし、`0` は許可しない
- `read_level >= write_level` を DB 制約でも保持する
- 初期 admin は SQL seed ではなく API セットアップで作成する
- access 制御は `permission` とは別テーブルで管理する
- API キーは `user/auth` 側で扱い、権限そのものではなく user identity の確定に利用する
