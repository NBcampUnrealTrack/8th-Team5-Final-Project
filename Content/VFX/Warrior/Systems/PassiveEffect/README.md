# 전사 패시브 VFX

제작: 2026-09-22. 각 효과는 Niagara System 하나로 재생한다. 공격력/체력 수치, 체력 임계값, 적 처치 판정, GAS/GameplayCue 및 네트워크 연결은 포함하지 않는다.

| 패시브 | Niagara System | 동작 |
|---|---|---|
| 전투 본능 | /Game/VFX/Warrior/Systems/PassiveEffect/NS_HB_CombatInstinct | 몸 주변의 붉은 오라, 옅은 안개, 작은 상승 입자와 발밑 맥동. 지속 루프 |
| 피의 갈증 | /Game/VFX/Warrior/Systems/PassiveEffect/NS_HB_BloodThirst | 주변 붉은 입자가 몸으로 모인 뒤 회복 잔광을 남기는 단발 효과 |

## 생성 기준

- 약 180 cm 캐릭터, Scale (1,1,1), 로컬 +Z 위쪽, 기준점은 캐릭터 메시의 발밑/루트다.
- 이동 중 캐릭터를 따라야 하므로 Spawn System Attached를 사용하고 Skeletal Mesh Component에 상대 위치 (0,0,0)으로 붙인다. 메시 루트의 좌표가 다른 캐릭터에는 오프셋을 조정한다.
- 캡슐/Actor 중심에 오프셋 없이 붙이면 효과가 높게 뜬다.
- 두 효과 모두 Local Space. 몸으로 모이는 중심은 발밑 기준 Z=98 cm다. 적 시체 위치에서 출발하는 추적 투사체는 아니다.
- Auto Activate=true, Pooling Method=None을 기준으로 확인했다.

## 전투 본능: 상태 유지와 종료

조건 진입 시 한 번 Spawn System Attached 하고 반환된 컴포넌트를 보관한다. 매 프레임 생성하지 않는다. 조건이 유지되는 동안 무한 반복하며, 조건이 끝나면 해당 컴포넌트의 Deactivate를 호출한다. 기존 입자가 약 1.9초에 걸쳐 사라진다.

Auto Destroy=true 기준으로 종료 후 컴포넌트 자동 제거를 확인했다. 재진입할 때 기존 컴포넌트가 아직 유효하면 Activate(reset=true)로 재사용하거나, 제거됐다면 다시 생성한다. 취소 순간 즉시 제거가 필요하면 담당 로직에서 별도 처리한다.

레이어: CrimsonBreath, BodyHaze, RisingMotes, Heartbeat, ShoulderWisps.

## 피의 갈증: 단발 회복

처치 후 회복 이벤트 시 한 번 Spawn System Attached 하고 Auto Destroy=true로 둔다. 약 1.35초 내의 입자/잔광 연출 후 컴포넌트는 실제 관측 약 2.03초에 자동 제거됐다. 반복 타이머가 필요한 효과가 아니다.

레이어: BloodInflow, AbsorptionMist, RecoveryPulse, AfterglowMotes, VitalityHalo. 흡수 속도와 중심은 Add Velocity의 From Point 음수 속도 및 Point Origin에 있다.

## 조정

머티리얼 폴더: /Game/VFX/Warrior/Passives. PassiveTint와 Intensity로 색·밝기를 조정한다. 두 효과가 6개 전용 머티리얼을 공유하므로 공통 머티리얼을 바꾸면 둘 다 영향을 받을 수 있다. 이전 함성·혈흔·보스 검기 머티리얼은 수정하지 않았다.

## 테스트

맵 /Game/Levels/test_gm/L_PassiveVFXTest를 열고 Play한다. 왼쪽 전투 본능은 계속 유지되고, 오른쪽 피의 갈증은 비교를 위해 3초마다 반복한다. WASD와 마우스로 이동·관찰하고 Esc로 종료한다. 전용 Spectator 테스트 맵이므로 이전 3인칭 테스트 캐릭터의 R키 함성 동작은 사용하지 않는다.

기존 L_NiagaraTest, 보스 검기 스테이션 및 게임 캐릭터는 변경하지 않았다.

검증: 실제 PIE의 어두운/밝은 배경 화면 확인, 12초 관측에서 피의 갈증 4회 생성·동시 최대 1개·재생 사이 빈 구간 확인. 지속 오라는 6.02초까지 유지하고 Deactivate 후 1.91초에 제거. 부착 기준 액터 이동/90도 회전 후 컴포넌트 위치 오차 0 cm. 새 Niagara/머티리얼/셰이더 오류 로그 없음.

실제 체력/공격력 변화, 실게임 장비·1인칭 시야·멀티플레이 재생은 별도 검증이 필요하다. C++/Config/플러그인 변경 없음.
