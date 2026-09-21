"""Run the individual functions inside UE Python; never retain World/Actor objects between calls.

prepare_module('Adventure01'), prepare_module('Boss'), configure_preview().
Only new module copies and /Game/Levels/Test_CW/Maps/TestPCG_Preview are authored.
"""
import gc
import itertools
import json
from pathlib import Path

import unreal

ROOT = '/Game/Levels/Test_CW/Prototype'
MANIFEST = Path(unreal.Paths.project_dir()) / 'output/pcg_prototype/module_manifest.json'


def _bounds(actors):
    points = []
    for actor in actors:
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if not comp.get_editor_property('static_mesh'):
                continue
            low, high = comp.get_local_bounds()
            transform = comp.get_world_transform()
            for xyz in itertools.product((low.x, high.x), (low.y, high.y), (low.z, high.z)):
                v = unreal.MathLibrary.transform_location(transform, unreal.Vector(*xyz))
                points.append((v.x, v.y, v.z))
    if not points:
        raise RuntimeError('Module has no mesh bounds')
    return ([min(p[i] for p in points) for i in range(3)],
            [max(p[i] for p in points) for i in range(3)])


def prepare_module(kind):
    assert kind in ('Adventure01', 'Boss')
    target = ROOT + '/Modules/LI_' + kind
    source = '/Game/Levels/Test_CW/Maps/Test_' + kind
    if unreal.EditorAssetLibrary.does_asset_exist(target):
        raise RuntimeError('Module already exists; do not duplicate/recenter twice: ' + target)
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError('Save/review current dirty map before switching')
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    # This API creates AND opens the copy. save_map only writes a copy and leaves
    # the original current, so it must never be used before these actor edits.
    assert levels.new_level_from_template(target, source)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split('.')[0] == target
    del world
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = []
    for actor in subsystem.get_all_level_actors():
        remove = isinstance(actor, (unreal.PlayerStart, unreal.DirectionalLight, unreal.SkyLight,
                                    unreal.SkyAtmosphere, unreal.ExponentialHeightFog, unreal.VolumetricCloud))
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = comp.get_editor_property('static_mesh')
            remove = remove or bool(mesh and mesh.get_name() == 'SM_SkySphere')
        if remove:
            removed.append(actor.get_actor_label())
            assert subsystem.destroy_actor(actor)
    actors = list(subsystem.get_all_level_actors())
    low, high = _bounds(actors)
    offset = unreal.Vector((low[0]+high[0])/2, (low[1]+high[1])/2, 0)
    for actor in actors:
        if actor.get_attach_parent_actor() is None:
            actor.set_actor_location(actor.get_actor_location()-offset, False, False)
    # Bounds describe geometry, not proven playable space or opening locations.
    low, high = _bounds(actors)
    assert levels.save_current_level()
    info = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else {}
    info[kind] = {'asset': target, 'source': source, 'min': low, 'max': high,
                  'removed_environment': removed, 'actor_count': len(actors),
                  'source_origin_offset': [offset.x, offset.y, 0]}
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(info, indent=2), encoding='utf-8')
    print('MODULE_PREPARED', json.dumps(info[kind]))


def configure_preview():
    target_map = '/Game/Levels/Test_CW/Maps/TestPCG_Preview'
    current_map = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0]
    if any(p.get_name() != target_map for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()):
        raise RuntimeError('Dirty map must be reviewed first')
    data = json.loads(MANIFEST.read_text())
    graph_path = ROOT + '/PCG_DungeonPrototype'
    graph = unreal.load_asset(graph_path) if unreal.EditorAssetLibrary.does_asset_exist(graph_path) else None
    if graph is None:
        graph = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'PCG_DungeonPrototype', ROOT, unreal.PCGGraph, unreal.PCGGraphFactory())
        assert graph
        plan, _ = graph.add_node_of_type(unreal.HBDungeonPlanSettings)
        load, _ = graph.add_node_of_type(unreal.HBDungeonLoadSettings)
        plan.set_node_position(0, 0)
        load.set_node_position(400, 0)
        graph.get_output_node().set_node_position(800, 0)
        graph.get_input_node().set_node_position(-350, -250)
        assert graph.add_edge(plan, 'Rooms', load, 'Rooms')
        assert graph.add_edge(load, 'Out', graph.get_output_node(), 'Out')
        unreal.EditorAssetLibrary.save_loaded_asset(graph)
    # Graph references are safe across a map change; no World objects are retained.
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if current_map != target_map:
        assert levels.load_level(target_map)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in subsystem.get_all_level_actors():
        if isinstance(actor, (unreal.PlayerStart, unreal.SkyAtmosphere, unreal.ExponentialHeightFog, unreal.VolumetricCloud)) or actor.get_actor_label() in ('Floor', 'SM_SkySphere'):
            assert subsystem.destroy_actor(actor)
        elif isinstance(actor, unreal.DirectionalLight):
            light = actor.get_component_by_class(unreal.DirectionalLightComponent)
            light.set_mobility(unreal.ComponentMobility.MOVABLE)
            light.set_intensity(8.0)
            actor.set_actor_rotation(unreal.Rotator(-65, -25, 0), False)
    generators = [a for a in subsystem.get_all_level_actors() if isinstance(a, unreal.HBPCGPreviewGenerator)]
    assert len(generators) <= 1
    generator = generators[0] if generators else subsystem.spawn_actor_from_class(unreal.HBPCGPreviewGenerator, unreal.Vector(0,0,0))
    generator.set_actor_label('PCG_DungeonPrototype_Generator')
    generator.set_editor_property('graph', graph)
    generator.set_editor_property('adventure_module', unreal.load_asset(data['Adventure01']['asset']))
    generator.set_editor_property('boss_module', unreal.load_asset(data['Boss']['asset']))
    # Drop temporary module World references before any later map transition.
    for role, prop in [('Adventure01','adventure_bounds'), ('Boss','boss_bounds')]:
        generator.set_editor_property(prop, unreal.Box(min=unreal.Vector(*data[role]['min']), max=unreal.Vector(*data[role]['max'])))
    generator.set_editor_property('cell_size', 16000.0)
    generator.set_editor_property('floor_height', 5000.0)
    generator.set_editor_property('seed', 12345)
    generator.set_editor_property('random_seed_on_start', True)
    generator.set_editor_property('start_in_exploration', True)
    generator.get_editor_property('pcg').set_graph(graph)
    generator.get_editor_property('pcg').set_editor_property('regenerate_in_editor', False)
    camera = generator.get_editor_property('preview_camera')
    camera.set_editor_property('override_aspect_ratio_axis_constraint', True)
    camera.set_editor_property('aspect_ratio_axis_constraint', unreal.AspectRatioAxisConstraint.ASPECT_RATIO_MAINTAIN_XFOV)
    exposure = camera.get_editor_property('post_process_settings')
    exposure.set_editor_property('auto_exposure_bias', 2.0)
    exposure.set_editor_property('override_bloom_intensity', True)
    exposure.set_editor_property('bloom_intensity', 0.0)
    camera.set_editor_property('post_process_settings', exposure)
    generator.set_preview_view(0)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.HBPCGPreviewGameMode)
    assert levels.save_current_level()
    print('PREVIEW_CONFIGURED', graph.get_path_name(), generator.get_path_name())
