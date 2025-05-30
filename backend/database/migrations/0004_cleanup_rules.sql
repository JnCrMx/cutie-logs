CREATE TYPE filter_type AS ENUM (
    'INCLUDE', 'EXCLUDE'
);

CREATE TABLE cleanup_rules (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    description TEXT,
    enabled BOOLEAN NOT NULL DEFAULT TRUE,
    execution_interval INTERVAL NOT NULL,

    filter_minimum_age INTERVAL NOT NULL,
    filter_resources INTEGER[] NOT NULL DEFAULT '{}',
    filter_resources_type filter_type NOT NULL DEFAULT 'INCLUDE',
    filter_scopes TEXT[] NOT NULL DEFAULT '{}',
    filter_scopes_type filter_type NOT NULL DEFAULT 'INCLUDE',
    filter_severities log_severity[] NOT NULL DEFAULT '{}',
    filter_severities_type filter_type NOT NULL DEFAULT 'INCLUDE',
    filter_attributes TEXT[] NOT NULL DEFAULT '{}',
    filter_attributes_type filter_type NOT NULL DEFAULT 'INCLUDE',
    filter_attribute_values jsonb NOT NULL DEFAULT '{}'::jsonb CHECK (jsonb_typeof(filter_attribute_values) = 'object'),
    filter_attribute_values_type filter_type NOT NULL DEFAULT 'INCLUDE',

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_execution TIMESTAMP
);
CREATE INDEX cleanup_rules_name_index ON cleanup_rules (name);
CREATE INDEX cleanup_rules_created_at_index ON cleanup_rules (created_at);
CREATE INDEX cleanup_rules_updated_at_index ON cleanup_rules (updated_at);
