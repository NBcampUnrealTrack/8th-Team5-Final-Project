"""Create an isolated, playable single-player PCG fixture. Never overwrites existing assets.
Run using UnrealEditor-Cmd -ExecutePythonScript=<this file> after building the editor.
"""
import math
import unreal

ROOT = '/Game/Levels/Test_CW/GameplaySmoke'
GRAPH = '/Game/Levels/Test_CW/Prototype/PCG_DungeonPrototype'
MODE = '/Game/Core/MainGameModes/BP_HeavenBreachGameMode.BP_HeavenBreachGameMode_C'

def cube(location, scale):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    c.set_collision_profile_name('BlockAll')
    a.set_actor_scale3d(unreal.Vector(*scale))

def module(name, role, floor=unreal.HBAllowedFloor.ANY, corridor=False, portal=False):
    path = ROOT + '/Modules/' + name
    exists = unreal.EditorAssetLibrary.does_asset_exist(path)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if exists:
        assert levels.load_level(path)
    else:
        assert levels.new_level(path)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if exists and any(isinstance(a,unreal.HBDungeonModuleDefinition) for a in actors.get_all_level_actors()):
        return path
    print('AUTHOR_DEFINITION',name)
    d = actors.spawn_actor_from_class(unreal.HBDungeonModuleDefinition, unreal.Vector())
    d.set_editor_property('module_id', name)
    d.set_editor_property('module_kind', unreal.HBDungeonModuleKind.CORRIDOR if corridor else unreal.HBDungeonModuleKind.ROOM)
    info = d.get_editor_property('room_info')
    info.set_editor_property('room_role', role)
    info.set_editor_property('allowed_floor', floor)
    occupancy = actors.spawn_actor_from_class(unreal.HBDungeonOccupancy, unreal.Vector(0,0,200))
    occupancy.get_editor_property('occupancy').set_box_extent(unreal.Vector(500,500,225))
    cube((0,0,-25),(10,10,0.5))
    for i in range(4):
        angle = math.radians(i*90)
        x,y = math.cos(angle),math.sin(angle)
        c = actors.spawn_actor_from_class(unreal.HBDungeonConnector, unreal.Vector(x*500,y*500,0), unreal.Rotator(pitch=0,yaw=i*90,roll=0))
        c.get_editor_property('connector').set_editor_property('connector_id','Door'+str(i))
        if role == unreal.HBPrototypeRoomRole.BOSS:
            c.get_editor_property('connector').set_editor_property('role', unreal.HBDungeonConnectorRole.BOSS_ENTRANCE)
        elif corridor and i in (0, 2):
            c.get_editor_property('connector').set_editor_property('role', unreal.HBDungeonConnectorRole.DUNGEON_SIDE if i == 0 else unreal.HBDungeonConnectorRole.BOSS_SIDE)
        # Two wall segments leave a 300 cm opening in the center of each side.
        for side in (-325,325):
            cube((x*490-y*side,y*490+x*side,200), (0.2,3.5,4) if i%2==0 else (3.5,0.2,4))
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0,-200,100))
    start.set_editor_property('player_start_tag','DungeonSpawn')
    if portal:
        p = actors.spawn_actor_from_class(unreal.HBDungeonPortal, unreal.Vector(0,0,110))
        p.set_editor_property('portal_id','Transition')
        p.get_editor_property('trigger').set_box_extent(unreal.Vector(50,50,100))
        p.get_editor_property('arrival').set_relative_location(unreal.Vector(220,200,-10),False,False)
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(0,0,350))
    lc = light.get_component_by_class(unreal.PointLightComponent)
    lc.set_intensity(15000)
    lc.set_attenuation_radius(1800)
    assert levels.save_current_level()
    return path

def build():
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError('Run in a fresh editor process; do not discard dirty maps.')
    paths = [module('Ordinary',unreal.HBPrototypeRoomRole.ADVENTURE),
             module('Rest',unreal.HBPrototypeRoomRole.RECOVERY),
             module('Upgrade',unreal.HBPrototypeRoomRole.UPGRADE),
             module('Departure',unreal.HBPrototypeRoomRole.ADVENTURE,unreal.HBAllowedFloor.FLOOR1,portal=True),
             module('Arrival',unreal.HBPrototypeRoomRole.ADVENTURE,unreal.HBAllowedFloor.FLOOR2,portal=True),
             module('Boss',unreal.HBPrototypeRoomRole.BOSS,unreal.HBAllowedFloor.FLOOR2),
             module('Hall',unreal.HBPrototypeRoomRole.UNASSIGNED,unreal.HBAllowedFloor.FLOOR2,corridor=True)]
    target = ROOT + '/TestPCG_Gameplay'
    if unreal.EditorAssetLibrary.does_asset_exist(target):
        print('GAMEPLAY_FIXTURE_EXISTS',target)
        return
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.new_level(target)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    g = actors.spawn_actor_from_class(unreal.HBDungeonRuntime,unreal.Vector())
    g.set_editor_property('module_maps',[unreal.load_asset(p) for p in paths])
    g.set_editor_property('graph',unreal.load_asset(GRAPH))
    g.set_editor_property('random_seed_on_start',False)
    rules = g.get_editor_property('dungeon_rules')
    rules.set_editor_property('floor1_exit_map',unreal.load_asset(paths[3]))
    rules.set_editor_property('floor2_entry_map',unreal.load_asset(paths[4]))
    rules.set_editor_property('boss_corridor_map',unreal.load_asset(paths[6]))
    rules.set_editor_property('floor1_main_rooms',2)
    rules.set_editor_property('floor2_main_rooms',3)
    rules.set_editor_property('floor1_branch_rooms',1)
    rules.set_editor_property('floor2_branch_rooms',2)
    rules.set_editor_property('floor2_offset',unreal.Vector(10000,0,0))
    g.set_editor_property('dungeon_rules',rules)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,MODE))
    assert levels.save_current_level()
    print('GAMEPLAY_FIXTURE_READY',target)

build()
