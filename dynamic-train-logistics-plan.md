# Dynamic Train Logistics

An LTN/Cybersyn-style demand-driven train dispatcher for Satisfactory.

Stations declare what they have and what they want.
A dispatcher matches supply to demand and writes train schedules at runtime.
The player never hand-builds a timetable again.

---

## 1. Key decisions

**Bay index equals car index, at both ends.**
Freight platform *N* only ever transfers with freight car *N*.
Empty platforms consume an index slot.
A delivery is not station-to-station, rather **bay-to-bay** (since bays can mix load/unload, and may have differing items even inside a single station).
This is the structural difference from LTN, which matches wagon *counts* and handles items dynamically.
As a result, two stations can have identical bay counts and still be ineligible to route to each other.

**A train must always be moving, docking, or in a depot.**
A train can never idle in a cargo bay.
An idle train parked in a requester bay blocks the next train dispatched there, potentially causing deadlocks.
Therefore depots are mandatory and **total depot parking spaces must be greater than or equal to fleet size** (just like LTN/Cybersyn).

**Overshoot leaves residue, and residue needs an outlet.**
Satisfactory always fully loads train cars, with no ability to match arbitrary requests.
The requester unloads as much as fits (unload filters only match on item), potentially leaving remainders in the freight cars.

The dispatcher therefore must support clearing it (see *Draining residue*), so it will not result in contaminated stations nor accumulate over time. If there are **no** requests for the item **and** no unload bay anywhere for it with room, then the car will hold it indefinitely.

One or both building styles are required to make this tenable for a scaled network:

- **Full load requests cannot overshoot.** If the request equals car capacity, full load *is* the request. Without player-built gating to implement partial loads, this is the only way to ensure overshoot never occurs. Partial loads are achievable, just not by the dispatcher alone (see *Belt gating*).
- **A full car-sized requester buffer makes a full car always absorbable.** (with correct configuration) Storage behind each unload bay must match or exceed one car's full contents, or the bay backs up and the car keeps the difference. Threshold for request should also ALWAYS be at least a full car, to ensure there is space to unload it.

---

## 2. Concept model

This model requires:
1. Bays carry inventory constraints
2. Depot, station, and drain controllers carry bindings to world entities
3. The dispatcher owns decision-making about station eligibility and inventory levels
4. Orders transiently combine items required by bays that are connected to stations

### Bays

**Bays** are the lowest common denominator available for delivery scheduling, as they can either provide **OR** require items.

From the world placement, we know their:

- index (how many platforms are between it and the station)
- mode/direction (load or unload, set by the player in the vanilla UI)
- whether it is a fluid platform, from its class
- contents (visible via game API)

Players must provide:

- which item should be (un)loaded here (can guess from contents if uniform and non-empty)
- threshold (to make delivery worthwhile, must be either HAVE [load] or be MISSING [unload] this much)

Because bay index equals car index, a bay is also the unit that gets matched when creating/dispatching orders.

### Station Controller

One per participating station, wired to it via Circuitry; the station is the only mandatory binding.

From the binding, we know:

- station identity, and its name for display
- every bay on it, by walking the platform chain (see *Key technical references*)
- docking state

Players must provide:

- network name (which stations may interact with each other)
- max inbound trains (how many may be destined for or docked here at once)

It knows nothing about any other station, so adding one to the network is placing a building and wiring it.
Station controllers output desired inventory amounts, a ready flag and its own problems as wire outputs (see *Published outputs*).

### Depot Controller

One per depot station, wired the same way.
This is a separate buildable rather than a mode on the Station Controller, since a depot has no need to request or provide anything.

From the binding, we know station identity and current occupancy.

Players must provide:

- parking spaces (how many trains fit)
- max train length in cars

A depot has no bays, so nothing is read from its chain.

### Drain Controller

Optional, one per drain site, wired the same way.
This is where leftovers go when nothing in the network wants them (see *Draining residue*).

From the binding, we know station identity and every bay on it, walked as normal.

Players must provide:
- max inbound trains

Since a drain station by design accepts any item (or fluid), further configuration is not needed and its bay table renders read-only.
Since there are no configured items, its bays never enter the matching index so the dispatcher never treats it as a provider or requester.

Note: a network with no drain station still works but excess residue removes trains from the eligible pool and raises a `Problem` instead.

### Dispatcher

A subsystem, not a placed entity.

This is the only part of the mod requiring global scope, with access to all depots, stations, and bays.
It matches supply to demand, reserves stock, picks trains, and writes timetables.
Controllers still own registration, so the dispatcher is not responsible for missing or mis-configured controllers.

### Orders

A transient record of one delivery: item, source and destination stations, the bay indices in play, the train, and wanted inventory against delivered.
Lives in the dispatcher and is never written into vanilla state.

### Coupling

Bays determine whether an order *could* exist, while stations determine whether it *may proceed now*.

Controllers only define local state for *their* bays and station, so if they are removed no cleanup pass is necessary.
The dispatcher can rebuild its state for related item(s) trivially, removing this station and bay(s).

Vanilla timetables are shared mutable state, so exactly one authority may write them.
Controllers only read and publish inventory levels, allowing the dispatcher to own the time table.
This is why dTT is a conflict (see *Mod relationships*).

### How player values arrive

- **Typed** into the UI. The normal path.
- **Wired** from Circuitry, overriding any typed field with a live signal. Opt-in per field, so dynamic is available but never mandatory.
- **Suggested** from bay contents, for the item field only.

Suggestion is reliable on load bays, which buffer items, and only works on unload bays when processing there is slow enough to leave a backlog.

Belt contents are not a usable source, since Circuitry's belt gate counts throughput past a point, not what a belt is carrying holistically.
Neither are the platform's own filter arrays (see *Key technical references*).

### Data structures

Small footprint, because the matching index is built from **config**, not contents.

All amounts are `Quantity`, fixed-point to two decimal places.
Solids are whole numbers and fluids are fractional, and since exact quantities are unreachable anyway (see *Known limitations*) rounding at hundredths costs nothing.
One type provides a single path through matching, reservation, and orders for fluids and solids.

Bay room is likewise two fields rather than one, because there are two questions and only one of them is type-dependent.
`canPartialUnload` answers *can it accept at all*, which is slot-based for solids and headroom for fluids, so the difference stays inside the boolean.
`free` answers *how much*, in item units either way, which is what drain checks and the requester ceiling need. Slot counts never leave the chain walk.

```
BayRef {
  station: StationId
  index:   int
}

BayState {                                 # everything the platform chain gives us
  index:            int                    # position in the chain
  mode:             enum { None, Load, Unload }  # None = empty platform, still holds an index
  isFluid:          bool                   # from the platform class
  contents:         map<ItemDescriptor, Quantity>
  capacity:         Quantity
  free:             Quantity               # how much more it can take. solids: empty slots x stack + partial-stack room
  canPartialUnload: bool                   # whether it can take anything at all. solids: a fully empty slot exists. fluids: headroom
}

Bay : BayState {                           # a bay the player has configured
  item:      ItemDescriptor?
  threshold: Quantity                      # minimum to make delivery worthwhile. amount present for `load`, missing for `unload`.
  committed: Quantity                      # promised to orders, not yet loaded
  inflight:  Quantity                      # en route to this bay
}

Station {
  id:          StationId                  # AFGTrainStationIdentifier
  name:        string                     # cache, for display and re-resolution
  network:     string
  bays:        Bay[]                      # ordered; position is the index
  maxInbound:  int
  inbound:     int                        # destined for or docked here
  mixedDelivery: bool                     # unavailable if any bay is Load
  bound:       bool                       # false once its station is gone
}

Depot {
  station:        StationId
  network:        string
  parkingSpaces:  int                     # number of trains that can park here
  occupied:       int                     # int to allow multiple, if parking spaces > 1
  maxTrainLength: int                     # in cars; 0 is unconstrained
}

DrainStation {
  station:    StationId
  network:    string
  bays:       BayState[]                  # drain targets; read-only, used when draining residue
  maxInbound: int
  inbound:    int
}

Car {
  isFluid:  bool                          # from the vehicle class
  item:     ItemDescriptor?
  amount:   Quantity
  capacity: Quantity
}

Train {
  id:       TrainId
  network:  string
  cars:     Car[]                         # position is the bay index it meets
  atDepot:  StationId?                    # set means available
  order:    OrderId?
}

Order {
  id:        OrderId
  item:      ItemDescriptor
  provider:  StationId
  requester: StationId
  indices:   int[]                        # bays at both ends
  wanted:    Quantity
  delivered: Quantity
  train:     TrainId
  stage:     enum { ToProvider, Loading, ToRequester, Unloading, ToDepot, Done }
}

Problem {
  severity: enum { Info, Warn, Error }
  code:     enum { NoProvider, NoIndexOverlap, NoEligibleTrain, AtInboundLimit,
                   ResidueNoOutlet, DrainBayCoverage, Unbound, Overshoot, ... }
  item:     ItemDescriptor?
  station:  StationId?
  train:    TrainId?
}
```

Working state:

```
stations:       map<StationId, Station>
depots:         map<StationId, Depot>
drainStations:  map<StationId, DrainStation>
trains:         map<TrainId, Train>
orders:         map<OrderId, Order>       # active only
history:        Order[]                   # ring buffer, configurable length
providerIndex:  map<ItemDescriptor, BayRef[]>
requesterIndex: map<ItemDescriptor, BayRef[]>
noProviderFor:  set<ItemDescriptor>       # negative cache
problems:       Problem[]
residueTrains:  set<TrainId>              # residue with no outlet; membership is the exclusion
```

Only player-provided configuration persists, since all world-derivable state can be detected on load again.
The `Bay`/`BayState` split falls on that boundary: `BayState` is always read from the world, and only the fields `Bay` adds are ever saved.
Everything else is read from the world each cycle (`contents`, `mode`, `cars`, `inbound`, `occupied`) or rebuilt on load from live timetables and docking state (`orders`, `committed`, `inflight`).

Matching is a simple lookup.
Bay contents between polls are unknown, but bay configuration is what constrains eligibility, so that is all the index needs.
Unlike LTN (which reads live circuit signals), this trusts declared config and uses an atomic read-and-reserve on contents prior to dispatch to keep orders consistent.

---

## 3. User-facing design

### Setup

**Per station:** place a Station Controller, wire it to the station, set item and threshold on the bay rows you care about.
Roles come from bay modes, so a station with both load and unload bays is a provider and a requester with no extra configuration.

**Per depot:** place a Depot Controller, wire it to the station, set parking spaces and the longest train that fits.
Separate from the Station Controller because a depot has no bays, items or thresholds.

**Per drain site:** place a Drain Controller and wire it.
Only max inbound is configurable; its bays are only needed to match a train with residue.

**Per train:** nothing. Self-driving, on the network's track, parked in a depot.

### Binding

Binding is physical: wire the controller to the station.
Names cannot serve here because duplicate station names are legal and a picker cannot tell two identical names apart.

This needs a connection descriptor registering the Train Station as wireable, since Circuitry ships one for the freight platform but not the station.
**Circuitry is therefore a hard dependency**, which also provides the Notificator, signal gating, and wired overrides.

A rebuilt station is a new object with no way to reattach by name, so its controller becomes unbound and the player re-wires it.

Controllers are interactable (E opens the UI).

### Defaults

Every field has a working default, so a placed-and-wired controller works without the panel ever being opened:

- **Network**: one shared network, hidden until a second is needed.
- **Provider threshold**: a quarter of a car, computed per bay so it scales with stack size and works for fluids. Preparation continues during travel, so this only asks whether an order is worth committing.
- **Requester threshold**: the highest level the bay can still receive into, which is also the maximum. For solids that is one stack below capacity, since vanilla requires a fully empty slot to unload partially; for fluids it is a headroom margin. The player types a plain number either way and the bay computes its own ceiling.
- **Max inbound trains**: 1. The mod cannot see whether a holding lane exists, so raising it is the deliberate act of someone who built buffer track.
- **Depot max train length**: 0, meaning unconstrained.

### Import from existing timetables

A converted network already has the player's intent in its stop filters, so walk every train's timetable and prefill the bay tables.

A stop gives an item set per direction with no bay association, so infer:

1. Split by direction. The load list applies to load bays, the unload list to unload bays.
2. One bay and one item in that direction resolves unambiguously.
3. Several of either resolves on current bay contents.

Step 3 covers most real networks, since a bay per item is how people frequently build.
Two bays holding the same item, or a bay empty at import time, cannot be resolved and are flagged for review.

Prefill only; never convert before the player has reviewed.

### Management UI

Management is station-local, so interacting with a controller shows that station's tabs.
A global view across all stations with search and collapsible groups is deferred at this time.

Tab order is **Status | Config | Problems**, opening on Problems when any exist, Config on invalid state (any mandatory config empty), and Status otherwise.
Problems scopes to the station you opened it from, so walking up to a working station does not show another's error.

- **Status** — supply and demand for this station, plus its active orders.
- **Config** — the bay table. Index, mode, contents, item, threshold. Rows come from the platform chain, so the table always reflects what is built.
- **Problems** — see below.

### Published outputs

To provide the player with the ability to prepare for a train's arrival, the station controller publishes:

- **desired amounts** — item and amount, indexed by bay, for the next outbound order. Load bays only, since a mixed station can be provider on one order and requester on another at once.
- **ready flag** — whether every participating bay holds the requested amount.
- **status string** — for display or notification text.

> Note: The Depot Controller publishes residue and drain problems.
> A stuck train is parked at a depot, so that is where the problem is observable (an unmatched drain station is not routable, so has no train to report about).
> A drain station reports only its own coverage gaps, which are configuration-based rather than a failed order.

Players use these for:

- **gating** a train at a rail signal until the ready flag sets, which avoids the short-car case in *Dispatch timing*
- **alerting** through Circuitry's Notificator, which takes a fire bool, title, description and duration
- **metering** a bay's feed belt against the desired amount, which is what makes partial loads exact (see *Belt gating*)

A notifier should be wired per station, and per depot for residue.
One depot notifier would do functionally, but per-depot carries location and leaves room for map markers later.

### The problems pane

Every problem carries a severity:

- **Error** — nothing will move until it is fixed. Ex. unbound controller, no eligible bay pairing, residue with no outlet.
- **Warn** — bad but not terminal. Ex. Dismantled bay with orders cancelled, dTT detected.
- **Info** — working as configured, but worth knowing. Ex. ungated partial load, threshold gap, station at inbound limit, drain station with fewer bays than the longest train (reported on the drain station itself, since it is a config gap rather than a failed order).

Message content is per-problem and depends on what the UI can render. A workable baseline:

> No eligible bay pairing for Iron Plate. Considered: P (provider), P2 (provider), R (requester).

Index mismatches are the exception and need **both bay shapes rendered side by side**, because no text form communicates them. Provider P:

```
1: Load / Iron Plate
2: Empty platform
3: Load / Iron Plate
```

Requester R:

```
1: Empty platform
2: Unload / Iron Plate
3: Unload / Screws
```

P offers plates at {1, 3} and R accepts them at {2}, so nothing moves however much stock and demand exist.
Both stations look correct alone and no vanilla UI shows the mismatch.
The fix is physical: add a load bay at P index 2, or move R's plate bay to 1 or 3.

---

## 4. System design

### Registration

Controllers register with the dispatcher on construction (after inputs are valid) and unregister on dismantle.
As a result, the dispatcher never (needs to) scan the world for stations, so its view is exactly the set of placed controllers.

### Scanning

Unlike controller construction, inventory demand changes do not produce events, nor does a rewire, a bay mode flip, or a bay being added or removed.
These require polling on a fixed cadence, 5 s default.

**Poll requesters only.** No order exists without a request, so provider supply appearing cannot create work by itself.
Poll all requester bays rather than only unmet ones, since a satisfied requester still has to be watched for becoming unsatisfied.

**Providers are read at match time**, which doubles as re-verification: re-walk both bay chains and reserve in one step immediately before writing the timetable. Dispatch is the commitment point, so poll staleness costs nothing.

**A negative cache** covers the one wasteful case: an item requested with no provider anywhere.
Invalidate it on bay config change, not contents (inventory changing is not observable without the polling the cache exists to avoid).
The problem entry carries a **Recheck** button for a forced rescan.

> Note: this may auto-resolve if a new station is placed with a bay providing the item.

Cost therefore scales with unmet demand rather than base size, and a satisfied network does almost no work per cycle.
Cadence only governs how quickly new demand is noticed. Status needs provider levels too, but lazily on open.

**Events cover one thing the poll cannot.** `AFGTrain` has a self-driving error delegate, so faults arrive as push.
More importantly the inbound-train count decrements on the docking hook, because a stale count blocks (or allows) an otherwise-(il)legal dispatch rather than merely delaying a refresh.

**Dismantled bays** may be encountered during either request or provider checks.
On encountering, their owning station configuration is fully reloaded.

**Trains with residue** are held in a set, so an eligibility check is a single lookup and the set membership *is* the exclusion.
It must be rebuilt on load from car inventories for trains at a depot station, and entries clear when a requester or drain station empties the train.

### Problem lifetime

Problems are fundamentally based on world-state and therefore derived, not accumulated.
Anything the poll recomputes therefore clears itself once its cause is gone: a missing provider that now exists, no space for a train, etc.

The only event hook that causes a Problem is a train returning to the depot with residue.

### Matching

Per network, per item:

1. **Providers** are load bays holding the item above threshold, less committed supply.
   **Requesters** are unload bays short of threshold, less in-flight.
2. **Intersect** their bay indices. An empty intersection is a correct outcome, recorded as unroutable.
3. **Check both stations** against max inbound. Either at limit defers the order.
4. **Pick a train** whose car at every index in the set exists and matches that bay's `isFluid`, preferring one already holding the item since existing contents count as partial fulfilment.
5. Delivery size is the intersection size times car capacity.

Fluid is a third condition on the same index match: provider bay, requester bay and car must agree. Freight platforms only ever transfer with freight cars, so a mismatch is ineligible rather than an error.
Selection therefore requires an exact match on car *type per index* rather than simply car count, allowing one train to serve a fluid at bay 0 and solid at bay 1.

To ensure simultaneous orders never conflict:
1. **Committed supply** stops two orders claiming the same stock. Read and reserved atomically at dispatch.

2. **Max inbound trains** counts trains destined for or docked at a station. It increments on dispatch and decrements on undock, because the station is free the moment the train pulls out, not when the order completes.
> Note: It is station-scoped even though bays are matched individually, since a station has one dock serving whichever train is in it, and it applies at both ends.

This must default to 1, since the mod cannot see whether a holding lane exists, and raising it is the deliberate act of someone who built buffer track.
This inbound count must survive a save mid-dock, so reconcile it from live timetables and docking state on load.

Depot parking spaces is the same concept with a length constraint, since trains physically sit there.

Bay contents are not a limit, since thresholds are already met for an order to be placed.

### Dispatch timing

The desired amount is only known once an order is assigned, so preparing a bay cannot precede it.
Dispatch when a request and provider can be matched, then let travel double as preparation time; holding a train at a depot while a bay fills wastes minutes on a large rail network.

However, a train arrival before a bay is ready gives a short car.
The car holds the right item and the requester unloads all of it, so the shortfall reappears as demand, but the next delivery may then arrive full against a partly-satisfied request, which is a residue path.
Exactness is therefore enforced at the dock, not at dispatch (see *Belt gating*).
Circuit logic can provide controls enabling precision, or players can accept occasional short and over-full deliveries.

### Writing the schedule

A schedule consists of exactly three stops: provider, requester, depot.

**Writing the schedule is setting the filters.** The stop's rule set carries them and is the only writable path, so there is no separate filter-configuration step anywhere in the design.

**An empty filter list means allow everything.** `UFGNoneDescriptor` exists to block a direction, which would be pointless if omission already blocked.
So every stop sets **both** lists explicitly, with None on the direction it does not want.
A train delivering ingots to a mixed station that also provides plates would otherwise silently load plates.

**One filter list per stop, not per bay.** Every platform at that station copies the same rule set, so a visit gets one load list and one unload list applied by bay mode.
Mixed stations still work (load bays take the load list, unload bays the unload list, in one dock).
Chained deliveries fall out of this and need no feature.

> Note: filters will NOT allow for delivering multiple items to multiple bays!
> See the mixed delivery feature.

### Draining residue

Residue clears without the player intervening. LTN has no equivalent; stuck cargo there MUST be fixed by hand.

Per-car inventory is readable, so the dispatcher tracks it and clears residue two ways.

**Absorb it.** The matcher counts existing car contents as partial fulfilment, so a car holding 200 iron plate on a plate order delivers 200 plus whatever it loads.
This is the common path and needs no dedicated trip.

**Drain it.** Leftovers are not a request, so this sits outside matching and runs on the depot arrival hook:

1. Train arrives at a depot.
2. Read its car contents. Nothing left over, done.
3. Look for an unload bay wanting that item **at that car's index**.
4. If one exists, stop. Normal matching will pick this train up, preferring it because absorption counts as fulfilment.
5. Otherwise look for a drain station with a matching drain bay, and route there.

Step 3 is index-scoped for the same reason everything else is: a requester wanting iron plate at bay 2 does nothing for residue sitting in car 4.

A drain schedule is two stops, drain station then depot.

**Drain eligibility is per car.** The dirty car sits at index *N*, so the drain station needs an unload bay at index *N* of matching type with room to take the leftover.
`BayState` answers all of that from one chain walk (including `canPartialUnload`, which hides the solid/fluid difference behind a boolean), so matching never branches on bay type.
Reading it at the time of the check also means a platform flipped to load since the station was placed is simply seen as ineligible.

A drain station's bays therefore have to cover the longest train on its network, in both count and type, or those cars have nowhere to drain.
Cheap to detect, and raised as Info since it is invisible until a train sticks.

Until cleared, a residue-carrying train is restricted, because filters are item-scoped rather than bay-scoped.
At best, it reduces capacity in the car; at worst, every unload bay at a stop accepts the filtered item, so a car holding it over a bay expecting something else would contaminate that belt.
Unlike residue, contamination is unrecoverable, since the items are now on the wrong line in the factory.

**A residue-carrying train is eligible for an order only if** the cars at the needed indices are empty, or the needed item is the residue item AT that index.

Residue with no outlet parks the train and is surfaced in Problems on that depot, with the item named.
The train joins `residueTrains`, which only suppresses re-running the drain check.
A later request or a drain controller being placed resolves it with no further action required by the player.

### Depot selection

Simply the first free parking space, straight-line distance as tiebreak.
No rail path-cost query seems to exist in the API currently, so proximity is a later refinement and the tooltip should say straight-line rather than imply routing.

### Failure handling

- **Controller or bay dismantled mid-flight** — unregister, cancel referencing orders, send the train to a depot. Raised as a warning when the train arrives, fired from that depot's notifier.
- **Provider drained mid-flight** — vanilla handles it; fully-load stops when the bay runs dry and the train leaves short. Recorded as short-delivered, with no retry loop on the same pair.
- **Player hand-empties a bay** — same path.

Orders therefore track wanted against delivered, and completed orders go into a **delivery history** of configurable length, since short deliveries are only diagnosable in hindsight.

### Persistence

In-flight quantities are the mod's alone; nothing in the game tracks them. Release on dock and rebuild from live timetables and car inventories on load.

Config lives in a mod-owned save actor keyed by station identifier, with the name cached for re-resolution.
Nothing is written into vanilla station state, so uninstall leaves stations untouched and every timetable legal and hand-editable.

### Multiplayer

Server-authoritative throughout rather than a phase, with all UI actions through a remote call object.
Retrofitting it is painful enough to make it a constraint on every feature.

Concurrent editing is not a hard problem: two players open a panel, one edits, the RPC round-trips and the other's view updates.
Benign as long as edits propagate rather than applying locally first.
(Can also use optimistic concurrency pattern like updates send a "previous" and "new" value, to ensure updates only work on previous match)

### Re-assign on undock (later)

On undocking from a delivery, re-run matching before returning to a depot and dispatch straight on if a request suits.

Cybersyn does this and it SOMETIMES beats sending a fresh train, but can also be worse since we have no way to tell without rail distance calculation.

---

## 5. Mod relationships

| Mod | Status | Why |
|---|---|---|
| **Circuitry** | **Required** | Binding is a wire to the station, so there is no controller without it. Also supplies the Notificator, signal gating, and combine-inventories. |
| **Dynamic Train Routes** | Recommended | Path override with congestion penalties. Touches routing, never stop selection. |
| **DynamicTimeTable** | **Conflict** | A second authority on the timetable. |

dTT hooks `SetStops`, so every dispatch fires it, and if the target station is in one of its groups it may replace that choice on the next tick.
Its re-entrancy guard covers its own writes only, so it cannot distinguish a dispatch from a player edit.
It balances on booking counts with no distance term, so it offers nothing over in-house pooling, and its own rule that two stops from one group disable redirection means a schedule can silently switch it off.

Its hooks early-return for ungrouped stations, so it is inert if the player keeps these stations out of groups. Detect it at load and say so in Problems.

---

## 6. Phasing

Phases scope provable implementation, not public releases. A phase is done when its behaviour is demonstrable.

**Phase 1 — spine.**
All three controllers, wire binding and the Train Station connection descriptor, chain walk, bay table derivation, save actor, registry, poll loop, read-only Status, timetable import. No dispatch.
Import belongs here because it is read-only and exercises the timetable parsing dispatch will need.

**Phase 2 — dispatch.**
Depots included, since dispatch without them is not shippable.
One train length per network, declared, and full loads only.
Fluid bays and cars are type-checked from phase 2, so fluid stations are correctly ineligible rather than mis-dispatched, even though fluid support proper lands in phase 6.
Three-stop schedules, committed supply, max inbound, release on undock, parking-space validation, residue draining, Problems, and published outputs so gating is available from the first dispatching build.

**Phase 3 — bays and shapes.**
Index-set matching, mixed-mode stations, unroutable reporting with shape rendering, variable train lengths with per-pairing capacity and per-length depot pools.

Phase 2 alone teaches habits that phase 3 breaks, so this should not slip past first public release.

**Phase 4 — partial loads.**
Requests finer than a full car, which need the desired amount to equal the request.
Wired overrides on typed fields, and bulk edit across controllers, which matters as soon as a mod changes car or stack capacity.

**Phase 5 — mixed deliveries (opt-in).**
One train serving several items to a single requester. Off by default and documented before it ships (see *Mixed deliveries*).

**Phase 6 — fluids, priorities, per-item network overrides, per-car residue eligibility.**
Fluid matching is already the same index rule by then and quantities are fractional from phase 1, so phase 6 is mostly the capacity and headroom figures plus testing.
Fluid bays appear in the chain walk from phase 1, so they should be visible and inert rather than hidden, since a bay that vanishes reads as broken.

---

## 7. Design guidance and tips

These are things that should be surfaced in a consumable way to installers of the mod.

They contain build habits and known interactions rather than mod features, though several are the difference between a network that works and one that quietly misbehaves.

Home is the mod page and/or GitHub wiki.
Tooltips can carry short forms, and hooking the game's own tips system would be better still, but most of this is too long for in-game.

### Basics

- **One item per bay.** Mixing parts in a car is legal, but a bay per item is what makes auto-suggest, matching, and diagnosis work.
- **You can add bays, but not reorder or change them.** Adding to the end is safe; inserting shifts everything after it and breaks existing pairings.
- **Raise max inbound only after building a holding lane.** A provider feeding several consumers wants 2 or more, but only with buffer track.
- **Point your stations the right way.** The mod does not check facing and will pair two stations a train cannot approach.
- **Import does not require a sane network.** It prefills from whatever the timetables say, including two items per bay. Read the populated table before accepting it.

### Buffering

**Buffer storage should match or exceed one car's contents, at both ends.**

On the requester side this is what prevents residue: if the unload bay can dump a full car into storage, a full load is always absorbable.

On the provider side it keeps the bay prepared while belts refill.
Produce at line rate into storage, then refill the bay from storage on a high-tier belt (a line making 60/min can feed a bay at faster rates from a full container).

**Throughput matters more than it looks for large stacks.** A 500-stack item at 1200/min still takes minutes to fill a car, so if buffer-to-bay throughput is below the threshold you set, gated trains sit waiting.
Raise throughput or threshold, or use full loads with enough storage that gating is unnecessary.

**Mind the gap between provider and requester thresholds.** Requesting at 1000 while providing only above 2000 keeps a reserve at the source.
This may be useful for player pickup or sites that dual-feed Dimensional Depot and the train network.

However it also delays deliveries, and can starve the requester entirely during heavy external draw.
Deliberate reserve and accidental stall look identical without close scrutiny.

### Belt gating

The controller publishes desired amounts and a ready flag so belt-level control can be implemented for precise partial load support.

To do this, you must:

- **Hold the train** with a rail signal set blocked until the ready flag sets, so it does not arrive at a half-prepared bay.
- **Meter the bay** by gating its feed belt against the published amount. The bay then holds exactly the request and the train takes exactly that, which is what makes partial loads exact.

**Limitations**:
- High capacity items like wire WILL take a long time to load, since belts load in items/min, NOT stacks/min.
- Fluid may not work with this pattern, unless valves can have values set programatically(?)
- Expect small overshoot regardless, since belts have items in flight when a threshold is crossed. Requester buffers absorb it.
- A gated train reports long-wait-at-signal, which is expected rather than a fault.
- DTR penalises blocked paths, so a gate can push it into rerouting.

### Residue

- **It clears itself.** Requests can consume leftover residue from a previous over-delivery, and drain stations can take anything otherwise.
- **A drain trip runs only when nothing can use the item at that car's index.** The drain station needs an unload bay at the same index and type, with room to take the leftover.
- **A residue-carrying train cannot take unrelated orders**, because a car holding the delivered item over a bay expecting something else can contaminate that belt or reduce the amount the train can fill.
- **Residue with no outlet parks the train** and is flagged in Problems on the depot it is parked at.
- Practical habit: place one drain station whose bays cover your longest train, matching freight and fluid positions. It then drains everything, whatever index the leftover sits in.

### Mixed load/unload stations

For instance, a station with some load and some unload bays (ingots-in, plates-out smelter at the same station).
This is supported, but likely to introduce Problems.

- **Proper station design avoids it.** Separate the in and out stations and none of this applies.
- Every unload bay at a stop accepts the filtered item, so a car at an unrelated unload index can contaminate that bay's belt.
- One filter list per stop means different items cannot go to different indices in one visit.

### Mixed deliveries (opt-in)

One train carrying several items to one requester or from one provider to several requesters, useful for topping up a secondary item on a trip already being made.

However, this is a deep rabbit hole and one likely not implementable due to the following:
- This takes advantage of **load and unload** filters being independent but requires complex station planning.
- Since the filters are independent, we can try to ensure that contamination is **unlikely** at a given station, such that nothing is (un)loaded at a provider/requester respectively.
- Recall that **Bays** are the fundamental base for automatic scheduling.
- As long as there are no **Bay**-level conflicts, multiple stations may be chained; station A providing Wire at Bay 1 and station B with Iron Plates at Bay 2 (and a gap in Bay 1) will work. If Iron Plates are ALSO provided in Bay 1, those may load before the Wire and cause a Problem.
- Similarly for the one provider -> many requesters case, each bay index must ONLY be consumed at ONE of the stations.
- Leave it off unless you have read this section.

### Removing the mod, or standing it down

Uninstall is clean: modded buildables vanish and every timetable left behind is a legal vanilla one.
However, trains hold whatever the dispatcher last wrote, which is arbitrary rather than wrong.

So the mod needs a **release all trains** action that clears dispatcher-owned schedules and parks everything, and the publish guide needs a shutdown procedure.
The same action serves standing a network down temporarily.

Removing a single controller is the same case in miniature (cancel its orders, release its trains, warn if needed).

Make sure to include a note to players that they will have to manually route trains!

---

## 8. Known limitations

What the mod cannot do, as opposed to what is not yet built.

- **No exact quantities from the dispatcher.** Vanilla stop rules carry item filters and once-vs-fully, with no amount field.
- **No control over which car gets what.** Bay index equals car index, fixed at both ends, and freight and fluid never mix.
- **No rail distances.** Depot choice and re-assign-on-undock use straight-line distance and cannot know whether a decision was actually shorter.
- **No station-direction checking.** Geometrically unreachable pairings are dispatched and fail.
- **A dock cannot be refused.** Vanilla produces `SDLE_InvalidNextStop` rather than a graceful skip, so a train is held at a signal instead and that error is only ever diagnosis.
- **Deliveries cannot be partial.** Only pickups can be metered.
- **Config is trusted.** Bay shape and contents are read, but item and threshold are taken as declared, so a mis-declared bay produces a wrong or empty delivery rather than an error.
- **Residue needs an outlet.** An item with no request or drain configured will be stuck in the depot.

---

## 9. Key technical references

**Station identity is `AFGTrainStationIdentifier`**, not the buildable and not the name.
Enumerate via `AFGRailroadSubsystem::GetAllTrainStations`, `->GetStation()` for the buildable, `->GetStationName()` for display.
Names are non-unique and unenforced.

**Platform chain walk.**
`station->GetStationOutputConnection()->GetConnectedTo()`, then per hop `platform->GetConnectionInOppositeDirection(conn)->GetConnectedTo()`.
Returns every platform including Empty Platforms, so indices stay intact.
Cast to `AFGBuildableTrainPlatformCargo` for load mode, inventory, and docking status.
The chain is doubly linked, so bay to owning station is the same walk backwards.

**Timetable** — `GetStops / SetStops / GetStop / IsValidStop / GetCurrentStop`. A stop holds the station identifier plus a rule set.

**Rule set** — Load/Unload Once vs Fully Load/Unload, a duration, a duration-AND flag, and separate load and unload item filter arrays.
**No quantity field exists**, which is the one LTN feature that cannot be ported.

**Platform filters exist but are not configuration.**
`AFGBuildableTrainPlatformCargo` carries `mLoadItemFilter` and `mUnloadItemFilter` directly beneath `mDockingRuleSet`, whose comment reads "The current docking rules set from a docked train."
None of the three are `SaveGame` and none have a setter or Blueprint getter — they are copied from the docked train's stop and discarded.
The authored filter lives on `FTimeTableStop`, which is `{Station, DockingRuleSet}`, both `SaveGame` and `BlueprintReadWrite`.
Patch 1.2.2.1 names it precisely: "Fixed Unpacked fluids not being shown in Train Station **time table** load/unload filter."

Expect player confusion here, since Update 5 moved "Stop Settings" into the Freight Station UI. The filter is edited from the platform while being stored per train.

**Hooks** — `AFGTrain::OnDocked`, `AFGRailroadTimeTable::SetStops`, `AFGTrainStationIdentifier::SetStationName`, `AFGTrain::SetSelfDrivingEnabled`.
Bracket your own `SetStops` calls against re-entry, as dTT does with an internal set.

**Trains** — `AFGRailroadSubsystem::GetAllTrains`. Consist is the vehicle list, with freight cars a distinct class from locomotives.
The self-driving error enum covers no-path, station-unreachable, unreachable-with-signals, and long-wait-at-signal.

**Save** — an actor implementing the save interface, `ShouldSave` returning true, save-marked properties. Circuitry and dTT both do this.

**Dock timing** — 27.08 s per cycle with belts paused throughout, so any dispatch latency under a few seconds is invisible.

---

## 10. Unverified

Test before committing design weight to any of these.

- **Whether a dependent mod can register the Train Station as a Circuitry-connectable building.** Only the freight platform is registered today. Circuitry's API exposes list registration publicly, including a FactoryGame override, but this is untested. Binding depends on it entirely, so verify before anything else.
- **Whether a depot-parked train with a stale timetable re-paths, or whether self-driving must be toggled off.**
- **Whether `AFGTrain::OnDocked` also covers undock, or undock is a separate hook.** Decides how the inbound count decrements.
- **Whether an empty filter list allows everything.** Inferred from the existence of `UFGNoneDescriptor` plus a reported case of no filter loading everything, but not header-verified.
- **Behaviour when a train is longer than the platform chain.** Expected silent no-transfer on overhanging cars.
- **Whether autopilot resumes after `SDLE_InvalidNextStop`.** `ReportSelfDrivingError` and `ClearSelfDrivingError` exist; recovery does not follow from the headers.
- **Rail path cost queryability.** Depot tiebreak stays straight-line if absent.
- **Whether fluid partial unload has a guard equivalent to the empty-slot rule**, and what headroom it needs. Feeds `canPartialUnload` and the requester ceiling for fluid bays; any figure now is a placeholder.
- **Whether valves can be controlled via circuit network.** Non-blocking, enables partial load of fluids.
- **Fluid platform and car capacities.** Needed for defaults, not for matching.
- **Whether a Circuitry connection survives blueprinting.** Not blocking, a workflow bonus.
