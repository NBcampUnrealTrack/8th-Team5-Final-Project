# 실게임 PCG와 랜덤 포탈

## 사용 방법

1. UE 에디터를 재시작한다. 새 클래스가 추가되므로 Live Coding만으로 전환하지 않는다.
2. 게임 맵의 GameMode Override는 기존 `BP_HeavenBreachGameMode` 또는 `HeavenBreachGameMode` 파생 클래스를 사용한다.
3. 맵에 `HB Dungeon Runtime`을 하나 배치한다. Preview Generator는 이 맵에서 제거한다.
4. `Dungeon - Modules`에 모듈을 등록하고 `Transitions`에서 출구·입구·포탈·보스 복도를 선택한다.
   `Advanced / Graph`에는 기존 `PCG_DungeonPrototype`의 Plan → Load 그래프를 지정한다.
   랜덤 포탈은 `Portal Placement Mode = Random Points`, `Portal Class`, `Portal Pair Count`를 설정한다.
   이 모드에서는 고정 포탈 ID 대신 방에 배치한 `HB Portal Spawn Point`를 사용한다.
   [포인트 배치와 상세 사용법](RulesAndPortals.md)을 참고한다.
5. **시작 방 후보인 일반 모험방마다** PlayerStart를 하나 배치한다.
   PlayerStart의 태그를 따로 지정할 필요는 없다.
   캐릭터 캡슐 전체가 바닥 위 빈 공간에 놓이도록 배치한다. 시작점은 생성된 첫 방의 인스턴스에서만 찾는다.
6. `Door Seal Extent`는 막을 문의 반 크기(cm)다. 기본값은 두께 60, 폭 320, 높이 440cm인 벽이다.
   커넥터는 문의 바닥 중앙에 두며 +X가 바깥쪽이다. 다른 문 크기는 이 값이나 DoorSeal 파생 BP로 맞춘다.
7. 혼자 검증할 때는 Number of Players = 1, Net Mode = Standalone으로 실행한다.
   멀티 검증은 Number of Players = 2 이상, Net Mode = Play As Listen Server로 실행한다.

Runtime 생성기를 배치한 Standalone 맵에서는 기존 GameMode의 최소 인원 설정이 2여도 1명으로 테스트한다.
생성기가 없는 맵의 기존 인원·매치 흐름은 유지한다. Listen 서버는 서버의 방 구성과
최종 포탈 쌍을 클라이언트에 전달한다. Dedicated Server 생성은 현재 지원하지 않는다.

## 실행 흐름

HeavenBreachGameMode → 카탈로그 검증 → PCG Plan → PCG Load → 모든 방 로딩/표시 완료
→ 실제 커넥터·점유 재검증 → 미사용 문 차단 → 포탈 준비·최종 결과 배포
→ 시작점 충돌/바닥 검사와 방 스폰 준비 → 서버 Ready
→ 클라이언트 방·포탈 복원 및 로딩 완료 보고 → 기존 매치 시작
→ 기존 DefaultPawnClass 스폰/Possess → 매치 타이머 시작.

랜덤 포탈은 서버에서만 추첨·바닥 정렬·캡슐 충돌 검사를 한다. 포탈에 NavMesh 경로 검사를
적용하지 않는다. 클라이언트는 포탈 최종 데이터가 도착할 때까지 READY를 기다리며,
서버가 정한 Trigger와 Arrival을 그대로 복원한다. 늦은 접속도 같은 최종 결과를 사용한다.
실제 순간이동은 서버에서 처리하며 위치와 바라보는 방향을 원격 플레이어에게 전달한다.

실게임 생성기는 Preview 카메라, 자유 비행 Pawn, HUD, 키 입력, 연결 테스트 모드를 갖지 않는다.
기존 캐릭터·컨트롤러·입력·HUD는 메인 GameMode BP의 설정을 사용한다.
사망·탈출·결과 맵·세션 처리는 기존 동작을 사용한다.

상태는 `State`와 `Status`, 이벤트는 `OnReady`/`OnFailed`로 제공한다. 로딩 UI는 이 이벤트에 연결할 수 있다.
실패하면 부분 생성된 방·포탈·차단벽을 정리하고 스폰을 막는다. 자동 재시도는 없으며 설정 수정 후 다시 실행한다.
생성기는 매치 중 재생성을 제공하지 않는다. 매치 시작 전 GameMode의 `Regenerate Dungeon`으로
다시 생성할 수 있으며, 기존 방·포탈·연결을 정리하고 새 생성 번호의 결과를 동기화한다.
문 통행, 캐릭터 캡슐 여유, 방 내부 길, AI NavMesh는 원본 모듈에서 제작·검증해야 한다.
전체 방은 시작 시 로딩한다. 방 내부 PCG, 방별 언로드, 몬스터·보상 초기화는 별도 작업이다.

## 검증 맵

`/Game/Levels/Test_CW/GameplaySmoke/TestPCG_Gameplay`는 단순한 바닥과 벽으로 구성한 독립 검증 맵이다.
기존 콘텐츠 맵을 수정하지 않고 실제 캐릭터 스폰·포탈·출입구 차단을 확인한다.
`author_gameplay_smoke.py`로 작성하며 이미 작성된 모듈/맵은 덮어쓰지 않는다.

자동화 테스트 `HeavenBreach.PCGModule.Gameplay`는 준비 전 스폰/매치 시작 차단과
실제 PIE에서 스트리밍, 캐릭터 Possess, 타이머, 포탈 이동, 미사용 커넥터 차단을 검사한다.

`HeavenBreach.PCGModule.Network.RandomPortals`는 현재 포탈 BP와 임시 검증 방을 사용해
Listen 서버·원격 클라이언트에서 최종 쌍/위치 일치, 왕복 이동, 늦은 접속,
매치 시작 전 재생성 및 정리를 검사한다. 시드 대량 검사는 수행하지 않는다.

검증 결과: Unity Build 성공, PCG 자동화 16개 통과(통합 테스트 1개는 기존 에셋 경고 포함).
테스트는 로비에서 전달할 직업 시작 구성을 미리 준비한다. 맵을 직접 PIE로 실행하면 기존 캐릭터의
직업 기본값 대체 로그가 발생할 수 있다. 기존 입력 에셋의 `IA_Interact` 빈 트리거 진단은
테스트에서 해당 메시지만 예상 오류로 구분했으며, 입력 에셋 자체는 수정하지 않았다.
