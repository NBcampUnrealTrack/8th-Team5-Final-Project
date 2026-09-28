# PCG dungeon module preview

**Current setup:** register maps once in `Module Maps`, then choose transition maps, portal actors and the boss
corridor in `Transitions`. A sole portal is selected automatically; multiple portals use an actor-label dropdown. See [Rules and portals](RulesAndPortals.md) for authoring and validation.
`Use Dungeon Rules` defaults to true; missing selections stop generation with an explanatory error.
Turn it off only for unrestricted LD door-fitting tests. The active rule planner is `HBDungeonRuleLayout`,
with physical connector alignment and a portal transition.

Open `/Game/Levels/Test_CW/Maps/TestPCG_Preview`, use **one-player Standalone/PIE**, and press Play.
After loading all modules, the preview creates one **collision-free inspection pawn** inside the first room.
It has no combat or interaction components. Existing room and door collisions are preserved.
Click the Play viewport to focus keyboard input. Wait for the green READY message before regenerating.

- `R`: generate a new random seed after unloading the previous modules.
- `T`: regenerate the displayed seed.
- `1`: both floors, `2`: floor 1, `3`: floor 2.
- `Tab`: switch between exploration and overview; exploration position is preserved.
- Exploration: mouse look, `WASD` fly, `Q` / `E` descend / ascend, `Shift` five-times speed.
- Overview: `WASD` pan; `Q` / `E` zoom in / out.
- `Home`: enter the starting room. `PageUp` / `PageDown`: previous / next room (wraps).
- Regeneration temporarily returns to overview, then resets exploration to the new starting room.
- To reproduce a run on the next Play: set the generator's `Seed` and turn off `Random Seed On Start`.

The serialized graph `Prototype/PCG_DungeonPrototype` connects **HB - Plan and Validate Dungeon**
to **HB - Load Room Modules**, then Output. Both execute on the game thread and disable PCG caching
because they depend on the preview actor and create runtime streaming levels.
PCG is on-demand and explicitly started from BeginPlay; no player proximity source is required.
Automatic editor-property regeneration is disabled; use the preview actor's regeneration controls.
READY is reported only after every streaming level is loaded and visible, PCG has completed,
and the actual portal pair has been validated and reciprocally linked.

Blue = adventure, green = recovery, gold = upgrade, red = boss, silver = corridor. White arrows are
main-path door links, yellow arrows are optional links, and magenta is the configured portal transition.
Floor 2 starts at the configured offset from the generator; logical floor IDs do not depend on world Z.

## Scope

The rule planner generates two main paths joined by the selected portal pair, with optional terminal
branches and one selected corridor immediately before the sole floor-2 boss. Per-floor main/branch
counts are configurable. Role caps, floor eligibility, special-role adjacency, and at most three
successive adventure rooms per logical floor are enforced during backtracking and independently checked afterward.
Changing floors resets the streak; an Adventure arrival room counts as the first room on the new floor.
Corridors are excluded from role counts. Floor 2 must have more optional branches than floor 1.

Door transforms are aligned and occupancy boxes use AABB broad-phase and OBB narrow-phase checks.
Source maps and portal positions are authored by the user; generation does not edit those maps.
The inspector can traverse portals despite having no collision.

Actual passage clearance, room-internal walkability, floor-2 interior complexity, AI navigation,
multiplayer dungeon synchronization and room gameplay remain separate concerns. The collision-free
inspector cannot prove that a real character can walk through every authored doorway.

## Authoring and checks

The older `author_preview.py` helpers prepare legacy preview assets; use the current setup guide
above to configure transition maps, portal actors, floor eligibility and the boss corridor explicitly.
Python helper functions are called from UE Python individually and released before switching maps.
It refuses to overwrite/recenter an existing module. `prepare_module` creates and opens a new level from the source template and verifies its package path before geometry edits.
`configure_preview` creates the graph and assigns the preview generator/GameMode to TestPCG_Preview.

Automation tests: `HeavenBreach.PCGModule` includes current rule generation/replay, portal authoring
and actual Pawn travel, catalog extraction, physical connector alignment and occupancy checks.
`HeavenBreach.PCGPrototype` retains tests for the older grid planner.
Runtime inspection: generator `GetVerificationSummary()` includes validity, visibility and pawn counts;
`GetLayoutSignature()` supports before/after comparisons. Logs use `LogHBPCGPreview`.
`verify_preview.py` records initial, identical-seed replay, and new-seed runtime results under
`output/pcg_prototype`. Each snapshot requires READY, valid layout, zero retiring levels and no extra pawns.
When the inspector exists, it must be the sole possessed pawn with actor collision disabled.
`verify_exploration.py` exercises the movement function used by keyboard polling against a blocking
wall trace, mouse-look deltas, accelerated vertical movement, overview round-trip and room shortcuts.
Its separate regeneration check verifies pawn reuse and the transformed starting-room position.
Exploration evidence is saved under `output/pcg_exploration`.

Ensure all selected module maps are included when cooking. A packaged build is a separate verification from PIE.
