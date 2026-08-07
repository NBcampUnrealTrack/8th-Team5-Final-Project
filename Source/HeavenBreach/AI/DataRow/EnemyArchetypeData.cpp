// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/DataRow/EnemyArchetypeData.h"

const FEnemyStatRow& UEnemyArchetypeData::GetStats() const
{
	// 
	if (const FEnemyStatRow* Row = StatRow.GetRow<FEnemyStatRow>(TEXT("EnemyArchetypeData")))
	{
		return *Row;
	}
	return FallbackStats;
}