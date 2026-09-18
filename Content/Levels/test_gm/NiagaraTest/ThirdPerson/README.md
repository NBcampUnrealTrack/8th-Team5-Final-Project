# Niagara 3인칭 캐릭터 테스트

`/Game/Levels/test_gm/L_NiagaraTest`에서 Play하면 전투의 함성 구역에 3인칭 테스트 캐릭터가 생성된다.

| 에셋 | 역할 |
|---|---|
| `BP_NiagaraThirdPersonCharacter` | 엔진 Character를 부모로 하는 독립 테스트 캐릭터 |
| `ABP_NiagaraThirdPerson_Test` | `ABP_Warrior_F`의 포즈/상태 머신을 복사하고 일반 Character의 이동 값을 사용하도록 변경한 테스트 애님 |
| `BP_NiagaraThirdPersonGameMode` | 이 테스트 레벨에서 위 캐릭터를 생성하는 GameMode |

모두 `/Game/Levels/test_gm/NiagaraTest/ThirdPerson`에 있다. 원본 `/Game/Player/BP_HBPlayerCharacter`와 `ABP_Warrior_F`, 스켈레탈 메시, 게임용 GameMode 및 Config는 수정하지 않았다.

## 참조 및 범위

- 원본 캐릭터의 메인 메시: `/Game/Characters/Warrior/Meshes/Character/SK_HB_Warrior`.
- 메시의 상대 위치 `(0,0,-90)`, 회전 Yaw `-90`, 배율 및 머티리얼 오버라이드는 원본 BP 기본값을 따른다.
- 원본 애님 부모 `HBPlayerAnimInst`는 `HBPlayerCharacter`로 캐스팅하므로, 테스트 애님 복사본의 부모를 `AnimInstance`로 바꿨다. 일반 Character의 속도/방향/낙하/웅크리기 및 기존 점프 전환용 `IsInAir?`를 업데이트한다.
- AimPitch는 0으로 유지하여 카메라를 돌려도 상체가 시점 Pitch를 따라 기울어지지 않는다. 포즈/블렌드스페이스/애니메이션 시퀀스는 원본을 참조한다.
- 장비/인벤토리/GAS/전투/상호작용은 포함하지 않는다. 따라서 장비 외형을 적용하기 전 메인 메시 모습이며, 실게임 캐릭터 동작 검증용이 아니다.
- 새 애님은 복사본이므로 이후 원본 AnimGraph 변경은 자동 동기화되지 않는다.

## 조작

| 입력 | 동작 |
|---|---|
| WASD | 카메라 수평 방향 기준 이동, 최대 320 cm/s |
| 마우스 | 3인칭 시점 회전 |
| Space | 점프, 떼면 점프 입력 종료 |
| 왼쪽 Ctrl | 누르는 동안 웅크리기 |
| R | 현재 발밑에 Preview System 단발 재생 |
| Esc | PIE 종료 |
| Shift+F1 | 에디터 마우스 포커스 해제 |

카메라는 SpringArm 길이 360cm, FOV 85이며 카메라 충돌 검사를 사용한다. 프로젝트 Input 설정을 변경하지 않고 이 테스트 BP에서만 키 상태를 읽는다.

## VFX 변경

`BP_NiagaraThirdPersonCharacter`의 Class Defaults → **VFX Preview**:

- `Preview System`: 기본 `NS_HB_WarCry`. R키로 생성할 시스템을 변경한다.
- `Preview Offset`: 메시 컴포넌트의 월드 위치에 더하는 오프셋. 기본 `(0,0,0)`으로 발밑 기준이며, 웅크릴 때도 발밑 위치를 따른다. 흉부에서 타격 VFX를 확인할 때는 Z를 약 90 올릴 수 있다.

생성 Rotation은 0, Scale은 1이며 Auto Activate / Auto Destroy를 사용한다. R키는 순수 시각 미리보기로 실제 함성 스킬을 실행하지 않는다. 이미 생성된 효과는 생성 위치에 남는다. 맵의 고정 스테이션은 별도로 기존 반복 재생을 유지한다.

기존 자유 비행 방식이 필요하면 **이 맵의 World Settings → GameMode Override**를 `/Game/Levels/test_gm/NiagaraTest/BP_NiagaraTestGameMode`로 되돌리면 된다. 프로젝트 기본 GameMode는 변경할 필요가 없다.

## 확인

- PIE에서 실제 W/D/S, 마우스, Space, Ctrl, R 입력 확인.
- 이동 속도 320 cm/s, 이동 애니메이션, 낙하/웅크리기 상태, 점프 전환 변수 및 R키 함성 표시 확인.
- 테스트 캐릭터/애님/게임모드 Blueprint 컴파일 및 저장. C++ 변경 없음.
- 실제 게임 전투, 장비, 서버/클라이언트 복제 동작은 이 테스트 캐릭터의 검증 범위에 포함하지 않는다.
