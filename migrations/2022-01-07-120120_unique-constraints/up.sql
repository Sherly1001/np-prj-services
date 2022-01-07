-- Your SQL goes here

alter table users add constraint username_unique unique (username);

alter table user_file_permissions
add constraint user_file_unique unique (user_id, file_id);
