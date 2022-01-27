-- This file should undo anything in `up.sql`

alter table files
    drop column current_version,
    add column content text;

drop table content_versions;
