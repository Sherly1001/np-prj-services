-- Your SQL goes here

create table content_versions (
    id bigint not null primary key,
    file_id bigint references files(id) on delete cascade,
    update_by bigint references users(id) on delete set null,
    content text
);

alter table files
    drop column content,
    add column current_version bigint
    not null references content_versions(id) on delete set null;
