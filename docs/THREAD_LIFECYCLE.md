# Consolidated physical Thread lifecycle

The owner constructs a concrete Thread, installs System providers, initializes,
starts/resumes, pauses if needed, requests termination and performs an external
Shutdown before destroying members used by application hooks. There is no manager
registration, assigned Thread ID, automatic object deletion, dispatcher or second
termination task.

Initialize is transactional. Root signal/stack validation, application initialization,
capability initialization and binding finalization all precede task publication.
The joinable task begins behind the same common work latch; only a successful
publication opens that gate. Failure unwinds entered capabilities, application
initialization and provisional signal storage. The state returns to Uninitialized.
A successful Initialize freezes topology and leaves the task blocked in Initialized.

Start/Resume commits Running and a new activation generation before Wake. Worker-side
activation completes before an application quantum can be claimed. Pause commits
Paused before Wake; in-flight bounded work may finish, but subsequent claims fail.
Bounded capability admission while paused remains capability-owned.

A quantum claim under the short root control lock linearizes the start of a bounded
quantum against Pause and Terminate. Locks are released before application service.
ThreadAdmissionClaim uses the same short gate for bounded ordinary-context domain
publication. IsAccepting alone is only a state snapshot; an admission claim is the
linearization point. Release the claim before Wake or application code. Termination
closes new claims and IsAccepting immediately, interrupts the common
wait, then quiesces every capability and performs root teardown. Pending framework
ownership is released without starting further domain callbacks. A claimed quantum
is already in flight and may finish; termination is cooperative, never forceful.

The entry boundary captures the first escaping exception, records its phase/time,
commits termination and quiesces the whole composition. A teardown failure cannot
skip other capabilities or replace the first fatal cause. No application callback
is retried. Reinitialization is explicit and only follows successful Shutdown.

The root entry returns normally into the System provider trampoline. The provider
owns native task exit and join completion; Join does not return until the entry has
returned and no context access remains. No native TLS slots, native task lookup,
forced suspension or external task deletion is used by Threads. A worker Shutdown
only requests termination and reports SelfJoin; an external owner finishes Join.

The common signal is never reset at the final readiness/wait boundary. A racing
producer either contributes readiness seen by the final scan or leaves a latched
signal that makes the subsequent wait return. Deadline expiry re-evaluates current
capability state; it is not another work record. Retaining the signal after a
successful initialization makes stale Wake within the live object's lifetime safe.
Providers must outlive the object; external callers must not outlive the owner.
