// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SealedDoorTypes.generated.h"

/**
 * 봉인문·열쇠 등급.
 *
 * EBoxGrade / EItemGrade 와 값 순서를 일부러 맞춰두었다.
 * 상자 등급과 개념이 달라 재사용하지 않지만, 나중에 인덱스로 대응시킬 일이 생겨도 어긋나지 않게 하기 위함.
 */
UENUM(BlueprintType)
enum class ESealedDoorGrade : uint8
{
	Common		UMETA(DisplayName = "일반"),
	Rare		UMETA(DisplayName = "희귀"),
	Epic		UMETA(DisplayName = "영웅"),
	Celestial	UMETA(DisplayName = "천상")
};

UENUM(BlueprintType)
enum class ESealedDoorState : uint8
{
	Sealed	UMETA(DisplayName = "봉인됨"),
	/** 열쇠를 소비하고 봉인 해제 연출이 진행 중인 상태. 이 동안에는 상호작용할 수 없다. */
	Opening	UMETA(DisplayName = "개방 중"),
	Opened	UMETA(DisplayName = "개방됨")
};

/** 문을 열기 전에는 항상 Undetermined 여야 한다. 클라이언트가 내부를 미리 알 수 없어야 하기 때문. */
UENUM(BlueprintType)
enum class ESealedRoomResult : uint8
{
	Undetermined	UMETA(DisplayName = "미결정"),
	MonsterRoom		UMETA(DisplayName = "몬스터 방"),
	RewardRoom		UMETA(DisplayName = "보상방")
};

USTRUCT(BlueprintType)
struct FSealedRoomSpawnEntry
{
	GENERATED_BODY()

	/** 스폰할 몬스터 BP 또는 BoxActor BP */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor", meta = (ClampMin = 0))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor", meta = (ClampMin = 0))
	int32 MaxCount = 1;
};

/**
 * DT_SealedDoorGrade 의 행. 행 이름은 SealedDoorGradeToRowName() 이 돌려주는 값과 일치해야 한다.
 *
 * [스폰 배치 규칙 — 데이터 작성 전에 반드시 확인]
 * - 몬스터 방(MonsterRoom) 결과에서는 MonsterRoomSpawns 만 스폰된다.
 * - 보상방(RewardRoom) 결과에서는 RewardRoomSpawns 와 GuardSpawns 가 함께 스폰된다.
 * - 이 스폰들은 모두 문 액터 인스턴스의 SpawnPoints 배열 하나를 공유해서 소비한다.
 *   포인트는 쓰일 때마다 줄어들고, 풀이 바닥나면 남은 스폰 요청은 조용히 버려진다
 *   (경고 로그만 남고 문은 정상적으로 열린다). 따라서 SpawnPoints 개수는
 *   "어느 한 결과가 요구할 수 있는 최대 총량" 이상이어야 한다 —
 *   RewardRoom 은 RewardRoomSpawns + GuardSpawns 의 최대 합, MonsterRoom 은
 *   MonsterRoomSpawns 의 최대 합을 기준으로 잡는다.
 */
USTRUCT(BlueprintType)
struct FSealedDoorGradeData : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 등급 문을 여는 데 필요한 열쇠. DT_Item 의 행 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor")
	FName RequiredKeyRowName;

	/** 몬스터 방이 나올 확률. 일반=0.5, 희귀 이상=0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor", meta = (ClampMin = 0, ClampMax = 1))
	float MonsterRoomChance = 0.0f;

	/** 몬스터 방 결과일 때 배치. 문 인스턴스의 SpawnPoints 풀을 RewardRoomSpawns/GuardSpawns 와 공유한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor")
	TArray<FSealedRoomSpawnEntry> MonsterRoomSpawns;

	/**
	 * 보상방 결과일 때 배치. BoxActor BP 를 넣으면 내용물은 DT_BoxGradeLoot 이 처리한다.
	 * 같은 결과에서 GuardSpawns 와 SpawnPoints 풀을 함께 소비하므로,
	 * 두 배열의 최대 합이 SpawnPoints 개수를 넘지 않도록 데이터를 잡아야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor")
	TArray<FSealedRoomSpawnEntry> RewardRoomSpawns;

	/** 보상방 결과에 함께 배치할 호위 적. 희귀 이상에서 사용. RewardRoomSpawns 와 SpawnPoints 풀을 공유한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor")
	TArray<FSealedRoomSpawnEntry> GuardSpawns;

	/** 프롬프트에 쓰는 문 이름. 예: "희귀 봉인문" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor")
	FText DoorDisplayName;

	/**
	 * 봉인 해제 연출 시간(초). 열쇠를 소비한 뒤 이 시간이 지나야 문이 실제로 열리고
	 * 내용물이 배치된다. 그동안 플레이어는 무방비 상태이므로, 상위 등급일수록 길게 잡아
	 * 기획서 8장(등급이 높을수록 연출 강도 상승)과 22장(개방음을 이용한 PvP 유도)을 함께 만족시킨다.
	 * 0 이하면 즉시 개방한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor", meta = (ClampMin = 0, Units = "s"))
	float OpenDurationSeconds = 2.0f;

	/**
	 * 문짝이 실제로 열리는 데 걸리는 시간(초). OpenDurationSeconds 와 별개다 —
	 * 홀드가 끝난 뒤부터 이 시간 동안 문짝이 회전한다.
	 *
	 * 상위 등급일수록 길게 잡아 "무거운 문"(기획서 8장)의 무게감을 준다.
	 * 뒤늦게 이 문을 보게 된 클라이언트가 여는 연출을 재생할지, 이미 열린 상태로 둘지도
	 * 이 값으로 판정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor", meta = (ClampMin = 0, Units = "s"))
	float SwingDurationSeconds = 1.0f;
};

/** ESealedDoorGrade -> DT_SealedDoorGrade 행 이름 */
HEAVENBREACH_API FName SealedDoorGradeToRowName(ESealedDoorGrade InGrade);
