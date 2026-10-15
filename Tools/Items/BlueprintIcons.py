# -*- coding: utf-8 -*-
"""에디터 Output Log(Cmd)에서 실행:
py "<프로젝트 경로>/Tools/Items/BlueprintIcons.py"

아이콘 PNG 폴더 기본값: <프로젝트>/Claude outputs/BlueprintIcons
"""
import csv
import io
import os

import unreal

LIB = unreal.DataTableFunctionLibrary
EAL = unreal.EditorAssetLibrary

ICON_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Claude outputs", "BlueprintIcons")
DEST = "/Game/Items/Icon/BP"
ITEM_DT = "/Game/Items/DataTables/DT_Item.DT_Item"

FOLDER_TO_ICON = {
    "01_Helmet": "BP_Helmet",
    "02_Armor": "BP_Armor",
    "03_Legs": "BP_Legs",
    "04_Sword": "BP_Sword",
    "05_Dagger": "BP_Dagger",
    "06_Bow": "BP_Bow",
    "07_Staff": "BP_Staff",
    "08_Shield": "BP_Shield",
}

NAME_TO_ICON = [
    (("방패",), "BP_Shield"),
    (("지팡이",), "BP_Staff"),
    (("단검", "비수"), "BP_Dagger"),
    (("활", "궁"), "BP_Bow"),
    (("모자", "투구"), "BP_Helmet"),
    (("바지", "각반"), "BP_Legs"),
    (("옷",), "BP_Armor"),
    (("검",), "BP_Sword"),
]


def import_icons():
    files = sorted(f for f in os.listdir(ICON_DIR) if f.lower().endswith(".png"))
    tasks = []
    for f in files:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(ICON_DIR, f)
        task.destination_path = DEST
        task.destination_name = os.path.splitext(f)[0]
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    textures = {}
    for f in files:
        name = os.path.splitext(f)[0]
        tex = unreal.load_asset("%s/%s.%s" % (DEST, name, name))
        if tex is None:
            raise RuntimeError("임포트 실패: " + f)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("srgb", True)
        EAL.save_loaded_asset(tex, False)
        textures[name] = tex
    return textures


def export_rows(table):
    rows = list(csv.reader(io.StringIO(LIB.export_data_table_to_csv_string(table))))
    return rows[0], rows[1:]


def to_csv(header, rows):
    buf = io.StringIO()
    csv.writer(buf, quoting=csv.QUOTE_ALL, lineterminator="\n").writerows([header] + rows)
    return buf.getvalue()


def icon_for(row, col):
    current = row[col["Icon"]]
    for folder, icon in FOLDER_TO_ICON.items():
        if "/BP/%s." % folder in current:
            return icon
    name = row[col["ItemName"]]
    for keys, icon in NAME_TO_ICON:
        if any(k in name for k in keys):
            return icon
    return None


def apply_icons(textures):
    table = unreal.load_asset(ITEM_DT)
    header, rows = export_rows(table)
    col = {name: header.index(name) for name in ("ItemName", "ItemType", "Icon")}

    new_rows = [list(r) for r in rows]
    expected = {}
    report, skipped = [], []
    for r in new_rows:
        if r[col["ItemType"]] != "Blueprint":
            continue
        icon = icon_for(r, col)
        if icon is None or icon not in textures:
            skipped.append("%s %s" % (r[0], r[col["ItemName"]]))
            continue
        r[col["Icon"]] = "/Script/Engine.Texture2D'%s/%s.%s'" % (DEST, icon, icon)
        expected[r[0]] = icon
        report.append("%s %s -> %s" % (r[0], r[col["ItemName"]], icon))

    original = LIB.export_data_table_to_csv_string(table)
    LIB.fill_data_table_from_csv_string(table, to_csv(header, new_rows))
    after_header, after_rows = export_rows(table)

    problem = None
    if [r[0] for r in after_rows] != [r[0] for r in rows]:
        problem = "DT_Item 행 목록이 바뀜"
    else:
        icon_col = after_header.index("Icon")
        before = {r[0]: r for r in rows}
        for r in after_rows:
            if r[0] in expected:
                if "%s/%s.%s" % (DEST, expected[r[0]], expected[r[0]]) not in r[icon_col]:
                    problem = "%s 아이콘이 안 바뀜: %s" % (r[0], r[icon_col])
                    break
                other = [v for i, v in enumerate(r) if i != icon_col]
                if other != [v for i, v in enumerate(before[r[0]]) if i != icon_col]:
                    problem = "%s 의 다른 칸이 바뀜" % r[0]
                    break
            elif r != before[r[0]]:
                problem = "%s 가 의도치 않게 바뀜" % r[0]
                break
    if problem:
        LIB.fill_data_table_from_csv_string(table, original)
        raise RuntimeError(problem)

    return table, report, skipped


if not os.path.isdir(ICON_DIR):
    raise RuntimeError("아이콘 폴더 없음: " + ICON_DIR)

textures = import_icons()
unreal.log_warning("[BlueprintIcons] 텍스처 %d개 임포트 -> %s" % (len(textures), DEST))

table, report, skipped = apply_icons(textures)
for line in report:
    unreal.log_warning("[BlueprintIcons] " + line)
for line in skipped:
    unreal.log_error("[BlueprintIcons] 종류를 못 찾음 - 직접 지정 필요: " + line)

if EAL.save_loaded_asset(table, False):
    unreal.log_warning("[BlueprintIcons] 설계도 %d개 아이콘 교체 저장 완료" % len(report))
else:
    unreal.log_error("[BlueprintIcons] 저장 실패 - DT_Item 을 직접 저장해 주세요")
