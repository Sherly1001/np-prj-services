-- This file should undo anything in `up.sql`

alter table users drop constraint username_unique;
alter table user_file_permissions drop constraint user_file_unique;
