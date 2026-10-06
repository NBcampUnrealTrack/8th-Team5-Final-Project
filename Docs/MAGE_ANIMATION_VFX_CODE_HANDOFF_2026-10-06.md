# 마법사 애니메이션·VFX 코드 연동 인계

작성일: 2026-10-06 · 게임: Deep & Deeper · 개발팀: 레테(LETHE)

이 문서는 마법사 상체 이동, 오른손 스태프 평타, 왼손 액티브 4종과 VFX를 실제 게임 코드에 연결하기 위한 인계서다. 내부 프로젝트명/모듈명은 `HeavenBreach`를 유지한다. 궁수 인계서와 함께 사용할 수 있지만, 마법사 테스트의 시간·소켓·입력은 아래 값을 따른다.

**완료된 범위는 애니메이션과 리슨서버 테스트용 표현이다.** 실제 피해, 감속, 비용, 쿨다운, 장비/로드아웃 연결은 코드 담당자가 구현하고 검증해야 한다. 테스트 BP를 통째로 실제 캐릭터에 복사해 게임플레이 구현이 끝난 것으로 취급하지 않는다.

## 1. 기준 커밋과 바로 확인할 위치

| 커밋 | 내용 |
|---|---|
| `33000fe4` | 마법사 상체 Idle/Walk/Run, 스태프 평타, 테스트 맵/캐릭터 추가 |
| `04f3ffff` | 평타 상하체 분리, 중복 DefaultSlot 제거 |
| `661234bc` | Z/X/C/V 액티브 4종 애니메이션과 서버 테스트 연결 |
| `5f397147` | Mixed Magic 기반 VFX 4종과 리슨서버 테스트 연결 |

공통 루트는 `/Game/Levels/test_gm/MageRetarget`이다. 아래에서 `R`은 이 경로, `F`는 `R/VFX`를 뜻한다.

| 용도 | 에셋 |
|---|---|
| 실행 맵 | `R/L_Test_MageRetarget` |
| 테스트 캐릭터 / 컨트롤러 / 게임모드 | `R/BP_Test_MageCharacter`, `R/BP_Test_MageController`, `R/BP_Test_MageGameMode` |
| 상하체 분리 확인 | `R/ABP_Test_Mage` |
| 테스트 표현 데이터 | `R/DA_Test_MagePresentation` |
| 최종 시퀀스 / 몽타주 | `R/Animations`, `R/Montages` |
| 리타겟 중간 자료 | `R/Animations/SourceRetarget`, `R/Retarget/RTG_MageUE4_ToHB` |
| 구매 애니메이션 원본 | `/Game/Asset/FPCombatPrototype` |
| VFX 원본과 공통 의존 에셋 | `/Game/Asset/Mixed_Magic_VFX_Pack` |

테스트 입력은 LMB=스태프 1회 타격, Z=원소 구체, X=순간이동, C=서리 지대, V=마력 폭발이다. 1/2/3은 스태프 MA/MC/MN 교체다. 액티브 4개를 모두 테스트하기 위한 임시 입력이므로 실제 게임의 출격 전 2개 선택/스킬 슬롯 입력과 구분한다. 직업 선택 구조를 새로 만들라는 요청은 아니며, 무기에 따른 표현/사용 기술 연결을 따른다. 남아 있는 `JobData` 코드는 아래 코드 현황을 확인한다.

## 2. 캐릭터·스태프 연결

현재 타깃 메시: `/Game/Characters/Warrior/Meshes/Character/SK_HB_Warrior`

타깃 스켈레톤: `/Game/Asset/MMORPG_Characters/IdaFaber/Meshes/Boy/SKEL_UE5_M`

단일 Body 메시와 머리 카메라를 사용하는 기준이다. 오른손은 스태프, 왼손은 마법 시전이다. 기존 공통 하체/Aim Offset/카메라 설정을 유지한다.

제출된 스태프 복사본은 다음 3개다. **현재 폴더 철자 `Mashes/weapon` 그대로 사용한다.** 원본 구매 메시를 이동하거나 덮어쓰지 않았다.

```text
/Game/Characters/Mage/Mashes/weapon/SM_MMO_MA_WEAPON_Scale80
/Game/Characters/Mage/Mashes/weapon/SM_MMO_MC_WEAPON_Scale80
/Game/Characters/Mage/Mashes/weapon/SM_MMO_MN_WEAPON_Scale80
```

세 메시 모두 정점 XYZ 크기를 0.8로 줄인 결과이므로 **컴포넌트 Scale은 (1,1,1)**이다. 다시 0.8을 적용하면 64%가 된다. 테스트 맵의 MA/MC는 `R/Meshes/SM_MMO_*_WEAPON_Scale80`, MN은 `R/Meshes/SM_MageStaff_Scale80`이라는 별도 테스트 이름으로 참조한다. 실제 장비 연결에는 위의 직업 폴더 복사본을 사용한다.

테스트 `StaffVisual`은 `hand_r`에 부착하고, 중앙 그립을 위해 상대 위치 `(-9.5, 2.6, 20.0)`을 사용한다. 회전/부착 규칙까지 `BP_Test_MageCharacter`의 BeginPlay와 StaffVisual 설정을 함께 참고한다. 실제 장비의 무기 소켓 좌표계가 다르면 이 상대 위치만 그대로 넣지 말고 MA/MC/MN 모두에서 중앙 그립을 확인한다. 장비 교체 후 이전 AnimInstance 참조를 재사용하지 않고 현재 Body의 `GetAnimInstance()`를 다시 얻는다.

## 3. AnimBP 상하체 분리 — 반드시 유지할 구조

상체 시퀀스: `AN_Mage_Idle`, `AN_Mage_WalkForward/Left/Right`, `AN_Mage_RunForward/Left/Right`, `BS_Mage_UpperLocomotion` (`R/Animations`).

구매 에셋의 SwordRightHand와 MagicLeftHand 계열을 타깃에 리타겟한 후 오른손 스태프/왼손 마법 조합으로 정리했다. 최종 평타는 Blender에서 보정한 실제 키를 사용한다. 액티브는 구매한 ManaBlast, ManaBuff, ReleaseLoop의 왼팔 동작을 사용하고 나머지 기본 로컬 포즈는 승인된 마법사 Idle을 유지한다. 최종 에셋 대신 `SourceRetarget`의 전체 포즈를 바로 플레이하면 하체/반대팔 구성이 달라진다.

`ABP_Test_Mage`에서 확인할 흐름:

```text
Idle_Run / Crouch 상태 내부:
  공통 하체 이동 포즈(Base) + MageUpperLocomotion(상체)
  -> Layered Blend Per Bone(spine_01 기준)

최종 AnimGraph:
  기본 이동 포즈 -> DefaultSlot(상체 몽타주) -> 상체 Layered Blend Per Bone
  -> 기존 Aim Offset -> Output Pose
```

몽타주 레이어는 `spine_01`, BlendDepth=0, Mesh Space Rotation Blend=True이다. **최종 상하체 레이어 뒤에 DefaultSlot을 또 연결하지 않는다.** 이 중복 슬롯 때문에 평타 동안 다리가 정지했으며 `04f3ffff`에서 수정했다. Idle_Run만 고치고 Crouch를 빠뜨리거나 시퀀스/데이터만 교체해서는 같은 결과가 나오지 않는다.

코드 연동 시에는 현재 공통 하체/AO를 가진 실제 AnimBP에 이 상체 흐름을 옮기거나 무기 행의 `CharacterAnimClass` 경로를 사용한다. 테스트 AnimBP로 교체할 경우 다른 무기/장비의 공통 기능까지 보존되는지 담당자가 확인해야 한다.

## 4. 몽타주·발동 시간표

모두 `R/Montages`, Slot=`DefaultSlot`, 액티브 Root Motion 이동 없음. 액티브 Blend In=0.10초, Blend Out=0.16초다. 아래 표식은 `AnimNotify_PlayMontageNotify`의 이름이다.

| 입력/동작 | 최종 몽타주 | 길이 / 테스트 재생률 | Notify 이름 / 에셋 시간 | 현재 발동 방식 |
|---|---|---|---|---|
| LMB 스태프 | `AM_Mage_StaffBasicAttack_Blender` | 1.2초 / 1x | `MageStaffStrike` / 0.333333초 | 모션 표현만. 실제 타격 없음 |
| Z 원소 구체 | `AM_Mage_LeftMagicFire` | 1.0초 / 1x | `MageMagicRelease` / 0.4초 | 현재 몽타주 위치를 타이머로 검사, 서버가 1발 생성 |
| X 순간이동 | `AM_Mage_Teleport` | 1.5초 / **2x** | `MageTeleport` / **0.6초** | 서버 Delay **0.3초**, 재생 중이면 이동 1회 |
| C 서리 지대 | `AM_Mage_FrostField` | 1.0초 / 1x | `MageFrostFieldRelease` / 0.35초 | 서버 Delay 0.35초, 최초 지정 지점에서 필드 1회 |
| V 마력 폭발 | `AM_Mage_ManaExplosion` | 2.2초 / 1x | `MageManaExplosionRelease` / 1.6초 | 서버 Delay 1.6초, 그 시점의 시전자 중심에서 폭발 1회 |

**`AM_Mage_StaffBasicAttack`(1.0초, 표식 0.3초)은 이전 후보이며 현재 최종 평타가 아니다.** 코드에는 `_Blender` 버전을 연결한다.

X의 에셋 시간과 실제 시간을 혼동하지 않는다. 2배속이므로 Notify 0.6초가 요청 후 약 0.3초이며 모션은 약 0.75초에 종료, 테스트 입력 잠금은 약 0.8초에 해제된다. 재생률을 변경하면 이동/손 준비 VFX 시간도 함께 조정한다.

V는 ManaBuff를 1.5배속으로 재생한 1초 준비 + 0.2초 전환 + ManaBlast 1초로 구성했다. 발동 후 0.75초 회복을 포함해 테스트 잠금은 요청 후 2.35초에 해제된다.

**현재 Notify는 코드 담당자용 타이밍 표식이며 테스트의 별도 발동 소스가 아니다.** 실제 GAS 연동은 서버 몽타주 이벤트/AbilityTask 또는 검증된 서버 타이머 중 하나를 발동 소스로 선택한다. 기존 Delay/타이머와 Notify에서 동시에 피해·투사체·이동을 실행하지 않는다. 같은 시전의 중복 이벤트 방지 상태와 중단/무기 교체 시 정리를 추가한다. 전용 서버에서 몽타주 시간에 의존할 경우 메시의 애니메이션 업데이트 정책도 검증한다.

## 5. BP_Test_MageCharacter — 현재 테스트 호출 계약

대상: `R/BP_Test_MageCharacter`, `R/BP_Test_MageController`.

| 동작 | 확인할 이벤트 흐름 |
|---|---|
| 스태프 | `RequestStaffAttack -> ServerStaffAttack -> MulticastStaffAttack`, 실제 로컬 재생 `PlayStaffAttack` |
| Z | `RequestLeftMagicFire -> ServerLeftMagicFire -> MulticastLeftMagicFire`, `PlayLeftMagicFire`, `PollElementalOrbFX -> PerformElementalOrbRelease`, `CleanupOrbCastFX` |
| X | `RequestTestTeleport -> ServerTestTeleport -> MulticastStartTestTeleport -> PerformTestTeleport -> MulticastFinishTestTeleport`; 이동 성공에서만 `MulticastTeleportBlinkFX` |
| C | `RequestTestFrostField -> ServerTestFrostField -> MulticastStartTestFrost -> PerformTestFrostField -> MulticastTestFrostField -> MulticastFinishTestFrost` |
| V | `RequestTestManaExplosion -> ServerTestManaExplosion -> MulticastStartTestMana -> PerformTestManaExplosion -> MulticastTestManaExplosion -> MulticastFinishTestMana` |

스태프/Z에는 소유자 선재생과 multicast의 소유자 중복 재생 방지 처리가 있다. X/C/V는 서버 검증 후 multicast로 시작한다. RPC 이름만 따라 하지 말고 각 이벤트의 현재 분기와 재생 성공 검사를 확인한다. 입력·시전 잠금, 몽타주 중단, 서버 권한과 1회 방출 조건도 함께 옮겨야 한다.

소유자 전용 화면 처리는 `IsLocallyControlled()`로 판정하고 실제 이동/게임플레이 판정은 Authority에서 실행한다. 리슨 호스트는 두 조건을 모두 만족할 수 있다. 클라이언트가 넘긴 조준점/시간/반경을 그대로 신뢰하는 실제 게임 구현은 피하고 서버에서 범위·시전 상태를 검증한다.

Z의 타이머는 60Hz로 현재 AnimInstance의 몽타주 위치를 확인한다. 이 테스트의 기반 캐릭터 Actor Tick은 착지 후 꺼질 수 있어 그 Tick에 발사를 의존하지 않았다. 단순히 이벤트를 Character Tick으로 옮기면 구체가 나오지 않을 수 있다. 타이머는 중단 시 비우고 손 효과를 제거한다.

## 6. 스킬별 VFX 연결과 좌표·수명

아래 BP의 로컬 효과 생성/유한 수명/재질 설정을 포함해 사용하는 것이 가장 간단하다. Niagara 시스템만 단독 Spawn하면 같은 크기·색·정리 결과를 보장하지 않는다. 원본 팩에는 긴 수명/루프 emitter가 있으므로 Auto Destroy만으로 종료를 맡기지 않는다. 파생 시스템/머티리얼을 사용하고 원본 팩을 수정하지 않는다. 일부 emitter는 테스트 BP에서 **생성만 억제**했으며 emitter 업데이트 비용까지 제거한 최적화는 아니다.

손 준비 효과는 Body 메시 `hand_l` 기준 상대 위치 `(18,-6,0)`을 사용한다. 정상 종료, 중단, 무기 교체, 소유자 파괴 모두에서 효과/타이머 참조를 정리한다. 로컬 효과를 multicast로 생성하면서 동일 효과 Actor까지 복제하면 이중 생성된다. 게임플레이 Actor 복제와 관찰자 로컬 VFX 생성 경로를 구분한다.

### Z: 원소 구체 (`F/ElementalOrb`)

- 시스템: `NS_Mage_ElementalOrb_Cast`, `NS_Mage_ElementalOrb_Projectile`, `NS_Mage_ElementalOrb_Impact`.
- Actor: `BP_Test_MageElementalOrb`(서버 생성·이동 복제), `BP_Test_MageOrbImpactFX`(multicast 적중으로 각 월드에서 로컬 생성).
- 손 준비 scale=0.2, 투사체 Niagara scale=0.3, 적중 scale=0.4. 파생 MI 16개 사용.
- 서버는 몽타주 위치 0.4초 이상 + 발사 대기 상태일 때 대기를 먼저 소모하고 **1발만** 생성한다. 그 전에 몽타주가 중단되면 발사 취소.
- 실제 `FirstPersonCamera`의 World Location/Rotation으로 구한 조준 지점을 향해 왼손에서 발사한다. 카메라 전방으로 곧바로 생성하지 않고 손 시작점과 목표 방향을 분리한다. 서버 조준 검증은 실제 코드에서 담당한다.
- 속도=1800cm/s, 구 충돌 trace 반경=10cm, 미적중 수명=4초. 적중 판정/적중 multicast는 서버 1회, 적중 후 투사체 정리 약 0.35초, 로컬 적중 Actor 수명=1.6초.
- 이 Actor의 충돌은 표현용 테스트다. 실제 피해/아군 제외/벽 충돌/중복 적중 방지는 게임플레이 투사체에서 결정한다. 생산용 투사체를 사용하면 테스트 Actor와 함께 2개를 발사하지 않는다.

### X: 순간이동 (`F/Teleport`)

- 시스템: `NS_Mage_Teleport_Cast`, `NS_Mage_Teleport_Departure`, `NS_Mage_Teleport_Arrival`.
- Actor: `BP_Test_MageTeleportCastFX`, `BP_Test_MageTeleportDepartureFX`, `BP_Test_MageTeleportArrivalFX`, `BP_Test_MageTeleportViewFX`.
- 손 준비 scale=0.16 / 수명=0.34초. 출발 scale=0.45 / 도착 scale=0.32, 각 수명=0.85초. 출발·도착 효과는 캐릭터 중심에서 Z를 25cm 내린 위치.
- 기본 테스트 이동 거리 `TestTeleportDistance`=400cm. 조준 Yaw 전방의 지상 이동이며 점프/루트 모션이 아니다. 캡슐 경로 sweep, 벽 여유 12cm, 지면/법선 Z>=0.7, 이동 거리>40cm, 높이차<=100cm, 최종 `TeleportTo` 성공을 확인한다.
- **이동 성공에서만** 서버가 이동 전 좌표와 실제 이동 후 좌표를 `MulticastTeleportBlinkFX`로 전달한다. 클라이언트에서 예상 목적지를 다시 계산하거나 따로 이동시키지 않는다. 막힌 위치에서는 손 준비만 보이고 출발/도착 효과와 이동 횟수는 증가하지 않는다.
- 도착 화면 효과는 소유자에게만 카메라 상대 `(80,0,-20)`, scale=0.12, 수명=0.35초. 다른 관찰자에게 ViewFX를 생성하지 않는다.
- 체크 변수: `TestTeleportPlayCount`, `TestTeleportMoveCount`, `TeleportCastFXCount`, `TeleportBlinkFXCount`, `LastTeleportFXFrom/To`.

### C: 서리 지대 (`F/FrostField`)

- 시스템: `NS_Mage_FrostField_Cast`, `NS_Mage_FrostField_Mist`.
- Actor: `BP_Test_MageFrostCastFX`(scale=0.16, 수명=0.42초), `BP_Test_MageFrostFieldFX`(관찰자 로컬 Actor).
- 서버 요청 시 실제 카메라 방향으로 최대 1200cm 조준 레이와 지면 검증 후 **지점/법선을 고정**한다. 시전 중 카메라를 움직여도 지점이 따라 움직이지 않는다. 0.35초에 재생/활성/미방출 조건을 통과하면 multicast 1회.
- 기본 `TestFrostRadius`=200cm, `TestFrostDuration`=4초. Field Actor XY scale=`Radius/200`, Z=1. 생성 시 `FieldDuration`을 전달/설정한다. 시전 취소 시 새 필드를 만들지 않는다. 기존에 성공한 필드는 새 시전과 별개로 수명 동안 유지한다.
- 지면 법선에 맞춘 평면 서리, 낮은 안개 3개, 작은 얼음 조각 7개. 0.28초 성장, 마지막 0.6초 페이드, 종료 시 안개 비활성화, 전체 Actor 수명=`FieldDuration+0.15초`.
- 바닥 재질 `Materials/M_Mage_FrostGround`, 조각 `M_MageFrost_CrystalStatic` 사용. 원본 얼음 WPO로 지면 조각이 과하게 흔들리지 않도록 복제 재질의 WPO를 0으로 처리했다. 바닥은 **평면 테스트 메시**로 복잡한 지형에 완전히 밀착하는 구현은 아니다.
- 실제 감속 영역/유지 시간은 서버 게임플레이 영역과 이 반경/기간을 맞춘다. 판정용 overlap을 로컬 FX Actor에 맡기지 않는다.

### V: 마력 폭발 (`F/ManaExplosion`)

- 시스템: `NS_Mage_ManaExplosion_Cast`, `NS_Mage_ManaExplosion_Burst`, `NS_Mage_ManaExplosion_Impact`.
- Actor: `BP_Test_MageManaCastFX`, `BP_Test_MageManaBurstFX`.
- 준비 scale=0.14, 수명 상한=1.62초. 준비 Actor는 Owner/현재 AnimInstance/해당 몽타주를 확인하여 시전 중단 시 즉시 파괴한다.
- 서버 발동 1.6초에서 **현재 시전자 위치**와 발밑 좌표를 캡처해 `MulticastTestManaExplosion`으로 전달한다. 최초 요청 위치나 각 관찰자 위치를 다시 계산하지 않는다. 시전 중 이동하면 실제 폭발 시점의 시전자 중심을 따른다.
- 기본 `TestManaRadius`=300cm. Burst Actor는 발밑에 두고 XY scale=`Radius/300`, Z=1. 코어는 상대 Z=65 / scale=0.65, 충격은 Z=4 / scale=0.5.
- `Materials/M_Mage_ManaShockwave`의 600cm 평면 충격파: 약 0.7초 확장, 1.2초까지 페이드. Niagara 방출 정지=0.45초, 전체 로컬 Actor 수명=1.65초.
- 이동 구체/긴 트레일/렌즈와 돌·용암·잔류 연기는 억제하고 청색·보라색 마력과 원형 입자를 유지했다. 복제한 부모 재질 2개와 MI 15개까지 함께 참조한다.
- `Deactivate` 후 남은 입자가 종료되는 동안 `IsActive`가 True일 수 있다. 방출 정지와 최종 Actor/컴포넌트 제거로 정리를 확인한다. 이미 성공한 폭발은 그 뒤 모션 중단과 별개로 정상 소멸한다.
- 체크 변수: `ManaCastFXCount`, `TestManaBurstFX`, `bTestManaActive`; 발동 1회 방지와 2.35초 입력 잠금 해제를 유지한다.

## 7. C++ 클래스별 현재 상태와 담당자 연동 사항

아래는 2026-10-06 확인한 코드 상태(`origin/develop`=`3b36c72d` 기준)다. 이번 애니메이션/VFX 제출에서는 이 클래스들을 수정하지 않았다. 구현 시 담당 클래스 안에서 필요한 부분만 연결하고 데이터 구조/공통 장비/스켈레톤 변경은 소유자와 합의한다.

### AHBPlayerCharacter / FHBWeaponPresentation

대상 파일:

```text
Source/HeavenBreach/Player/HBPlayerCharacter.h
Source/HeavenBreach/Player/HBPlayerCharacter.cpp
Source/HeavenBreach/Player/HBWeaponPresentation.h
```

- `RefreshWeaponVisual()`은 아이템 행 이름 -> 무기 종류 순서로 `DT_WeaponPresentation`을 찾고, `RefreshCharacterAnimClass()`가 해당 행의 `CharacterAnimClass`로 Body AnimBP를 교체한다. 장비 바뀔 때마다 교체하면 진행 중 몽타주가 끊기므로 같으면 유지한다.
- `FHBWeaponPresentation`에는 CharacterAnimClass와 Draw/Release/CancelMontage 등 활 표현 필드가 있다. **마법사 액티브 4종 몽타주/VFX 필드는 아직 없다.** `DA_Test_MagePresentation`만 설정하면 실제 스킬이 자동 연결되는 구조가 아니다. 액티브 표현 참조를 기존 스킬 데이터에 둘지 별도 표현 데이터에 둘지는 코드/데이터 담당자가 정한다.
- 스태프는 정적 메시다. `WeaponMeshComp`의 `EquipMeshPath`와 실제 스태프 소켓/중앙 그립을 연결한다. `AttachSocket` 행 필드는 현재 스켈레탈 무기 경로에서 사용하는 값이므로 정적 스태프에도 자동 적용된다고 가정하지 않는다. 테스트 `StaffVisual`과 실제 WeaponMeshComp를 동시에 표시하지 않는다.
- `TryActivateJobSkill()`에는 SelectedSkillQId/EId와 JobData 조회가 아직 남아 있다. 현재 실제 경로는 `ASC->TryActivateAbilityByClass()`이며 주석은 비용/쿨다운을 Ability에 맡긴다. 기획의 무기별 기술/2개 선택과 현재 로드아웃 코드를 담당자가 맞춘다. 테스트 Z/X/C/V 폴링을 생산 입력에 중복 추가하지 않는다.
- 카메라 조준은 머리 카메라의 실제 World Rotation과 어긋나지 않아야 한다. 테스트는 FirstPersonCamera 회전으로 수정되어 있다. 실제 `ComputeAimPoint()` 등 기존 helper가 GetBaseAimRotation을 사용하는 지점은 카메라 기준과 비교한다.
- 영향/확인: 다른 무기 장착 시 기존 AnimBP 복귀, 장비 UI/인벤토리 경로 유지, 캐릭터/무기/카메라 소켓, owner/remote에 동일한 장비. 시전 중 교체·사망·메시 교체에서는 몽타주/타이머/손 FX를 정리한다.

### UGA_MageBasicAttack

대상: `Source/HeavenBreach/Player/Abilities/GA_ClassBasicAttacks.h/.cpp`, `ActivateAbility()`.

- 현재 코드는 거리 1500cm/반경 55cm의 **즉시 원거리 trace** 판정 후 종료하며, 스태프 근접 모션과 연결되어 있지 않다. 무기 파손 검사, 공격 속도 기반 쿨다운, 성공 시 내구도 감소 경로가 있다.
- 오른손 스태프 일반 평타에는 최종 `_Blender` 몽타주를 재생하고 `MageStaffStrike` 약 0.333초를 기준으로 서버 근접 판정을 연결한다. 기존 원거리 피해와 새 근접 피해가 함께 실행되지 않도록 판정 소스를 교체한다. 기존 `ApplySkillDamage()`가 원거리 피해 종류를 고정하는 점도 아래에서 확인한다.
- 평타 중 Walk/Run/Crouch 하체와 AO가 유지되는지, 같은 공격의 다중 피해/내구도 감소가 없는지, MA/MC/MN 모두 중앙 그립이 맞는지 테스트한다. 실제 타격 범위와 trace 소켓은 코드 담당자 결정 사항이다.

### UGA_MageMagicBolt / UGA_MageTeleport / UGA_MageManaExplosion

대상: `Source/HeavenBreach/Player/Mage/Abilities/GA_MageAbilities.h/.cpp`, 각 `ActivateAbility()`.

| 클래스 | 현재 코드 | 이번 표현과 맞출 부분 |
|---|---|---|
| `UGA_MageMagicBolt` | 즉시 line target 1400cm 판정/피해, 마나 +8 후 종료 | 원소 구체로 연결한다면 0.4초 서버 투사체 생성과 실제 적중 판정으로 연결. 기존 즉시 피해/+8을 유지할지는 스킬 기획 확인 |
| `UGA_MageTeleport` | ActorForward 600cm로 즉시 `SetActorLocation(..., sweep=true)` 후 종료 | 2x 모션의 실제 0.3초에 서버 이동 1회. 테스트 400cm와 실제 600cm는 다른 값. 거리/안전 검증을 합의하고 이동 성공의 실제 좌표에 FX 연결 |
| `UGA_MageManaExplosion` | 즉시 시전자 중심 반경 450cm 판정, 현재 마나 50%에서 계산한 계수로 피해 후 종료 | 1.6초 시전 유지/중단 조건과 발동 시점 위치에 연결. 테스트 반경 300cm와 다름. 실제 반경을 FX에 전달하고 기존 즉시 피해와 중복하지 않음 |

**현재 헤더에는 `UGA_MageFrostField`가 없다.** IceLance(직선), FireZone(화염), GravityField(당김)는 이 C 서리 지대와 다른 동작이다. 서리 지대 담당 Ability/GameplayEffect를 정하고 서버 지정 지점·반경·기간·감속을 구현한 뒤 C 표현을 연결한다. 테스트 FX가 감속 로직까지 제공하는 것은 아니다.

### UHBPrototypeAbilityBase

대상: `Source/HeavenBreach/Player/Abilities/HBPrototypeAbilityBase.h/.cpp`.

- 현재 NetExecutionPolicy=ServerOnly. `BeginAbilityWithCooldown()`은 CommitAbility와 쿨다운 적용을 수행하며 전달된 ManaCost가 양수이면 수동 마나 차감도 한다. `FinishAbility()`는 즉시 EndAbility다.
- `GA_MageAbilities.cpp`의 `HB_BEGIN_MAGE`는 Cost 매개변수를 받아도 이 helper에는 **0.f**를 전달한다. 오래된 파일 주석의 “Player가 비용 차감”과 현재 `TryActivateJobSkill()` 경로가 일치하지 않는다. 마력 폭발의 `Spent` 계산만으로 마나가 실제 차감됐다고 판단하지 않는다. 생산용 Ability의 CheckCost/ApplyCost/Commit 흐름을 확인해 비용·쿨다운을 한 경로에서 처리한다.
- 시전 Ability를 즉시 Finish하지 말고 발동/회복/중단 상태에 맞춰 종료한다. 중단 시 발동 대기, 입력 잠금, 준비 FX, 이벤트/타이머를 정리하고 서버 발동을 1회로 제한한다.
- `ApplySkillDamage()`는 현재 `EHBDamageSource::Ranged`를 고정한다. 스태프 근접 공격을 이 helper에 그대로 넣으면 근접/원거리 보정이 어긋날 수 있다. 피해 담당 인터페이스와 공격 종류를 확인한다. 적/팀 판정, 마나/쿨다운, 내구도/증강 처리는 기존 담당자 코드와 함께 검증한다.

## 8. 권장 연동 순서

1. 테스트 맵에서 MA/MC/MN, 상체 이동, LMB와 Z/X/C/V의 소유자/관찰자 표현을 먼저 확인한다.
2. 실제 스태프 장비 행과 중앙 그립, AnimBP 상체/하체/AO부터 연결한다. 공격 전후 및 웅크린 이동에서 다리 정지가 없어야 한다.
3. 스태프 평타 -> Z -> X -> C -> V 순으로 하나씩 실제 Ability에 연결한다. 각 스킬의 서버 발동 소스는 한 개만 둔다.
4. Notify 시간과 재생률, 시전 잠금/중단/장비 교체 처리를 연결하고 해당 서버 성공 이벤트에 VFX를 붙인다.
5. 실제 기획의 반경·기간·이동거리·비용을 서버 게임플레이와 관찰자 FX에 동일하게 전달한다. 테스트 수치를 밸런스 확정값으로 취급하지 않는다.
6. 서버 권한 피해/감속/이동과 비용·쿨다운·장비 내구도/파손, 실제 선택한 2개 스킬 입력을 통합한다. 테스트 캐릭터/컨트롤러의 임시 경로와 동시에 실행하지 않는다.

## 9. 검증 기록과 코드 연동 후 확인할 것

에셋 제작 단계의 정상 속도 2인 리슨서버 PIE 자동 확인: Z 14건, X 19건, C 21건, V 20건 통과. 서로 다른 스킬 단계의 기록이며 생산 코드 통합 테스트 74건을 뜻하지 않는다. 호스트/클라이언트, 스태프 3종, Idle/Walk/Run/Crouch, 반복/동시 요청, 시전 중단, 잘못된 목표/막힌 이동, 효과 좌표/크기/수명/정리와 다른 스킬/평타 회귀를 포함한다. 상하체 수정 단계에는 Walk/Run/Crouch 양쪽 다리의 이동 유지도 별도 확인했다.

사용자가 애니메이션과 VFX 결과를 플레이/화면으로 확인하고 마무리했다. C의 실제 OS 키 입력 자동 검증은 수행하지 않았으며, 바인딩과 요청 경로를 통한 PIE 검증이다. 최종 VFX 검증에서 V의 실제 OS 키 입력을 재검증한 것은 아니다. Blueprint 컴파일/정상 속도 실행 확인과 직접 키 입력 확인을 구분한다.

원본 VFX 팩 166개 파일은 제작 중 해시를 유지했다. 파생 VFX 82개와 테스트 캐릭터 연결을 제출했으며 최종 VFX 제출 시 Blueprint 컴파일 메시지 0, 관련 Runtime Error 0, PIE 종료/저장되지 않은 에셋 0을 확인했다. 구매 에셋의 원본을 일반 인터넷 무료 자료로 취급하지 말고 팀의 Fab 라이선스/소유 권한에 따라 프로젝트 안에서 사용한다.

코드 담당자의 완료 확인 항목:

- [ ] 실제 장비 3종 모두 Scale=1 / 중앙 그립, 스태프 1개만 표시, 상하체/AO 유지.
- [ ] 실제 2개 선택 입력에서 올바른 몽타주와 스킬이 1회 실행.
- [ ] 각 서버 발동 시점과 관찰자 VFX가 일치; X 재생률 2x 반영.
- [ ] 시전 전 중단/피격/사망/장비 교체에서 피해·이동·투사체가 발생하지 않고 준비 효과/잠금 해제.
- [ ] 성공한 투사체/필드는 모션 종료와 독립된 수명; 중복 피해/출발·도착 FX/필드 생성 없음.
- [ ] 크로스헤어 조준, 최초 C 지정 지점, 발동 시점 V 중심, X 실제 이동 좌표 일치.
- [ ] 실제 게임플레이 반경/기간/거리와 FX 크기가 일치; 비용·쿨다운·내구도 처리 1회.
- [ ] 리슨 호스트와 클라이언트 각각 시전/관찰, 지연·패킷 손실, 중도 참가, 실제 다른 PC에서 확인.
- [ ] 전용 서버를 지원한다면 애니메이션 Tick에 발동이 종속되지 않는지 확인.
- [ ] 이동·웅크리기 중 공격/시전, 무기 전환 후 기본 포즈 복귀, FX 최종 소멸과 Runtime Error 없음.

Late Join/다른 PC/피해·감속·GAS 통합/인벤토리·실제 로드아웃 보장은 이번 표현 테스트에 포함되지 않는다. 이런 범위는 위 항목을 통과한 뒤 완료 처리한다.

## 10. 제작 자료 보관과 유지보수

최종 UE 시퀀스/몽타주/파생 FX가 팀원이 사용할 제출 결과다. 로컬 `Saved/MageRetargetTest`, `Saved/MageVFX`에는 Blender/리타겟/재질 제작 스크립트, 중간 JSON, 프레임/런타임 관측 기록이 있다. 단계별 생성 스크립트를 현재 에셋에 일괄 재실행하면 중복 생성이 생길 수 있어 설치/연동 스크립트로 사용하지 않는다. 이 문서를 사용하는 데 그 로컬 파일들은 필요하지 않다.

추가 모션 보정은 승인된 포즈와 뼈 길이를 유지하고 어깨·팔꿈치·손목의 자연스러움, Body/장비 침범, 소유자/관찰자 화면을 함께 확인한다. 공유 스켈레톤 소켓이나 원본 팩/팀원 C++를 표현 수정의 지름길로 바꾸지 않는다.

일반 작업 기록과 파이프라인 md는 로컬 보관 원칙이다. **이 인계서 1개만 사용자의 명시적인 문서 제출 요청으로 커밋/푸시한다.**
