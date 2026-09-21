"""Call snapshot('initial'|'replay'|'new_seed') in UE Python after READY.

Call request_regeneration(False|True) separately, then wait for READY before the
next snapshot. Functions retain no World/Actor references after returning.
"""
import json
from pathlib import Path
import unreal

OUTPUT = Path(unreal.Paths.project_dir()) / 'output/pcg_prototype'


def snapshot(stage):
    assert stage in ('initial', 'replay', 'new_seed')
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds) == 1, 'Requires one-player PIE'
    generators = unreal.GameplayStatics.get_all_actors_of_class(worlds[0], unreal.HBPCGPreviewGenerator)
    assert len(generators) == 1
    generator = generators[0]
    assert generator.get_editor_property('ready'), generator.get_verification_summary()
    summary = generator.get_verification_summary()
    assert 'Valid=1' in summary and 'Retiring=0' in summary, summary
    pawns = unreal.GameplayStatics.get_all_actors_of_class(worlds[0], unreal.Pawn)
    explorer = generator.get_editor_property('explorer')
    assert len(pawns) == (1 if explorer else 0), summary
    if explorer:
        assert pawns[0] == explorer and not explorer.get_actor_enable_collision()
        assert unreal.GameplayStatics.get_player_controller(worlds[0], 0).get_controlled_pawn() == explorer
    report = {'summary': summary, 'signature': generator.get_layout_signature(),
              'seed': generator.get_editor_property('active_seed')}
    if stage != 'initial':
        initial = json.loads((OUTPUT / 'runtime_final_initial.json').read_text())
        report['same_signature'] = report['signature'] == initial['signature']
        assert report['same_signature'] == (stage == 'replay')
        assert (report['seed'] == initial['seed']) == (stage == 'replay')
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / ('runtime_final_' + stage + '.json')).write_text(json.dumps(report, indent=2))
    print('VERIFIED', stage, json.dumps(report))


def request_regeneration(new_seed):
    world = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.HBPCGPreviewGenerator)[0]
    assert generator.get_editor_property('ready'), generator.get_verification_summary()
    generator.regenerate(new_seed)
    print('REGENERATION_REQUESTED', new_seed)
