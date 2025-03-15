CREATE TYPE log_severity AS ENUM (
    'UNSPECIFIED',
    'TRACE', 'TRACE2', 'TRACE3', 'TRACE4',
    'DEBUG', 'DEBUG2', 'DEBUG3', 'DEBUG4',
    'INFO', 'INFO2', 'INFO3', 'INFO4',
    'WARN', 'WARN2', 'WARN3', 'WARN4',
    'ERROR', 'ERROR2', 'ERROR3', 'ERROR4',
    'FATAL', 'FATAL2', 'FATAL3', 'FATAL4'
);

CREATE TABLE logs (
    resource INTEGER NOT NULL,
    timestamp TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    scope TEXT NOT NULL,
    severity log_severity NOT NULL,
    attributes JSONB NOT NULL,
    body JSONB NOT NULL,

    PRIMARY KEY(resource, timestamp, scope)
);
