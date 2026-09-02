# Thread Configuration

Thread configuration uses ESPressio concepts rather than native scheduler types.

Key creation-time settings include requested processor/core placement, priority, stack size and lifecycle/cleanup policy.

## Processor placement

Processor count is discovered through ESPressio System execution capabilities. A specific core/processor setting is a request whose validity depends on the installed platform provider and target processor count.

Single-processor targets remain valid; Threads does not require multiple cores.

## Priority

Priority is expressed through the portable Threads/Task configuration surface. The target runtime maps it to its scheduler semantics.

## Stack size

Do not reduce stack defaults merely to reclaim memory. Use stack high-water telemetry under realistic workload and retain a safety margin appropriate to the application.

## Creation-time immutability

Changing stack/priority/core settings while the underlying execution exists is intentionally prevented/ignored according to the public lifecycle contract so getters cannot claim configuration different from the running worker.

## Cleanup policy

Automatic free-on-terminate and explicit release modes affect manager cleanup ownership. Choose deliberately when object lifetime is controlled externally.