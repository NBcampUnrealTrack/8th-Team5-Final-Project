# 단일 플레이어 실게임 PCG

## 사용 방법

1. UE 에디터를 재시작한다. 새 클래스가 추가되므로 Live Coding만으로 전환하지 않는다.
2. 게임 맵의 GameMode Override는 기존 `BP_HeavenBreachGameMode` 또는 `HeavenBreachGameMode` 파생 클래스를 사용한다.
3. 맵에 `HB Dungeon Runtime`을 하나 배치한다. Preview Generator는 이 맵에서 제거한다.
4. `Dungeon - Modules`에 모듈을 등록하고 `Transitions`에서 출구·입구·포탈·보스 복도를 선택한다.
   `Advanced / Graph`에는 기존 `PCG_DungeonPrototype`의 Plan → Load 그래프를 지정한다.
5. **시작 방 후보인 일반 모험방마다** PlayerStart를 하나 배치한다.
   PlayerStart의 **Player Start Tag**를 `DungeonSpawn`으로 지정한다. Actor Tags 배열과 다르다.
   캐릭터 캡슐 전체가 바닥 위 빈 공간에 놓이도록 배치한다. 시작점은 생성된 첫 방의 인스턴스에서만 찾는다.
6. `Door Seal Extent`는 막을 문의 반 크기(cm)다. 기본값은 두께 60, 폭 320, 높이 440cm인 벽이다.
   커넥터는 문의 바닥 중앙에 두며 +X가 바깥쪽이다. 다른 문 크기는 이 값이나 DoorSeal 파생 BP로 맞춘다.
7. Number of Players = 1, Net Mode = Standalone으로 실행한다.

Runtime 생성기를 배치한 Standalone 맵에서는 기존 GameMode의 최소 인원 설정이 2여도 1명으로 테스트한다.
생성기가 없는 맵의 기존 인원·매치 흐름은 유지한다. 리슨/전용 서버 동기화는 아직 지원하지 않는다.

## 실행 흐름

HeavenBreachGameMode → 카탈로그 검증 → PCG Plan → PCG Load → 모든 방 로딩/표시 완료
→ 실제 커넥터·점유·포탈 재검증 → 미사용 문 차단 → 시작점 충돌/바닥 검사
→ Ready → 기존 매치 시작 → 기존 DefaultPawnClass 스폰/Possess → 매치 타이머 시작.

실게임 생성기는 Preview 카메라, 자유 비행 Pawn, HUD, 키 입력, 연결 테스트 모드를 갖지 않는다.
기존 캐릭터·컨트롤러·입력·HUD는 메인 GameMode BP의 설정을 사용한다.
사망·탈출·결과 맵·세션 처리는 기존 동작을 사용하며 이번 작업에서 리슨 서버용으로 변경하지 않는다.

상태는 `State`와 `Status`, 이벤트는 `OnReady`/`OnFailed`로 제공한다. 로딩 UI는 이 이벤트에 연결할 수 있다.
실패하면 부분 생성된 방·포탈·차단벽을 정리하고 스폰을 막는다. 자동 재시도는 없으며 설정 수정 후 다시 실행한다.
생성기는 매치 중 재생성을 제공하지 않는다. 문 통행, 캐릭터 캡슐 여유, 방 내부 길, AI NavMesh는 원본 모듈에서 제작·검증해야 한다.
전체 방은 시작 시 로딩한다. 방 내부 PCG, 방별 언로드, 몬스터·보상 초기화는 별도 작업이다.

## 검증 맵

`/Game/Levels/Test_CW/GameplaySmoke/TestPCG_Gameplay`는 단순한 바닥과 벽으로 구성한 독립 검증 맵이다.
기존 콘텐츠 맵을 수정하지 않고 실제 캐릭터 스폰·포탈·출입구 차단을 확인한다.
`author_gameplay_smoke.py`로 작성하며 이미 작성된 모듈/맵은 덮어쓰지 않는다.

자동화 테스트 `HeavenBreach.PCGModule.Gameplay`는 준비 전 스폰/매치 시작 차단과
실제 PIE에서 스트리밍, 캐릭터 Possess, 타이머, 포탈 이동, 미사용 커넥터 차단을 검사한다.

검증 결과: Unity Build 성공, PCG 자동화 16개 통과(통합 테스트 1개는 기존 에셋 경고 포함).
테스트는 로비에서 전달할 직업 시작 구성을 미리 준비한다. 맵을 직접 PIE로 실행하면 기존 캐릭터의
직업 기본값 대체 로그가 발생할 수 있다. 기존 입력 에셋의 `IA_Interact` 빈 트리거 진단은
테스트에서 해당 메시지만 예상 오류로 구분했으며, 입력 에셋 자체는 수정하지 않았다.
