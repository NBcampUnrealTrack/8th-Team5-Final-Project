# -*- coding: utf-8 -*-
"""에디터 Output Log(Cmd)에서 실행:
py "<프로젝트 경로>/Tools/Items/ItemDescriptions.py"
"""
import unreal

DT_PATH = "/Game/Items/DataTables/DT_Item.DT_Item"
OVERWRITE = False

RENAMES = {
    "BAG_003": "가죽 두루마리 꾸러미",
    "BAG_004": "사냥꾼의 천 꾸러미",
}

SELL = " 상점에서 골드로 바꿀 수 있다."

DESCRIPTIONS = {
    "골드": "지상과 천계 어디서든 통용되는 금화. 상점 거래와 제작 비용으로 쓰인다.",

    "깨진 후광 장식": "천계 조각상의 머리 위를 밝히던 후광의 일부. 빛은 꺼졌지만 금속의 값어치는 남아 있다." + SELL,
    "검게 그을린 금빛 장식 파편": "신전 외벽을 두르던 금빛 장식이 붕괴의 불길에 그을린 채 떨어져 나왔다. 그을음만 닦아내면 쓸 만한 금속이다." + SELL,
    "검게 물든 성역의 인장": "성역을 지키던 인장이 붕괴 과정에서 검게 변질되었다. 불길한 기운에도 수집가들은 높은 값을 쳐준다." + SELL,
    "얼룩진 천계 휘장": "잔존 병사들이 지니고 다니던 휘장. 오래된 얼룩 아래로 천계 군단의 문양이 희미하게 보인다." + SELL,
    "금 간 성유리 조각": "천계 건축물의 창을 이루던 유리 조각. 금이 갔지만 빛을 받으면 여전히 희미하게 발광한다." + SELL,
    "부서진 기도패": "제단 앞에서 기도를 올릴 때 쓰던 작은 의식 도구. 새겨진 기도문은 절반쯤 지워졌다." + SELL,
    "저주받은 의식 장식": "천계의 의식에 쓰이던 정교한 장식품. 오래 쥐고 있으면 손끝이 서늘해진다." + SELL,
    "봉인된 천계 문서 조각": "해독할 수 없는 천계 문자가 빼곡히 적힌 기록물의 일부. 학자들이 앞다투어 사들인다." + SELL,
    "녹슨 증기 늑대의 톱니": "증기로 움직이던 늑대형 구조체의 관절에서 빠진 톱니. 녹이 슬어 더는 맞물리지 않는다." + SELL,
    "마모된 석재 갑각 조각": "석재 구조체의 몸을 덮던 갑각 조각. 오랜 세월 깎여 나가 모서리가 둥글다." + SELL,
    "뒤틀린 발굴자의 장갑 파편": "천계 유적을 파헤치던 발굴자가 남긴 장갑의 일부. 무언가에 짓눌린 듯 기괴하게 뒤틀려 있다." + SELL,

    "끊어진 천계 활시위": "잔존 사수의 활에서 끊어져 나온 시위. 천계의 섬유로 꼬아 만들어 끊어진 뒤에도 탄력이 남아 있다. 장비 제작의 기초 재료.",
    "파손된 병장기 조각": "잔존 병사가 쓰던 무기와 갑주의 파편. 녹여서 다시 벼리면 새 장비가 된다. 장비 제작의 기초 재료.",
    "부식된 구조체 외장 파편": "천계 구조체의 몸체를 덮던 외장이 부식된 채 떨어져 나왔다. 여전히 단단해 방어구와 방패 제작에 쓰인다.",
    "잠식된 구조체 마력핵 조각": "구조체를 움직이던 마력핵의 조각. 변질된 기운에 잠식되었지만 아직 마력이 맥동한다. 희귀 등급 이상 장비의 핵심 재료.",
    "잔존 사수의 조준석": "잔존 사수의 눈 역할을 하던 작은 결정. 들여다보면 먼 곳이 또렷해진다. 원거리 장비의 핵심 재료.",
    "잔존 병사의 전투 인장": "잔존 병사가 가슴에 달고 다니던 전투 인장. 수많은 전투의 기억이 새겨져 있다. 근접 장비의 핵심 재료.",
    "천계의 잔광 결정": "천계의 빛이 응축되어 굳은 결정. 강한 개체에게서만 드물게 얻을 수 있다. 영웅 등급 이상 장비 제작에 쓰인다.",
    "타락한 성역의 핵": "성역의 중심에서 빛나던 핵이 타락해 검붉게 물들었다. 천상 등급 장비에만 쓰이는 최상위 재료.",
    "탁해진 천계수": "천계의 분수와 수로에서 길어 온 물. 붕괴 이후 탁해졌지만 정제하면 포션의 원료가 된다. 연금 재료.",
    "빛바랜 성광 가루": "신전 벽면과 제단에서 떨어진 빛의 가루. 빛은 바랬지만 연금술로 다시 깨울 수 있다. 연금 재료.",
    "터진 압력관 조각": "증기 구조체의 몸속을 흐르던 압력관이 터진 잔해. 열에 강해 제작 재료로 쓰인다.",
    "균열난 골렘 핵편": "골렘의 핵이 깨지며 떨어져 나온 파편. 균열 사이로 미약한 마력이 새어 나온다. 제작 재료.",
    "오염된 발굴 도구 조각": "발굴자들이 쓰던 도구의 금속 부분. 정체 모를 기운에 오염되었지만 제련하면 쓸 만하다. 제작 재료.",
    "수호자의 깨진 문장": "유적을 지키던 수호자가 지니던 문장. 깨졌지만 새겨진 수호의 힘이 일부 남아 있다. 제작 재료.",

    "하급 회복 포션": "천계수에 성광 가루를 녹여 만든 기본 회복 물약. 가벼운 상처를 빠르게 아물게 한다.",
    "중급 회복 포션": "정제된 천계수를 진하게 달인 회복 물약. 깊은 상처도 금세 메워 준다.",
    "상급 회복 포션": "순도 높은 천계수로 빚은 최상급 회복 물약. 한 모금에 온몸으로 따뜻한 빛이 퍼진다.",
    "하급 마나 포션": "옅은 푸른빛을 띠는 마나 물약. 바닥난 마력을 조금 채워 준다.",
    "중급 마나 포션": "성광 가루를 듬뿍 녹여 빛이 짙은 마나 물약. 지친 정신을 맑게 깨운다.",
    "상급 마나 포션": "천계의 마력이 그대로 녹아든 마나 물약. 마시는 순간 마력이 넘쳐흐른다.",
    "조화의 하급 포션": "체력과 마나를 함께 채워 주는 균형의 물약. 둘 중 하나만 지쳐 있어도 마실 수 있다.",
    "조화의 중급 포션": "붉은 빛과 푸른 빛이 섞여 소용돌이치는 물약. 몸과 정신을 동시에 회복시킨다.",
    "조화의 상급 포션": "두 빛이 완벽히 어우러진 최상급 조화의 물약. 전투 한가운데서도 다시 일어설 힘을 준다.",
    "연착의 포션": "마시면 몸이 깃털처럼 가벼워지는 금빛 물약. 일정 시간 동안 높은 곳에서 떨어질 때 받는 피해가 줄어든다.",
    "뒤틀린 천계의 재각인석": "백색 결정 속에서 금빛 문양이 쉼 없이 꿈틀거린다. 장비에 새겨진 각인을 지우고 새로 새길 수 있다.",
    "화살": "활에 메겨 쏘는 기본 화살.",

    "일반 천계 봉인 열쇠": "천계의 봉인된 문을 여는 열쇠. 일반 등급 봉인에 맞는다.",
    "희귀 천계 봉인 열쇠": "푸른 문양이 새겨진 봉인 열쇠. 희귀 등급 봉인에 맞는다.",
    "영웅 천계 봉인 열쇠": "보랏빛 후광이 감도는 봉인 열쇠. 영웅 등급 봉인에 맞는다.",
    "천상 천계 봉인 열쇠": "날개 장식이 달린 금빛 봉인 열쇠. 천상 등급 봉인에 맞는다.",

    "횃불": "어두운 천계 유적을 밝혀 주는 횃불. 보조 장비 칸에 장착한다.",

    "허리 주머니": "허리에 차는 작은 주머니. 자잘한 전리품 몇 개를 넣어 둘 수 있다.",
    "낡은 자루": "여기저기 기운 자국이 있는 자루. 볼품없지만 제법 많이 들어간다.",
    "가죽 두루마리 꾸러미": "두툼한 가죽을 둘둘 말아 끈으로 묶은 꾸러미. 말린 틈 사이에 자잘한 물건을 끼워 넣을 수 있다.",
    "사냥꾼의 천 꾸러미": "사냥꾼들이 사냥감과 도구를 감싸 들고 다니던 두꺼운 천 꾸러미. 펼치면 제법 많은 것을 담을 수 있다.",
    "여행자 배낭": "지상의 여행자들이 메고 다니던 배낭. 넉넉한 수납공간을 갖췄다.",
    "탐험가 배낭": "천계 원정을 위해 보강한 배낭. 무거운 전리품도 거뜬히 버틴다.",
    "순례자의 등짐": "천계로 향하던 순례자들이 지고 다닌 등짐. 낡은 기도문이 수놓여 있다.",
    "원정대 배낭": "대규모 원정대를 위해 만든 대형 배낭. 웬만한 전리품은 모두 담을 수 있다.",
    "천계 유물 상자": "천계의 유물을 보관하던 상자. 겉보기보다 훨씬 많은 것을 품는다.",
}


import csv
import io
import re

LIB = unreal.DataTableFunctionLibrary
TEXT_MACRO = re.compile(r'^\s*(?:NSLOCTEXT|LOCTEXT|INVTEXT)\s*\((.*)\)\s*$', re.S)
QUOTED = re.compile(r'"((?:[^"\\]|\\.)*)"')


def plain(cell):
    m = TEXT_MACRO.match(cell or "")
    if not m:
        return (cell or "").strip()
    parts = QUOTED.findall(m.group(1))
    return parts[-1].strip() if parts else ""


def parse(csv_text):
    return list(csv.reader(io.StringIO(csv_text)))


def normalized(rows, skip_cols):
    return [[plain(c) for i, c in enumerate(r) if i not in skip_cols] for r in rows]


def reload_from_disk(table):
    try:
        pkg = table.get_outermost()
        unreal.EditorLoadingAndSavingUtils.reload_packages([pkg])
        return True
    except Exception as e:
        unreal.log_warning("[ItemDesc] 디스크에서 다시 불러오기 실패: %s" % e)
        return False


table = unreal.load_asset(DT_PATH)
if table is None:
    raise RuntimeError("DT_Item 을 찾지 못했습니다: " + DT_PATH)

original_csv = LIB.export_data_table_to_csv_string(table)
rows = parse(original_csv)
header = rows[0]
if "ItemName" not in header or "Description" not in header:
    raise RuntimeError("[ItemDesc] CSV 헤더에서 ItemName/Description 을 못 찾음: " + ", ".join(header[:12]))
name_col = header.index("ItemName")
desc_col = header.index("Description")

changed, skipped, seen, renamed = [], [], set(), []
for r in rows[1:]:
    if len(r) <= max(name_col, desc_col):
        continue
    new_name = RENAMES.get(r[0])
    if new_name and plain(r[name_col]) != new_name:
        renamed.append("%s -> %s" % (plain(r[name_col]), new_name))
        r[name_col] = new_name
    name = plain(r[name_col])
    if name not in DESCRIPTIONS:
        continue
    seen.add(name)
    current = plain(r[desc_col])
    if current == DESCRIPTIONS[name]:
        continue
    if current and not OVERWRITE and r[0] not in RENAMES:
        skipped.append(name)
        continue
    r[desc_col] = DESCRIPTIONS[name]
    changed.append(name)

missing = [n for n in DESCRIPTIONS if n not in seen]

if not changed and not renamed:
    unreal.log_warning("[ItemDesc] 바꿀 행이 없습니다.")
else:
    buf = io.StringIO()
    csv.writer(buf, quoting=csv.QUOTE_ALL, lineterminator="\n").writerows(rows)
    LIB.fill_data_table_from_csv_string(table, buf.getvalue())

    after = parse(LIB.export_data_table_to_csv_string(table))
    skip_cols = {desc_col, name_col}
    before_norm = normalized(parse(original_csv), skip_cols)
    after_norm = normalized(after, skip_cols)
    desc_ok = all(
        plain(a[desc_col]) == DESCRIPTIONS[plain(a[name_col])]
        for a in after[1:]
        if len(a) > max(name_col, desc_col) and plain(a[name_col]) in changed
    )

    if before_norm != after_norm or not desc_ok:
        diffs = []
        for b, a in zip(before_norm, after_norm):
            if b != a:
                diffs.append(b[0] if b else "?")
        unreal.log_error("[ItemDesc] 설명 외 값이 달라졌거나 설명이 안 들어갔습니다. 저장하지 않았습니다. 다른 행: " + ", ".join(diffs[:20]))
        if not reload_from_disk(table):
            LIB.fill_data_table_from_csv_string(table, original_csv)
            unreal.log_error("[ItemDesc] DT_Item 을 저장하지 말고 에디터를 저장 없이 재시작해 주세요.")
    else:
        if unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False):
            unreal.log_warning("[ItemDesc] 설명 %d개 입력 후 저장했습니다." % len(changed))
            if renamed:
                unreal.log_warning("[ItemDesc] 이름 변경: " + ", ".join(renamed))
        else:
            unreal.log_error("[ItemDesc] 설명 %d개를 넣었지만 저장에 실패했습니다. DT_Item 을 직접 저장해 주세요." % len(changed))

if skipped:
    unreal.log_warning("[ItemDesc] 이미 설명이 있어 건너뜀: " + ", ".join(skipped))
if missing:
    unreal.log_warning("[ItemDesc] DT_Item 에서 이름을 못 찾음: " + ", ".join(missing))
