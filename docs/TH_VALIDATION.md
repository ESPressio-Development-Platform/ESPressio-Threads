# TH1–TH15 semantic test migration

Branch primitives_redesign revalidated at f6f6fe1a8dc687870f71189843611d7174dc70cf.
All four existing workflow files and their embedded C++ probes were read before
executing replacement tests. There were no registered native test sources.

- critical-tls-lifecycle-tests: retain TLS exclusion, pthread and WiFi-driver
  coexistence compilation. Migrate root lifecycle and remove manager calls.
- task-runtime-backend-tests: retain Task/System ownership and native API exclusion.
  Delete positive assertions for forced deletion/suspension/dispatcher/auto-release;
  replace with cooperative exit, no second task, owner-controlled lifetime tests.
- resource-profile-tests: replace configurable termination queue with exact root
  and capability resident storage, one stack and maximum-path floor validation.
- dependency-refresh-tests: replace specialized/Serializable Precision inheritance
  and injectable System clock with the sole host plus monotonic Precision. Retain
  no-RTTI compilation against actual coordinated dependencies.

The implementation removes the manager/dispatcher/release hierarchy and all
specialized Precision inheritance, observer and Serializable representation code.
A platform source scan found no external include or qualified use of ThreadSafe;
that unused dynamic synchronized-value helper is removed as well. New root code
has no Observable, family or native FreeRTOS dependency.

Native evidence is grouped into five fixtures (all pass):

| Fixture | Required behaviors covered |
|---|---|
| thread_lifecycle | Bare idle wait, work dispositions, initialization rollback, typed failures, pause/resume, cooperative termination/join, reinitialization, self-join refusal, first fatal cause, teardown suppression, stack rejection, heap denial |
| composition | Static/get-capability contracts, exact concrete resource accounting, maximum-path stack floor, exactly one context/signal, freeze, partial rollback, both pack-order fairness permutations, one natural quantum, Precision precedence, whole-composition fault/quiescence, no callback replay, heap denial |
| precision | Free eligibility, cadence, skipped nominal slots, runtime period change, stale eligibility rejection, Bump, Start/Resume epoch reset, both delta modes, fixed ring/rolling frequency, bounded active window, explicit execution budget, uint64 cadence exhaustion |
| common_wake_and_deadlines | Wake before/during waits, 40,000 producer attempts, coalesced/stale wakes, ISR-form provider dispatch, no latch reset, bounded paused accumulation, admission/termination linearization, earliest-deadline aggregation, cancellation and explicit UINT64_MAX deadline |
| timing_independence | Actual Timing/System/Units/Observable integration: bootstrap System rebases and accepted continuous synchronization leave Precision monotonic deadlines unchanged; synchronized clocks cannot be injected into the root |

All seven public headers and four README C++ snippets compile independently with
warnings as errors and no RTTI. Nine negative compile cases reject malformed
composition, empty host, duplicate tags, exclusive claims, physical Thread ancestry,
absent capability access, zero Precision storage, external-storage sum overflow
and an untyped clock injection.
An active-source guard checks exact manifest dependencies, predecessor mechanisms,
native API/TLS exclusion and absence of dynamic containers/closures.

Existing package versions and version macros remain unchanged. The ESP32 workflow
compiles the coordinated no-RTTI pthread/WiFi probe and all three maintained examples.
Native tests use the Task test provider; the actual Timing integration stubs only
Arduino String for Units, not the platform libraries. Physical ISR behavior,
worst-case target stack use and cadence lateness require the later ESP32/provider
integration campaign; host success does not certify hardware timing.

The root's single work signal remains lifetime-owned after successful initialization.
Join completion is supplied by the System provider, with no additional Thread work
signal or termination task. Provisional first-initialization resources roll back on
failure. Ordinary admission claims use the short root control gate; state snapshots
alone do not authorize publication across a termination race.

GitHub execution results are recorded in the platform implementation checkpoint.
