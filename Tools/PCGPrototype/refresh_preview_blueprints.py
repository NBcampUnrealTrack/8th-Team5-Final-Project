"""Compile/resave ONLY the two preview BPs after the native catalog upgrade.

Run inside UE Python or the PythonScript commandlet. Never saves source module maps.
Existing room/corridor arrays are preserved; native PostLoad migrates old map slots.
"""
import json
from pathlib import Path

import unreal

ROOT = '/Game/Levels/Test_CW/Prototype/Blueprints/'
report = {}
for name in ('BP_PCGPreviewGameMode', 'BP_PCGPreviewGenerator'):
    asset = unreal.load_asset(ROOT + name)
    if not isinstance(asset, unreal.Blueprint):
        raise RuntimeError('Expected existing Blueprint: ' + ROOT + name)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    cdo = unreal.get_default_object(asset.generated_class())
    record = {'path': asset.get_path_name(), 'class': asset.generated_class().get_path_name()}
    if name == 'BP_PCGPreviewGenerator':
        record['room_modules'] = [str(v) for v in cdo.get_editor_property('room_modules')]
        record['corridor_modules'] = [str(v) for v in cdo.get_editor_property('corridor_modules')]
        # Read source-map diagnostics into a transient cache; source maps are never saved.
        cdo.refresh_module_catalog()
        record['catalog_errors'] = list(cdo.get_editor_property('catalog_errors'))
        record['catalog_count'] = len(cdo.get_editor_property('module_catalog'))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + ROOT + name)
    report[name] = record

target = Path(unreal.Paths.project_dir()) / 'output/pcg_module_tests/blueprint_catalog.json'
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('PCG_BLUEPRINT_CATALOG ' + json.dumps(report, ensure_ascii=False))
