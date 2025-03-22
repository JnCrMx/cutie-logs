CREATE TABLE log_resources (
    id SERIAL PRIMARY KEY,
    attributes JSONB NOT NULL,
    created_at TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT NOW(),

    CONSTRAINT log_resources_attributes_check CHECK (jsonb_typeof(attributes) = 'object'),
    CONSTRAINT log_resources_attributes_unique UNIQUE (attributes)
);
CREATE INDEX log_resources_created_at_index ON log_resources (created_at);
CREATE INDEX log_resources_attributes_index ON log_resources USING GIN (attributes);
