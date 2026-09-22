import json
from pathlib import Path
import unreal

report = {}
for name in ('BP_PCGPreviewGameMode', 'BP_PCGPreviewGenerator'):
    path = '/Game/Levels/Test_CW/Prototype/Blueprints/' + name
    bp = unreal.load_asset(path)
    if not isinstance(bp, unreal.Blueprint):
        raise RuntimeError('Missing Blueprint: ' + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    entry = {'class': bp.generated_class().get_path_name()}
    if name == 'BP_PCGPreviewGenerator':
        for prop in ('test_room_count', 'allow_module_reuse', 'max_connection_attempts', 'connection_search_seconds', 'show_connection_debug'):
            entry[prop] = cdo.get_editor_property(prop)
        entry['room_modules'] = [str(v) for v in cdo.get_editor_property('room_modules')]
        entry['graph'] = str(cdo.get_editor_property('graph'))
    report[name] = entry
target = Path(unreal.Paths.project_dir()) / 'output/pcg_module_tests/connection_blueprints.json'
target.write_text(json.dumps(report, indent=2), encoding='utf-8')
print('CONNECTION_BLUEPRINT_CHECK ' + json.dumps(report))
