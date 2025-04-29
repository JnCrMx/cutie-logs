CREATE TABLE cleanup_rules (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    description TEXT,
    enabled BOOLEAN NOT NULL DEFAULT TRUE,
    execution_interval INTERVAL NOT NULL,

    minimum_age INTERVAL NOT NULL,
    include_resources INTEGER[],
    exclude_resources INTEGER[],
    include_scopes TEXT[],
    exclude_scopes TEXT[],
    include_attributes TEXT[],
    exclude_attributes TEXT[],
    include_attribute_values JSONB CHECK (jsonb_typeof(include_attribute_values) = 'object'),
    exclude_attribute_values JSONB CHECK (jsonb_typeof(exclude_attribute_values) = 'object'),

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX cleanup_rules_name_index ON cleanup_rules (name);
CREATE INDEX cleanup_rules_created_at_index ON cleanup_rules (created_at);
CREATE INDEX cleanup_rules_updated_at_index ON cleanup_rules (updated_at);
