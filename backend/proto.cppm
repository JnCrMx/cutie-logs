module;

#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.hpp"
#include "opentelemetry/proto/logs/v1/logs.pb.hpp"
#include "opentelemetry/proto/common/v1/common.pb.hpp"

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
    namespace common::v1 {
        using opentelemetry::proto::common::v1::AnyValue;
        using opentelemetry::proto::common::v1::ArrayValue;
        using opentelemetry::proto::common::v1::KeyValue;
        using opentelemetry::proto::common::v1::KeyValueList;
    }
}

export namespace hpp_proto {
    extern "C++" {
        using hpp_proto::read_binpb;
        using hpp_proto::default_traits;
    }
}
