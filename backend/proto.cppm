module;

#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"

export module proto;

export namespace opentelemetry::proto {
    namespace collector::logs::v1 {
        using opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest;
        using opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse;
        using opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess;
    }
    namespace logs::v1 {
        using opentelemetry::proto::logs::v1::ResourceLogs;
    }
}
