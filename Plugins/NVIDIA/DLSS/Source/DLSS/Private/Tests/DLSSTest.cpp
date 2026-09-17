/*
* Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#if WITH_TESTS

// plugin includes
#include "DLSS.h"

// engine includes
#include "HAL/PlatformMisc.h"
#include "ID3D11DynamicRHI.h"
#include "ID3D12DynamicRHI.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#if WITH_NVAPI
#include "nvapi.h"
#endif
#include "RHI.h"

namespace {
	FString GetDLSSSRSupportString(EDLSSSupport DLSSSupport)
	{
		const TCHAR* SupportStr = TEXT("Unknown");
		switch (DLSSSupport) {
		case EDLSSSupport::Supported:
			SupportStr = TEXT("Supported");
			break;
		case EDLSSSupport::NotSupported:
			SupportStr = TEXT("NotSupported");
			break;
		case EDLSSSupport::NotSupportedIncompatibleHardware:
			SupportStr = TEXT("NotSupportedIncompatibleHardware");
			break;
		case EDLSSSupport::NotSupportedDriverOutOfDate:
			SupportStr = TEXT("NotSupportedDriverOutOfDate");
			break;
		case EDLSSSupport::NotSupportedOperatingSystemOutOfDate:
			SupportStr = TEXT("NotSupportedOperatingSystemOutOfDate");
			break;
		case EDLSSSupport::NotSupportedIncompatibleAPICaptureToolActive:
			SupportStr = TEXT("NotSupportedIncompatibleAPICaptureToolActive");
			break;
		default:
			checkf(false, TEXT("Unknown EDLSSSupport value, please update this test function"));
		}
		return FString::Printf(TEXT("DLSS-SR support: %s"), SupportStr);
	}

	template<class T>
	bool IsRenderDocEnabled(T* D3DDevice)
	{
		IID RenderDocID;
		if (SUCCEEDED(IIDFromString(L"{A7AA6116-9C8D-4BBA-9083-B4D816B71B78}", &RenderDocID)))
		{
			TRefCountPtr<IUnknown> RenderDoc;
			if (SUCCEEDED(D3DDevice->QueryInterface(RenderDocID, (void**)RenderDoc.GetInitReference())))
			{
				return true;
			}
		}
		return false;
	}

	// find any reasons DLSS-SR wouldn't be supported
	bool ShouldSRWork()
	{
#if (!PLATFORM_WINDOWS || !PLATFORM_CPU_X86_FAMILY || !WITH_NVAPI)
		// Currently only supported on Windows/x86-64
		return false;
#else
		// DLSS-SR requires D3D12, D3D11, or Vulkan
		ERHIInterfaceType RHIType = GDynamicRHI ? GDynamicRHI->GetInterfaceType() : ERHIInterfaceType::Null;
		if ((RHIType != ERHIInterfaceType::D3D12) && (RHIType != ERHIInterfaceType::D3D11) && (RHIType != ERHIInterfaceType::Vulkan))
		{
			return false;
		}

		// DLSS-SR requires NVIDIA GPU
		if (!IsRHIDeviceNVIDIA())
		{
			return false;
		}

		// DLSS-SR requires RTX hardware
		bool bRequiredArchFound = false;
		{
			NvU32 NumNVGPUs = 0;
			NvPhysicalGpuHandle NVGPUHandles[NVAPI_MAX_PHYSICAL_GPUS];

			NvAPI_Status NVStatus = NvAPI_EnumPhysicalGPUs(NVGPUHandles, &NumNVGPUs);
			if (NVStatus == NvAPI_Status::NVAPI_OK)
			{
				TArrayView<const NvPhysicalGpuHandle> GPUHandles(NVGPUHandles, NumNVGPUs);
				for (const NvPhysicalGpuHandle& GPUHandle : GPUHandles)
				{
					NV_GPU_ARCH_INFO GPUArchInfo;
					GPUArchInfo.version = NV_GPU_ARCH_INFO_VER;
					NVStatus = NvAPI_GPU_GetArchInfo(GPUHandle, &GPUArchInfo);
					if (NVStatus == NvAPI_Status::NVAPI_OK)
					{
						if (GPUArchInfo.architecture >= NV_GPU_ARCHITECTURE_GV100)
						{
							bRequiredArchFound = true;
							break;
						}
					}
				}
			}
		}
		if (!bRequiredArchFound)
		{
			return false;
		}

		// renderdoc interferes with DLSS-SR
		if (IsRHID3D12())
		{
			ID3D12DynamicRHI* D3D12RHI = GetID3D12DynamicRHI();
			if (IsRenderDocEnabled(D3D12RHI->RHIGetDevice(0)))
			{
				return false;
			}
		}
		else if (IsRHID3D11())
		{
			ID3D11DynamicRHI* D3D11RHI = GetID3D11DynamicRHI();
			if (IsRenderDocEnabled(D3D11RHI->RHIGetDevice()))
			{
				return false;
			}
		}

		// DLSS-SR needs at least Windows 10.0.16299
		if (!FPlatformMisc::VerifyWindowsVersion(10, 0, 16299))
		{
			return false;
		}

		// Someone might disable NGX, perhaps because it interferes with their own tests?
		IConsoleVariable* CVarNGXEnableLocal = IConsoleManager::Get().FindConsoleVariable(TEXT("r.NGX.Enable"));
		if (!CVarNGXEnableLocal || (CVarNGXEnableLocal->GetInt() == 0))
		{
			return false;
		}
		IConsoleVariable* CVarNGXEnableAllowCommandLineLocal = IConsoleManager::Get().FindConsoleVariable(TEXT("r.NGX.Enable.AllowCommandLine"));
		if (CVarNGXEnableAllowCommandLineLocal && (CVarNGXEnableAllowCommandLineLocal->GetInt() != 0))
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("ngxdisable")))
			{
				return false;
			}
		}

		// TODO: technically very old drivers would also prevent DLSS-SR from working although it's unlikely anyone will be testing that

		// we can't find any reason it shouldn't work
		return true;
#endif	// PLATFORM_WINDOWS && PLATFORM_CPU_X86_FAMILY
	}
}	// anonymous namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDLSSSRTest, "Nvidia.DLSS.QueryDLSSSRSupport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::ProductFilter
)

bool FDLSSSRTest::RunTest(const FString& Parameters)
{
	const IDLSSModuleInterface& DLSSModule = FModuleManager::GetModuleChecked<IDLSSModuleInterface>("DLSS");

	// log SR support
	const EDLSSSupport DLSSSRSupport = DLSSModule.QueryDLSSSRSupport();
	AddInfo(GetDLSSSRSupportString(DLSSSRSupport));

	// If we think SR should work, QueryDLSSSRSupport should agree
	if (ShouldSRWork())
	{
		// If this fails, something may be preventing NGX or DLSS-SR from initializing
		TestTrueExpr(DLSSSRSupport == EDLSSSupport::Supported);
	}
	else
	{
		// If this fails, DLSS-SR is reported as supported on an unexpected configuration
		TestTrueExpr(DLSSSRSupport != EDLSSSupport::Supported);
	}

	return true;
}

#endif	// WITH_TESTS

