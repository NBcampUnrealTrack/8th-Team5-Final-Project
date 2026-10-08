# 궁수·전사 VFX 리메이크 코드 연동 인계

작성일: 2026-10-08. 게임 공개명: **Deep & Deeper**, 팀: **레테 / LETHE**.

궁수 관통 사격·사냥꾼의 표식과 전사 전투의 함성·전투 본능·피의 갈증의 최종 VFX 연결 규칙을 정리한다. 궁수 리메이크 에셋은 `39f63f489156f9ac270a4e2eaae22c3be32a0155`에 제출되어 최신 develop에 포함되어 있다. 이번 추가 에셋 제출은 전사 3종과 테스트 맵 연결이다. 이 인계 문서는 사용자 요청으로 제출하며 다른 로컬 기록은 제출하지 않는다.

## 1. 담당자가 먼저 확인할 사항

- 생산 C++/GAS의 피해·자원·쿨타임·버프 판정을 이번 VFX 작업에서 변경하지 않았다. 해당 판정 결과에 표시를 연결한다.
- Niagara 컴포넌트 생성 자체는 네트워크 복제를 보장하지 않는다. 서버가 확정한 이벤트/상태를 각 클라이언트에서 표현한다. 리슨 서버의 로컬 표현도 한 번만 실행한다.
- 몽타주 Notify와 서버 타이머 양쪽에서 같은 발사·적용을 호출하지 않는다. 이벤트의 실행 주체를 하나로 정한다.
- 아래 변환은 **현재 테스트 BP의 컴포넌트 기준 상대 변환**이다. 다른 부모 컴포넌트에 숫자만 복사하면 방향/크기가 달라진다. 부모·메시 축·스케일을 함께 가져오거나 새 구조에서 재계산한다.
- 원본 `/Game/Asset/Mixed_Magic_VFX_Pack`의 머티리얼·텍스처·Niagara 모듈 의존성이 남아 있다. 파생 시스템 하나만 파일로 복사하지 말고 Unreal의 Migrate로 의존성을 함께 가져간다.

궁수 애니메이션, 발사 타이밍, 활 3종, owner/remote 조준 규칙은 [기존 궁수 인계 문서](BOW_ANIMATION_CODE_HANDOFF_2026-10-02.md)를 함께 참고한다. 이 문서는 그 문서의 **VFX 연결 부분을 갱신**하며 애니메이션·IK·Aim Offset·소켓을 교체하는 문서가 아니다.

## 2. 최종 에셋 경로

아래 표에서 `B = /Game/Levels/test_gm/BowRetarget`, `P = B/VFX/Skills/PiercingShot/MixedMagic`, `H = B/VFX/Skills/HunterMark/MixedMagic`이다. `B`, `P`, `H`는 문서의 경로 약칭이다.

| 효과 | 최종 에셋 | 현재 연결 위치 |
|---|---|---|
| 관통 장전 입자 | `P/NS_Archer_Piercing_Charge` | `B/BP_Test_BowDrawArrow` 및 관통 투사체의 `PiercingWindParticles` |
| 관통 화살 주변 제트스트림 | `P/Materials/M_Archer_Piercing_Jetstream` | 두 BP의 Static Mesh 컴포넌트 `PiercingJetstream` |
| 관통 비행 입자 | `P/NS_Archer_Piercing_Flight` | `B/Skills/PiercingShot/BP_Test_PiercingArrowProjectile`의 `PiercingPackFlight` |
| 관통 비행 트레일 | `P/NS_Archer_Piercing_Ribbon` | 같은 투사체의 `PiercingFlightTrail` |
| 표식 시전자 손 입자 | `H/NS_Archer_HunterMark_Cast` | `B/BP_Test_BowCharacter`의 기존 적용 시점 생성 노드 |
| 표식 범위 펄스 | `H/NS_Archer_HunterMark_Survey` | `B/Skills/HunterMark/BP_Test_HunterMarkPulse`의 `HunterSurvey` |
| 표식 대상 아이콘 | `H/NS_Archer_HunterMark_Target` | `B/Skills/HunterMark/BP_Test_HunterMarkTarget`의 `HunterMarkAura` |
| 전투의 함성 | `/Game/VFX/Warrior/Systems/WarCryEffect/NS_HB_WarCry_Remake` | `L_NiagaraTest`의 함성 스테이션, `BP_NiagaraThirdPersonCharacter.PreviewSystem` |
| 전투 본능 | `/Game/VFX/Warrior/Systems/PassiveEffect/NS_HB_CombatInstinct_Aura_Remake` | `L_PassiveVFXTest`의 `CombatInstinct_Loop` |
| 피의 갈증 | `/Game/VFX/Warrior/Systems/PassiveEffect/NS_HB_BloodThirst_Remake` | 같은 맵의 `BloodThirst_TestStation.EffectSystem` |

전사 전용 부모 머티리얼/인스턴스는 `/Game/VFX/Warrior/WarCry/MixedMagic/Materials` 및 `/Game/VFX/Warrior/Passives/MixedMagic/Materials`에 있다. 기존 이름의 `NS_HB_WarCry`, `NS_HB_CombatInstinct`, `NS_HB_BloodThirst`는 보존했다. 생산 코드의 기존 참조는 담당자가 위 최종 경로로 교체해야 한다.

## 3. 궁수 관통 사격

### 표현과 현재 컴포넌트 설정

회색~청색의 넓고 끝이 좁아지는 두 줄 나선 기류가 화살을 감싼다. 팩 `M_Core` 기반 제트스트림 머티리얼이 `T_Noise_14`로 투명한 빈틈과 흐름을 만들며 Niagara 입자·리본이 비행 표현을 보완한다. 제트스트림은 **Niagara 시스템이 아니라 Static Mesh의 머티리얼**이므로 Niagara만 연결하면 현재 외형이 재현되지 않는다.

| 컴포넌트 | 부모 기준 위치(cm) | 상대 회전(Pitch/Yaw/Roll) | 상대 스케일 | 자동 활성화 |
|---|---|---|---|---|
| `PiercingJetstream` | `(0,-18,0)` | `(-90,90,0)` | `(1,1,1)` | 관통 상태에 따라 메시 표시 |
| 장전 화살 `PiercingWindParticles` | `(0,24,0)` | `(0,0,0)` | `(0.08,0.08,0.08)` | Off |
| 투사체 `PiercingWindParticles` | `(0,24,0)` | `(0,0,0)` | `(0.08,0.08,0.08)` | On |
| 투사체 `PiercingPackFlight` | `(0,12,0)` | `(0,0,0)` | `(0.12,0.12,0.12)` | On |
| 투사체 `PiercingFlightTrail` | `(0,-20,0)` | `(0,0,0)` | `(1,1,1)` | On |

`PiercingJetstream`의 Bounds Scale은 2.6이다. 위 값에는 별도로 확대된 관통 화살 메시/부모 변환이 영향을 준다. 기본 화살은 `(0.8,0.8,0.8)`, 관통 화살은 긴 축 Y 기준 `(1.6,1.2,1.6)`를 사용한다. 원격 플레이어의 화살 거치 계산과 소유자 크로스헤어 정렬을 합치거나 장전 화살을 카메라 중앙에 독립 생성하지 않는다.

### 상태와 종료 처리

1. `Z` 준비 → `LMB` 당기기/유지: 기존 관통 장전 화살의 위치 갱신을 유지하면서 제트스트림 표시와 장전 입자를 활성화한다.
2. 완전 장전 후 해제 → Release 몽타주의 기존 발사 시점에서 투사체 **한 개**를 생성한다. 이 시점은 `BowSkillFire`, Release 약 0.083333초다.
3. 투사체에는 제트스트림·비행 입자·리본을 함께 연결하고 이동 방향은 기존 크로스헤어 조준 계산을 사용한다.
4. 조기 해제/RMB 취소/인터럽트/장비 변경/사망에는 장전 메시를 숨기고 입자 방출을 종료한다. 최종 컴포넌트/장전 표시 액터도 정리한다.
5. 충돌·미스 수명 종료에는 투사체를 정리한다. 트레일 잔상을 별도 보존하려면 유한 수명의 분리 컴포넌트를 사용하고 최종 삭제 책임을 둔다.

`Deactivate` 직후 `IsActive=True`이거나 잔여 입자가 보일 수 있다. 활성 플래그 하나만으로 취소가 끝났다고 판단하지 않는다. 일반 공격·트리플·회피에 관통 기류가 남지 않아야 한다.

## 4. 궁수 사냥꾼의 표식

기획은 지정 범위 내 적에게 일정 시간 표식을 적용하는 광역 동작이다. 테스트의 반경 400cm·지속 5초·조준 Trace 1800cm는 표현 확인 값이며 최종 밸런스 수치가 아니다. 범위 중심 선정과 실제 적/팀 판정은 담당자가 확정한다.

### 발동 시점과 세 효과

- `X` → `AM_Bow_HunterMarkAoE`의 `HunterMarkApply`, 약 0.75초에서 한 번 적용한다. Notify 전에 취소하면 Cast/Survey/표식 적용이 모두 0회다.
- **Cast:** `hand_r` 위치에 스케일 0.3으로 생성한다. 전방은 `GetBaseAimRotation`을 사용한다. 손목의 상대 회전만 따라 붙이면 입자가 뒤로 튈 수 있다.
- **Survey:** 서버가 확정한 범위 중심에 한 번 생성한다. 현재 BP는 액터 전체 스케일을 `Radius / 50`으로 계산하고 Niagara 컴포넌트 상대 스케일은 1이다. 반경 400cm에서 액터 스케일은 8이며 컴포넌트에 다시 8을 곱하지 않는다. 펄스 액터는 약 1.4초 후 종료한다. 시각적 가장자리 최대 반경은 판정 반경의 약 0.955배다.
- **Target:** 실제 표식 상태에 따라 대상에 붙은 카메라 방향 금색 눈/다이아몬드/후광 아이콘을 켠다. 테스트 컴포넌트 위치 `(0,0,215)`는 테스트 표적의 루트 기준이고 상대 스케일은 0.2다. 현재 보이는 판 크기는 약 64cm다. 실제 적에서는 머리 위 위치를 메시 Bounds/전용 소켓으로 조정한다.

### 지속 아이콘 연결 순서

`HunterMarkAura`는 Auto Activate Off, 초기 숨김 상태다. 대상 아이콘의 원본 Niagara는 유한 효과이므로 다음 설정을 같이 가져가야 지속 아이콘이 된다.

```text
표식 시작/갱신
  → SetVisibility(True)
  → Activate(Reset=True)
  → SetAgeUpdateMode(DesiredAge)
  → SetDesiredAge(0.18)

표식 만료/해제/사망
  → SetVisibility(False)
  → Deactivate
  → 대상 제거 시 컴포넌트도 정리
```

시뮬레이션 나이를 0.18초에 유지하며 머티리얼의 시간 입력이 후광/노이즈를 움직인다. 이 방식에서 입자 위치는 샘플링된 나이에 유지된다. 상태 유지용 컴포넌트는 매 프레임 새로 생성하지 않는다. 갱신된 표식은 이전 만료 타이머를 교체/무효화해 이전 타이머가 새 표식을 끄지 않도록 한다.

### 기존 마크 중복 방지

기존 `MarkGlyphVisual`은 `hidden_in_game=True`, `visible=False`를 유지한다. `SetVisibility`의 Target 입력에 예전 `MarkGlyphVisual` getter와 새 `HunterMarkAura` getter가 동시에 연결되면 **두 마크가 같이 켜진다**. 기존 getter와 실행 대상 링크를 제거했다. 새 코드에서도 가시성을 바꾸는 대상은 새 아이콘 하나여야 한다.

`BP_Test_HunterMarkPulse`의 옛 평면은 숨김 상태로 루트/MID 경로를 유지한다. 생산에 이식할 때 표현만 새 Niagara로 바꾸면서 기존 유효한 초기화 경로를 깨지 않도록 한다. 테스트 표적 BP는 실제 적/ASC/팀 판정을 대신하지 않는다.

## 5. 전사 VFX 연결

### 전투의 함성 — 유한 1회 효과

`NS_HB_WarCry_Remake`는 붉은 몸 주변 기류·불씨와 주황색 압력 고리를 사용한다. 팩 `NS_Potion`의 4개 네이티브 이미터와 Spawn/Initialize/Velocity/Noise/Drag/Lifetime 등의 모듈을 유지하고 전용 Shockwave/Aura/HotSparks/Embers 머티리얼로 형태를 바꿨다.

- **캐릭터 발 위치 기준**, 월드 +Z 위쪽, 스케일 1로 한 번 생성한다. `SpawnSystemAtLocation`을 쓴다면 충격파 중심을 발의 지면 위치에 둔다.
- 시각적 고리 지름은 스케일 1에서 대략 480cm이며 효과 수명은 약 0.95초다. 이 크기/수명은 게임 버프 반경/지속과 독립적이다.
- Auto Destroy를 켜고 서버가 확정한 함성 발동 이벤트당 각 클라이언트에서 한 번 재생한다.
- 현재 `L_NiagaraTest`의 함성 스테이션과 `/Game/Levels/test_gm/NiagaraTest/ThirdPerson/BP_NiagaraThirdPersonCharacter.PreviewSystem`이 새 시스템을 참조한다. `R`은 기존 `PreviewVFX` 시험 입력이다.

### 전투 본능 — 상태 동안 유지

최종 경로는 **`NS_HB_CombatInstinct_Aura_Remake`**다. 몸에 붙은 붉은 기류, 낮은 발 주변 박동 고리, 적은 수의 붉은 불씨가 지속된다. 중간 제작 후보 `NS_HB_CombatInstinct_Remake`는 최종 에셋이 아니다.

- 팩 `NS_Dark_Solo_Projectile`을 기반으로 8개 이미터의 연결을 유지한다. 보이는 렌더러는 Dark_Core/Radial_Wave/Lightning의 3개다. 나머지 렌더러는 숨겨 두었지만 이미터 시뮬레이션 비용은 남아 있어 최적화 완료라고 볼 수 없다.
- 상태 진입 시 **한 번** `SpawnSystemAttached`, Auto Destroy On으로 몸 메시의 발 기준 루트에 부착한다. 상대 위치/회전 0, 스케일 1, +Z 위쪽 기준이다. 다른 메시의 원점이 골반이면 발 위치로 상대 오프셋을 보정한다.
- 상태가 유지될 때 컴포넌트 핸들을 보관한다. 매 Tick 재생성하거나 짧은 타이머로 반복 생성하지 않는다.
- 상태 종료 시 `Deactivate`로 방출을 중지하고 잔여 입자가 소멸한 뒤 Auto Destroy로 제거한다. 급한 장비 교체/사망/액터 종료에서는 보관한 컴포넌트의 최종 제거도 보장한다.
- 늦게 접속한 클라이언트는 현재 버프 상태에서 표현을 재구성해야 한다. 1회 Multicast 시작 이벤트만으로는 이 요구를 충족하지 못한다.

### 피의 갈증 — 회복 발생 시 1회

`NS_HB_BloodThirst_Remake`는 붉은 가느다란 입자·안개·작은 후광이 가슴 쪽으로 모였다가 사라지는 유한 효과다. 팩 `NS_Potion`의 4개 이미터와 네이티브 모듈을 사용한다.

- 실제 서버 회복 성공 이벤트에 연결하고 각 클라이언트에서 **한 번** 재생한다. 회복량/발동 조건은 생산 코드가 결정한다.
- 현재 효과는 **발 기준 위치로 생성하면 내부 표현이 약 Z=100cm 가슴 방향으로 모이도록** 제작됐다. 가슴 소켓에 원점까지 그대로 붙이면 높이가 중복된다. 발 기준 메시 루트에 부착하거나 별도로 원점 보정을 한다.
- 스케일 1, Auto Destroy On을 사용한다. 테스트 스테이션의 3초 반복은 비교용이며 생산 반복 주기가 아니다.
- 회복이 없는 타격·중복 판정에 표시가 켜지지 않도록 한다. 유지 버프용 루프가 아니므로 전투 본능과 다른 종료 경로를 사용한다.

## 6. 담당자 연동 체크리스트

| 구분 | 연결해야 하는 생산 책임 |
|---|---|
| 관통 | 서버 발사 판정 1곳, owner/remote 장전 상태, 유효 조준 방향, 취소/투사체 종료 |
| 표식 | 서버 범위·팀·적 판정, 적용/갱신/만료 상태, Cast/Survey 1회, 대상 아이콘 유지 |
| 함성 | 서버 함성 성공 이벤트, 발 위치 재생, 기존 시스템 참조 교체 |
| 전투 본능 | 실제 버프 상태의 시작/종료, 컴포넌트 핸들, 사망/장비 교체/늦은 접속 처리 |
| 피의 갈증 | 실제 회복 성공 이벤트 1회, 발 기준 원점, Auto Destroy |

Gameplay Cue나 상태 복제 등 프로젝트의 기존 방식에 맞춰 연결하되, 이벤트 재생과 지속 상태 복원을 구분한다. 이번 작업은 실제 적 데미지/GAS·비용/쿨타임·다른 PC·늦은 접속·전용 서버 보장을 검증한 생산 통합이 아니다.

## 7. 테스트 방법과 확인 범위

### 궁수: 이미 수행한 격리 테스트

`/Game/Levels/test_gm/BowRetarget/L_Test_BowRetarget`에서 2인 리슨 서버로 확인했다.

1. `6/7/8`로 AA/AC/AN을 교체하고 `Z → LMB 유지 → 해제`. 소유자/원격 장전 기류와 비행 리본, 플레이어당 한 발을 확인한다.
2. 조기 해제 및 유지 중 RMB 취소. 투사체 0발, 장전 표시 제거, 이후 일반 공격에 관통 VFX가 남지 않는지 확인한다.
3. 서버/클라이언트 각각 `X`. 범위 안 3개 테스트 표적만 표식이 켜지고 범위 밖 1개는 유지되는지, 5초 후 사라지는지 확인한다.
4. X 적용 전 RMB 취소는 적용/펄스 0회다. 재시전에서도 예전 마크와 새 아이콘이 중복되지 않아야 한다.

관통 최종 정상 속도 감사는 263 표본, 표식 최초 감사는 315 표본, **기존 마크 중복 제거 후 추가 감사는 162 표본**이었다. 중복 제거 후 서버/클라이언트 시전·조기 취소·만료에서 기존 마크 숨김과 새 상태 일치를 확인했다. 이전 최초 감사만으로 기존 마크 제거를 검증했다고 보지 않는다.

### 전사: 현재 추가된 테스트 연결

1. `/Game/Levels/test_gm/L_NiagaraTest`: 함성 스테이션 자동 반복과 `R` 미리보기. 발 중심 고리/몸 기류, 1회 효과의 최종 제거를 확인한다.
2. `/Game/Levels/test_gm/L_PassiveVFXTest`: 왼쪽 전투 본능은 지속, 오른쪽 피의 갈증은 3초마다 한 번이다. WASD/마우스로 가까이 확인한다. 이 맵에 새로운 숫자 키 토글을 추가하지 않았다.
3. 전투 본능을 한 번 부착한 후 이동/회전시켜 발 기준 따라옴을 확인하고 상태 종료 시 방출 종료와 최종 제거를 확인한다. 피의 갈증은 한 번 생성한 컴포넌트가 남지 않아야 한다.

전사 함성 감사는 13초 동안 957 표본, 패시브 감사는 13초 동안 770 표본이었다. 패시브는 이동/180도 회전·복귀, 6초 시점 전투 본능 종료, 직접/부착 컴포넌트의 Auto Destroy를 확인했고 위치 부착 오차는 0cm였다. 전사 테스트는 단일 PIE의 표현/정리 검증이며 전사 생산 멀티플레이 버프 복제 검증은 아니다.

PR 제출 전 최신 develop `fabcdbb9875cfb00af704608a3f8023355b166cb`를 fast-forward 통합하고 Development Editor 빌드를 다시 성공시켰다. 재실행한 단일 PIE 감사에서도 함성 901 표본/13.003초, 패시브 699 표본/13.0042초를 확인했다. 함성 미리보기 호출·직접 생성 효과 제거, 전투 본능의 6초 유지 후 직접/부착 효과 제거, 피의 갈증 1회 효과 제거가 모두 통과했고 부착 오차는 0cm, 감사 오류는 0건이었다. 기존 R 미리보기 BP도 컴파일 오류 없이 새 함성 시스템을 참조한다.

### 이번 범위에서 바꾸지 않은 항목

사용자 요청으로 검 궤적·패링·금속/돌/나무 타격·살점/피 타격 리메이크는 건너뛰었다. 궁수/마법사 패시브 VFX는 폐기 상태를 유지한다. 원본 Fab 팩, 기존 전사 시스템, 애니메이션·공유 스켈레톤·소켓·Aim Offset·카메라·생산 GAS 코드는 VFX 제작 과정에서 변경하지 않았다.
