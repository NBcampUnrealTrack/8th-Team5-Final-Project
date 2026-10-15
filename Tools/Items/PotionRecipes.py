# -*- coding: utf-8 -*-
"""에디터 Output Log(Cmd)에서 실행:
py "<프로젝트 경로>/Tools/Items/PotionRecipes.py"
"""
import csv
import io
import math
import re

import unreal

LIB = unreal.DataTableFunctionLibrary
EAL = unreal.EditorAssetLibrary

RECIPE_DT = "/Game/Craft/DT_CraftRecipe.DT_CraftRecipe"
ITEM_DT = "/Game/Items/DataTables/DT_Item.DT_Item"

TEMPLATE_RECIPE = "Po_001"
WATER = "ITM_019"
POWDER = "ITM_020"

POTION_RECIPES = [
    ("Po_002", "POT_002", 3, [(WATER, 4), (POWDER, 3)]),
    ("Po_003", "POT_003", 2, [(WATER, 5), (POWDER, 3)]),
    ("Po_004", "POT_004", 5, [(POWDER, 3), (WATER, 2)]),
    ("Po_005", "POT_005", 3, [(POWDER, 4), (WATER, 3)]),
    ("Po_006", "POT_006", 2, [(POWDER, 5), (WATER, 3)]),
    ("Po_007", "POT_007", 2, [(POWDER, 3), (WATER, 1)]),
    ("Po_008", "POT_008", 3, [(WATER, 2), (POWDER, 2)]),
    ("Po_009", "POT_009", 2, [(WATER, 3), (POWDER, 3)]),
    ("Po_010", "POT_010", 1, [(WATER, 3), (POWDER, 3)]),
]

POTION_ORDER = [
    ("ITM_001", "Po_001"),
    ("POT_002", "Po_002"),
    ("POT_003", "Po_003"),
    ("POT_004", "Po_004"),
    ("POT_005", "Po_005"),
    ("POT_006", "Po_006"),
    ("POT_008", "Po_008"),
    ("POT_009", "Po_009"),
    ("POT_010", "Po_010"),
    ("POT_007", "Po_007"),
]

GOLD_RATIO = 0.8
GOLD_MARGIN = 10

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
    out = []
    for row_name, count in pairs:
        elem = with_row_name(parts[0], row_name)
        elem = re.sub(r"Count=\d+", "Count=%d" % count, elem)
        out.append(elem)
    return "(" + ",".join(out) + ")"


def ingredient_pairs(cell):
    return [(row_name_of(p), int(re.search(r"Count=(\d+)", p).group(1))) for p in INGREDIENT.findall(cell or "")]


def to_int(value):
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return 0


def ceil10(value):
    return int(math.ceil(value / 10.0) * 10)


def grouped(rows, order):
    wanted = set(order)
    present = [name for name in order if any(r[0] == name for r in rows)]
    first = next((i for i, r in enumerate(rows) if r[0] in wanted), len(rows))
    by_name = {r[0]: r for r in rows}
    before = [r for r in rows[:first] if r[0] not in wanted]
    after = [r for r in rows[first:] if r[0] not in wanted]
    return before + [by_name[name] for name in present] + after


def reorder_items():
    table = unreal.load_asset(ITEM_DT)
    header, rows = export_rows(table)
    new_rows = grouped(rows, [item for item, _ in POTION_ORDER])
    if [r[0] for r in new_rows] == [r[0] for r in rows]:
        return table, False

    by_name = {r[0]: r for r in rows}
    original = LIB.export_data_table_to_csv_string(table)
    LIB.fill_data_table_from_csv_string(table, to_csv(header, new_rows))
    after_header, after_rows = export_rows(table)
    problem = None
    if [r[0] for r in after_rows] != [r[0] for r in new_rows]:
        problem = "DT_Item 순서가 예상과 다름"
    else:
        for r in after_rows:
            if r != by_name[r[0]]:
                problem = "DT_Item %s 내용이 바뀜" % r[0]
                break
    if problem:
        LIB.fill_data_table_from_csv_string(table, original)
        raise RuntimeError(problem)
    return table, True


def load_items():
    header, rows = export_rows(unreal.load_asset(ITEM_DT))
    name_col = header.index("ItemName")
    sell_col = header.index("SellValue")
    heal_col = header.index("HealAmount") if "HealAmount" in header else None
    effect_col = header.index("UseEffect") if "UseEffect" in header else None
    if heal_col is not None and effect_col is not None:
        for r in rows:
            numbers = [int(n) for n in re.findall(r"\d+", r[effect_col])]
            heal = to_int(r[heal_col])
            if r[0].startswith("POT_") and heal > 0 and numbers and heal not in numbers:
                unreal.log_warning("[PotionRecipes] %s UseEffect '%s' 와 HealAmount %d 가 다름" % (r[0], r[effect_col], heal))
    return {r[0]: {"name": r[name_col], "sell": to_int(r[sell_col])} for r in rows}


def gold_for(items, result_row, count, pairs):
    output_sell = items[result_row]["sell"] * count
    material_sell = sum(items[row]["sell"] * n for row, n in pairs)
    return max(ceil10(output_sell * GOLD_RATIO), ceil10(output_sell - material_sell + GOLD_MARGIN), GOLD_MARGIN)


def update_recipes(items):
    for _, result_row, _, pairs in POTION_RECIPES:
        for row in [result_row] + [p[0] for p in pairs]:
            if row not in items:
                raise RuntimeError("DT_Item 에 %s 가 없음" % row)

    table = unreal.load_asset(RECIPE_DT)
    header, rows = export_rows(table)
    col = {name: header.index(name) for name in ("RecipeName", "RecipeItem", "MaxUses", "Ingredients", "ResultItem", "ResultCount", "GoldCost")}
    template = next((r for r in rows if r[0] == TEMPLATE_RECIPE), None)
    if template is None:
        raise RuntimeError("템플릿 레시피 %s 가 없음" % TEMPLATE_RECIPE)

    new_rows = [list(r) for r in rows]
    index_of = {r[0]: i for i, r in enumerate(new_rows)}
    expected = {}
    report = []

    for recipe_row, result_row, count, pairs in POTION_RECIPES:
        row = list(template)
        row[0] = recipe_row
        row[col["RecipeName"]] = items[result_row]["name"]
        row[col["ResultItem"]] = with_row_name(template[col["ResultItem"]], result_row)
        row[col["ResultCount"]] = str(count)
        row[col["Ingredients"]] = build_ingredients(template[col["Ingredients"]], pairs)
        gold = gold_for(items, result_row, count, pairs)
        row[col["GoldCost"]] = str(gold)
        if recipe_row in index_of:
            new_rows[index_of[recipe_row]] = row
        else:
            index_of[recipe_row] = len(new_rows)
            new_rows.append(row)
        expected[recipe_row] = (result_row, str(count), str(gold), pairs)
        report.append("%s %s x%d  %s  %dG (개당 판매가 %dG)" % (
            recipe_row, items[result_row]["name"], count,
            ", ".join("%s x%d" % (items[r]["name"], n) for r, n in pairs), gold, items[result_row]["sell"]))

    new_rows = grouped(new_rows, [recipe for _, recipe in POTION_ORDER])
    untouched = {r[0]: list(r) for r in rows if r[0] not in expected}

    def check(after_header, after_rows):
        if [r[0] for r in after_rows] != [r[0] for r in new_rows]:
            return "레시피 행 목록이 예상과 다름"
        acol = {name: after_header.index(name) for name in col}
        for r in after_rows:
            if r[0] in expected:
                got = (row_name_of(r[acol["ResultItem"]]), r[acol["ResultCount"]], r[acol["GoldCost"]], ingredient_pairs(r[acol["Ingredients"]]))
                if got != expected[r[0]]:
                    return "레시피 %s 값이 예상과 다름: %s" % (r[0], got)
            elif r != untouched[r[0]]:
                return "레시피 %s 가 의도치 않게 바뀜" % r[0]
        return None

    original = LIB.export_data_table_to_csv_string(table)
    LIB.fill_data_table_from_csv_string(table, to_csv(header, new_rows))
    after_header, after_rows = export_rows(table)
    problem = check(after_header, after_rows)
    if problem:
        LIB.fill_data_table_from_csv_string(table, original)
        raise RuntimeError(problem)

    template_gold = to_int(template[col["GoldCost"]])
    template_pairs = ingredient_pairs(template[col["Ingredients"]])
    template_result = row_name_of(template[col["ResultItem"]])
    template_count = to_int(template[col["ResultCount"]])
    if template_result in items:
        output_sell = items[template_result]["sell"] * template_count
        material_sell = sum(items.get(r, {"sell": 0})["sell"] * n for r, n in template_pairs)
        if template_gold + material_sell <= output_sell:
            unreal.log_warning("[PotionRecipes] %s 는 되팔면 이득이 남음 (골드 %d + 재료 %d <= 판매 %d)" % (
                TEMPLATE_RECIPE, template_gold, material_sell, output_sell))

    return table, report


item_table, reordered = reorder_items()
items = load_items()
recipe_table, report = update_recipes(items)
for line in report:
    unreal.log_warning("[PotionRecipes] " + line)

saved = EAL.save_loaded_asset(recipe_table, False)
if reordered:
    saved = EAL.save_loaded_asset(item_table, False) and saved
    unreal.log_warning("[PotionRecipes] DT_Item 포션 %d종을 한곳으로 모음" % len(POTION_ORDER))

if saved:
    unreal.log_warning("[PotionRecipes] 포션 레시피 %d개 저장 완료" % len(report))
else:
    unreal.log_error("[PotionRecipes] 저장 실패 - DT_CraftRecipe / DT_Item 을 직접 저장해 주세요")
