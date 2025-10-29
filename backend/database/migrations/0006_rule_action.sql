CREATE TYPE rule_action AS ENUM (
    'DROP', 'TRANSFORM'
);

ALTER TABLE cleanup_rules ADD COLUMN "action" rule_action NOT NULL DEFAULT 'DROP';
ALTER TABLE cleanup_rules ADD COLUMN "action_options" JSONB DEFAULT NULL;
