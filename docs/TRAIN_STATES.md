# Train states and reservations

A delivery shares trains, stock, and station space with other deliveries. The dispatcher must protect those commitments while a trip is underway, release them when they are no longer needed, and explain any difference between the planned and actual delivery to the player. This document describes that intended behavior; the lifecycle handlers are not implemented yet.

## Preventing two assignments from claiming the same train

To avoid replacing a trip that has already been promised, availability must depend on the train’s assignment as well as its location. A parked train may already be preparing to leave:

1. Train 1 is standing in Depot A.
2. The dispatcher assigns Train 1 a trip to a drain station.
3. Before Train 1 physically departs, another request appears.

Assigning Train 1 again in step 3 would leave the first trip with reserved stations but no train to serve them. Its assignment therefore makes it unavailable before it physically leaves. The same protection applies during a fault: replacing its trip would lose track of the cargo and station visits that still need to be resolved.

## Explaining why a delivery differed from the plan

To tell the player why an order delivered less or more than expected, the trip record must retain the original request, planned pickup and arrival, starting cargo, and actual pickup and delivery for each bay. If the actual pickup replaced the planned pickup, the history could no longer show that the provider loaded less than expected. If only the delivered amount remained, it could not distinguish a short pickup from a requester that ran out of room.

Example: Motor Factory station can hold 1,000 plates. It has 700 and requests when at least 300 are missing. Train 1 has one car that holds 400 plates, with 50 plates left from a previous trip.

| Quantity | Plates | What it means |
| --- | ---: | --- |
| Original request | 300 | Missing when the dispatcher assigned the trip |
| Already aboard | 50 | Residue before the pickup |
| Planned pickup | 350 | Enough new plates to fill the car |
| Planned arrival | 400 | Existing residue plus planned pickup |
| Actually picked up | 250 | Plate Foundry station had less available when the train docked |
| Actually leaving pickup | 300 | The 50 already aboard plus 250 newly loaded |
| Actually delivered | 180 | Another source filled part of the requester while Train 1 was travelling |
| Left aboard | 120 | Cargo remaining after unloading |

In this example, the other source added 120 plates to Motor Factory station, leaving only 180 spaces when Train 1 arrived. Assume no other inventory changes during the example.

The history can now show two separate deviations: pickup was 100 plates below plan, and 120 of the plates aboard could not be delivered. That gives the player a reason to inspect both the provider’s supply and the requester’s available space, rather than treating the whole difference as a pickup failure.

The dispatcher also needs these observations to make the next decision correctly. After pickup, it reduces the incoming reservation from 400 to the 300 actually aboard, so the requester is not waiting for 100 plates that will never arrive on this train. After unloading, it records 180 delivered and releases the incoming reservation. The requester is now full because the other source also delivered plates; subtracting 180 from the original request would incorrectly suggest it still needs another 120.

Pickup can also **exceed** the original request. If Plate Foundry station has enough stock, this car loads 350 even though the requester originally lacked only 300. The stop's item filter does not impose an amount limit. Source-side belt gating can restrict the pickup; a request threshold alone cannot.

Because belts and other deliveries can change the requester’s inventory, its total stock change could credit this train for items it never carried. Measuring each car before and after transfer isolates what this train actually picked up or delivered.

## Releasing resources as the delivery progresses

Other orders should be able to use a resource as soon as this trip has finished with it. The pickup station becomes available before the dropoff station, while the train still needs its return parking after both cargo visits are over. The lifecycle below releases each reservation at the point where another order can safely use that resource.

| State or event | Pickup station | Dropoff station | Train and depot |
| --- | --- | --- | --- |
| Idle at depot | No reservation | No reservation | Train occupies parking and can be assigned |
| Trip assigned, possibly still parked | Reserve a visit and planned pickup quantities | Reserve a visit and planned incoming quantities | Assign the train and reserve its return parking |
| Depart depot | Keep reservations | Keep reservations | Release physical parking occupancy; keep return parking reserved |
| Dock and load | Keep the station visit reserved | Keep reservations | Measure newly loaded cargo |
| Undock pickup | Release the visit and pickup reservations | Update incoming quantities to what is actually aboard for this delivery | Travel to requester |
| Dock and unload | No pickup reservation | Keep the station visit reserved | Measure delivered cargo |
| Undock dropoff | No pickup reservation | Release the visit and incoming reservations | Record delivery and residue; keep the train assigned until it parks |
| Arrive at depot | No reservation | No reservation | Turn return parking into occupied parking, finish the trip, inspect residue |

A docked train still occupies the station, so its visit remains reserved until undock. Releasing it at arrival would let the dispatcher send another train beyond the station’s configured capacity. Releasing it only when the whole trip finishes would unnecessarily keep the station unavailable while the first train travels elsewhere.

Repeated notifications must not release the same reservation twice: doing so would make a busy station appear to have more capacity than it really does.

Inventory and reservation updates must describe the same moment in the trip. If the requester already holds the delivery but those plates are still counted as incoming, the dispatcher counts them twice. If incoming is cleared before inventory is refreshed, it may request the same plates again. Matching therefore waits until the bay’s transfer and its accounting update are complete.

## Making sure a proposed trip can actually proceed

Suppose Train 1 will pick up at **Plate Foundry station**, unload at **Motor Factory station**, and return to **Depot B**. Before dispatch, it needs:

- Exclusive use of Train 1.
- One allowed inbound visit at Plate Foundry station, plus the pickup quantities at its bays.
- One allowed inbound visit at Motor Factory station, plus incoming quantities at its bays.
- One parking space at Depot B.

Claiming only some of these resources could leave a train and stock unavailable to other orders while this trip waits for a dropoff slot. Reserving the complete set together lets the dispatcher either proceed with the trip or leave those resources available for another proposal.

The selected stations’ inventories and platform settings may have changed since matching, or another trip may now own the train. Refreshing that pair and train just before reservation catches those changes without scanning every station in the network. Route distances can likewise be [looked up only when needed and reused](../dynamic-train-logistics-plan.md#proposed-locality-cache-for-path-queries), so checking a trip does not require recalculating unrelated routes.

If the timetable cannot be assigned, the trip cannot use its reservations. Undoing that attempt and retrying from fresh state frees those resources for a delivery that can proceed.

That recovery must undo any timetable change as well as the reservations. If Train 1 received the schedule before the operation failed, it could still travel to Plate Foundry station while a retry promises that same train or station space elsewhere. The schedule adapter therefore needs to either succeed or leave no new active schedule behind. If it cannot undo the change, retaining the train’s assignment until its schedule is repaired or it is safely parked prevents that second, conflicting promise.

Production can continue while the train travels, which lets the provider’s dispatch threshold be lower than the planned pickup. Reserving 350 plates when only 200 are present protects that supply as it replenishes; otherwise another order could claim the same production. It does not guarantee that the missing 150 will be ready in time, which is why the actual pickup updates the requester’s incoming quantity.

## Ensuring the train has somewhere to park

A delivery needs somewhere to finish so its train does not wait in a cargo station or on busy track. Reserving return parking before departure makes that destination available even if another train reaches the depot first.

For example, Depot B has two spaces. One train is parked and one has a reserved return space. Depot B has no free space for a third train, even though one space looks empty.

A train returning to its starting depot can keep its own space. While parked, it occupies that space; after departure, the space is reserved for its return. Counting those as two separate spaces would prevent a train in a one-space depot from leaving even though its return is already accounted for.

On arrival, the reserved space becomes occupied again. Cancelling a delivery does not remove the train’s need to park, so it keeps a return space until a replacement is secured. If the depot itself disappears, arranging replacement parking becomes part of recovering the trip.

## Returning residue-carrying trains to useful work

Leftover cargo can limit which deliveries a train can serve. A drain trip clears it so the train can take other work. Like a delivery, it needs exclusive use of the train, capacity at the drain station until undock, and parking for its return.

A drain trip should resolve the residue that made it necessary. Each affected car therefore needs an unload bay at its position, of the correct type and with enough room. Checking only one dirty car could send the train on a trip that leaves it unable to take new work.

Whether the train can resume useful work depends on the available outlets:

- Available for a safe delivery that uses or safely carries that residue.
- Assigned to a drain trip; unavailable even before departure.
- Held at the depot when neither a safe delivery nor a drain outlet exists, so its residue cannot be unloaded onto an unsuitable belt.

New demand or drain space may make a previously impossible trip useful. Reconsidering affected trains when those conditions change lets them return to work without requiring the player to clear a permanent “stuck” flag.

A requester for the residue is not enough on its own. The dispatcher needs a feasible safe trip. Normal matching currently also requires a provider, so a requester with no usable provider cannot by itself rescue the train through that path.

For what survives a saved game and how a trip resumes, see [Saving and recovery](SAVING.md).
