# 궁수 애니메이션 작업 인계 — 2026-09-30 최종 기준

이 문서는 왼손 활대/오른손 시위로 바꾼 궁수 애니메이션, 장전 유지·발사, 화살/시위 정렬, 이동 안정화와 마지막 왼팔 시야 보정의 현재 적용 상태를 정리한다. 다음 궁수 작업은 이 문서부터 읽는다. 제출 브랜치는 `feat/BattleSystem`이다.

`Docs/BOW_RETARGET_TEST_2026-09-29.md`의 최초 리타겟 롤백 상태는 과거 기록이다. 해당 문서의 오른손 활대, 자동 BasicShot, 화살·네트워크 미구현 설명을 현재 상태로 복원하지 않는다.

## 1. 유지할 방향과 현재 상태

- 게임 기획은 직업 선택보다 **장착 무기에 따른 공격/사용 가능 스킬**이다. 활은 궁수 공격, 스태프는 마법 공격을 사용하는 방향이며, 이번 작업에서 직업 전환 DataAsset 구조를 새로 도입하지 않았다.
- 하나의 몸 메시와 머리 카메라를 사용한다. 별도 1인칭 팔 메시로 이관한 작업이 아니다.
- 활대는 왼손, 시위는 오른손이다. 각 캐릭터 자신의 `hand_l/BowSocket`에 활을 부착한다.
- Idle에서는 검지·엄지 쪽 손 일부가 화면 아래에서 살짝 보이고 화살은 없다.
- 왼쪽 마우스를 누르면 장전/당기기, 계속 누르면 완전히 당긴 자세 유지, 놓으면 발사/복귀한다. 자동 재장전은 현재 입력 흐름에 연결하지 않았다.
- 장전 중 화살 깃은 오른손 시위 접점, 화살촉은 크로스헤어 근처에 놓인다. 화면 기준 오른쪽 아래에서 중앙으로 약 45도인 화살 구도를 유지한다.
- 시위는 활대가 놓인 실제 평면에서 당겨진다. 카메라 X 방향만 늘려 옆으로 꺾던 이전 보정은 대체되었다.
- 마지막 작업에서 장전 중 왼쪽 팔꿈치를 낮춰 시야 가림을 줄였고, 시작 시 `BowVisual` None 접근 오류를 수정했다. 사용자가 최종 화면을 확인한 작업본이다.

## 2. 주요 에셋과 역할

테스트 루트 `R = /Game/Levels/test_gm/BowRetarget`.

| 에셋 | 역할 / 확인할 내용 |
|---|---|
| `R/L_Test_BowRetarget` | 현재 테스트 맵. 활 선택·장전·발사·상하 조준·이동·리슨서버 확인 |
| `R/BP_Test_BowCharacter` | 캐릭터별 활 부착/선택, `bBowDrawing`, 몽타주, 시각 동기화, 테스트 화살/투사체 |
| `R/BP_Test_BowController` | 실제 LMB 누르기/놓기 입력 및 숫자 키 테스트 연결 |
| `R/BP_Test_BowGameMode` | 테스트 캐릭터/컨트롤러 설정 |
| `R/DA_Test_BowPresentation` | 테스트 표시 설정. 런타임에서 AnimClass를 다시 적용하므로 캐릭터와 함께 맞춰야 함 |
| `R/ABP_Test_Bow_Lowered` | 현재 몸 AnimBP. 낮은 Idle, 장전 중 양팔 IK, 이동 안정화, 왼팔 시야 보정, 초기화 검사 |
| `R/ABP_Test_BowString80` | 활 자체 AnimBP. 몸의 접점/곡선을 읽어 시위 중앙 뼈 제어 |
| `R/BP_Test_BowDrawArrow` | 캐릭터별 장전 표시용 화살. Idle/발사 후 숨김, 실제 메시 길이에 따른 화살촉 정렬 |
| `R/BP_Test_BowArrowProjectile` | 서버에서 생성하는 복제 테스트 투사체. 소유자 무시, 팁 충돌, 이동/수명 |
| `R/Animations/BS_Bow_UpperLocomotion` | Step2 Idle와 수정한 걷기/달리기 상체. 웅크린 상체에도 사용 |
| `R/Animations/Step2/AN_Bow_Idle_LeftHand_Step2_ArmSpace` | 양팔 손 역할을 바꾼 뒤 겨드랑이 공간을 확보한 Idle 원본 |
| `R/Animations/Step2/Locomotion/` | 같은 방식으로 보정한 방향별 걷기/달리기 |
| `R/Animations/Step2/Attack/AN_Bow_BasicShot_LeftHand_ArmSpace` | 초기에 연결한 전체 BasicShot. 현재 LMB 유지/놓기 흐름은 Step5를 사용 |
| `R/Animations/Step5/` + `R/Montages/Step5/` | 현재 AA/AC의 DrawHold와 Release 분리 에셋 |

### 실제 테스트 활

루트: `/Game/Asset/MMORPG_Characters/IdaFaber/Meshes/Scaled80`.

- `SK_AA_BOW_Scale80`
- `SK_AC_BOW_Scale80` — 기본 선택
- `SK_AN_BOW_Scale80`

Blender에서 원본 메시·골격을 XYZ 0.8로 베이크하고 별도 에셋으로 임포트했다. 원본 구매 메시의 재스케일이 아니다. AA에서 생성한 축소 골격을 공유하며 AA/AC는 별도 PhysicsAsset이 있다. AN은 전용 PhysicsAsset이 없고, 동작/화살은 AC fallback을 사용한다. AN을 AA/AC와 동일한 수준으로 검증한 전용 모션으로 간주하지 않는다. 이후 테스트에서 검증한 BowVisual 변환/스케일은 현재 BP 연결을 기준으로 유지하고 완성된 리그를 다시 0.8배 하지 않는다.

## 3. 입력, 장전/발사와 시각 동기화

| 입력 | 현재 동작 |
|---|---|
| LMB 누르기 | `RequestBowAction(1)`로 선택 활의 DrawHold 시작 |
| LMB 유지 | 몽타주 마지막 장전 자세 유지, `BowRaised/BowStringContact` 유지 |
| LMB 놓기 | `RequestBowAction(10)`로 Release, 표시 화살 숨김, 서버 테스트 투사체 생성 |
| 6 / 7 / 8 | AA / AC / AN 축소 활 선택 |
| WASD / 마우스 | 이동 / 시점 |
| 기존 Crouch 입력 | 웅크리기와 웅크린 이동. 하체는 기존 Crouch, 상체는 활 locomotion |

숫자 1~5는 과거 개별 클립 진단용이다. 현재 장전 유지/발사를 검증하는 주 입력은 LMB다.

`bBowDrawing`으로 중복 누르기/놓기를 처리한다. DrawHold는 24fps/1초이며 auto blend-out을 꺼 마지막 포즈를 유지한다. 별도 호흡 루프를 만든 것은 아니다. Release는 약 0.667초이며 복귀 후 낮은 Idle로 돌아온다. 당김 구간은 UE 0-based 12~24프레임(0.5~1초)이다.

시각 상태는 기존 `RequestBowAction`의 소유자 재생 + reliable server/multicast 경로를 사용한다. 양쪽 캐릭터가 각자의 활/몸/현재 AnimInstance를 사용해야 한다. 로컬과 원격의 판단에 `HasAuthority`만 사용하지 않는다.

표시 화살과 실제 투사체는 별도 액터다. 서버는 놓기 때 표시 화살촉 위치를 재구성해 카메라 trace의 `TestBowAimPoint`로 발사한다. 투사체는 복제 이동, 약 5000cm/s, 중력 0, 팁 Sphere 충돌, 소유자 무시, 약 5초 수명을 가진 테스트다. 피해/GAS/탄약/인벤토리/프로덕션 장비 권한 로직을 구현한 작업이 아니다. 선택/장전 상태의 late join 또는 relevancy 복귀 재생은 지속 상태 복제까지 보장하지 않는다.

## 4. 현재 양팔·시위 보정의 핵심

### `ABP_Test_Bow_Lowered`

- 낮은 Idle의 기본 손 구도를 유지하고 `BowRaised`로 장전 보정을 켜고 복귀 때 푼다.
- `Draw45RightArmIK`/오른손 방향 보정으로 팔 전체를 자연스럽게 시위 축을 따라 당긴다. 화살에 맞추기 위해 손을 따로 움직이는 방식이 아니다.
- `spine_03` 회전만 자세별 목표로 안정화하고 RInterpTo(18)로 전환한다. 양손/팔꿈치 목표를 `spine_03` Bone Space로 바꿔 머리 카메라 위치를 한 프레임 늦게 따라가던 이동 중 떨림을 줄였다. 척추 위치 고정/FABRIK/IK 스트레칭은 최종 구현에 없다.
- 왼손의 위치·회전으로 활대, 화살, 시위가 같은 평면을 사용하게 맞췄다. AA/AN의 서로 다른 기본 시위 위치는 같은 당김 축을 따라 보정한다.
- 마지막 보정: `left_pole_world`를 `(0,-65,25)` → `(10,-45,-15)`cm(카메라 공간)로 낮췄다. 왼손의 활 잡는 위치/방향은 유지한다.
- `PlaneLeftShoulderSpace`는 장전 중 왼쪽 견갑대의 메시 컴포넌트 X +4cm, 본 공간 Pitch +4도를 적용한다. 팔꿈치를 낮춰도 가슴/겨드랑이 표면과 겹치지 않도록 확보한 공간이다. 팔 본 길이/스케일은 유지한다. Idle에서는 곡선 가중치 0이다.
- 시작 오류 해결: 성공한 테스트 캐릭터 캐스트 다음에 `IsValid(BowVisual)` 분기를 두어 생성 전/파괴 후 손 목표 계산을 건너뛴다. 준비되면 다음 tick부터 정상 갱신한다. 기존 오류는 `Set Draw45RightWrist`의 pure 무기 메시 조회가 아직 없는 BowVisual을 읽는 것이었다.

### `ABP_Test_BowString80`

- 몸이 먼저 평가되도록 tick prerequisite를 유지하고 실제 `middle_02_r` 접점과 `BowStringContact`를 읽는다.
- `string_mid`의 활 로컬 평면은 X=0, 당김 방향은 -Y다. `ReferenceNockHeight`로 장전 높이 Z=-8cm를 먼저 맞춰 위아래로 미끄러지지 않게 한다.
- 목표를 자신의 컴포넌트 공간으로 변환하고 곡선으로 시위 중앙에 적용한다. 표시 화살의 깃도 실제 접점을 사용한다.

### 최종 숫자 기준

벡터의 좌표계를 바꿔 읽으면 안 된다. 현재 그래프 export/핀 입력이 최종값이다.

| 항목 | 값 / 좌표계 |
|---|---|
| 왼손목 | `(27.5164,-2.8859,-9.4094)`cm / 카메라 |
| 왼팔 팔꿈치 목표 | `(10,-45,-15)`cm / 카메라 |
| 완전 장전 시위 접점 | `(13,7.8,-7.8)`cm / 카메라 |
| 준비 시위 접점 | `(20.8538,6.7236,-6.7236)`cm / 카메라 |
| 당김 거리 | AC 기준 8cm / 월드 거리 |
| 오른팔 유지 팔꿈치 목표 | `(0,35,-15)`cm / 카메라 |
| 오른팔 준비 팔꿈치 목표 | `(20,35,-5)`cm / 카메라 |
| 시위 접점 높이 | Z=-8cm / 활 컴포넌트 |

Translation/Rotation 핀이 노출된 ModifyBone은 노드 구조체의 표시값보다 **핀 DefaultValue**를 확인한다. 재시작 후에도 `PlaneLeftShoulderSpace`의 Translation `(X=4,Y=0,Z=0)`과 Rotation `4,0,0`, 초기화 분기가 유지되는 것을 확인했다.

## 5. 검증 결과와 제한

- AA/AC 2인 리슨서버, 한 프로세스의 양쪽 소유자와 상대 복제 캐릭터를 검증했다. 서기/웅크리기, 상하 조준(-40~40도 또는 웅크림 ±30도), 이동 장전, 짧은 클릭과 놓기를 포함한다.
- 마지막 v8 검사: 실제 몸 포즈 337개, 캐릭터 관측 1,348개. 오른팔 관통 0, **완전히 장전 유지 중 왼팔 관통 0**, 크로스헤어 중심/주변 12px 손 가림 0.
- 왼팔 화면 점유 약 16.2% → 10.7%(약 34% 감소), 팔꿈치 높이는 카메라 기준 약 7.7cm 낮아졌다. 1440x900/FOV90의 실제 메시 광선 격자 검사에 따른 근사치다.
- 완전 장전 화살의 화면 각도 약 44.73~45.35도. 정지/이동 샘플의 화살촉 중심 오차 <0.00001px, 화살 깃 접촉 오차 <0.000001cm. 보간 타이밍과 모든 시점에서의 보편적 보증으로 해석하지 않는다.
- 추가 좌우 이동/뒷걸음/웅크린 이동에서 장전 접점 화면 떨림 표준편차 <0.0001px. 이동하며 시점을 계속 돌리는 이전 별도 검사에는 tick 순서에 따른 화살 표시 갱신 오차가 기록되어 있으므로 극단적 회전은 육안 확인을 유지한다.
- 수정 이후 새 PIE 시작·장전/발사·종료 로그에서 블루프린트 런타임 오류/LogScript 경고 0건. 다른 팀 시스템의 SkillLoadout/입력 매핑 일반 로그까지 모두 해결했다고 주장하지 않는다.
- 기존 왼팔 Idle/전환 일부 표면 겹침은 남는다. 모든 프레임/의상에서 양팔 관통 0이라고 주장하지 않는다. 메시 검사는 해부학적으로 연결된 어깨 9cm와 혼합 웨이트 경계를 제외한 유한 표본이다.
- 제출 시 테스트 에셋 78개, Blueprint/AnimBP 9개 컴파일·저장, 누락된 `/Game` 패키지 의존성 0, dirty content package 0, PIE Idle. C++ 변경이 없어 별도 C++ 빌드는 이 제출에서 수행하지 않았다.

### 팀원이 직접 재현하기

1. 위 테스트 맵을 열고 2인 Listen Server PIE를 시작한다.
2. 각 창에서 6/7로 AA/AC를 선택하고 Idle에서 화살이 없는지 본다.
3. LMB를 유지해 장전 완료 뒤에도 자세/화살/시위가 유지되는지 본다.
4. WASD, 웅크리기, 상하 조준으로 시위 접촉, 낮은 왼팔, 크로스헤어 주변 시야를 확인한다.
5. LMB를 놓아 투사체가 크로스헤어 방향으로 나가고 낮은 Idle로 복귀하는지 본다.
6. 반대 창에서 각 캐릭터 자신의 활과 양팔 동작을 확인한다. 출력 로그에서 BowVisual None 오류가 없는지 확인한다.

## 6. 로컬 증거와 다음 작업 주의점

아래 Saved/SourceArt는 로컬 진단/제작 자료이며 저장소에 커밋하지 않는다. 공유 가능한 현재 결과와 재현법은 이 문서와 Content 에셋이다.

| 로컬 경로 (`Saved/BowRetargetTest/` 아래) | 읽을 내용 |
|---|---|
| `HeldLeftArm_v8/` | **마지막 기준**. README, config, 실제 그래프, verification, visibility_comparison, motion_summary, log_verification, 화면 |
| `BowPlane_v7/` | 활 평면 정렬/시위 높이의 기반. 팔꿈치/어깨/초기화는 v8로 대체 |
| `DrawMoveStability_v5/` | 현재 Bone Space 이동 안정화 이유와 구현. 폐기한 위치 고정 후보는 재적용하지 않음 |
| `HoldReleaseCrouch_v1/` | 현재 유지/놓기 입력, Step5, 웅크린 상체 연결 |
| `RightIdleClearance_v1/` | 낮은 Idle 오른팔의 clavicle 기준 IK |
| `BowScale80/` | 활 3종 축소/임포트/round-trip 검증 |
| `BasicShotArmSpace/`, `LocomotionArmSpace/`, `IdleArmSpaceStep2/` | 승인된 단계별 리타겟/보정 소스 |
| `Submission_20260930/` | 제출 당시 그래프와 패키지 의존성 확인 |

Blender 소스는 `SourceArt/Animations/`의 단계별 폴더와 `SourceArt/Meshes/BowsScale80/`에 있다. JSON과 Unreal animation data controller로 바디 골격 재바인딩 없이 키를 반영한 방식이다.

`BlenderMirror`, 최초 Idle2/RightHanded 실패본, 카메라 중앙에 뜨던 화살, `StringAngle_v6`의 잘못된 축 당김, 임시 spine 위치 고정 후보는 현재 기준이 아니다. 과거 생성/임포트/노드 구성 스크립트는 일회성 이력이며 재실행으로 승인된 포즈를 덮어쓰지 않는다.

현재 제출에는 테스트 Content 전체와 Scaled80 활, 공유 `SKEL_UE5_M`/`SK_HB_Warrior`의 기존 로컬 변경을 함께 포함한다. 마지막 v8 보정은 테스트 AnimBP만 수정한 것이며 공유 바이너리의 기존 변경을 v8에서 새로 작성한 것으로 오해하지 않는다. 팀원의 생산 C++/GAS, 무기별 스킬 권한/피해 통합은 후속 담당 범위다.
