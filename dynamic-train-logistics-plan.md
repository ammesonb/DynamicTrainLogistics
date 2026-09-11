# Dynamic Train Logistics

A demand-driven train dispatcher for Satisfactory. Players declare what each platform provides or requests. The dispatcher finds a safe delivery, reserves the train and stations, and writes its timetable.

The C++ domain builds without Unreal so matching can be tested on macOS. Game integration is still required before any behavior is considered proven in Satisfactory.

## 1. Where to read

- [Matching](docs/MATCHING.md): quantity rules, safe train lengths, examples, and the current search contract.
- [Train states and reservations](docs/TRAIN_STATES.md): trip progress, actual transfers, and why reservations remain in place. This dispatcher lifecycle is specified but not implemented.
- [Saving and recovery](docs/SAVING.md): entity-owned save state, shared trip records, and saved route estimates.
- [Domain headers](Source/DynamicTrainLogistics/Public/Domain): current C++ types. These are the source of truth for the implemented representation; the plan does not duplicate their declarations.
- [Matcher tests](Tests/src/matcher_test.cpp): executable examples and regression cases.

Keep player concepts and integration notes here, matching explanations in Matching, lifecycle explanations in Train states, and persistence in Saving. Changes to a rule should update its examples and tests together.

## 2. Core rules

### Platform position determines the car it serves

Both ends must serve the same physical position. Empty platforms retain their position. The domain uses zero-based bay indices. `Train.vehicles` includes every engine and freight car, ordered from the docking engine at vehicle 0. Bay N meets vehicle N+1. `cargoAtBay(N)` returns null for an engine or a bay beyond the train. Players must face stations correctly and use a layout with the same vehicle-to-bay alignment at both stops. Automatic correction of station facing or reversal between stops is outside the initial scope.

Matching individual bays does not mean the dispatcher can switch individual bays off. **Load and unload item filters belong to the entire stop.** Every reached platform in that direction can transfer the allowed item.

For example, a provider loads plates at positions 0 and 2, but the requester unloads plates only at position 0. A one- or two-car train can serve position 0. A three-car train reaches the unwanted pickup at position 2 and must be rejected. Alternatively, choose a provider without the extra pickup.

If the unwanted load bay is before the required bay, shortening the train cannot help. Residue at other positions must also be checked against every unload platform the train reaches. A plate order must never unload plates onto a screw belt.

### Threshold means a minimum worthwhile quantity

The player enters an item count for solids or volume for fluids:

- **Provider:** at least this much stock must be available after subtracting committed pickups.
- **Requester:** at least this much must be missing after subtracting incoming deliveries.

For requesters, missing means platform capacity for the configured item minus stock of that item minus incoming deliveries. External storage is not part of that inventory read. Zero or negative remaining demand cannot create a delivery, even with a zero threshold.

Example: a platform holds up to 1,000 plates. With 700 in stock and a request threshold of 300, it requests. If 100 plates are already incoming, it waits: only 200 remain missing.

The threshold is not an exact order size, a stock reserve, or a departure condition. A provider threshold of 200 means “do not arrange pickup until at least 200 are available”; it does not mean “leave 200 behind.”

### Physical capacity and player quantities are different

Keep inventory, thresholds, reservations, and delivery totals in item counts or fluid volume. Store solid storage capacity in slots and convert it using the selected item's stack size. An empty car does not have one universal item capacity.

Partial stacks contribute their actual item count. A stack containing 25 plates counts as 25 plates, whether or not it is full. Items/minute describes production throughput and is not an inventory quantity.

Missing stock and usable unloading room are separate questions. Foreign items and inventory slot rules can reduce usable room. The adapter must determine actual unloading constraints; a suspected empty-slot requirement does not justify redefining the request threshold.

### A delivery is not guaranteed to fill or empty a car

The intended default is **Load/Unload Once**, with explicit item filters: transfer what the stop can transfer in one visit, then depart. The stop rule includes a duration and its combination mode; do not silently choose Fully Load/Unload and assume it has the same departure behavior.

The current matcher estimates delivery size using full car capacity. Actual loading may be smaller because the provider is short. Actual unloading may be smaller because the requester cannot receive it all. Record the observed quantities and reconcile reservations.

A planned full car is a capacity estimate, not a command to transfer that exact amount. The plan depends on confirming Once behavior, including empty and blocked platforms, in-game.

### Idle trains belong in depots

The dispatcher must never deliberately leave an idle train in a cargo station. A train remains assigned through its return to a depot.

Reserve pickup and dropoff station slots together, and hold each slot until undock. Reserve return parking before sending the train out. Depot capacity must cover the managed fleet, but total capacity alone is insufficient: each trip needs an owned parking claim.

“No deliberate waiting in a cargo bay” does not promise that the mod can prevent a vanilla pathing or docking fault. Keep ownership during faults and report them.

## 3. Controllers and player setup

### Station controller

One per participating station, bound through Circuitry. The mandatory binding is the station itself. Read its identity, display name, platform chain, and docking state from the game.

The player configures:

- Item and threshold for each participating bay.
- Network membership, which limits which stations and trains may interact.
- Maximum inbound trains, including those already docked.

Bay load/unload direction comes from the vanilla platform setting. A mode flip takes effect on the next relevant refresh and must be checked again before dispatch.

### Depot controller

A separate controller bound to a parking station. Configure network, parking spaces, and maximum supported train length; zero means no configured length limit.

A depot has no cargo demand. Track physical occupancy separately from assigned return parking. A train can be physically parked and already assigned to a delivery or drain trip.

### Drain controller

An optional controller bound to an unload site. Configure network and maximum inbound trains. Its platform chain is read-only in the mod UI; it does not join provider/requester indices.

Drain sites accept residue only when the reached bays, car types, filters, and usable room make the entire proposed unload safe. Fluids already present in a drain platform must be compatible with the arriving fluid. Do not assume every fluid can mix in one drain bay.

A network can operate without a drain, but residue without a safe outlet removes the affected train from the available pool.

### Inputs and defaults

Typed values are the normal path. Optional Circuitry signals override individual fields only when enabled. Uniform nonempty inventory can suggest an item; an empty bay cannot provide that evidence. Belt throughput is not an inventory scan.

Initial defaults and suggestions:

- One shared network.
- Maximum inbound trains: 1. Raising it requires player-provided holding space.
- Depot maximum train length: 0, unconstrained by configuration.
- Provider threshold suggestion: a quarter of a compatible car's item capacity.
- Requester threshold suggestion: one compatible car's item capacity missing, to make room for a full-car delivery under ordinary single-item conditions.

Capacity-based suggestions require a known compatible car and item. Show the computed quantity and let the player change it. If no compatible car is known, require a quantity rather than inventing a game capacity. Different car capacities require rechecking the chosen train's delivery size; a suggestion is not a safety proof.

### Import existing timetables

Read existing schedules to suggest bay configuration. Stop filters describe items and direction, not which bay owns which item.

One item and one platform in that direction can be assigned unambiguously. For more complicated stops, inventory may provide evidence, but empty or ambiguous bays need review. Do not derive arbitrary bay assignments from the ordering of a stop's filter list.

Import is a preview until the player accepts it. Taking ownership of a train and replacing its timetable is a separate action from reading it.

## 4. Dispatcher design

### Registration and refresh

Controllers register and unregister their stations. The dispatcher searches the registered network, not all stations in the world.

Keep provider/requester indices by configured item. These are search aids. Before a proposal is committed, refresh the chosen provider and requester chains and the train, then validate current mode, binding, type, geometry, quantities, and assignment limits.

Poll requester inventory on a configurable cadence, initially five seconds. Provider inventory is read on demand for relevant requests and lazily for an open status panel. Do not poll every provider's inventory every cycle.

Configuration RPCs and construction/dismantle hooks should invalidate affected entries where available. Vanilla mode changes, rewires, and chain edits may lack events. Use a bounded rotating structural refresh for otherwise-unvisited registered stations so changes eventually become discoverable. In particular, an empty negative cache must not hide a newly created provider forever.

A negative cache can skip repeated searches for an item with no registered provider. Invalidate it on relevant configuration changes and structural refreshes, and expose Recheck in Problems. Never cache “no stock right now” as “no provider exists.”

Read only relevant inventory, but do not claim all staleness is harmless: stale geometry can cause an unsafe dispatch. Fresh validation and serialized reservation are the commitment boundary.

### Matching and assignment

The current matcher returns a proposal from a snapshot. It checks thresholds, network, both inbound limits, train availability, alignment, car types, and unintended transfers at either stop.

`findBestMatch` returns the highest-priority safe proposal across all requested items. `findProposals` exposes the best per item in priority order; those proposals may compete for the same train. The dispatcher must commit one, update the working state, and search again.

Assignment must claim the train, pickup station, dropoff station, per-bay quantities, and return parking together. A schedule failure must not leave half the resources reserved. See [Train states and reservations](docs/TRAIN_STATES.md) for the transaction and release rules.

Selection now prefers use of matching residue, then more requesting bays served, then a shorter train, then ascending item/station/train IDs. Each criterion takes precedence over every later one. Shorter means fewer total vehicles, including all engines, not rail distance. Repeated waiting can indicate insufficient trains, buffers, or unsuitable thresholds, but fixed priorities can also repeatedly favor the same requester under contention. Fair rotation remains unimplemented. See [Matching](docs/MATCHING.md#choosing-between-useful-deliveries) for examples.

### Writing schedules

A delivery visits provider → requester → depot. A drain trip visits drain → depot.

Every stop sets both direction filters explicitly. At the provider, allow loading the order item and block unloading. At the requester, block loading and allow unloading the order item. At the depot, block both. An empty list is not a safe spelling of Block; the adapter must translate the explicit filter mode using verified game semantics.

The default transfer rule is Once. Express the actual shared docking rule and duration fields in the adapter contract. Avoid independent “fully load” and “fully unload” booleans that cannot map unambiguously to the game rule.

The final depot stop must leave the train parked under dispatcher ownership, not repeat the previous delivery. Verify autopilot and timetable behavior before implementing this transition.

Only the dispatcher writes managed timetables. Prevent re-entry around its own writes and detect competing timetable ownership.

### Residue and failures

Matching residue at a selected car position counts toward the planned delivery. For a 400-plate car already holding 50, reserve up to 350 new plates at pickup and account for 400 arriving at dropoff.

A train can also carry unrelated residue if the proposed filters block it at every reached stop. Do not equate “has residue” with “always unsafe”; inspect the whole stop. Residue still needs an eventual outlet.

On depot arrival, inspect cargo. Prefer a feasible safe delivery that can absorb it; otherwise arrange a safe drain trip. If neither exists, park the train and show a problem naming the item and position. Relevant new demand, configuration, or drain capacity must trigger a recheck.

If a controller or bay disappears during a trip, retain train ownership while cancelling obsolete claims and selecting a safe depot. Do not simply erase the trip. Record the original request, planned pickup/arrival, starting residue, actual pickup, and actual delivery separately for each bay. Another source can fill the requester during travel, so even a successful pickup can become a partial delivery. A pickup can also exceed the original request because stop filters do not set quantity limits. Refresh current demand after delivery; do not derive it from the old request minus this trip's delivery.

### Persistence and multiplayer

Each entity defines the state it needs preserved and exposes it as ordinary C++ data. It knows which values cannot be recovered and which can be rebuilt. The save adapter serializes that state, translates object references, and supplies it back on load; it does not decide which entity fields matter or how derived state is reconstructed.

For example, a station supplies its configuration, while the dispatcher supplies its authoritative trip ledger. The UE adapter can store that ledger in a mod-owned world subsystem actor without moving ownership of trip state into the adapter. Controllers may expose related trip IDs, but pickup and dropoff controllers must not save competing copies of the assignment.

Persist controller configuration, network membership, stable object references, and active assignment intent and progress. Reconcile live trains and timetables on load, then rebuild counters. Current cargo cannot recover original quantities or past deliveries.

The current `Order` is not yet a complete save record: it needs a return depot and separate per-bay original requested quantity, starting cargo, planned pickup/arrival, actual pickup, and actual delivery. `MatchCandidate.wanted` currently means planned full-car arrival, not the original requested quantity. Do not repurpose it to stand for all three stages. Runtime numeric handles require a stable-reference mapping in the UE save adapter. See [Saving and recovery](docs/SAVING.md) for ownership and restoration.

All dispatch and edits are server-authoritative. UI actions go through the server RPC path; the server publishes accepted state to all viewers. Save/load recovery completes before dispatch resumes.

## 5. UI and published outputs

Station-local tabs: Status, Config, Problems. Open Problems when there is an active problem, Config when mandatory values are missing, otherwise Status.

- Status shows inventory, missing quantities, reservations, and assigned trips.
- Config shows bay position, direction, item, and **minimum available** or **minimum missing**, plus station settings. Label the threshold by direction instead of leaving its meaning implicit.
- Problems shows current causes and relevant stations/bays. A geometry problem can carry several `{station, indices}` groups and render the two layouts side by side. Do not reduce a multi-station explanation to one station ID.

Problems are derived and deduplicated per owning station/item/train as appropriate. Merge their supporting locations. Clear a problem when its cause is gone. A failed search currently returns only `nullopt`; diagnostic generation remains unimplemented.

Station outputs: per-bay desired pickup amounts for the next assigned outbound train, a ready flag, and status. Desired pickup excludes cargo already in the car. With several inbound orders, publish the next one explicitly rather than summing them into a misleading single-train target.

Depot outputs: stuck-train status with item and train identity. Multiple stuck trains require a selection or list in the UI; a single wire output can report one at a time.

## 6. Player guidance

### Buffering and overshoot

Provide storage and throughput appropriate to each car's contents. A container behind an unload platform helps only as fast as its belts clear the platform. It is not additional instantaneous unloading room.

Requesting when at least one car's worth is missing reduces residue when the chosen car fits that space. Smaller thresholds allow earlier visits but can leave cargo aboard. Larger thresholds mean fewer, larger shortages before a request is considered; they do not mean “keep the platform fuller.”

### Locomotives, grades, and platform alignment

Two locomotives are a useful fleet convention for acceleration and hill margin, not a universal requirement for three loaded cars. The [Freight Car weight table](https://satisfactory.wiki.gg/wiki/Freight_Car#Weight) gives one locomotive approximate limits of 13 full cars on a 1 m ramp, five on a 2 m ramp, and 2.8 on the illustrative 4 m ramp grade. The page warns that rails cannot be laid directly on the last ramp type. These are limiting estimates, not comfortable operating targets; leave margin for curves, restarting on slopes, and the actual route.

A locomotive adds both pulling force and substantial mass. The [locomotive forces reference](https://satisfactory.wiki.gg/wiki/Electric_Locomotive#Weights_and_forces) gives 2,000 kN maximum force and 300 t per engine, with force falling at higher speed. Two engines therefore improve acceleration for the same freight load, but do not double acceleration. They also add length and power demand. Base fleet guidance on expected loaded operation, including residue, rather than on empty outbound trains.

The wiki's fluid figures require caution: its current 2,400 m³ capacity, per-volume weight formula, and stated 100 t maximum do not agree. Do not hard-code a fluid haulage limit from that table without checking the installed game.

Multiple front locomotives change cargo alignment: a second engine behind the docking locomotive occupies a platform position before the first freight car. Station layouts need that gap, and the adapter must preserve it. The domain now represents that gap explicitly with a `Locomotive` entry in `Train.vehicles`. `readTrainConsist` must return every vehicle from the intended docking engine onward; filtering engines out would break the mapping. Tests cover two front engines and engines between or after freight cars. The UE adapter still needs to verify actual docking orientation, especially if the train reverses between stops. Physical train length, cargo capacity, and platform positions must remain distinct.

This guidance does not add a traction simulator or a new eligibility rule to the matcher. In-game validation should include a fully loaded stop-and-restart on the steepest intended grade, with the intended engine placement.

### Optional belt gating

A future exact-pickup mode can publish a smaller desired amount and let player circuitry meter inventory before arrival. Hold the train outside the station until ready, and control the feed belt. Belts already in flight, pre-existing platform stock, and transfer behavior limit precision; do not promise exact amounts solely from a threshold.

Large-stack items can take substantial time to prepare because belts move items per minute, not stacks per minute. Fluid valve control requires verification. A long wait at a player-controlled signal may be expected; routing mods may treat it differently.

### Mixed stations and deliveries

A station with both load and unload bays can participate if both direction filters and every reached car are safe. Separate stations are easier to reason about, but mixed direction alone is not an error.

Several different items in one delivery are outside the current matcher. Any future support must prove transfer safety for the complete filter list at every stop. Opt-in is not permission to contaminate a belt. Do not describe contamination as merely unlikely.

### Standing down or uninstalling

Provide a release-all action that takes managed trains to safe parking and ends dispatcher ownership. Removing one controller requires the same treatment for its affected trips.

Removing the mod may leave legal vanilla schedules, but legal does not mean safe to repeat indefinitely. Explain the shutdown procedure and how players resume manual routing.

## 7. Build and validation order

The current working component is the portable matcher and its types. Run `make test` on macOS. Tests rebuild when domain headers change.

The next useful prototype is a complete delivery in a fake world: propose, atomically reserve both stations and parking, write through a fake schedule sink, simulate loading and unloading, release claims on undock, and return to idle at a depot. Include competing requests, failed writes, residue, cancellation, and save/load during a trip. Do not call this complete merely because the order structs exist.

Then integrate that same loop with one provider, requester, and depot in-game. Prove binding, position alignment, filters, departure behavior, docking events, parking, and save recovery before increasing layout complexity.

Drain trips use the same reservation and lifecycle rules. Fluid transfers need their actual capacity and room rules verified. Variable lengths already affect safety and cannot be postponed behind a fixed-length assumption. Fair scheduling, exact-pickup gating, multi-item trips, locality-based distance ranking, and reassignment without visiting a depot remain later work.

## 8. Integration references and open questions

### Documented interfaces

- [SML subsystems](https://docs.ficsit.app/satisfactory-modding/v3.7.0/Development/ModLoader/Subsystems.html) support centralized game state and persistence.
- [Save game integration](https://docs.ficsit.app/satisfactory-modding/v3.9.1/Development/Satisfactory/Savegame.html) describes save participation for actors and properties.
- [Train docking rule set](https://docs.ficsit.app/ficsit-networks/latest/reflection/structs/TrainDockingRuleSet.html) exposes a shared Once/Fully definition, duration, duration combination, and separate load/unload item filters. This is a reflection reference, not a substitute for testing game behavior.

Use the installed SML/FactoryGame headers when writing the UE adapter; documentation versions may differ from the installed SDK.

### Rail path queries

The public [FactoryGame navigation header](https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/master/Source/FactoryGame/Public/RailroadNavigation.h) declares exported `FRailroadNavigation::FindPathSync(locomotive, station, filter)`. Its result contains a path whose points carry distance-to-end values. Rail-distance support is therefore a concrete integration lead, not an absent API.

The public query starts at a locomotive and does not reverse it; the connection-to-connection helper is private. Verify current-position offsets, physical distance versus routing penalties, SDK compatibility, side effects, and synchronous runtime cost before using it. Do not assume the first path-point distance is the exact locomotive-to-stop distance. Path queries are not implemented in the portable matcher.

### Proposed locality cache for path queries

This is proposed adapter work. The current matcher does not query rail paths or rank by distance. Look up a route only when it affects a pending decision, reuse it while applicable, and allow its estimate to survive a saved game. Start with distance from the candidate train to its pickup station; this is not the total delivery distance or an estimate of travel time.

1. **Filter first.** Run cargo, bay, station-limit, and train-availability checks before requesting a path. Initially use distance to break ties after the existing residue, bays-served, and train-length preferences, but before IDs. If locality later deserves a higher priority, change that ordering explicitly.
2. **Query a representative.** For an eligible locomotive, call `FindPathSync` to the pickup station. Record the locomotive's leading position in the intended travel direction, track/offset, heading, next directed connection, destination identity, rail graph identity, filter settings, timestamp, and the returned path/distance data. Keep physical distance separate from any routing cost with penalties.
3. **Look nearby for reuse.** Start with a configurable radius of roughly 50 standard foundation widths (about 400 m) around that recorded position. This is a proposed tuning value, not a measured optimum. Radius means distance from the representative, not a chain of neighbors that can spread indefinitely.
4. **Require a shared route, not just proximity.** Reuse the representative's distance estimate only for locomotives on a verified common directed approach, such as the same branch-free track section heading toward the same next connection. Require the same destination and path filter. Membership in the same connected rail graph is necessary but insufficient. Tracks can pass close together while one heads away from the station or takes a long loop.
5. **Reuse or query again.** Within that compatible group, the same approximate distance is acceptable for coarse ranking. If an along-track offset correction can be verified, use it. Otherwise, straight-line distance to the pickup can be a cheap tie-break between these already-compatible candidates; it is not proof of their rail-distance order. Outside the radius, across a junction, or without evidence of a common directed approach, run a new query and create another representative.
6. **Verify the proposed winner.** A borrowed estimate never proves reachability. Before committing the trip, obtain a valid path result for that actual locomotive, either from its still-valid exact cache entry or a fresh query. If unreachable, exclude it and reconsider alternatives. Validate the other trip legs separately when the adapter supports those checks.

Example: three idle trains queue along the same approach to Plate Foundry station. One path query gives all three a useful approximate distance. A fourth train is only 20 m away on the opposite-direction main line: it gets a separate query, even though it is inside the radius.

Cache exact and borrowed results distinctly. Key them by destination, directed start location, graph revision, and path-filter settings. Keep the representative's head position with each shared estimate. Do not promote a borrowed value to an exact result, or share an unreachable result using radius alone.

Invalidate affected entries after track, station, signal-rule, direction, or filter changes. Moving to another directed section or beyond the representative radius requires reassessment. Where events or graph revisions are unavailable, use short-lived entries and a bounded structural refresh; expiry limits staleness but does not prove a route remains valid. Dynamic routing penalties may need shorter expiry than static geometry. Saved distance estimates may be reused after their references and route context have been checked. Rebuild runtime path objects on load; do not discard useful saved estimates merely because the game was restarted. See [Saving](docs/SAVING.md#route-estimates-can-survive-a-save).

Provide a **Refresh route** action for a selected train-to-station route. Discard affected exact and shared cache entries, then queue a fresh query when an eligible locomotive is available. This lets the player request a new distance after building a shortcut; automatic invalidation or expiry still handles ordinary stale entries. The locomotive parameter limits when the query can be run, so do not promise immediate calculation without a suitable train.

Keep a bounded number of entries, evict unused ones, and cap synchronous path queries per dispatcher cycle. Defer unresolved candidates when the budget is exhausted rather than treating an unknown distance as zero or an unreachable route. Measure query time, cache hit rate, and ranking error against exact paths before choosing a radius or expiry.

The future distance tie-break must run before tied alternatives are discarded by `findMatch`. Applying it only to today's `findProposals` output would miss equally ranked trains/providers that already lost the ID tie-break. Approximate ranking is a deliberate performance tradeoff; it must never weaken the existing transfer-safety checks.

### Adapter entry points to verify against installed headers

The earlier API investigation identified these candidates. Keep them as implementation leads until checked locally:

- Station identity: `AFGTrainStationIdentifier`, resolved to a buildable through `GetStation()`. Names are non-unique display labels, not identity.
- Platform chain: station output connection, followed by the opposite connection at each platform. Preserve Empty Platforms; inspect cargo platforms for direction and inventory.
- Timetable: `GetStops`, `SetStops`, `GetCurrentStop`, and stop docking rule sets.
- Train enumeration and vehicle order: railroad subsystem and vehicle list. Locomotives, reverse orientation, and platform alignment need explicit mapping; dropping locomotives from a list is not automatically correct.
- Docking and fault notifications: `AFGTrain::OnDocked` and self-driving error notifications. Verify exactly which transitions each reports.
- Blocking a transfer direction: `UFGNoneDescriptor`. Verify how empty lists and None are interpreted rather than relying on their names.

### Required in-game checks

- Can the mod register a Circuitry connection for a Train Station, and does the connection survive rewire and save/load?
- Does Once leave after an empty pickup or blocked unload? How do duration settings affect it?
- What exact solid inventory condition permits partial unloading? Can partial-stack space be used, and is a fully empty slot also required?
- What are the fluid capacities and partial-unload conditions for the installed game version?
- Which notification reliably marks undock, and can cargo observations be paired across save/load mid-transfer?
- Does a depot stop remain parked, loop, or need autopilot disabled? What happens when a new timetable is written to a parked train?
- How do overhanging cars, multiple locomotives, and reverse approaches align with platforms?
- What happens after self-driving errors, and which recovery steps are supported?
- Validate `FRailroadNavigation::FindPathSync` for reachability and distance from candidate trains. Logical network membership alone does not prove reachability.
- What is the actual Circuitry module name? `.Build.cs` currently names `FicsitWiremod`; confirm against its shipped plugin manifest.

Dynamic Train Timetable conflicts with dispatcher timetable ownership. Routing mods can remain relevant to path selection but must be tested with player signal gates. No distance-based optimization is claimed by the current domain.
