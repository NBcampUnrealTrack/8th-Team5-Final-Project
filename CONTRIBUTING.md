# 협업 규칙 (CONTRIBUTING)

언리얼 프로젝트 협업 규칙 문서입니다. 이 문서가 **원본**이며, 규칙이 바뀌면 이 파일을 수정하는 PR을 올립니다.

---

## 1. 브랜치 — Git Flow (develop 완충)

`develop`이 `feat`와 `main` 사이의 **완충 지대**입니다. 매일의 통합·머지·QA는 `develop`에서 일어나고, `main`은 발표·제출처럼 안정 시점에만 `develop`을 받습니다. → `main`이 깨질 위험을 `develop`이 흡수합니다.

| 브랜치 | 역할 |
|---|---|
| `main` | 제출/발표용 안정판. release 시점에만 `develop` → `main` 머지. **직접 커밋·push 금지** |
| `develop` | 기본 브랜치. 모든 `feat`가 모이는 통합 완충 지대. 여기서 멀티 QA 후 `main`으로 올림 |
| `feat/*` | 새 기능. `develop`에서 분기, `develop`으로 PR. **하루~3일 내 완료** |
| `fix/*` | 버그 수정 (`develop` 기준) |

### 브랜치 네이밍

```
<type>/<scope>-<short-description>
```

규칙:

- **영문 소문자 + 하이픈**
- `scope`는 담당 시스템 (`core` / `player` / `enemy` / `net` / `level` / `ui` 등)
- **한글 금지** (터미널·CI 깨짐)

예시:

```
feat/player-dash
feat/net-lobby-session
fix/enemy-patrol-stuck
fix/ui-hp-bar-flicker
```


---

## 2. Pull Request 규칙

### PR 만들기 전 (로컬에서 확인)

1. 로컬에서 **빌드 성공** 확인 (Development Editor)
2. **핵심 기능 테스트** 완료
3. **Blueprint 및 Map 충돌** 확인
4. `develop` 최신 변경분을 merge한 뒤 **재빌드·재테스트**
5. 수정한 `.uasset` / `.umap`의 **LFS lock 해제**

### PR 생성

1. GitHub에서 PR 생성
2. **base 브랜치 = `develop`** 확인 (기본값)
3. 템플릿 채우기 (자동 삽입됨)
   - **구현 내용**과 **테스트 방법**을 작성 — 리뷰어가 그대로 따라할 수 있게
   - **스크린샷·GIF 첨부 권장**

### 머지

- **매일 아침 전원 모여서** 작업물 PR을 소개, 확인 후 Merge
- 급하거나 바로 필요한 경우가 아니라면 **바로 Merge 금지**
- `develop` → `feat` 방향 머지는 자유. `feat` → `develop`은 PR만
- 긴급 머지를 했다면 그날 반드시 공유

---

## 3. Commit 규칙

Commit 시 어떠한 용도로 수정이 된 브랜치인지 알기 위해 **구별하고 분류하여** 커밋합니다.

| Prefix | 용도 |
|---|---|
| `[ADD]` | 신규 파일 또는 에셋 추가 |
| `[FEAT]` | 새로운 기능 구현 |
| `[FIX]` | 버그 수정 |
| `[UPD]` | 기존 기능 또는 수치 변경 |
| `[REFACTOR]` | 구조 개선 |
| `[DOC]` | 문서 변경 |

### 형식

```
[PREFIX] (scope) 요약

- 상세 내용 (필요할 때만)
```

예시:

```
[FEAT] (player) 대시 어빌리티 추가
[ADD] (enemy) 근접 몹 스켈레탈 메시 및 애니메이션 에셋 추가
[FIX] (net) 클라이언트에서 HP 동기화 누락 수정
[UPD] (player) 대시 쿨타임 2.0 → 1.5초
[REFACTOR] (core) GameMode 상태 관리 분리
[DOC] 브랜치 네이밍 규칙 보완
```


---

## 4. 언리얼 전용 규칙 — 실제 사고 방지선

바이너리(`.uasset`, `.umap`)는 **머지가 불가능합니다.** 충돌하면 "한쪽을 버리는" 선택만 남으므로, 아래는 권고가 아니라 필수입니다.

### 4-1. 자신이 사용중이 아닌 uasset 건들기 금지
- 다름사람의 uasset은 작업 차단
- 필요할 경우 해당 건 공유 후 수정

### 4-2. 레벨(Map)

| 규칙 | 이유 |
|---|---|
| **Persistent Level 직접 편집 금지** — 서브레벨 / Level Instance로 분리해 각자 담당 | 맵 충돌 시 한 사람 작업이 통째로 사라짐 |
| 중요 레벨 작업은 반드시 모두에게 알리고 작업 시작 | 동시 수정 원천 차단 |

### 4-3. 그 외

| 규칙 | 이유 |
|---|---|
| 담당 `scope` 밖의 블루프린트는 손대지 않기 | 리뷰 불가능한 diff 방지 |
| `Binaries/` `Intermediate/` `Saved/` `DerivedDataCache/` 커밋 금지 | 저장소 오염 (`.gitignore` 처리됨) |
| 에셋 이름 변경·이동은 반드시 **에디터에서** 수행하고 단독 커밋 | 탐색기에서 옮기면 레퍼런스가 끊김 |
| `.uproject` / `Config/*.ini` 수정은 아침 회의에서 공유 | 전원에게 영향 |
| 엔진 버전·플러그인 변경은 전원 합의 후 | 한 명이 올리면 나머지가 프로젝트를 못 엶 |

---

