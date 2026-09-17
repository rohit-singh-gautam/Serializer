#include "example_support.hpp"

#include <rohit/stream.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>

namespace iostream_example {
namespace model = iostream_model;

// Populate inventory, metrics, events, and administrative records with repeatable values.
archive make_archive() {
  constexpr std::uint32_t device_count = 24;
  constexpr std::uint32_t series_per_device = 3;
  constexpr std::uint32_t samples_per_series = 1024;
  constexpr std::uint32_t log_count = 128;
  constexpr std::size_t log_message_bytes = 2048;
  constexpr std::uint64_t sample_period_ns = 1'000'000'000;
  constexpr std::uint64_t capture_start_ns = 1'700'000'000'000'000'000;
  constexpr std::uint64_t mebibyte = 1024 * 1024;

  archive value{};
  auto& metadata = value.metadata;
  metadata.archive_id = "iostream-large-archive";
  metadata.title = "A complete telemetry snapshot";
  metadata.description = "Memory, file, buffered, and custom stream examples";
  metadata.format_revision = 1;
  metadata.created_ns = capture_start_ns;
  metadata.interval.start_ns = capture_start_ns;
  metadata.interval.end_ns = capture_start_ns + sample_period_ns * samples_per_series;
  metadata.interval.sample_period_ns = sample_period_ns;
  metadata.interval.synchronized = true;
  metadata.interval.clock_source = "example-clock";
  metadata.producer.source_id = "collector-01";
  metadata.producer.host_name = "example-host";
  metadata.producer.agent_version = "1.0";
  metadata.producer.trusted = true;
  metadata.collector_build.product = "Serializer iostream example";
  metadata.collector_build.version = "1.0";
  metadata.collector_build.features = {"inventory", "metrics", "events"};
  metadata.owner.organization = "Example operations";
  metadata.owner.primary_contact.name = "Example operator";
  metadata.owner.primary_contact.email = "operator@example.invalid";
  metadata.owner.escalation_contacts = {metadata.owner.primary_contact};
  metadata.retention.policy_id = "local-demo";
  metadata.retention.metrics_days = 7;
  metadata.retention.maximum_archive_bytes = message_limits().max_input_bytes;
  metadata.tags = {"iostream", "large-schema", "round-trip"};
  metadata.labels = {{"purpose", "example"}, {"encoding", "UTF-8"}};

  for (std::uint32_t index = 0; index < device_count; ++index) {
    model::device_snapshot device{};
    device.device_id = "device-" + std::to_string(index);
    device.display_name = "Compute node " + std::to_string(index);
    device.role = model::device_role::compute;
    device.health = model::health_state::healthy;
    device.source = metadata.producer;
    device.owner = metadata.owner;
    device.placement.building = "example-site";
    device.placement.rack_name = "rack-01";
    device.placement.latitude = 12.5;
    device.operating_system.family = "example-os";
    device.operating_system.architecture = "portable";
    device.agent_build = metadata.collector_build;
    device.configuration.profile = "standard";
    device.configuration.collection_enabled = true;
    device.configuration.enabled_metrics = {"cpu", "memory", "requests"};
    device.processors.resize(1);
    device.processors.front().model = "Example processor";
    device.processors.front().cores = 8;
    device.processors.front().core_utilization = {0.25, 0.5, 0.75};
    device.memory.total_bytes = 8192 * mebibyte;
    device.memory.available_bytes = 4096 * mebibyte;
    device.disks.resize(1);
    device.disks.front().device_id = "disk-0";
    device.disks.front().capacity_bytes = 65536 * mebibyte;
    device.interfaces.resize(1);
    device.interfaces.front().name = "net0";
    device.interfaces.front().addresses = {"192.0.2.1"};
    device.interfaces.front().link_up = true;
    device.processes.resize(1);
    device.processes.front().executable = "collector";
    device.processes.front().process_id = 100 + index;
    device.containers.resize(1);
    device.containers.front().name = "example-service";
    device.temperatures.resize(1);
    device.temperatures.front().current_c = 42.5;
    device.fans.resize(1);
    device.fans.front().current_rpm = 1200;
    device.power_supplies.resize(1);
    device.power_supplies.front().output_w = 250.5;
    device.accelerators.resize(1);
    device.accelerators.front().model = "Example accelerator";
    device.counters.resize(1);
    device.counters.front().name = "completed_requests";
    device.counters.front().value.integer_value = 4096 + index;
    device.histograms.resize(1);
    device.histograms.front().name = "latency_ms";
    device.histograms.front().bounds = {1.0, 2.0, 4.0};
    device.histograms.front().counts = {10, 20, 30};
    device.labels = {{"group", "demo"}, {"index", std::to_string(index)}};

    for (std::uint32_t series_index = 0; series_index < series_per_device; ++series_index) {
      model::metric_series series{};
      series.name = "metric-" + std::to_string(series_index);
      series.unit = "count";
      series.period_ns = sample_period_ns;
      series.labels = {{"device", device.device_id}};
      for (std::uint32_t sample = 0; sample < samples_per_series; ++sample) {
        series.timestamps_ns.push_back(capture_start_ns + sample * sample_period_ns);
        // Binary fractions retain exact values in both binary and JSON round trips.
        series.values.push_back(static_cast<double>(sample + index + series_index) / 4.0);
        series.quality_flags.push_back(1);
      }
      series.minimum = series.values.front();
      series.maximum = series.values.back();
      series.average = (series.minimum + series.maximum) / 2.0;
      device.metrics.push_back(std::move(series));
    }
    value.devices.push_back(std::move(device));
  }

  for (std::uint32_t index = 0; index < log_count; ++index) {
    model::log_entry entry{};
    entry.timestamp_ns = capture_start_ns + index * sample_period_ns;
    entry.sequence = index;
    entry.level = model::severity::info;
    entry.source = "collector";
    entry.message = "line\n\"quoted\" \\ path UTF-8: \xc2\xb5 " +
                    std::string(log_message_bytes, static_cast<char>('a' + index % 26));
    entry.attributes = {{"sequence", std::to_string(index)}};
    value.logs.push_back(std::move(entry));
  }
  value.traces.resize(1);
  value.traces.front().operation = "capture";
  value.traces.front().duration_ns = sample_period_ns;
  value.traces.front().events = {"started", "completed"};
  value.devices.front().logs = {value.logs.front()};
  value.devices.front().traces = value.traces;

  value.services.resize(1);
  auto& service = value.services.front();
  service.configuration.service_id = "service-01";
  service.configuration.build = metadata.collector_build;
  service.configuration.endpoints.resize(1);
  service.configuration.endpoints.front().host = "example.invalid";
  service.configuration.endpoints.front().port = 443;
  service.configuration.dependencies.resize(1);
  service.configuration.dependencies.front().target_service = "storage";
  service.status.health = model::health_state::healthy;
  service.status.ready_replicas = 2;
  service.certificates.resize(1);
  service.certificates.front().subject = "example.invalid";
  service.device_ids = {value.devices.front().device_id};
  service.complete = true;

  value.sites.resize(1);
  value.sites.front().name = "Example site";
  value.sites.front().zones.resize(1);
  value.sites.front().zones.front().name = "zone-a";
  value.sites.front().racks.resize(1);
  value.sites.front().racks.front().rack_id = "rack-01";
  value.links.resize(1);
  value.links.front().link_id = "link-01";
  value.links.front().source_device = value.devices.front().device_id;
  value.routes.resize(1);
  value.routes.front().destination = "192.0.2.0";
  value.routes.front().prefix_length = 24;
  value.volumes.resize(1);
  value.volumes.front().volume_id = "volume-01";
  value.volumes.front().total_bytes = 65536 * mebibyte;
  value.backups.resize(1);
  value.backups.front().backup_id = "backup-01";
  value.backups.front().verified = true;
  value.alert_rules.resize(1);
  value.alert_rules.front().rule_id = "cpu-warning";
  value.alert_rules.front().thresholds = {{"warning", 0.75}};
  value.alerts.resize(1);
  value.alerts.front().rule_id = value.alert_rules.front().rule_id;
  value.alerts.front().level = model::severity::warning;
  value.policies.resize(1);
  value.policies.front().action = "read";
  value.policies.front().decision = "allow";
  value.audit.resize(1);
  value.audit.front().action = "capture";
  value.audit.front().changed_fields = {"last_capture"};
  value.deployments.resize(1);
  value.deployments.front().deployment_id = "deployment-01";
  value.deployments.front().state = model::deployment_state::succeeded;
  value.deployments.front().steps.resize(1);
  value.deployments.front().steps.front().name = "verify";
  value.maintenance.resize(1);
  value.maintenance.front().title = "Example maintenance";
  value.maintenance.front().interval = metadata.interval;
  value.exports.resize(1);
  value.exports.front().job_id = "export-01";
  value.dashboards.resize(1);
  value.dashboards.front().title = "Overview";
  value.dashboards.front().panels.resize(1);
  value.dashboards.front().panels.front().query = "metric-0";
  value.incidents.resize(1);
  value.incidents.front().title = "Example incident";
  value.incidents.front().responders = metadata.owner.escalation_contacts;
  value.summary.device_count = device_count;
  value.summary.service_count = static_cast<std::uint32_t>(value.services.size());
  value.summary.site_count = static_cast<std::uint32_t>(value.sites.size());
  value.summary.alert_count = static_cast<std::uint32_t>(value.alerts.size());
  value.summary.sample_count = device_count * series_per_device * samples_per_series;
  value.summary.log_count = value.logs.size() + value.devices.front().logs.size();
  value.summary.health = model::health_state::healthy;
  value.annotations = {{"note", "one complete EOF-delimited message"}};
  return value;
}

// Keep staging and decoding comfortably bounded above this demonstration's actual message size.
codec::decode_limits message_limits() {
  constexpr std::size_t mebibyte = 1024 * 1024;
  codec::decode_limits limits{};
  limits.max_input_bytes = 16 * mebibyte;
  limits.max_string_bytes = mebibyte;
  limits.max_collection_elements = mebibyte;
  limits.max_nesting_depth = 32;
  limits.max_allocation_bytes = 32 * mebibyte;
  limits.max_work_units = 64 * mebibyte;
  return limits;
}

// Canonical binary bytes compare all fields, including nested collections and default-valued fields.
void verify_round_trip(const archive& expected, const archive& actual, std::size_t encoded_bytes) {
  if (encoded_bytes < minimum_payload_bytes) {
    throw std::runtime_error{"Example payload must exceed multiple stream batches"};
  }
  rohit::full_stream_auto_alloc expected_bytes;
  rohit::full_stream_auto_alloc actual_bytes;
  expected.serialize_out<codec::binary_integer>(expected_bytes);
  actual.serialize_out<codec::binary_integer>(actual_bytes);
  if (expected_bytes.current_offset() != actual_bytes.current_offset() ||
      !std::equal(expected_bytes.begin(), expected_bytes.curr(), actual_bytes.begin())) {
    throw std::runtime_error{"Decoded archive differs from the original"};
  }
}

// Creation is exclusive; a collision retries without touching the other owner's directory.
temporary_file::temporary_file() {
  constexpr unsigned maximum_attempts = 64;
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = std::filesystem::temp_directory_path();
  for (unsigned attempt = 0; attempt < maximum_attempts; ++attempt) {
    const auto candidate =
        root / ("serializer-iostream-" + std::to_string(stamp) + "-" + std::to_string(attempt));
    if (std::filesystem::create_directory(candidate)) {
      directory = candidate;
      path = directory / "message.bin";
      return;
    }
  }
  throw std::runtime_error{"Unable to create an unused temporary directory"};
}

// Stream owners are declared after this owner so their handles close before file cleanup.
temporary_file::~temporary_file() {
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  std::filesystem::remove(directory, ignored);
}

// Expose only the owned message's path, not ownership of its directory.
const std::filesystem::path& temporary_file::name() const {
  return path;
}
} // namespace iostream_example
