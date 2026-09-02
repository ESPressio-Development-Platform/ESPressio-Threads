# Manager and Registry

`ThreadManager` tracks registered Threads, assigns/validates identities, provides iteration/snapshot facilities and owns automatic-cleanup eligibility/claims.

## Registry semantics

The manager is the authoritative registry for live ESPressio Thread objects. Registration failures are explicit, including invalid/null registration, duplicate custom IDs and exhaustion of the configured ID space.

## Lock boundaries

Manager snapshots exist so registry locks do not remain held while virtual methods, observers or other user-controlled code execute.

The 1.0.0 memory policy places ESPressio-owned variable-size registry and snapshot bookkeeping in `ExternalPreferred` storage where the System provider supports it.

## Iteration and cleanup

Active iteration is tracked so cleanup that would invalidate an iteration can be deferred safely. Final iteration release can perform pending cleanup.

## Ownership

The manager decides when automatically releasable Thread objects can be claimed and removed. A termination dispatcher requests/evaluates cleanup through the manager rather than deleting arbitrary Thread objects itself.