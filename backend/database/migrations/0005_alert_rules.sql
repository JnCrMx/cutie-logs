CREATE TABLE alert_rules (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    description TEXT,
    enabled BOOLEAN NOT NULL DEFAULT TRUE,

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

    notification_provider TEXT NOT NULL,
    notification_options jsonb NOT NULL,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    last_alert TIMESTAMP,
    last_alert_successful BOOLEAN DEFAULT NULL,
    last_alert_message TEXT DEFAULT NULL,
);
CREATE INDEX alert_rules_name_index ON alert_rules (name);
CREATE INDEX alert_rules_created_at_index ON alert_rules (created_at);
CREATE INDEX alert_rules_updated_at_index ON alert_rules (updated_at);
