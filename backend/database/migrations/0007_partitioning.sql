ALTER TABLE logs RENAME TO logs_legacy;
ALTER INDEX logs_pkey RENAME TO logs_legacy_pkey;
ALTER INDEX logs_resource_index RENAME TO logs_legacy_resource_index;
ALTER INDEX logs_timestamp_index RENAME TO logs_legacy_timestamp_index;
ALTER INDEX logs_scope_index RENAME TO logs_legacy_scope_index;
ALTER INDEX logs_severity_index RENAME TO logs_legacy_severity_index;
ALTER INDEX logs_attributes_index RENAME TO logs_legacy_attributes_index;
ALTER INDEX logs_body_index RENAME TO logs_legacy_body_index;

CREATE TABLE logs (
    resource INTEGER NOT NULL,
    timestamp TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    scope TEXT NOT NULL,
    severity log_severity NOT NULL,
    attributes JSONB NOT NULL,
    body JSONB NOT NULL,

    PRIMARY KEY(resource, timestamp, scope)
) PARTITION BY RANGE (timestamp);

CREATE INDEX logs_resource_index ON logs ("resource");
CREATE INDEX logs_timestamp_index ON logs ("timestamp");
CREATE INDEX logs_scope_index ON logs ("scope");
CREATE INDEX logs_severity_index ON logs ("severity");
CREATE INDEX logs_attributes_index ON logs USING GIN ("attributes");
CREATE INDEX logs_body_index ON logs USING GIN ("body");

ALTER TABLE logs ATTACH PARTITION logs_legacy FOR VALUES FROM ('2000-01-01') to (CURRENT_TIMESTAMP);
CREATE TABLE logs_first PARTITION OF logs FOR VALUES FROM (CURRENT_TIMESTAMP) to ((CURRENT_DATE + 1)::timestamp);
