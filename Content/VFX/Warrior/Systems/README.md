# 전사 VFX 팀원 사용 가이드

2026-09-18. 혈흔과 검 타격의 레이어를 **효과별 Niagara System 하나**에 통합했다. 아래 에셋 하나를 재생하면 모든 레이어가 함께 나온다. 게임 전투 로직 연결은 담당자가 진행한다.

## 바로 사용할 에셋

공통 폴더: `/Game/VFX/Warrior/Systems`

- 혈흔 3종: `/Game/VFX/Warrior/Systems/BloodEffect`
- 검 타격 4종: `/Game/VFX/Warrior/Systems/SwordEffect`
- Systems 바로 아래의 동일 이름 에셋은 이동 전 참조를 유지하는 리다이렉터다. 새로 지정할 때는 위 하위 폴더의 시스템을 선택한다.

| 용도 | Niagara System | 내부 이미터 |
|---|---|---|
| 기본 혈흔 | `NS_HB_BloodImpact` | 핏방울 + 튀김 |
| 약한 혈흔 | `NS_HB_BloodImpact_Light` | 핏방울 + 튀김 |
| 강한 혈흔 | `NS_HB_BloodImpact_Heavy` | 핏방울 + 튀김 |
| 패링 | `NS_HB_SwordParry` | 스파크 + 긴 스파크 + 섬광 |
| 금속 타격 | `NS_HB_SwordHitMetal` | 스파크 + 섬광 |
| 돌 타격 | `NS_HB_SwordHitStone` | 돌 파편 + 먼지 + 작은 스파크 |
| 나무 타격 | `NS_HB_SwordHitWood` | 나무 파편 + 먼지 |

## Blueprint 사용

1. `Spawn System at Location`을 추가한다.
2. System Template에 위 에셋 하나를 지정한다.
3. Location은 타격 위치, Rotation은 원하는 분사 방향으로 지정한다. **로컬 +X가 분사 기준축**이므로 방향 벡터를 `Make Rot from X`에 넣어 사용할 수 있다.
4. Scale은 기본 `(1, 1, 1)`. 혈흔 Light/Heavy의 기존 0.7/1.35 배율은 시스템 내부 크기와 속도에 이미 반영되어 있다.
5. Auto Activate와 Auto Destroy를 켠다. 단발 재생 후 종료되므로 별도 Actor Blueprint와 Set Life Span은 필요 없다. 기본 Pooling Method는 None이다.

이미터는 Local Space를 사용한다. 중력은 월드 아래 방향이다. 피격 위치에 남는 타격 효과에는 위의 위치 기반 생성이 적합하며, 움직이는 대상에 붙여 재생하면 이후 이동/회전이 입자에도 영향을 준다.

각 시스템 안에서 Spawn Burst, Initialize Particle, Add Velocity, Scale Color 등으로 조절한다. 통합본의 이미터는 내부 복사본이므로 원본 레이어 시스템을 수정해도 자동 동기화되지 않는다. 이후 실사용 효과 수정은 `Systems`의 해당 에셋을 기준으로 한다.

## 테스트 및 의존성

- `/Game/Levels/test_gm/L_NiagaraTest`에서 Play: 7종을 2초 간격으로 직접 생성한다.
- 테스트 액터의 Effect System에 새 시스템을 지정한다. 위치/회전/스케일이 그대로 전달된다.
- 기존 `/Blood`, `/SwordImpact`의 Blueprint와 개별 레이어 시스템은 보존했다. 실사용에는 위 통합본을 선택한다.
- 통합본도 `/Blood`, `/SwordImpact`에 있는 머티리얼 및 혈흔 텍스처를 사용한다. 다른 프로젝트로 옮길 때 콘텐츠 브라우저의 **Migrate**로 의존성을 함께 가져간다.
- GameplayCue, 피격 판정, 사운드, 네트워크 재생 연결은 포함하지 않는다.

## 검증 기록

- 7종 모두 Blueprint 없이 직접 Spawn System at Location으로 활성화 및 자동 제거를 확인했다.
- 테스트 레벨 PIE 6.34초 동안 각 시스템이 3회 생성됐으며, 각 종류의 동시 컴포넌트 최대값은 1이었다. 기존 효과 Blueprint Actor는 생성되지 않았다.
- 혈흔 3단계, 패링/금속, 돌/나무의 실제 PIE 화면을 확인했다.
- C++ 변경 없음. 실제 게임 전투 이벤트 및 멀티플레이 연결 검증은 별도이다.
