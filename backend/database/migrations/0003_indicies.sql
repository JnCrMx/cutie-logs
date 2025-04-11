CREATE INDEX logs_resource_index ON logs ("resource");
CREATE INDEX logs_timestamp_index ON logs ("timestamp");
CREATE INDEX logs_scope_index ON logs ("scope");
CREATE INDEX logs_severity_index ON logs ("severity");
CREATE INDEX logs_attributes_index ON logs USING GIN ("attributes");
CREATE INDEX logs_body_index ON logs USING GIN ("body");
