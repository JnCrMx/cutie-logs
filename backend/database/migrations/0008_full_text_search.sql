CREATE extension if not exists pg_trgm;

CREATE INDEX logs_body_fulltext_index ON logs USING GIN (jsonb_to_tsvector('english', "body", '"all"'));
CREATE INDEX logs_body_trigram_index ON logs USING GIN ((body::text) gin_trgm_ops);
