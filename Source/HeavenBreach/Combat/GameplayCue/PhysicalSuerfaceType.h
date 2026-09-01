#pragma once

#include "Chaos/ChaosEngineInterface.h"

/**
 * Project Settings -> Physics -> Physical Surface 의 이름과의 짝
 */
namespace PSType
{
	constexpr EPhysicalSurface Flesh = SurfaceType1;
	constexpr EPhysicalSurface Metal = SurfaceType2;
	constexpr EPhysicalSurface Wood = SurfaceType3;
	constexpr EPhysicalSurface Stone = SurfaceType4;
}