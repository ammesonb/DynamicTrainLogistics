# Matching

To satisfy a shortage without creating a problem elsewhere, matching must find available supply, a train that fits both stations, and stops that will transfer only the intended cargo. A plentiful provider is not useful if its extra load bays would fill cars the requester cannot empty.

The result is a proposed delivery for the dispatcher to reserve and assign. Keeping that distinction matters because another trip can claim the train, or a station’s inventory can change, before the proposal is committed.

## Choosing a train that lines up at both stations

For cargo to reach the intended belt, a freight car must meet the correct platform at both pickup and dropoff. The docking engine occupies the station itself, and every vehicle behind it advances one platform position. Extra engines therefore change where the freight cars arrive:

```text
World position       Station     Bay 0       Bay 1       Bay 2
Two front engines    Engine      Engine      Freight     Freight
Engine in middle     Engine      Freight     Engine      Freight
```

In the first train, its first freight car serves bay 1. In the second, freight cars serve bays 0 and 2. Counting only freight cars would send cargo to the wrong positions.

An engine cannot load or unload. A cargo platform opposite an engine is therefore harmless during that visit. Freight cars farther back keep their positions; an engine in the middle does not close the gap.

Correct station facing and compatible train layouts let the same freight car meet the intended bay at both stops. Players provide that layout; the design does not attempt to repair a reversed station or rearrange the train between visits.

The examples use bay numbers starting at zero. Unless shown otherwise, each train has one engine at the front and the stated number of freight cars behind it. Example capacities are deliberately small, not Satisfactory defaults.

## Avoiding trips too small to be worthwhile

A train visit uses track and station time even if it moves very little. Thresholds let the player decide how much supply or shortage justifies that visit. A provider’s threshold means “have at least this much available before arranging pickup.” A requester’s threshold means “be missing at least this much before arranging delivery.” Both are entered as item counts or fluid volume.

| Bay direction | Quantity compared with the threshold |
| --- | --- |
| Load | Stock minus pickups already promised |
| Unload | Capacity for the configured item minus its stock and deliveries already incoming |

Already-promised pickups and incoming deliveries reduce these quantities so two orders do not claim the same supply or answer the same shortage. A positive amount must remain and meet the threshold; otherwise the proposed visit has nothing useful to collect or supply, even if the player set a zero threshold.

For example, a bay holds up to 1,000 plates and requests when at least 300 are missing. With 700 plates in stock, it requests. If another 100 are already incoming, it waits: only 200 remain missing.

The inventory read covers the platform, so external containers do not automatically increase its measured shortage or unloading room. Other items can also occupy slots that the requested item would need. As a result, meeting the request threshold establishes that a visit is worthwhile, but does not guarantee the whole delivery will fit.

Keeping player quantities in items makes the threshold useful across different storage sizes. The matcher converts physical slots to capacity for the selected item: a four-slot car holds 400 items that stack to 100, or 200 items that stack to 50. Counting a partial stack by its actual contents—25 plates, for example—avoids promising supply that is not there.

The current matching policy plans up to a full car at each selected position. That may exceed the original request. If pickup is short, the dispatcher updates the incoming reservation after loading. After delivery, it records what actually arrived and checks the requester's remaining demand. [Explaining delivery differences](TRAIN_STATES.md#explaining-why-a-delivery-differed-from-the-plan) explains why those numbers stay separate.

### Example: enough supply, enough demand, and a compatible train

```text
Provider: Plate Foundry station      Requester: Motor Factory station
Network: A                          Network: A
Bay 0: load plates                  Bay 0: unload plates
Stock: 400                          Capacity: 1,000; stock: 600
Minimum available: 200              Minimum missing: 300

Train 1: parked and unassigned
One engine and one empty freight car, with room for 400 plates
Both stations have room for another assigned visit; no quantities are reserved.
```

Plate Foundry station has enough available stock. Motor Factory station is missing 400, enough to request. The freight car lines up with bay 0 at both stations. The proposal is a 400-plate delivery using Train 1.

If the car already holds 50 plates, the planned arrival is still 400, but the pickup reservation is only 350. Cargo already aboard does not need to be promised by the provider again.

## Preventing cargo transfers the order did not ask for

Stop filters apply to every platform reached by the train. To avoid collecting cargo with no planned destination or unloading it onto the wrong belt, matching must inspect the other vehicle positions as well as the requested ones.

### Ending the train before an extra load bay

```text
Bay     Plate Foundry station       Motor Factory station
0       Load plates                 Unload plates
1       Empty platform              Unload plates
2       Load plates                 Empty platform
```

Assume both load bays have enough stock, both unload bays have enough missing, and the train's freight cars are empty.

A train with three freight cars is rejected. The plate load filter would fill the car at bay 2 as well as the car at bay 0, but this trip has no destination for the plates in the last car.

A train with one or two freight cars can serve bay 0. It ends before the extra load bay. With two empty cars, the requester also has a car opposite bay 1, but that car carries nothing to unload.

This works because the unwanted pickup is after the useful one. The item filter applies to the whole stop; selecting bay 0 in the order does not switch the other load bays off.

### Using an engine position to avoid an unwanted pickup

```text
Bay     Plate Foundry station       Motor Factory station
0       Load plates                 Empty platform
1       Load plates                 Unload plates
```

With one front engine, shortening the train cannot solve this pairing. One freight car cannot reach bay 1. Two freight cars reach it, but the first also loads unwanted plates at bay 0.

**Two front engines and one freight car do work.** The second engine sits opposite bay 0 and cannot load anything. The freight car sits opposite bay 1 at both stations, where the delivery belongs.

The choice is therefore about vehicle positions, not freight-car count alone. A different provider or a changed platform layout can also solve it.

An unwanted load bay opposite a freight car remains unsafe even when currently empty: it may fill before arrival. The matcher also rejects an unconfigured load bay that could load the item, rather than assuming it will stay empty. A bay configured for another item can still be unsafe if it actually holds the proposed item.

### Keeping leftover cargo off the wrong belt

Suppose a plate delivery uses bay 0, but the freight car opposite bay 1 also holds plates. If the requester's bay 1 feeds a screw belt, that train is unsafe. The plate unload filter applies there too.

If that other car instead holds screws, the plate-only filter blocks them from unloading. It can carry them through this visit, provided the pickup stop also causes no unwanted transfer. Those screws still need a safe destination later.

Even a same-item bay may already have deliveries assigned to it. Allowing another car to unload there without including it in the order would bypass those incoming reservations. The current matcher therefore rejects allowed unloads outside the selected bays as well as those that would contaminate a belt.

## Finding useful candidates without overcrowding stations

A requester’s shortage gives the search a reason to start. Looking for its configured providers avoids inspecting unrelated stock throughout the network. Their available supply must meet the pickup threshold so the train is not sent for an amount the player considers too small.

Network membership keeps proposals within the groups the player intends to work together. Stored configuration narrows that search, but a removed platform or a load bay flipped to unload can no longer supply the item. Checking its current setting prevents an old entry from qualifying an unusable station.

Station limits protect the dock and the track leading to it. A provider and requester can have matching bays and still be unable to accept another train because earlier deliveries already occupy their allowed inbound visits. Such a pairing waits until capacity is released; raising the limit only helps if the player has built room for the waiting trains.

Finally, both stations must support the train’s actual freight positions and cargo types. The whole-stop checks above rule out extra pickups and misplaced residue. Priority is considered only after these requirements are satisfied, because a useful-looking delivery must not earn permission to put cargo on the wrong line.

## Choosing between useful deliveries

When several deliveries are possible, the matcher first favors work that uses leftover cargo, then work that serves more bays per visit. It compares candidates in this order, using later preferences only when earlier ones are equal:

1. **Uses residue:** contributing leftover cargo to a delivery reduces the amount that otherwise needs another outlet.
2. **More requesting bays served:** serving several bays in one visit can avoid separate trips to address those shortages.
3. **Shorter train:** for otherwise equal work, a smaller train occupies less track and leaves larger trains available for deliveries that need their farther-back freight cars. Engines count because they occupy physical length too.
4. **Stable IDs:** a fixed final tie-break makes the same situation produce the same choice, which helps explain and reproduce behavior.

For example, suppose all three proposals are safe:

| Proposal | Uses matching residue | Bays served | Total engines and cars |
| --- | --- | --- | --- |
| Plate delivery using Train 1 | No | 2 | 3 |
| Screw delivery using Train 9 | Yes | 1 | 4 |
| Plate delivery using Train 5 | No | 1 | 2 |

The screw delivery wins because it uses residue. Once Train 9 is assigned, the two-bay plate delivery wins over the smaller train serving only one bay. Among trains serving those same two bays without residue, the shorter train wins.

Simply carrying residue through a stop does not reduce it, so only cargo contributing to this delivery earns the preference. Treating that as a yes/no benefit also avoids comparing unlike units, such as a count of screws against a volume of water.

Comparing all requested items allows a better delivery to win even if its station happened to be examined later. After the dispatcher reserves the winner, another search accounts for the resources it used; selecting several old proposals at once could assign the same train or station space twice.

This chooses the next delivery, not the best combination of all deliveries at once. If a requester repeatedly waits, first look for practical causes: too few trains, insufficient buffers, thresholds causing excessive small visits, station limits, or continual residue. Fixed priorities can also repeatedly favor one requester over another whenever they compete. More capacity may remove the competition; fair rotation is a separate possible improvement if waiting remains a problem. No waiting-time preference is currently implemented.

## Catching changes before committing the trip

A good proposal can become unusable if a player changes a platform or another order claims its train. Refreshing the selected stations and train immediately before reservation catches those changes. Checking and reserving them together prevents another assignment from taking the resources between those two steps.

This targeted refresh gives the dispatcher current information where it matters without reading every provider each cycle. Relevant providers are read when demand needs them, and the chosen pair is checked before commitment. Route distances can likewise be calculated when needed and reused to avoid repeating unrelated work.

The matcher and ranking are implemented. The dispatcher lifecycle and game adapters are still to be built. [Train states and reservations](TRAIN_STATES.md) describes what follows a match; [Saving and recovery](SAVING.md) covers what must survive a saved game. Detailed API and representation notes belong in the [plan](../dynamic-train-logistics-plan.md) and source headers.
