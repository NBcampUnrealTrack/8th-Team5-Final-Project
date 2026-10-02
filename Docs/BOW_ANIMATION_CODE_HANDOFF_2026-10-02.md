# 궁수 애니메이션 코드 연동 인계

작성일: 2026-10-02. 기준 에셋 커밋: `4ac2e8b5` (`feat/BattleSystem`).

궁수 기본 사격과 네 액티브 스킬의 모션을 사용자 확인까지 마무리했다. 이 문서는 **Idle 오른쪽 겨드랑이 수정 전 기준과 현재의 차이**, **팀원의 C++/GAS 연동에 필요한 에셋·입력·타이밍·표시 계약**을 정리한다. 현재 체크아웃의 소스, Unreal 실제 그래프, 몽타주/시퀀스 메타데이터를 확인했다. 생산 코드 수정이나 실제 적 피해 통합은 이번 제출에 포함하지 않았다.

사용자가 이 인계 MD의 커밋·푸시를 명시적으로 요청했으므로 이 파일만 제출한다. 다른 로컬 Docs, Saved, SourceArt 자료는 제출 대상이 아니다.

## 1. Idle 오른쪽 겨드랑이 수정 전 커밋

| 구분 | 커밋 | 의미 |
|---|---|---|
| 정확한 수정 직전 | `c08ca3954b84ddb4164a4ae876a278a660ba910b` | `974aef96`의 바로 부모. 겨드랑이 비교 기준 |
| 이전 궁수 제출 | `64dddb6e` | 기본 리타겟/장전/발사 제출. 해당 AnimBP의 LFS 객체가 위 부모 커밋과 동일 |
| 겨드랑이 보정 | `974aef969aba8c11983ccc773cc7470e915dd0a7` | `[FIX] (animation) Reduce bow idle right elbow opening`. 변경 파일은 아래 AnimBP 하나 |
| 현재 완성본 | `4ac2e8b5` | 겨드랑이 보정을 유지하고 관통·표식·트리플·회피 스텝샷까지 포함 |

대상: `Content/Levels/test_gm/BowRetarget/ABP_Test_Bow_Lowered.uasset`.

Git의 실제 LFS 포인터를 비교했다. 수정 전 객체 SHA256은 `79180bb349d14471d3bb45a9373cd021cff883688ebdef3eb1f0170805bd51bf`, 보정 직후 객체는 `960868123cc7017e527dc4ed6173c543e58552c4900d09e8fe3c8b99bac5ca2f`다. `64dddb6e`와 `c08ca395`의 해당 파일은 같다.

### 1.1 겨드랑이 보정 자체의 차이

**Idle 시퀀스를 새로 만든 변경이 아니라 AnimBP의 `IdleRightArmIK` 보정이다.** 따라서 Idle 시퀀스만 복사하면 최종 오른팔 간격은 재현되지 않는다.

| 항목 | 수정 전 | 보정 후 / 현재 테스트 그래프 |
|---|---|---|
| IK 본 | `hand_r` | 유지 |
| 기준 공간 | `clavicle_r` Bone Space | 유지 |
| 팔꿈치 JointTargetLocation(cm) | `(-52.385956, -4.514003, 20.919014)` | `(-35.144794, 47.913654, 30.616186)` |
| 손 EffectorLocation(cm) | `(-25.228960, -21.773302, 26.077904)` | 유지 |
| `bMaintainEffectorRelRot` | 팔꿈치 변경과 손목 회전의 동반 변화를 검토 | `False`, 입력 손목 방향 보존 |
| 알파 | `1 - BowRaised` | 유지. 활을 올려 장전하면 이 Idle IK가 빠짐 |
| 정면 상완 벌어짐 측정 | 오른팔 약 57.3도 | 오른팔 약 28.3도, 왼팔 약 28.7도 |

각도는 당시 서로 다른 Idle 시점의 유한 표본이다. 현재 그래프의 목표값은 다시 확인했고, 뼈 길이를 늘려 맞춘 방식은 아니다. 이후 가로 활 Idle/손목/조준 후보는 사용자 요청으로 폐기하고 이 보정 단계로 되돌렸다. **가로 활 Idle은 현재 기준이 아니다.** 가로 활은 관통 사격에서 사용한다.

옛 AnimBP 전체를 현재 위에 덮어쓰면 이후 스킬 재생·조준·회피 전체 몸 경로가 사라진다. 비교가 필요하면 별도 체크아웃에서 수정 전 파일을 열고, 현재 연동에는 이 IK 설정과 뒤의 스킬 경로를 함께 가져간다.

### 1.2 수정 전 기준 이후 추가된 궁수 작업

| 커밋 | 추가/수정 | 팀원에게 필요한 점 |
|---|---|---|
| `974aef96` | 위 Idle 오른팔 간격 보정 | 시퀀스만 이관하지 말고 IK도 반영 |
| `20b8c8ee` | AN 활 손잡이의 왼손 소켓 적합 보정 | 보정된 `Scaled80/SK_AN_BOW_Scale80` 사용 |
| `cf8ea99b` | 관통 준비/장전/발사/취소, 큰 화살과 기류 VFX | 초기 자동 사격 `PiercingShot` 대신 `PiercingHorizontal` 사용 |
| `03cd34c5` | 광역 사냥꾼의 표식, 전방 손 입자와 범위/표식 테스트 | 단일 대상 표식과 다른 기획 |
| `f89ea5b5` | 트리플 전용 기울기·그립·양팔, 동시 3발 | 장전 화살 3개와 실제 투사체 3개를 각각 연결 |
| `0a46bc89` | 캐릭터 Actor Tick이 꺼져도 활 조준 갱신 | 독립 `BP_Test_BowPresentationTick` 추가 |
| `4ac2e8b5` | 지면 회피 스텝샷, 97.5cm 후퇴, 길어진 당기기, 자동 1발 | `EvasiveStepShot`의 `_LongDraw` 몽타주와 전체 몸 루트 모션 사용 |

해당 기간 기존 Step5 기본 사격 시퀀스에는 Git 변경이 없다. 기본 사격에 스킬용 보정이 중복 적용되지 않도록 AnimBP 경로를 분리한 것이 중요하다. 팀의 전사/장비/생산 코드 변경은 이 표의 궁수 제작 변경과 별도로 관리한다.

## 2. 테스트 기준과 에셋 위치

이하 `R = /Game/Levels/test_gm/BowRetarget`다. `{B}`는 `AA`, `AC`, `AN`, `{단계}`는 `Prepare`, `DrawHold`, `Release`, `Cancel`이다. 아래 패턴은 실제 에셋 이름이며 생략한 앞부분은 `R`이다.

| 역할 | 에셋 |
|---|---|
| 테스트 맵 | `R/L_Test_BowRetarget` |
| 몸/입력 | `R/BP_Test_BowCharacter`, `R/BP_Test_BowController` |
| 몸 애니메이션 | `R/ABP_Test_Bow_Lowered` |
| 활 시위 | `R/ABP_Test_BowString80` |
| 장전 화살 표시 | `R/BP_Test_BowDrawArrow` |
| 조준 갱신 / 회피 시점 감시 | `R/BP_Test_BowPresentationTick`, `R/BP_Test_BowEvasionTick` |
| 일반 투사체 | `R/BP_Test_BowArrowProjectile` |
| 관통 투사체 | `R/Skills/PiercingShot/BP_Test_PiercingArrowProjectile` |

몸 골격은 `/Game/Asset/MMORPG_Characters/IdaFaber/Meshes/Boy/SKEL_UE5_M`, 테스트 몸은 `SK_HB_Warrior`다. 활은 `hand_l/BowSocket`, 오른손은 시위를 당긴다. 하나의 몸 메시와 머리를 따르는 1인칭 카메라를 기준으로 제작했다.

활 3종은 `/Game/Asset/MMORPG_Characters/IdaFaber/Meshes/Scaled80/SK_{B}_BOW_Scale80`이다. XYZ 0.8은 메시/리그에 이미 베이크되어 있으므로 장착에서 다시 전체 0.8을 곱하지 않는다. AN은 이 축소본의 손잡이만 AC 그립에 맞췄으며 구매 원본과 공유 BowSocket은 그 보정 작업으로 바꾸지 않았다.

### 2.1 현재 테스트 입력

| 입력 | 동작 | 입력 규칙 |
|---|---|---|
| 6 / 7 / 8 | AA / AC / AN 활 선택 | 내부 `RequestBowAction` 번호는 7 / 8 / 9로 숫자 키와 다름 |
| LMB | 기본 사격 | 누르기 → 당기기 → 유지 → 놓기/발사 |
| Z | 관통 사격 | 준비 → LMB 당기기/유지 → 완전 장전 후 해제/1발 |
| X | 사냥꾼의 표식 | 한 번 누르면 제스처/범위 적용/복귀 |
| C | 트리플 어택 | 준비 → LMB 당기기/유지 → 완전 장전 후 해제/동시 3발 |
| V | 회피 공격 | 한 번 누르면 지면 후퇴 → 당기기 → 자동 1발 → 복귀 |
| RMB | 준비/장전 스킬 취소 | 관통/트리플 취소, 표식 적용 Notify 전 취소 |

컨트롤러는 테스트용 키 폴링이다. 생산 키/Enhanced Input/GAS 입력으로 연결할 때 키 이름보다 위 입력 의미를 옮긴다. V는 LMB 해제를 기다리는 스킬이 아니다. Idle에서는 장전 화살이 보이지 않는다.

## 3. 몽타주와 실행 타이밍

곡선은 **시퀀스**에 있고 주요 Notify는 **몽타주**에 있다. 원본 시퀀스만 재생하면 몽타주 Notify가 오지 않는다. 이름이 같은 이벤트를 추가해도 gameplay 호출이 자동으로 만들어지지는 않는다.

### 3.1 기본 사격

- DrawHold: `R/Animations/Step5/AN_Bow_DrawHold_AA80`, `R/Animations/Step5/AN_Bow_DrawHold_AC80`, 길이 1.0초/24fps.
- Release: `R/Animations/Step5/AN_Bow_Release_AA80`, `R/Animations/Step5/AN_Bow_Release_AC80`, 길이 약 0.666667초/24fps.
- 대응 몽타주: `R/Montages/Step5/AM_Bow_DrawHold_AA80`, `R/Montages/Step5/AM_Bow_DrawHold_AC80`, `R/Montages/Step5/AM_Bow_Release_AA80`, `R/Montages/Step5/AM_Bow_Release_AC80`.
- **기본 사격의 AN 전용 Step5 에셋은 없다.** 현재 선택 그래프는 AN에서도 AC80 DrawHold/Release를 사용한다. 반면 관통/트리플/회피 스킬에는 AN80 전용 에셋이 있다. 기본 공격 주소를 세 활 이름으로 일괄 생성하지 않는다.
- 구매 BasicShot의 당기기+발사+재장전 전체를 현재 LMB 동작 대신 연결하지 않는다.
- 생산 기본 공격의 최소 당김/데미지/속도는 현재 `UGA_PlayerBowAttack`의 값과 별도다. 장전 끝 포즈 유지와 조기 해제 취소를 함께 맞춘다.

### 3.2 Z 관통 / C 트리플

| 단계 | 길이(24fps) | Auto Blend Out | 실행 의미 |
|---|---|---|---|
| Prepare | 0.625초 | Off | 준비 끝 포즈 유지. 준비 중 LMB를 미리 유지했어도 완료 후 Draw로 전환 |
| DrawHold | 0.833333초 | Off | 당긴 뒤 끝 포즈 유지. 전환/취소 콜백과 유지 상태를 구분 |
| Release | 0.666667초 | On | `BowSkillFire`가 2프레임, 약 0.083333초에 있음 |
| Cancel | 전체 1.458333초 | On | 현재 준비/장전 진행률에 대응하는 역방향 위치부터 복귀 |

양쪽 모두 몽타주 Blend In 0.06초, Blend Out 0.12초다. 완전 장전 전 LMB 해제와 RMB 취소는 **0발**이며 화살/VFX를 없애고 Idle로 복귀한다.

| 스킬 | 시퀀스 패턴 | 몽타주 패턴 |
|---|---|---|
| 관통 | `R/Animations/Skills/PiercingHorizontal/AN_Bow_PiercingHorizontal_{단계}_{B}80` | `R/Montages/Skills/PiercingHorizontal/AM_Bow_PiercingHorizontal_{단계}_{B}80` |
| 트리플 | `R/Animations/Skills/TripleShot/AN_Bow_TripleShot_{단계}_{B}80` | `R/Montages/Skills/TripleShot/AM_Bow_TripleShot_{단계}_{B}80` |

관통은 가로 활/뻗은 왼팔과 중립에 가까운 오른손목이다. 회색~청색 제트스트림과 소량 입자, 비행 트레일을 사용한다. 테스트 일반 화살 스케일은 `(0.8,0.8,0.8)`, 관통은 긴 축 Y 기준 `(1.6,1.2,1.6)`이다. 길이 확대 시 실제 메시 촉/후미 위치도 다시 계산한다.

트리플은 기본 사격과 반대 방향의 완만한 기울기다. 정확한 좌우 대칭/수평 활보다 자연스러운 관절과 크로스헤어 노출을 우선했다. 왼손 손가락은 승인된 Idle 그립, 가운데 두 번째 화살은 소유자의 크로스헤어 근처로 정렬한다. 활 중앙 근처에 호로 거치하고 손/활 관통을 피한다. **세 투사체를 같은 발사 이벤트에서 동시에 생성**하며 순차 발사나 별도 트리플 VFX는 없다.

### 3.3 X 사냥꾼의 표식

- 시퀀스: `R/Animations/Skills/HunterMark/AN_Bow_HunterMarkAoE`.
- 몽타주: `R/Montages/Skills/HunterMark/AM_Bow_HunterMarkAoE`.
- 24fps, 0~38프레임, 길이 약 1.583333초. `HunterMarkApply`는 18프레임/0.75초. Blend In 0.04초 / Out 0.12초.
- **지정 범위 안의 적들에게 적용**하는 동작이다. 테스트 값은 반경 400cm, 지속 5초, 조준 Trace 1800cm다. 최종 전투 수치로 확정한 값은 아니다.
- 테스트는 입력 시 크로스헤어 지점을 캡처하고 지면에 투영한다. 사용자는 지점 선정 방식을 더 볼 수 있다고 했으므로 담당자가 범위 중심/서버 검증 규칙을 확정해야 한다.
- 왼팔은 Idle 골격/그립에서 자연스럽게 이동해 조준을 드러내고, 오른손으로 전방 제스처를 한다. 적용 전 취소는 표식/범위 펄스 0개다.
- 테스트 표적은 `BP_Test_HunterMarkTarget`, 범위 펄스는 `BP_Test_HunterMarkPulse`다. `bSpawnHunterMarkTestTargets=True`는 격리 테스트용이므로 생산 적 목록/ASC/팀 판정으로 대체한다.
- 손 입자 `R/VFX/Skills/HunterMark/NS_Test_HunterMarkPalmBurst`는 적용 순간 `hand_r` 위치에 스케일 0.3으로 생성한다. 방향은 캐릭터 `GetBaseAimRotation`의 전방이며 손목 회전에 붙여 뒤로 튀게 하지 않는다.

### 3.4 V 회피 공격 — 최종 지면 스텝샷

- 시퀀스: `R/Animations/Skills/EvasiveStepShot/AN_Bow_EvasiveStepShot_{B}80`.
- **사용할 몽타주**: `R/Montages/Skills/EvasiveStepShot/AM_Bow_EvasiveStepShot_{B}80_LongDraw`.
- 30fps, 0~32프레임, 총 약 1.066667초. 65cm → **97.5cm 후퇴**로 수정했다.
- 루트 이동은 2~15프레임(약 0.066667~0.5초). 발 디딤을 재작성했고 하나 이상의 지지 발을 지면에 남긴다.
- 실제 당기기는 7~19프레임(약 0.233333~0.633333초), **0.4초**다. 완전 장전 후 자동 발사: 23프레임/**0.766667초**. 26~32프레임에 내려 복귀한다.
- 몽타주 Notify: `BowEvasiveRetreat` 0.066667초, `BowEvasiveFire` 0.766667초. Blend In 0.04초 / Out 0.08초, Auto Blend Out On.
- 시퀀스 Root Motion On, Root Lock `AnimFirstFrame`, AnimBP `RootMotionFromMontagesOnly`다. 발 디딤/하체까지 **전체 몸 슬롯 포즈**를 써야 한다. 상체 마스크만 복사하면 몸/발 싱크가 깨진다.
- 현재 테스트 발사는 `BP_Test_BowEvasionTick → TickEvasiveAttack`이 몽타주 위치 `>= 23/30`을 감시하고, `bEvasionFired`로 한 번만 서버 발사한다. 위 Notify만 연결한 구현이라고 오해하지 않는다. 생산 코드에서 Notify/Gameplay Event로 옮긴다면 기존 Tick 발사와 **하나만** 사용한다.
- 점프/`LaunchCharacter` 경로는 현재 테스트에서 제거했다. 루트 모션과 별도 후퇴/Launch를 동시에 적용하지 않는다. 장애물 충돌로 실제 후퇴 거리가 줄어들 수 있다.
- 중복 V는 추가 발사를 만들지 않고, 발사 전 몽타주 중단은 0발이다. 완료/중단 때 active/장전 화살 표시를 정리한다. 공중에서는 발동을 차단한다.
- 이전 `EvasiveAttack` 점프 후보와 접미사 없는 0.8초 `EvasiveStepShot` 몽타주는 최종 사용 대상이 아니다. 이번 커밋에는 최종 3개 시퀀스와 `_LongDraw` 3개 몽타주만 추가했다.

## 4. AnimBP·시위·장전 화살을 함께 이관

### 4.1 `ABP_Test_Bow_Lowered` / 생산 `ABP_Player_Bow`

실제 제작 포즈 위에 기본 사격 IK를 또 얹으면 손목·어깨와 활/화살 구도가 달라진다. `BowSkillPose`로 일반 보정과 스킬 포즈를 분리한 현재 테스트 그래프를 기준으로 이관한다.

| 시퀀스 곡선 | 용도 |
|---|---|
| `BowRaised` | Idle/활을 올린 자세의 기존 IK 가중치. Idle 오른팔은 `1-BowRaised` |
| `BowStringContact` | 오른손 접점에 시위가 붙는 가중치. 발사/표식/Idle에서 불필요한 당김 제거 |
| `BowSkillPose` | Blender 스킬 키와 일반 보정 경로 분리. 표식/관통/트리플/회피에서 사용 |
| `BowTripleGrip` | 트리플의 그립/활 상대 변환 전환. 회피에서는 0 |
| `BowEvasiveFullBody` | V의 전체 몸 슬롯 포즈 선택. 회피 시퀀스에서 1 |

테스트는 DefaultSlot의 전체 포즈를 `BowFullSlotPose`에 캐시한다. 평타/Z/X/C는 기존 `spine_03` 상체 마스크 경로, V는 `EvasionFullBody`로 전체 몸 경로를 선택한 뒤 스킬 조준 경로로 보낸다. 포즈 출력 하나를 두 곳에 직접 분기하지 말고 Save/Use Cached Pose를 유지한다. 별도 기존 `UpperBody` 슬롯도 있으므로 실제 몽타주 슬롯이 연결된 경로를 확인한다.

현재 생산 `/Game/Player/Animations/Bow/ABP_Player_Bow`의 실제 export에는 `BowSkillPose`/`BowEvasiveFullBody` 경로가 없다. 몽타주 주소만 꽂는 것으로 네 스킬 이관이 끝나지 않는다. 현재 생산 AnimBP와 테스트 AnimBP를 비교해 필요한 경로를 옮기고, 생산 캐릭터 변수/무기 컴포넌트로 바꾼다. 테스트 캐릭터 cast를 생산 Pawn에 그대로 남기지 않는다.

### 4.2 활 시위와 장전 화살

`ABP_Test_BowString80`과 `BP_Test_BowDrawArrow`는 해당 캐릭터 자신의 현재 body/anim instance/bow를 읽는다. 생산의 대응 에셋은 `/Game/Player/Animations/Bow/ABP_Bow_String`, `BP_BowHeldArrow`다.

- 몸 평가 → 활/시위 → 장전 화살의 갱신 순서와 tick prerequisite를 유지한다. 생성 전/파괴 후 무기 컴포넌트는 IsValid 검사로 건너뛴다.
- 무기나 CharacterAnimClass를 바꾸면 AnimInstance가 새로 만들어진다. 한 번 캐시한 이전 인스턴스를 계속 사용하지 않는다.
- 소유자 화면 정렬은 `IsLocallyControlled`, 실제 피해/투사체 권한은 Authority다. 리슨 호스트 이외의 원격 서버 Pawn은 Authority여도 로컬 소유자가 아니다.
- 시위 `string_mid`는 활의 실제 평면에서 오른손 접점을 따라 `>`로 당겨진다. 좌표 공간을 혼용하거나 소유자 카메라 위치를 다른 Pawn에 쓰지 않는다.
- 관통의 표면 접점은 `middle_02_r + BowRightVector * 1.3cm`; 일반 사격의 추가 오프셋은 0이다. 각 스킬의 실제 그립/활 상대 변환을 함께 사용한다.
- 소유자 화살촉의 크로스헤어 정렬과 원격의 활 거치를 따로 평가한다. 원격 화살을 카메라 정면에 떠 있는 소유자 배치로 표시하지 않는다.
- 장전 화살은 시각용이다. 트리플은 표시 3개를 관리하고 실제 발사 시 서버 투사체 3개를 별도로 만든다. Idle/취소/발사/스킬 종료에서 장전 표시를 정리한다.
- `BP_Test_BowPresentationTick`은 `UpdateBowPresentation`을 독립적으로 호출한다. 생산 캐릭터는 일부 상태에서 `SetActorTickEnabled(false)`를 쓰므로 캐릭터 Actor Tick만 믿으면 C 장전 중 활이 다시 크로스헤어를 가린다. 생산 컴포넌트/AnimInstance 등 독립 갱신 경로로 이관한다.

## 5. 현재 C++ 클래스별 연동 지점과 차이

아래는 **2026-10-02 현재 소스 확인 결과와 담당자가 이어서 구현할 사항**이다. 이번 문서 작성에서 해당 클래스를 수정하지 않았다.

### `AHBPlayerCharacter`

대상 파일: [HBPlayerCharacter.h](../Source/HeavenBreach/Player/HBPlayerCharacter.h), [HBPlayerCharacter.cpp](../Source/HeavenBreach/Player/HBPlayerCharacter.cpp).

- 현재 `GetEquippedWeaponTag()`가 장착 주무기의 `FWeaponData::WeaponType`을 `Weapon.*` 태그로 바꾸고 기본 공격을 고른다. 새 직업 전환 구조를 만들 필요 없이 이 무기 흐름에 맞춰 연동한다.
- `RefreshWeaponVisual()` → `RefreshCharacterAnimClass()`는 무기 표시 행의 SkeletalMesh/AnimClass/Socket/CharacterAnimClass를 읽는다. AnimBP 교체가 진행 중 몽타주를 끊으므로 무기 변경 시 스킬 종료·화살 표시·VFX·타이머도 정리한다.
- 연결 인터페이스는 `GetWeaponPresentation()`, `GetWeaponSkeletalMesh()`, `IsBowDrawing()`, `SetBowDrawing()`, `GetHeldArrowMesh()`, `ComputeAimPoint()`다.
- C++ 상태명은 `bDrawingBow`, 테스트 자식 BP에는 `bBowDrawing`이 있다. 동명 native 변수를 추가해 BP 변수가 자동 개명되는 충돌을 피한다.
- `HeldArrowActorClass`는 생산 장전 표시 액터이며 전용 서버를 제외한 각 머신에서 로컬로 생성한다. 이것을 실제 발사체로 사용하지 않는다.
- 영향을 받는 연결: Equipment/표시 DataTable/AnimBP/활 GA. 테스트: 무기 교체 중 장전·취소·재장전, 로컬/원격의 표시와 새 AnimInstance 조회.

### `FHBWeaponPresentation` / `UHBPlayerAnimInst`

대상 파일: [HBWeaponPresentation.h](../Source/HeavenBreach/Player/HBWeaponPresentation.h), [HBPlayerAnimInst.h](../Source/HeavenBreach/Player/Animations/HBPlayerAnimInst.h), [HBPlayerAnimInst.cpp](../Source/HeavenBreach/Player/Animations/HBPlayerAnimInst.cpp).

- 현재 표는 `/Game/Player/Animations/Bow/DT_WeaponPresentation`. 아이템 행 이름 우선, 없으면 WeaponType 행을 사용한다.
- `FHBWeaponPresentation`에는 메시/시위 AnimBP/소켓/캐릭터 AnimBP/기본 Draw·Release·Cancel 몽타주/HeldArrowMesh/StringIKOffset이 있다. **Prepare 및 네 스킬별 전체 몽타주 묶음 필드는 현재 없다.** 기본 사격 필드에 관통/트리플을 덮어 넣지 말고 담당자의 스킬별 데이터/GA 설정으로 분리한다.
- `UHBPlayerAnimInst::NativeUpdateAnimation()`은 `bBowDrawing`, `WeaponSkeletalMesh`, `StringIKOffset`을 캐릭터/표시 행에서 읽는다. 테스트의 `BowVisual` 조회를 이 인터페이스에 맞춰 옮긴다.
- 현재 AimPitch는 소유자/컨트롤러가 있는 서버는 즉시값, 컨트롤러 없는 원격은 `GetBaseAimRotation`과 보간을 사용한다. 스킬 조준 경로와 카메라 흔들림 강도를 함께 확인한다.
- 영향: 생산 몸/시위 AnimBP, 표시 화살. 테스트: 세 활 모델, 상하 조준, 걷기·달리기·웅크림, 원격 조준 추종.

### `UGA_PlayerBowAttack`

대상 파일: [GA_PlayerBowAttack.h](../Source/HeavenBreach/Combat/Attack/GA_PlayerBowAttack.h), [GA_PlayerBowAttack.cpp](../Source/HeavenBreach/Combat/Attack/GA_PlayerBowAttack.cpp).

- 현재 평타는 LocalPredicted, 표시 행 몽타주 우선/GA 값 폴백, 서버가 잰 당김 시간으로 `HBBow::ComputeShot()` 후 서버 투사체를 만든다. `AHBPlayerCharacter::StopAttack()`이 InputReleased를 전달한다.
- `WaitInputRelease(this, false)`를 사용한다. 현재 프로젝트 입력 방식에서 true는 시작 즉시 해제로 판정됐던 문제와 관련된다.
- 현재 기본 공격은 `OnReleased()`에서 **즉시 발사 후 Release 몽타주**를 재생한다. Z/C의 `BowSkillFire` 지연 발사와 같은 동작이라고 간주하지 않는다. 스킬로 재사용할 때는 서버 발사를 의도한 이벤트로 옮기고 이중 발사를 막는다.
- 스킬 상태에는 준비/장전 완료/취소와 발사 1회 보호가 추가로 필요하다. 회피는 입력 해제 대기 없이 자동 발사하고, 트리플은 한 발 발사 함수만 재생해서는 부족하다.
- 영향: ASC 입력 이벤트, 장전 상태, 실제 ArrowClass/피해/탄약. 테스트: 조기 해제 0발, 정상 평타 1발, 종료·중단 뒤 상태 태그/감속 GE/표시 제거.

### `UGA_ArcherEvasiveShot` / `UGA_ArcherPiercingArrow` / `UGA_ArcherMultiShot` / `UGA_ArcherTrackingMark`

대상 파일: [GA_ArcherAbilities.h](../Source/HeavenBreach/Player/Archer/Abilities/GA_ArcherAbilities.h), [GA_ArcherAbilities.cpp](../Source/HeavenBreach/Player/Archer/Abilities/GA_ArcherAbilities.cpp).

| 클래스 | 현재 소스의 ActivateAbility | 완성 모션과 맞추기 위해 필요한 연결 |
|---|---|---|
| `UGA_ArcherEvasiveShot` | 후방 `LaunchCharacter(700)` + 즉시 Line 피해 + 종료 | 지면 Root Motion/전체 몸 몽타주, 0.766667초 자동 1발, 완료/중단 정리. 별도 Launch와 중복 이동 금지 |
| `UGA_ArcherPiercingArrow` | 즉시 LineTargets 피해 ×1.4 + 종료 | Prepare/Draw/Hold/Release/Cancel 상태, 서버 발사 이벤트와 관통 판정/투사체/VFX |
| `UGA_ArcherMultiShot` | 즉시 ConeTargets 피해 ×0.8 + 종료 | 선택한 활에 맞는 트리플 모션/그립/3개 장전 표시, 하나의 서버 발사에서 3개 실제 투사체 또는 담당자가 정한 동등한 판정 |
| `UGA_ArcherTrackingMark` | LineTarget 한 대상에 8초 Tracked + 종료 | 지정 범위 여러 대상, 0.75초 적용 시점, 실제 적/ASC/팀 판정, 유지/해제. 테스트 5초와 현재 코드 8초의 수치 합의 필요 |

이 클래스들은 현재 새 몽타주를 기다리는 구현이 아니다. Ability가 즉시 끝난 채 모션만 별도로 틀면 취소/쿨다운/피해 시점이 어긋날 수 있다. 기존 `BeginAbilityWithCooldown`/`FinishAbility`와 Montage Task/Gameplay Event의 수명을 담당자가 맞춘다. 비용/쿨다운/탄약/관통 대상별 중복 피해/표식 중첩은 전투 담당 범위다.

테스트: 해당 스킬이 실제 장착 무기와 선택한 스킬 슬롯에서 발동하는지, 모션 이벤트 이전에는 피해/표식이 없는지, 취소·사망·경직·무기 교체에서 새 피해가 생기지 않는지 확인한다. `Z/X/C/V`는 테스트 편의 키이며 생산 스킬 슬롯 입력과 별도로 연결한다.

## 6. 동기화와 이벤트 처리 계약

현재 테스트의 기본 요청은 `RequestBowAction → ServerBowAction → MulticastBowAction → PlayBowAction`이다. 표식은 `ServerHunterMark/MulticastHunterMark`, 회피는 `ServerEvasiveAttack/MulticastEvasiveAttack`의 reliable 요청/표시 경로가 있다. 중복 입력·상호 배타 상태와 서버 1회 발사 검사를 포함한다.

생산 GAS로 옮길 때는 기존 GAS 몽타주 복제/예측과 이 테스트 Multicast를 함께 남겨 **몽타주를 두 번 시작하지 않는다.** 복제할 것은 스킬 종류, 단계/시작 시간, 선택 활, 확정 조준/범위 중심 등 필요한 상태이며, 각 화면의 장전 화살/시위는 그 Pawn의 실제 포즈에서 계산한다.

| 발생 시점 | 서버 게임 처리 | 각 머신 표시 |
|---|---|---|
| 스킬 준비/Draw 시작 | 발동 가능·무기·비용/상태 검사 | 해당 몽타주, 그립/화살 표시 |
| Z/C `BowSkillFire` | 활성 스킬/발사 1회 검사 후 1발/3발 | 장전 화살/VFX 정리, Release 복귀 |
| X `HunterMarkApply` | 확정 범위의 유효 적들에게 표식 | 전방 손 입자, 범위 펄스, 대상 표식 |
| V 23/30초 이벤트 | 유효 스킬에서 자동 1발 | 장전 화살 숨김, 반동/Idle 복귀 |
| 취소/중단/종료 | 더 늦은 콜백/중복 발사 차단, 상태/타이머 정리 | 몽타주/화살/VFX/그립/활 상대 변환 복구 |

소유자와 서버 양쪽에서 Notify가 발생할 수 있으므로 gameplay 발사는 서버 한 경로로 제한한다. 몽타주 인스턴스/발동 식별자나 1회 플래그로 중복을 막고, 중단된 과거 동작의 Notify가 새 발동에 적용되지 않도록 한다. 발사 궤적은 크로스헤어 조준점 방향이며 화면 표시 화살의 상대 변환만으로 피해 방향을 결정하지 않는다.

## 7. 담당자 이관 순서와 확인 방법

1. `4ac2e8b5` 에셋을 받은 뒤 `/Game/Levels/test_gm/BowRetarget/L_Test_BowRetarget`에서 승인된 기본 사격/Z/X/C/V를 먼저 재현한다.
2. 생산의 현재 `DT_WeaponPresentation`, `ABP_Player_Bow`, `ABP_Bow_String`, `BP_BowHeldArrow`, `UGA_PlayerBowAttack` 연결을 유지하며 Idle IK·스킬 포즈/조준·트리플 그립·V 전체 몸 경로를 이관한다.
3. 네 스킬별 몽타주 데이터와 입력 의미를 연결하고, 서버 피해/표식/발사 이벤트를 하나씩 붙인다. 기존 즉시 피해와 새 투사체/Notify 피해를 동시에 남기지 않는다.
4. 종료/취소/중단/무기 교체 시의 정리부터 확인한다. 그 다음 비용/쿨다운/탄약·실제 적 판정을 연결한다.
5. 세 활에서 소유자/원격의 몸·손·활·시위·화살·크로스헤어를 비교한다. 기본 사격, 이동 장전, 웅크림과 상하 조준이 유지돼야 한다.
6. C 장전 중 이동/재시작 이후에도 활이 크로스헤어를 가리지 않는지, 3발이 동시에 한 번만 나가는지 확인한다. V는 점프 없이 약 97.5cm 물러나 1발 후 복귀해야 한다.
7. Players=2 / Listen Server에서 각각 6/7/8 활을 선택해 두 플레이어 모두 스킬/취소/중복 입력을 시험한다. 이어 전용 서버/다른 PC/지연/late join을 담당자 환경에서 별도 검증한다.

최신 V는 2인 한 프로세스 리슨서버에서 세 활 모두 자동 1발/중복 V 방지/중단 전 0발/Idle 복귀를 확인했다. 캡슐 높이 변화와 Falling은 없었다. 후퇴는 대부분 97.5cm, 캡슐 접촉/슬라이딩이 있던 AC 호스트 표본은 약 97.01cm였다. Z/X/C는 각 제작 마무리 때 사용자 육안 확인과 별도 리슨서버/취소 검사를 했다. 모든 생산 기능의 이번 재검증을 의미하지 않는다.

이번 제출 전 관련 테스트 BP 5개를 컴파일·저장했고 컴파일 노드 오류와 미저장 Content는 없었다. C++ 변경이 없어 새 Development Editor 빌드는 하지 않았다. 생산 C++ 수정 후에는 저장소 CONTRIBUTING/AGENTS의 Editor 빌드와 PIE 확인을 수행한다. 모든 의상/보간 프레임에서 관통이 없다는 보장이나 실제 GAS 피해 통합 완료로 해석하지 않는다.

## 8. 추가 모션 제작을 맡을 경우

완성한 Content 에셋은 저장소에서 사용할 수 있다. `.blend`/FBX/평가 키 JSON은 로컬 SourceArt/Saved에만 있으며 Git으로 공유한 것으로 간주하지 않는다. 추가 제작 담당자는 필요한 소스 파일을 별도 전달받는다.

| 모션 | 현재 로컬 제작 소스 |
|---|---|
| 관통 | `SourceArt/Animations/PiercingFix_v4/HB_PiercingFix_v4.blend` |
| 표식 | `SourceArt/Animations/HunterMarkAoE_v2/HB_HunterMarkAoE_ClearAim_v2.blend` |
| 트리플 | `SourceArt/Animations/TripleShot_v1/HB_TripleShot_LeftCant_v1.blend`, 최종 ShoulderAngle_v4 |
| 회피 | `SourceArt/Animations/EvasiveStepShot_v2/HB_Bow_EvasiveStepShot_v2.blend`, 최종 LongDraw_v3 키 |

자연스러운 어깨·팔꿈치·손목과 고정 뼈 길이를 우선하고, 실제 Blender Action을 평가한 키를 같은 UE 골격에 반영했다. UE 반영은 JSON + Animation Data Controller였으며 모든 작업을 FBX 재임포트로 처리한 것은 아니다. 공유 skeleton/본 길이/스케일을 바꾸거나 폐기된 Idle/점프 후보를 다시 가져오지 않는다. 시퀀스 길이를 바꿀 때 몽타주 segment 길이, 발사 이벤트, 곡선과 프레임레이트를 함께 검증한다. 회피는 30fps, 기존 다른 스킬은 24fps다.
