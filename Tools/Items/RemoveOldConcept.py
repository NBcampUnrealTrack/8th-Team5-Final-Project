# -*- coding: utf-8 -*-
"""에디터 Output Log(Cmd)에서 실행:
py "<프로젝트 경로>/Tools/Items/RemoveOldConcept.py"
"""
import csv
import io
import re

import unreal

LIB = unreal.DataTableFunctionLibrary
ITEM_DT = "/Game/Items/DataTables/DT_Item.DT_Item"
WEAPON_DT = "/Game/Items/DataTables/DT_Weapon.DT_Weapon"

SELL = " 상점에서 골드로 바꿀 수 있다."

ITEM_EDITS = {
    "ITM_001": {"Description": '맑은 샘물에 발광 가루를 녹여 만든 기본 회복 물약. 가벼운 상처를 빠르게 아물게 한다.'},
    "ITM_002": {"Description": '던전 안팎 어디서든 통용되는 금화. 상점 거래와 제작 비용으로 쓰인다.'},
    "ITM_003": {"ItemName": '깨진 왕관 장식', "Description": '석상의 머리에 씌워져 있던 왕관 장식의 일부. 광택은 바랬지만 금속의 값어치는 남아 있다.' + SELL},
    "ITM_005": {"ItemName": '검게 물든 신전의 인장', "Description": '신전 문을 지키던 인장이 저주에 물들어 검게 변했다. 불길한 기운에도 수집가들은 높은 값을 쳐준다.' + SELL},
    "ITM_006": {"ItemName": '얼룩진 군단 휘장', "Description": '잔존 병사들이 지니고 다니던 휘장. 오래된 얼룩 아래로 옛 군단의 문양이 희미하게 보인다.' + SELL},
    "ITM_007": {"ItemName": '금 간 색유리 조각', "Description": '무너진 신전의 창을 이루던 유리 조각. 금이 갔지만 빛을 받으면 여전히 은은하게 빛난다.' + SELL},
    "ITM_009": {"Description": '던전 깊은 제단에서 쓰이던 정교한 장식품. 오래 쥐고 있으면 손끝이 서늘해진다.' + SELL},
    "ITM_010": {"ItemName": '봉인된 고대 문서 조각', "Description": '해독할 수 없는 고대 문자가 빼곡히 적힌 기록물의 일부. 학자들이 앞다투어 사들인다.' + SELL},
    "ITM_011": {"ItemName": '끊어진 사수의 활시위', "Description": '잔존 사수의 활에서 끊어져 나온 시위. 질긴 섬유로 꼬아 만들어 끊어진 뒤에도 탄력이 남아 있다. 장비 제작의 기초 재료.'},
    "ITM_013": {"Description": '골렘형 구조체의 몸체를 덮던 외장이 부식된 채 떨어져 나왔다. 여전히 단단해 방어구와 방패 제작에 쓰인다.'},
    "ITM_017": {"ItemName": '심연의 잔광 결정', "Description": '던전 깊은 곳의 빛이 응축되어 굳은 결정. 강한 개체에게서만 드물게 얻을 수 있다. 영웅 등급 이상 장비 제작에 쓰인다.'},
    "ITM_018": {"ItemName": '타락한 심층의 핵', "Description": '던전 최심층에서 빛나던 핵이 타락해 검붉게 물들었다. 전설 등급 장비에만 쓰이는 최상위 재료.'},
    "ITM_019": {"ItemName": '탁해진 샘물', "Description": '던전 지하 수로에서 길어 온 물. 탁해졌지만 정제하면 포션의 원료가 된다. 연금 재료.'},
    "ITM_020": {"ItemName": '빛바랜 발광 가루'},
    "ITM_021": {"ItemName": '뒤틀린 재각인석'},
    "ITM_026": {"Description": '고대 유적을 파헤치던 발굴자가 남긴 장갑의 일부. 무언가에 짓눌린 듯 기괴하게 뒤틀려 있다.' + SELL},
    "RELIC_003": {"ItemName": '잊힌 영웅의 검편'},
    "RELIC_005": {"ItemName": '수호자의 심장'},
    "KEY_001": {"ItemName": '일반 봉인 열쇠', "Description": '던전의 봉인된 문을 여는 열쇠. 일반 등급 봉인에 맞는다.'},
    "KEY_002": {"ItemName": '희귀 봉인 열쇠'},
    "KEY_003": {"ItemName": '영웅 봉인 열쇠', "Description": '보랏빛 광채가 감도는 봉인 열쇠. 영웅 등급 봉인에 맞는다.'},
    "KEY_004": {"ItemName": '전설 봉인 열쇠', "Description": '날개 장식이 달린 금빛 봉인 열쇠. 전설 등급 봉인에 맞는다.'},
    "BPD_004": {"ItemName": '심연의 단검 설계도'},
    "POT_002": {"Description": '정제된 샘물을 진하게 달인 회복 물약. 깊은 상처도 금세 메워 준다.'},
    "POT_003": {"Description": '순도 높은 샘물로 빚은 최상급 회복 물약. 한 모금에 온몸으로 따뜻한 기운이 퍼진다.'},
    "POT_005": {"Description": '발광 가루를 듬뿍 녹여 빛이 짙은 마나 물약. 지친 정신을 맑게 깨운다.'},
    "POT_006": {"Description": '심연의 마력이 그대로 녹아든 마나 물약. 마시는 순간 마력이 넘쳐흐른다.'},
    "TOR_001": {"Description": '어두운 던전을 밝혀 주는 횃불. 보조 장비 칸에 장착한다.'},
    "BAG_005": {"Description": '먼 길을 떠나는 여행자들이 메고 다니던 배낭. 넉넉한 수납공간을 갖췄다.'},
    "BAG_006": {"Description": '깊은 원정을 위해 보강한 배낭. 무거운 전리품도 거뜬히 버틴다.'},
    "BAG_007": {"Description": '먼 길을 걷던 순례자들이 지고 다닌 등짐. 낡은 기도문이 수놓여 있다.'},
    "BAG_009": {"ItemName": '고대 유물 상자', "Description": '고대 유물을 보관하던 상자. 겉보기보다 훨씬 많은 것을 품는다.'},
}
NOTES_FROM, NOTES_TO = "천상 등급", "전설 등급"

WEAPON_EDITS = {
    "DAG_004": {"WeaponName": '심연의 단검'},
}

TEXT_MACRO = re.compile(r'^\s*(?:NSLOCTEXT|LOCTEXT|INVTEXT)\s*\((.*)\)\s*$', re.S)
QUOTED = re.compile(r'"((?:[^"\\]|\\.)*)"')


def plain(cell):
    m = TEXT_MACRO.match(cell or "")
    if not m:
        return (cell or "").strip()
    parts = QUOTED.findall(m.group(1))
    return parts[-1].strip() if parts else ""


def parse(text):
    return list(csv.reader(io.StringIO(text)))


def to_csv(rows):
    buf = io.StringIO()
    csv.writer(buf, quoting=csv.QUOTE_ALL, lineterminator="\n").writerows(rows)
    return buf.getvalue()


def apply_table(path, edits, notes_col_name=None):
    table = unreal.load_asset(path)
    if table is None:
        raise RuntimeError("테이블을 찾지 못함: " + path)
    original = LIB.export_data_table_to_csv_string(table)
    rows = parse(original)
    header = rows[0]
    expected = {}
    for r in rows[1:]:
        for field, value in edits.get(r[0], {}).items():
            col = header.index(field)
            if plain(r[col]) != value:
                r[col] = value
                expected[(r[0], col)] = value
        if notes_col_name and notes_col_name in header:
            col = header.index(notes_col_name)
            if NOTES_FROM in r[col]:
                r[col] = r[col].replace(NOTES_FROM, NOTES_TO)
                expected[(r[0], col)] = plain(r[col])
    missing = [k for k in edits if k not in {r[0] for r in rows[1:]}]
    if not expected:
        return table, 0, missing, False

    LIB.fill_data_table_from_csv_string(table, to_csv(rows))
    after = parse(LIB.export_data_table_to_csv_string(table))
    before = parse(original)
    problem = None
    if [r[0] for r in after] != [r[0] for r in before]:
        problem = "행 목록이 바뀜"
    else:
        for b, a in zip(before[1:], after[1:]):
            for i, (bc, ac) in enumerate(zip(b, a)):
                key = (b[0], i)
                if key in expected:
                    if plain(ac) != expected[key]:
                        problem = "%s.%s 가 '%s' 로 들어감" % (b[0], header[i], plain(ac))
                elif plain(bc) != plain(ac):
                    problem = "%s.%s 가 의도치 않게 바뀜" % (b[0], header[i])
                if problem:
                    break
            if problem:
                break
    if problem:
        try:
            unreal.EditorLoadingAndSavingUtils.reload_packages([table.get_outermost()])
        except Exception:
            LIB.fill_data_table_from_csv_string(table, original)
        raise RuntimeError("[RemoveOldConcept] " + problem + " - 저장하지 않았습니다.")
    return table, len(expected), missing, True


item_table, item_count, item_missing, item_dirty = apply_table(ITEM_DT, ITEM_EDITS, "Notes")
weapon_table, weapon_count, weapon_missing, weapon_dirty = apply_table(WEAPON_DT, WEAPON_EDITS)

saved = True
for table, dirty in ((item_table, item_dirty), (weapon_table, weapon_dirty)):
    if dirty:
        saved = unreal.EditorAssetLibrary.save_loaded_asset(table, False) and saved

unreal.log_warning("[RemoveOldConcept] DT_Item 값 %d개, DT_Weapon 값 %d개 변경" % (item_count, weapon_count))
if item_missing or weapon_missing:
    unreal.log_warning("[RemoveOldConcept] 행을 못 찾음: " + ", ".join(item_missing + weapon_missing))
if saved:
    unreal.log_warning("[RemoveOldConcept] 저장 완료")
else:
    unreal.log_error("[RemoveOldConcept] 저장 실패 - DT_Item / DT_Weapon 을 직접 저장해 주세요")
