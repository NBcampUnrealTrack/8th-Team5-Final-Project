# -*- coding: utf-8 -*-
"""에디터 Output Log(Cmd)에서 실행:
py "<프로젝트 경로>/Tools/Items/AugmentIcons.py"

아이콘 PNG 폴더 기본값: <프로젝트>/Claude outputs/AugmentIcons
"""
import os

import unreal

EAL = unreal.EditorAssetLibrary

ICON_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Claude outputs", "AugmentIcons")
DEST = "/Game/Items/Icon/Augment"
COMMON_SET = "/Game/Combat/Augment/DA_CommonAugments.DA_CommonAugments"

AUGMENT_ICONS = {
    "Augment.Warrior.Smash.Efficiency": "Aug_Smash_Efficiency",
    "Augment.Warrior.Smash.Shield": "Aug_Smash_Shield",
    "Augment.Warrior.ChargeAttack.Efficiency": "Aug_ChargeAttack_Efficiency",
    "Augment.Warrior.ChargeAttack.Reach": "Aug_ChargeAttack_Reach",
    "Augment.Warrior.ChargeAttack.Guard": "Aug_ChargeAttack_Guard",
    "Augment.Warrior.BattleCry.Efficiency": "Aug_BattleCry_Efficiency",
    "Augment.Warrior.BattleCry.Duration": "Aug_BattleCry_Duration",
    "Augment.Warrior.BloodFrenzy.Efficiency": "Aug_BloodFrenzy_Efficiency",
    "Augment.Warrior.BloodFrenzy.Lifesteal": "Aug_BloodFrenzy_Lifesteal",
    "Augment.Warrior.BloodFrenzy.Duration": "Aug_BloodFrenzy_Duration",
    "Augment.Melee.Whirlwind.Wide": "Aug_Whirlwind_Wide",
    "Augment.Bow.TripleShot.Rapid": "Aug_TripleShot_Rapid",
    "Augment.Bow.PiercingShot.Rapid": "Aug_PiercingShot_Rapid",
    "Augment.Bow.EvasiveShot.Rapid": "Aug_EvasiveShot_Rapid",
    "Augment.Bow.EvasiveShot.Light": "Aug_EvasiveShot_Light",
    "Augment.Bow.HunterMark.Deep": "Aug_HunterMark_Deep",
    "Augment.Bow.HunterMark.Wide": "Aug_HunterMark_Wide",
    "Augment.Magic.ElementalOrb.Rapid": "Aug_ElementalOrb_Rapid",
    "Augment.Magic.ElementalOrb.Wide": "Aug_ElementalOrb_Wide",
    "Augment.Magic.FrostField.Bitter": "Aug_FrostField_Bitter",
    "Augment.Magic.FrostField.Wide": "Aug_FrostField_Wide",
    "Augment.Magic.ManaExplosion.Wide": "Aug_ManaExplosion_Wide",
    "Augment.Magic.Teleport.Far": "Aug_Teleport_Far",
    "Augment.Magic.Teleport.Rapid": "Aug_Teleport_Rapid",
}

SKILL_ICONS = {
    "TripleShot": "Skill_TripleShot",
}


def asset_path(name):
    return "%s/%s.%s" % (DEST, name, name)


def load_existing_icons():
    textures = {}
    for name in set(AUGMENT_ICONS.values()) | set(SKILL_ICONS.values()):
        if EAL.does_asset_exist(asset_path(name)):
            textures[name] = unreal.load_asset(asset_path(name))
    return textures


def import_icons(skip):
    if not os.path.isdir(ICON_DIR):
        return {}
    files = sorted(f for f in os.listdir(ICON_DIR) if f.lower().endswith(".png") and os.path.splitext(f)[0] not in skip)
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


def apply_augment_icons(textures):
    common = unreal.load_asset(COMMON_SET)
    if common is None:
        raise RuntimeError("DA_CommonAugments 를 찾지 못함")
    icons = {k: v for k, v in common.get_editor_property("augment_icons").items()}
    applied, missing = [], []
    for tag, name in AUGMENT_ICONS.items():
        if name in textures:
            icons[unreal.Name(tag)] = textures[name]
            applied.append(tag)
        else:
            missing.append("%s (%s.png)" % (tag, name))
    common.set_editor_property("augment_icons", icons)
    EAL.save_loaded_asset(common, False)

    stored = {str(k) for k, v in common.get_editor_property("augment_icons").items() if v is not None}
    for tag in applied:
        if tag not in stored:
            raise RuntimeError("DA_CommonAugments 에 %s 가 안 들어감" % tag)
    return applied, missing


def apply_skill_icons(textures):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    ar_filter = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/HeavenBreach", "HBJobData")], recursive_classes=True)
    report = []
    for data in registry.get_assets(ar_filter):
        job = data.get_asset()
        for prop in ("active_skills", "passive_skills"):
            skills = list(job.get_editor_property(prop))
            changed = False
            for skill in skills:
                skill_id = str(skill.get_editor_property("skill_id"))
                ability = skill.get_editor_property("ability_class")
                ability_name = ability.get_name() if ability else ""
                for key, name in SKILL_ICONS.items():
                    if name in textures and (key in skill_id or key in ability_name):
                        skill.set_editor_property("icon", textures[name])
                        changed = True
                        report.append("%s %s -> %s" % (job.get_name(), skill_id, name))
            if changed:
                job.set_editor_property(prop, skills)
        if any(r.startswith(job.get_name() + " ") for r in report):
            EAL.save_loaded_asset(job, False)
    return report


textures = load_existing_icons()
imported = import_icons(textures)
textures.update(imported)
unreal.log_warning("[AugmentIcons] 기존 텍스처 %d개 사용, 새로 임포트 %d개 -> %s" % (len(textures) - len(imported), len(imported), DEST))

applied, missing = apply_augment_icons(textures)
unreal.log_warning("[AugmentIcons] 증강 아이콘 %d개 연결 (DA_CommonAugments.AugmentIcons)" % len(applied))
for m in missing:
    unreal.log_warning("[AugmentIcons] 아이콘 없음 - 스킬 아이콘으로 표시: " + m)

skill_report = apply_skill_icons(textures)
for line in skill_report:
    unreal.log_warning("[AugmentIcons] 스킬 아이콘 " + line)
if SKILL_ICONS and not skill_report:
    unreal.log_error("[AugmentIcons] JobData 에서 트리플 샷 스킬을 찾지 못함 - 직접 Icon 지정 필요")
