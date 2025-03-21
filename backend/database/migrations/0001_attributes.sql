-- This is kind of a cache, but it contains extra analysis information about the types of attributes
CREATE TABLE log_attributes (
    attribute TEXT NOT NULL,
    count INTEGER NOT NULL,

    -- The count of each type of attribute, so we can deduce the type of the attribute
    count_null INTEGER NOT NULL,
    count_number INTEGER NOT NULL,
    count_string INTEGER NOT NULL,
    count_boolean INTEGER NOT NULL,
    count_array INTEGER NOT NULL,
    count_object INTEGER NOT NULL,

    PRIMARY KEY(attribute)
);
