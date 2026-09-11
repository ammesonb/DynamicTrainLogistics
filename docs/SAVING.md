# Saving and recovery

Each entity decides what it cannot reconstruct and asks the save adapter to preserve it. The adapter handles storage and reference translation. It does not decide which fields matter or how the entity rebuilds its state.

A station provides its configuration. The dispatcher provides its active trip assignments. Both can expose ordinary C++ data without depending on Unreal's save types.

For illustration, the interfaces could be used like this; these methods are not implemented yet:

```cpp
saveAdapter.write(dispatcher.saveState());
dispatcher.restoreState(saveAdapter.read());
```

## Save the decisions and observations that would otherwise be lost

For each active trip, preserve:

- The assigned train, pickup/dropoff stations, and return depot.
- The bay positions and items involved.
- The original requested quantity per bay.
- Cargo already aboard, planned pickup, and planned arrival quantities.
- Actual pickup and delivery observations recorded so far.
- Trip progress and any before-transfer observation needed to finish measuring a transfer after loading a save.

Preserving the original request alongside pickup and delivery observations lets the history still explain a short pickup or incomplete unload after loading a saved game. Replacing the plan with the actual result would erase the comparison the player needs to diagnose the trip. See the [worked delivery example](TRAIN_STATES.md#explaining-why-a-delivery-differed-from-the-plan).

A car holding 20 plates after delivery cannot tell us whether it delivered 80 or 180. Its current inventory is not a replacement for the trip's recorded observations.

Persist controller settings too: configured items, thresholds, network membership, inbound limits, and depot settings. Current inventory and physical train location can be read again. Reservation totals can be rebuilt from the restored assignments after checking which visits and transfers have finished.

## One owner for a shared trip

The dispatcher owns one authoritative trip record. Pickup and dropoff controllers can show that trip without saving separate copies of it.

This avoids two problems: the controllers disagreeing about the same assignment, and losing an active trip when one controller is dismantled. Where the adapter stores the record does not change who owns its meaning.

A mod-owned world subsystem actor is the proposed Unreal storage location for shared records. The integration references are [SML subsystems](https://docs.ficsit.app/satisfactory-modding/v3.7.0/Development/ModLoader/Subsystems.html) and [save game integration](https://docs.ficsit.app/satisfactory-modding/v3.9.1/Development/Satisfactory/Savegame.html). The adapter must use references that survive saving; raw pointer values and non-unique station names are not identities.

## Resume before assigning new work

On load, restore trip records and check them against the live trains, timetables, inventory, and docking state. Rebuild the remaining reservations before dispatch resumes. A saved trip may have been loading, unloading, or still parked with a new assignment.

If a referenced station or controller is gone, keep ownership of its train while cancelling obsolete reservations and arranging safe parking. Do not treat a surviving timetable as a new delivery or allow it to repeat a finished order.

The same transfer must not be counted twice if it was recorded before saving and observed again during recovery.

## Route estimates can survive a save

Route distances are useful to keep when the railway has not changed. Save estimates with their destination, starting rail position and direction, routing settings, and enough context to check whether they still apply. Reuse them lazily when that route is next needed; do not calculate every route at load time.

Saved estimates are hints until their references and route context have been checked. Runtime path objects and pointers must be rebuilt. If the railway changed or validity cannot be established, refresh before treating the result as proof that the selected train can reach its stop.

Provide **Refresh route** for a selected train-to-station route (or perhaps a global `invalid route cache` feature, after updating a sizeable chunk of the network, as new stations will not have cached distances anyways). This discards that route's cached results and queues a fresh query when an eligible locomotive is available. This also lets players discover a newly built shortcut without waiting for normal expiry. Relevant rail changes and route failures should invalidate affected entries automatically where possible; the button supplements that behavior.

The [locality-cache proposal](../dynamic-train-logistics-plan.md#proposed-locality-cache-for-path-queries) describes sharing approximate distances between nearby trains. A saved shared estimate remains approximate after loading; it must not silently become an exact result for another locomotive.
