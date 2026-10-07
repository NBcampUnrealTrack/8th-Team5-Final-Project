# -*- coding: utf-8 -*-
"""에디터 Output Log(Cmd)에서 실행:
py "<프로젝트 경로>/Tools/Items/BagWorldMeshes.py"
원본(.glb, 아이콘 .png)은 <프로젝트>/Saved/BagImport 에 둔다.
"""
import csv
import io
import os
import re

import unreal

SOURCE_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "BagImport")
MESH_ROOT = "/Game/Items/Asset/Bags"
ICON_DIR = "/Game/Items/Icon/Bag"
DT_PATH = "/Game/Items/DataTables/DT_Item.DT_Item"
MATERIAL_SOURCE = "BAG_008"
REIMPORT_MESHES = False

CHEST_MESH = "/Game/Asset/AK3D_ME_Building/Meshes/Props/Accessories/SM_Acces_Chest_01_Closed.SM_Acces_Chest_01_Closed"
CHEST_TEXTURE = "/Game/Asset/AK3D_ME_Building/Textures/Accessories/T_Acces_Chest_01_D.T_Acces_Chest_01_D"

TARGET_SIZE_CM = {
    "BAG_001": 22.0,
    "BAG_002": 40.0,
    "BAG_003": 40.0,
    "BAG_004": 42.0,
    "BAG_005": 60.0,
    "BAG_006": 62.0,
    "BAG_007": 65.0,
    "BAG_008": 70.0,
    "BAG_009": 55.0,
}

LIB = unreal.DataTableFunctionLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
TEXT_MACRO = re.compile(r'^\s*(?:NSLOCTEXT|LOCTEXT|INVTEXT)\s*\((.*)\)\s*$', re.S)
QUOTED = re.compile(r'"((?:[^"\\]|\\.)*)"')


def plain(cell):
    m = TEXT_MACRO.match(cell or "")
    if not m:
        return (cell or "").strip()
    parts = QUOTED.findall(m.group(1))
    return parts[-1].strip() if parts else ""


def import_file(path, dest):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.automated = True
    task.replace_existing = True
    task.save = True
    ASSET_TOOLS.import_asset_tasks([task])
    return list(task.imported_object_paths)


def assets_in(folder):
    out = []
    for path in EAL.list_assets(folder, recursive=True, include_folder=False):
        asset = EAL.load_asset(path)
        if asset is not None:
            out.append(asset)
    return out


def find_mesh(folder):
    for asset in assets_in(folder):
        if isinstance(asset, unreal.StaticMesh):
            return asset
    return None


def mesh_extent(mesh):
    box = mesh.get_bounding_box()
    size = box.max - box.min
    return size, max(size.x, size.y, size.z)


def ensure_collision(mesh, edit=True):
    try:
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        count = sub.get_simple_collision_count(mesh)
        if count == 0 and edit:
            sub.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
            return "박스 콜리전 추가"
        return "콜리전 %d개" % count
    except Exception:
        lib = unreal.EditorStaticMeshLibrary
        count = lib.get_simple_collision_count(mesh)
        if count == 0 and edit:
            lib.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
            return "박스 콜리전 추가"
        return "콜리전 %d개" % count


def object_path(asset):
    path = asset.get_path_name()
    return path


def soft_cell(asset, class_name):
    return "/Script/%s'%s'" % (class_name, object_path(asset))


def export_chest_for_icon():
    out_dir = os.path.join(SOURCE_DIR, "export")
    os.makedirs(out_dir, exist_ok=True)
    results = []
    for asset_path, file_name in ((CHEST_MESH, "chest.obj"), (CHEST_TEXTURE, "chest_D.png")):
        asset = EAL.load_asset(asset_path)
        if asset is None:
            results.append(file_name + " 없음")
            continue
        task = unreal.AssetExportTask()
        task.object = asset
        task.filename = os.path.join(out_dir, file_name)
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        ok = unreal.Exporter.run_asset_export_task(task)
        results.append("%s %s" % (file_name, "OK" if ok else "실패"))
    return ", ".join(results)


def import_meshes():
    meshes = {}
    imported = set()
    order = [MATERIAL_SOURCE] + sorted(k for k in TARGET_SIZE_CM if k not in (MATERIAL_SOURCE, "BAG_009"))
    for key in order:
        folder = "%s/%s" % (MESH_ROOT, key)
        mesh = None if REIMPORT_MESHES else find_mesh(folder)
        if mesh is not None:
            meshes[key] = mesh
            continue
        glb = os.path.join(SOURCE_DIR, "SM_%s.glb" % key)
        if not os.path.isfile(glb):
            raise RuntimeError("[BagMesh] 원본 없음: " + glb)
        import_file(glb, folder)
        imported.add(key)
        mesh = find_mesh(folder)
        if mesh is None:
            raise RuntimeError("[BagMesh] 임포트 후 스태틱 메시를 못 찾음: " + folder)
        meshes[key] = mesh

    source_mesh = meshes[MATERIAL_SOURCE]
    shared = {}
    for slot in source_mesh.static_materials:
        shared[str(slot.material_slot_name).lower()] = slot.material_interface

    for key, mesh in meshes.items():
        if key == MATERIAL_SOURCE or key not in imported:
            continue
        own = set()
        for index, slot in enumerate(mesh.static_materials):
            name = str(slot.material_slot_name).lower()
            target = shared.get(name)
            if target is None:
                unreal.log_warning("[BagMesh] %s 슬롯 '%s' 에 맞는 공용 머티리얼이 없음" % (key, name))
                continue
            if slot.material_interface is not None:
                own.add(slot.material_interface.get_path_name())
            mesh.set_material(index, target)
        EAL.save_loaded_asset(mesh, False)
        for path in own:
            if path.startswith("%s/%s/" % (MESH_ROOT, key)):
                EAL.delete_asset(path.split(".")[0])
    return meshes


def import_icons():
    icons = {}
    for key in sorted(TARGET_SIZE_CM):
        png = os.path.join(SOURCE_DIR, "icons", "T_Icon_%s.png" % key)
        if not os.path.isfile(png):
            continue
        import_file(png, ICON_DIR)
        tex = EAL.load_asset("%s/T_Icon_%s.T_Icon_%s" % (ICON_DIR, key, key))
        if tex is None:
            continue
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("srgb", True)
        EAL.save_loaded_asset(tex, False)
        icons[key] = tex
    return icons


def update_table(meshes, icons, chest):
    table = unreal.load_asset(DT_PATH)
    original_csv = LIB.export_data_table_to_csv_string(table)
    rows = list(csv.reader(io.StringIO(original_csv)))
    header = rows[0]
    for col in ("WorldMeshPath", "WorldScaleMultiplier", "Icon"):
        if col not in header:
            raise RuntimeError("[BagMesh] CSV 헤더에 %s 없음" % col)
    mesh_col = header.index("WorldMeshPath")
    scale_col = header.index("WorldScaleMultiplier")
    icon_col = header.index("Icon")
    touched = {mesh_col, scale_col, icon_col}

    plan = {}
    for key, mesh in meshes.items():
        plan[key] = mesh
    if chest is not None:
        plan["BAG_009"] = chest

    report = []
    for r in rows[1:]:
        if not r or r[0] not in TARGET_SIZE_CM:
            continue
        key = r[0]
        mesh = plan.get(key)
        if mesh is not None:
            size, longest = mesh_extent(mesh)
            scale = round(TARGET_SIZE_CM[key] / longest, 3) if longest > 0 else 1.0
            r[mesh_col] = soft_cell(mesh, "Engine.StaticMesh")
            r[scale_col] = "%.6f" % scale
            report.append("%s 크기 %.0fx%.0fx%.0f cm -> 배율 %.3f" % (key, size.x, size.y, size.z, scale))
        if key in icons:
            r[icon_col] = soft_cell(icons[key], "Engine.Texture2D")

    buf = io.StringIO()
    csv.writer(buf, quoting=csv.QUOTE_ALL, lineterminator="\n").writerows(rows)
    LIB.fill_data_table_from_csv_string(table, buf.getvalue())

    after = list(csv.reader(io.StringIO(LIB.export_data_table_to_csv_string(table))))
    before = list(csv.reader(io.StringIO(original_csv)))
    strip = lambda rs: [[plain(c) for i, c in enumerate(x) if i not in touched] for x in rs]
    if len(after) != len(before) or strip(after) != strip(before):
        unreal.log_error("[BagMesh] DT_Item 에서 다른 값이 바뀌어 저장하지 않았습니다. DT_Item 을 저장하지 말고 에디터를 다시 켜 주세요.")
        try:
            unreal.EditorLoadingAndSavingUtils.reload_packages([table.get_outermost()])
        except Exception:
            LIB.fill_data_table_from_csv_string(table, original_csv)
        return report, False
    ok = EAL.save_loaded_asset(table, False)
    return report, ok


meshes = import_meshes()
for key, mesh in sorted(meshes.items()):
    unreal.log_warning("[BagMesh] %s %s / %s" % (key, mesh.get_path_name(), ensure_collision(mesh)))
    EAL.save_loaded_asset(mesh, False)

chest = EAL.load_asset(CHEST_MESH)
if chest is None:
    unreal.log_warning("[BagMesh] 상자 메시를 못 찾음: " + CHEST_MESH)
else:
    unreal.log_warning("[BagMesh] BAG_009 상자 / %s" % ensure_collision(chest, edit=False))
    unreal.log_warning("[BagMesh] 상자 아이콘용 내보내기: " + export_chest_for_icon())

icons = import_icons()
unreal.log_warning("[BagMesh] 아이콘 %d개 임포트" % len(icons))

report, saved = update_table(meshes, icons, chest)
for line in report:
    unreal.log_warning("[BagMesh] " + line)
if saved:
    unreal.log_warning("[BagMesh] DT_Item 저장 완료")
else:
    unreal.log_error("[BagMesh] DT_Item 저장 실패")
