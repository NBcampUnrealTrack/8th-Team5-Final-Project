# PCG dungeon module preview

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
READY is reported only after every streaming level is loaded and visible and PCG has completed.

Blue = adventure, green = recovery, gold = upgrade, red = boss. White arrows are main-path links,
yellow arrows are optional links, and magenta is the planned floor transition. Floors are displayed
in separate panels with a height offset. Recovery/upgrade reuse the adventure geometry and have no gameplay.

## Scope

This verifies rule-based placement, seeded variation/replay, bounding-box separation, and module loading.
The planner uses a connected tree with 4–5 main-path rooms on each floor, one optional room on floor 1
and two on floor 2. The sole boss ends the floor-2 main path. Role caps, adjacency, and maximum three
successive adventure rooms are validated across the floor transition and on optional paths.

**Arrows are planned connections, not corridors or traversable doorways.** Original blocking walls remain.
Actual sockets/openings, branch door capacity, boss corridor, stairs between floors, enhanced floor-2
interiors, AI navigation, multiplayer replication, room interactions and gameplay movement are outside this preview.
The inspector flies through doors, walls, floors and gaps between rooms; this is not walkable dungeon connectivity.
Full geometric AABBs are conservative occupancy bounds, not playable-space bounds.

Original `Test_Adventure01` and `Test_Boss` are preserved. New module maps contain centered geometry
without their global lighting/sky/PlayerStart. TestPCG_Preview owns the global environment and preview GameMode.
AdventureEntry and BossEntry on the generator are module-local inspection entry positions, based on
the original PlayerStarts plus eye height. A camera light makes unlit interiors inspectable.

## Authoring and checks

`author_preview.py` functions are called from UE Python individually and released before switching maps.
It refuses to overwrite/recenter an existing module. `prepare_module` creates and opens a new level from the source template and verifies its package path before geometry edits.
`configure_preview` creates the graph and assigns the preview generator/GameMode to TestPCG_Preview.

Automation tests: `HeavenBreach.PCGPrototype` (256 seeds, deterministic replay, graph reachability,
role counts, rejection of cycles/overlaps/invalid starts/impossible spacing).
Runtime inspection: generator `GetVerificationSummary()` includes validity, visibility and pawn counts;
`GetLayoutSignature()` supports before/after comparisons. Logs use `LogHBPCGPreview`.
`verify_preview.py` records initial, identical-seed replay, and new-seed runtime results under
`output/pcg_prototype`. Each snapshot requires READY, valid layout, zero retiring levels and no extra pawns.
When the inspector exists, it must be the sole possessed pawn with actor collision disabled.
`verify_exploration.py` exercises the movement function used by keyboard polling against a blocking
wall trace, mouse-look deltas, accelerated vertical movement, overview round-trip and room shortcuts.
Its separate regeneration check verifies pawn reuse and the transformed starting-room position.
Exploration evidence is saved under `output/pcg_exploration`.

The three maps are explicitly listed for cooking. A packaged build is a separate verification from PIE.
