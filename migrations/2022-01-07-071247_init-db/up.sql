-- Your SQL goes here

create table users (
    id bigint not null primary key,
    username text not null,
    hash_passwd text not null,
    email text,
    avatar_ur text
);

create table types (
    id int not null primary key,
    type_name text not null
);

create table permissions (
    id int not null primary key,
    permission_type text not null
);

create table files (
    id bigint not null primary key,
    type_id int not null references types(id) on delete set default,
    owner bigint references users(id) on delete set null,
    everyone_can int references permissions(id) on delete set null,
    content text
);

create table user_file_permissions (
    id bigint not null,
    user_id bigint references users(id) on delete cascade,
    file_id bigint references files(id) on delete cascade,
    permission_id int references permissions(id) on delete set null
);
