CREATE KEYSPACE keyspace_comments WITH REPLICATION = {'class' : 'SimpleStrategy', 'replication_factor' : 3};

USE keyspace_comments;

CREATE TABLE IF NOT EXISTS comments (
    entity text,
    comment_id uuid,
    author text,
    text text,
    deleted boolean,
    created_by bigint,
    created_time bigint,
    updated_time bigint,
    PRIMARY KEY ((entity), created_time, comment_id)
) WITH CLUSTERING ORDER BY (created_time DESC);

CREATE INDEX ON comments (deleted);

INSERT INTO comments (comment_id, entity, author, text, deleted, created_by, created_time, updated_time)
VALUES (uuid(), 'preset-1234', 'user123', 'Отличный комментарий!', false, 1, toUnixTimestamp(now()), toUnixTimestamp(now()));
