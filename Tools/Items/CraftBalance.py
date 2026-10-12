# -*- coding: utf-8 -*-
"""에디터 Output Log(Cmd)에서 실행:
py "<프로젝트 경로>/Tools/Items/CraftBalance.py"
"""
import csv
import io
import re

import unreal

LIB = unreal.DataTableFunctionLibrary
EAL = unreal.EditorAssetLibrary

RECIPE_DT = "/Game/Craft/DT_CraftRecipe.DT_CraftRecipe"
SHOP_DT = "/Game/Shop/DT_ShopList.DT_ShopList"
ITEM_DT = "/Game/Items/DataTables/DT_Item.DT_Item"
WEAPON_DT = "/Game/Items/DataTables/DT_Weapon.DT_Weapon"
SHIELD_DT = "/Game/Items/DataTables/DT_Shield.DT_Shield"

REMOVE_RECIPES = ["No_002", "Ra_002", "Ep_002", "He_001", "He_002", "He_003", "He_004", "He_005", "He_006"]

RECIPE_EDITS = {
    "Ep_001": {"ResultItem": ("Ra_001", "WPN_003"), "ResultCount": "1", "GoldCost": "400",
               "Ingredients": [("ITM_012", 9), ("ITM_013", 4), ("ITM_016", 4), ("ITM_017", 2)]},
    "Ep_006": {"Ingredients": [("ITM_013", 7), ("ITM_012", 4), ("ITM_014", 2), ("ITM_016", 2), ("ITM_017", 2)]},
    "No_006": {"RecipeItem": ("No_008", "BPH_001")},
    "No_007": {"RecipeItem": ("No_008", "BPC_001"), "ResultItem": ("No_006", "CHEST_001")},
    "He_007": {"Ingredients": [("ITM_013", 22), ("ITM_014", 5), ("ITM_016", 3), ("ITM_017", 5), ("ITM_018", 3)]},
    "He_008": {"Ingredients": [("ITM_013", 16), ("ITM_014", 3), ("ITM_016", 3), ("ITM_017", 4), ("ITM_018", 2)]},
    "Po_001": {"GoldCost": "80"},
}

ITEM_EDITS = {
    "HELM_003": {"SellValue": "180"},
    "BPH_004": {"MaxStack": "1"},
    "BPC_004": {"MaxStack": "1"},
    "BPL_004": {"MaxStack": "1"},
    "BPS_004": {"MaxStack": "1"},
    "BPSW_004": {"MaxStack": "1"},
    "BPST_004": {"MaxStack": "1"},
    "BPB_004": {"MaxStack": "1"},
    "BPD_004": {"MaxStack": "1"},
}
DISABLE_DROP = ["BPD_001", "BPD_002", "BPD_003", "BPD_004", "BPSW_004", "BPST_004", "BPB_004", "BPS_004", "BPH_004"]

SHOP_REMOVE_ITEMS = ["ITM_014", "ITM_015", "ITM_016", "ITM_017", "ITM_018"]
SHOP_ADD = [("SHOP_BPST_001", "BPST_001"), ("SHOP_BPS_001", "BPS_001"), ("SHOP_TOR_001", "TOR_001"), ("SHOP_POT_004", "POT_004")]
SHOP_TEMPLATE_ITEM = "BPH_001"

ROWNAME = re.compile(r'RowName\s*=\s*"?([^",)]*)"?')
INGREDIENT = re.compile(r'\(ItemRow=\(.*?\),Count=\d+\)')


def export_rows(table):
    text = LIB.export_data_table_to_csv_string(table)
    rows = list(csv.reader(io.StringIO(text)))
    return rows[0], rows[1:]


def to_csv(header, rows):
    buf = io.StringIO()
    csv.writer(buf, quoting=csv.QUOTE_ALL, lineterminator="\n").writerows([header] + rows)
    return buf.getvalue()


def row_name_of(cell):
    m = ROWNAME.search(cell or "")
    return m.group(1) if m else ""


def with_row_name(cell, new_name):
    if not ROWNAME.search(cell):
        raise RuntimeError("RowName 을 찾지 못함: " + cell)
    return ROWNAME.sub('RowName="%s"' % new_name, cell, count=1)


def build_ingredients(template_cell, pairs):
    parts = INGREDIENT.findall(template_cell)
    if not parts:
        raise RuntimeError("재료 칸 형식을 읽지 못함: " + template_cell[:200])
    template = parts[0]
    out = []
    for row_name, count in pairs:
        elem = with_row_name(template, row_name)
        elem = re.sub(r"Count=\d+", "Count=%d" % count, elem)
        out.append(elem)
    return "(" + ",".join(out) + ")"


def ingredient_pairs(cell):
    return [(row_name_of(p), int(re.search(r"Count=(\d+)", p).group(1))) for p in INGREDIENT.findall(cell or "")]


def item_names():
    names = {}
    for path, col in ((ITEM_DT, "ItemName"), (WEAPON_DT, "WeaponName"), (SHIELD_DT, "ShieldName")):
        header, rows = export_rows(unreal.load_asset(path))
        idx = header.index(col)
        for r in rows:
            names[r[0]] = r[idx]
    return names


def apply(table, new_header, new_rows, check):
    original = LIB.export_data_table_to_csv_string(table)
    LIB.fill_data_table_from_csv_string(table, to_csv(new_header, new_rows))
    after_header, after_rows = export_rows(table)
    problem = check(after_header, after_rows)
    if problem:
        try:
            unreal.EditorLoadingAndSavingUtils.reload_packages([table.get_outermost()])
        except Exception:
            LIB.fill_data_table_from_csv_string(table, original)
        raise RuntimeError(problem)


def update_recipes(names):
    table = unreal.load_asset(RECIPE_DT)
    header, rows = export_rows(table)
    col = {name: header.index(name) for name in ("RecipeName", "RecipeItem", "Ingredients", "ResultItem", "ResultCount", "GoldCost")}
    by_name = {r[0]: r for r in rows}
    original = {r[0]: list(r) for r in rows}

    kept = [list(r) for r in rows if r[0] not in REMOVE_RECIPES]
    expected = {}
    for r in kept:
        edit = RECIPE_EDITS.get(r[0], {})
        for field, value in edit.items():
            if field in ("RecipeItem", "ResultItem"):
                template_row, new_name = value
                r[col[field]] = with_row_name(by_name[template_row][col[field]], new_name)
            elif field == "Ingredients":
                r[col[field]] = build_ingredients(r[col[field]], value)
            else:
                r[col[field]] = value
        result_name = names.get(row_name_of(r[col["ResultItem"]]))
        if result_name:
            r[col["RecipeName"]] = result_name
        expected[r[0]] = {
            "RecipeName": r[col["RecipeName"]],
            "RecipeItem": row_name_of(r[col["RecipeItem"]]),
            "ResultItem": row_name_of(r[col["ResultItem"]]),
            "ResultCount": r[col["ResultCount"]],
            "GoldCost": r[col["GoldCost"]],
            "Ingredients": ingredient_pairs(r[col["Ingredients"]]),
        }

    def check(after_header, after_rows):
        if [r[0] for r in after_rows] != [r[0] for r in kept]:
            return "레시피 행 목록이 예상과 다름"
        acol = {name: after_header.index(name) for name in col}
        for r in after_rows:
            got = {
                "RecipeName": r[acol["RecipeName"]],
                "RecipeItem": row_name_of(r[acol["RecipeItem"]]),
                "ResultItem": row_name_of(r[acol["ResultItem"]]),
                "ResultCount": r[acol["ResultCount"]],
                "GoldCost": r[acol["GoldCost"]],
                "Ingredients": ingredient_pairs(r[acol["Ingredients"]]),
            }
            if got != expected[r[0]]:
                return "레시피 %s 값이 예상과 다름: %s" % (r[0], got)
            if r[0] not in RECIPE_EDITS:
                untouched = [c for i, c in enumerate(r) if i != acol["RecipeName"]]
                before = [c for i, c in enumerate(original[r[0]]) if i != col["RecipeName"]]
                if untouched != before:
                    return "레시피 %s 의 다른 칸이 바뀜" % r[0]
        return None

    apply(table, header, kept, check)
    return table, len(rows) - len(kept)


def update_items():
    table = unreal.load_asset(ITEM_DT)
    header, rows = export_rows(table)
    drop_col = header.index("bDroppable")
    new_rows = [list(r) for r in rows]
    touched = {}
    for r in new_rows:
        for field, value in ITEM_EDITS.get(r[0], {}).items():
            r[header.index(field)] = value
            touched[(r[0], field)] = value
        if r[0] in DISABLE_DROP:
            r[drop_col] = "False"
            touched[(r[0], "bDroppable")] = "False"

    def check(after_header, after_rows):
        if [r[0] for r in after_rows] != [r[0] for r in rows]:
            return "DT_Item 행 목록이 바뀜"
        for before, after in zip(rows, after_rows):
            for i, (b, a) in enumerate(zip(before, after)):
                key = (before[0], header[i])
                if key in touched:
                    if a.strip().lower() != touched[key].lower():
                        return "DT_Item %s.%s 가 %s 로 들어감" % (key[0], key[1], a)
                elif b != a:
                    return "DT_Item %s.%s 가 의도치 않게 바뀜" % (before[0], header[i])
        return None

    apply(table, header, new_rows, check)
    return table, len(touched)


def update_shop():
    table = unreal.load_asset(SHOP_DT)
    header, rows = export_rows(table)
    item_col = header.index("ItemRow")
    template = next(r for r in rows if row_name_of(r[item_col]) == SHOP_TEMPLATE_ITEM)
    kept = [list(r) for r in rows if row_name_of(r[item_col]) not in SHOP_REMOVE_ITEMS]
    existing = {row_name_of(r[item_col]) for r in kept}
    for row_name, item in SHOP_ADD:
        if item in existing:
            continue
        new_row = list(template)
        new_row[0] = row_name
        new_row[item_col] = with_row_name(template[item_col], item)
        kept.append(new_row)

    def check(after_header, after_rows):
        got = [(r[0], row_name_of(r[item_col])) for r in after_rows]
        want = [(r[0], row_name_of(r[item_col])) for r in kept]
        return None if got == want else "상점 목록이 예상과 다름: %s" % got

    apply(table, header, kept, check)
    return table, len(rows), len(kept)


names = item_names()
recipe_table, removed = update_recipes(names)
item_table, item_changes = update_items()
shop_table, shop_before, shop_after = update_shop()

saved = all(EAL.save_loaded_asset(t, False) for t in (recipe_table, item_table, shop_table))
unreal.log_warning("[CraftBalance] 레시피 %d개 삭제, %d개 수정" % (removed, len(RECIPE_EDITS)))
unreal.log_warning("[CraftBalance] DT_Item 값 %d개 변경" % item_changes)
unreal.log_warning("[CraftBalance] 상점 목록 %d -> %d" % (shop_before, shop_after))
if saved:
    unreal.log_warning("[CraftBalance] 저장 완료")
else:
    unreal.log_error("[CraftBalance] 저장 실패 - DT_CraftRecipe / DT_Item / DT_ShopList 를 직접 저장해 주세요")
