"""Run exercise() after READY in one-player PIE. Uses the same Fly path as keyboard input.
No live Unreal objects are retained outside functions; safe across PIE/map transitions.
"""
import json
import re
from pathlib import Path
import unreal

OUTPUT = Path(unreal.Paths.project_dir()) / 'output/pcg_exploration'


def exercise():
    world = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    g = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HBPCGPreviewGenerator)[0]
    assert g.get_editor_property('ready') and g.get_editor_property('exploring')
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    p = g.get_editor_property('explorer')
    assert pc.get_controlled_pawn() == p and pc.get_view_target() == p
    assert not p.get_actor_enable_collision()
    assert len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)) == 1
    signature = g.get_layout_signature()
    # Prove actual level geometry blocks a collision trace, but not the inspector's movement.
    g.focus_room(0)
    start = p.get_actor_location()
    direction = p.get_actor_forward_vector()
    hit = unreal.SystemLibrary.line_trace_single(world, start, start+direction*10000,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [p], unreal.DrawDebugTrace.NONE)
    assert hit is not None, 'Expected blocking room geometry in front of inspection entry'
    hit_text = hit.export_text()
    assert 'bBlockingHit=True' in hit_text
    distance = float(re.search(r'Distance=([0-9.]+)', hit_text).group(1))
    assert distance > 0
    travel = distance + 300
    p.fly(unreal.Vector(1,0,0), unreal.Vector2D(0,0), travel/p.get_editor_property('move_speed'), False)
    assert (p.get_actor_location()-(start+direction*travel)).length() < 0.1
    rotation = p.get_actor_rotation()
    p.fly(unreal.Vector(0,0,0), unreal.Vector2D(50,20), 0.1, False)
    assert abs(p.get_actor_rotation().yaw-rotation.yaw) > 1
    assert abs(p.get_actor_rotation().pitch-rotation.pitch) > 1
    start = p.get_actor_location()
    p.fly(unreal.Vector(0,0,1), unreal.Vector2D(0,0), 1.0, True)
    assert abs((p.get_actor_location()-start).z-p.get_editor_property('move_speed')*5) < 0.1
    saved = p.get_actor_location()
    g.set_preview_view(0)
    assert not g.get_editor_property('exploring') and pc.get_view_target() == g
    g.set_exploration(True)
    assert pc.get_view_target() == p and (p.get_actor_location()-saved).length() < 0.1
    room_count = len(g.get_editor_property('rooms'))
    for index in range(room_count):
        g.focus_room(index)
        assert g.get_editor_property('inspection_room') == index
        assert pc.get_view_target() == p
    g.focus_room(-1)
    assert g.get_editor_property('inspection_room') == room_count-1
    g.focus_room(room_count)
    assert g.get_editor_property('inspection_room') == 0
    room = g.get_editor_property('rooms')[0]
    expected_start = unreal.MathLibrary.transform_location(room.get_editor_property('transform'), g.get_editor_property('adventure_entry'))
    assert (p.get_actor_location()-expected_start).length() < 0.1
    assert signature == g.get_layout_signature()
    assert len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)) == 1
    report = {'summary': g.get_verification_summary(), 'pawn_name': p.get_name(),
              'signature': signature, 'blocking_hit': hit_text, 'hit_distance': distance,
              'travel_through_surface': travel, 'look_input': True, 'vertical_fast_move': True,
              'overview_round_trip': True, 'room_shortcuts_checked': room_count, 'collision': False}
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT/'exploration_checks.json').write_text(json.dumps(report, indent=2))
    print('EXPLORATION_VERIFIED', json.dumps(report))


def verify_regeneration(same_seed):
    world = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    g = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HBPCGPreviewGenerator)[0]
    assert g.get_editor_property('ready') and g.get_editor_property('exploring')
    previous = json.loads((OUTPUT/'exploration_checks.json').read_text())
    p = g.get_editor_property('explorer')
    assert p.get_name() == previous['pawn_name']
    assert len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)) == 1
    assert not p.get_actor_enable_collision()
    assert (g.get_layout_signature() == previous['signature']) == same_seed
    assert g.get_editor_property('inspection_room') == 0
    room = g.get_editor_property('rooms')[0]
    expected_start = unreal.MathLibrary.transform_location(room.get_editor_property('transform'), g.get_editor_property('adventure_entry'))
    assert (p.get_actor_location()-expected_start).length() < 0.1
    assert 'Retiring=0' in g.get_verification_summary()
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    assert pc.get_controlled_pawn() == p and pc.get_view_target() == p
    report = {'summary': g.get_verification_summary(), 'same_pawn': True,
              'same_layout': same_seed, 'exploring': True, 'returned_to_start': True}
    (OUTPUT/('replay.json' if same_seed else 'new_seed.json')).write_text(json.dumps(report, indent=2))
    print('REGENERATION_VERIFIED', json.dumps(report))
