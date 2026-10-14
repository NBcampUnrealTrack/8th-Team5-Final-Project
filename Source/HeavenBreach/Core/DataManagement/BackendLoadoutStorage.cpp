#include "BackendLoadoutStorage.h"

#include "Engine/DataTable.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Item/ItemEngraving.h"

namespace
{
	const FString SlotTypeInventory = TEXT("INVENTORY");
	const FString SlotTypeEquipment = TEXT("EQUIPMENT");
	const FString SlotTypeQuickSlot = TEXT("QUICKSLOT");

	TSharedRef<FJsonObject> MakeItemJson(const FItemInstance& InItem)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();

		Object->SetStringField(TEXT("instanceId"), InItem.InstanceId.ToString());
		Object->SetStringField(TEXT("originalOwnerId"), InItem.OriginalOwnerId.ToString());

		const FString TablePath = InItem.ItemRow.DataTable != nullptr
			? InItem.ItemRow.DataTable->GetPathName()
			: FString();

		Object->SetStringField(TEXT("tablePath"), TablePath);
		Object->SetStringField(TEXT("rowName"), InItem.ItemRow.RowName.ToString());
		Object->SetNumberField(TEXT("stackCount"), InItem.StackCount);
		Object->SetBoolField(TEXT("rotated"), InItem.bRotated);
		Object->SetBoolField(TEXT("locked"), InItem.bLocked);
		Object->SetNumberField(TEXT("currentDurability"), InItem.CurrentDurability);

		// 각인. 모양은 ItemEngraving 이 정한다 (창고 저장과 같이 쓴다)
		ItemEngraving::WriteJson(InItem.Engraving, Object);

		return Object;
	}

	void ReadItemJson(const TSharedPtr<FJsonObject>& InObject, FItemInstance& OutItem)
	{
		FGuid::Parse(InObject->GetStringField(TEXT("instanceId")), OutItem.InstanceId);
		FGuid::Parse(InObject->GetStringField(TEXT("originalOwnerId")), OutItem.OriginalOwnerId);

		const FString TablePath = InObject->GetStringField(TEXT("tablePath"));
		OutItem.ItemRow.DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
		OutItem.ItemRow.RowName = FName(*InObject->GetStringField(TEXT("rowName")));

		OutItem.StackCount = InObject->GetIntegerField(TEXT("stackCount"));
		OutItem.bRotated = InObject->GetBoolField(TEXT("rotated"));
		InObject->TryGetBoolField(TEXT("locked"), OutItem.bLocked);

		double LoadedDurability = -1.0;
		InObject->TryGetNumberField(TEXT("currentDurability"), LoadedDurability);
		OutItem.CurrentDurability = static_cast<float>(LoadedDurability);

		ItemEngraving::ReadJson(InObject, OutItem.Engraving);
	}
}

bool FBackendLoadoutStorage::Initialize()
{
	return true;
}

void FBackendLoadoutStorage::Shutdown()
{
}

FString FBackendLoadoutStorage::BuildUrl(const FString& InAccountId) const
{
	return BaseUrl / InAccountId;
}

void FBackendLoadoutStorage::LoadLoadoutAsync(const FString& InAccountId, FOnLoadoutLoaded InCallback)
{
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BuildUrl(InAccountId));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	Request->OnProcessRequestComplete().BindLambda(
		[InCallback](FHttpRequestPtr InRequest, FHttpResponsePtr InResponse, bool bConnectedSuccessfully)
		{
			FPlayerLoadout Loadout;

			if (!bConnectedSuccessfully || !InResponse.IsValid() || InResponse->GetResponseCode() != 200)
			{
				UE_LOG(LogTemp, Warning, TEXT("Loadout: load request failed"));
				InCallback.ExecuteIfBound(false, Loadout);
				return;
			}

			TSharedPtr<FJsonObject> RootObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InResponse->GetContentAsString());

			if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("Loadout: load response parse failed"));
				InCallback.ExecuteIfBound(false, Loadout);
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* ItemArray = nullptr;
			if (RootObject->TryGetArrayField(TEXT("items"), ItemArray))
			{
				for (const TSharedPtr<FJsonValue>& ItemValue : *ItemArray)
				{
					const TSharedPtr<FJsonObject>* ItemObject = nullptr;
					if (!ItemValue->TryGetObject(ItemObject))
					{
						continue;
					}

					const FString SlotType = (*ItemObject)->GetStringField(TEXT("slotType"));
					const int32 SlotIndex = (*ItemObject)->GetIntegerField(TEXT("slotIndex"));

					if (SlotType == SlotTypeInventory)
					{
						FInventoryEntry Entry;
						ReadItemJson(*ItemObject, Entry.Item);
						Entry.Position = FIntPoint(
							(*ItemObject)->GetIntegerField(TEXT("posX")),
							(*ItemObject)->GetIntegerField(TEXT("posY")));

						Loadout.InventoryEntries.Add(Entry);
					}
					else if (SlotType == SlotTypeEquipment)
					{
						if (SlotIndex < 0)
						{
							continue;
						}

						if (Loadout.EquipmentSlots.Num() <= SlotIndex)
						{
							Loadout.EquipmentSlots.SetNum(SlotIndex + 1);
						}

						ReadItemJson(*ItemObject, Loadout.EquipmentSlots[SlotIndex]);
					}
					else if (SlotType == SlotTypeQuickSlot)
					{
						if (SlotIndex < 0)
						{
							continue;
						}

						if (Loadout.QuickSlots.Num() <= SlotIndex)
						{
							Loadout.QuickSlots.SetNum(SlotIndex + 1);
						}

						FGuid::Parse((*ItemObject)->GetStringField(TEXT("instanceId")), Loadout.QuickSlots[SlotIndex]);
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("Loadout: loaded inv=%d equip=%d quick=%d"),
				Loadout.InventoryEntries.Num(), Loadout.EquipmentSlots.Num(), Loadout.QuickSlots.Num());

			InCallback.ExecuteIfBound(true, Loadout);
		});

	Request->ProcessRequest();
}

void FBackendLoadoutStorage::SaveLoadoutAsync(const FString& InAccountId, const FPlayerLoadout& InLoadout, FOnLoadoutSaved InCallback)
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("accountId"), InAccountId);

	TArray<TSharedPtr<FJsonValue>> ItemArray;

	for (const FInventoryEntry& Entry : InLoadout.InventoryEntries)
	{
		const TSharedRef<FJsonObject> ItemObject = MakeItemJson(Entry.Item);
		ItemObject->SetStringField(TEXT("slotType"), SlotTypeInventory);
		ItemObject->SetNumberField(TEXT("slotIndex"), 0);
		ItemObject->SetNumberField(TEXT("posX"), Entry.Position.X);
		ItemObject->SetNumberField(TEXT("posY"), Entry.Position.Y);

		ItemArray.Add(MakeShared<FJsonValueObject>(ItemObject));
	}

	for (int32 Index = 0; Index < InLoadout.EquipmentSlots.Num(); ++Index)
	{
		const FItemInstance& Item = InLoadout.EquipmentSlots[Index];
		if (!Item.InstanceId.IsValid())
		{
			continue;
		}

		const TSharedRef<FJsonObject> ItemObject = MakeItemJson(Item);
		ItemObject->SetStringField(TEXT("slotType"), SlotTypeEquipment);
		ItemObject->SetNumberField(TEXT("slotIndex"), Index);
		ItemObject->SetNumberField(TEXT("posX"), 0);
		ItemObject->SetNumberField(TEXT("posY"), 0);

		ItemArray.Add(MakeShared<FJsonValueObject>(ItemObject));
	}

	for (int32 Index = 0; Index < InLoadout.QuickSlots.Num(); ++Index)
	{
		const FGuid& InstanceId = InLoadout.QuickSlots[Index];
		if (!InstanceId.IsValid())
		{
			continue;
		}

		const TSharedRef<FJsonObject> ItemObject = MakeShared<FJsonObject>();
		ItemObject->SetStringField(TEXT("slotType"), SlotTypeQuickSlot);
		ItemObject->SetNumberField(TEXT("slotIndex"), Index);
		ItemObject->SetStringField(TEXT("instanceId"), InstanceId.ToString());
		ItemObject->SetStringField(TEXT("originalOwnerId"), FString());
		ItemObject->SetStringField(TEXT("tablePath"), FString());
		ItemObject->SetStringField(TEXT("rowName"), FString());
		ItemObject->SetNumberField(TEXT("stackCount"), 0);
		ItemObject->SetBoolField(TEXT("rotated"), false);
		ItemObject->SetNumberField(TEXT("posX"), 0);
		ItemObject->SetNumberField(TEXT("posY"), 0);

		ItemArray.Add(MakeShared<FJsonValueObject>(ItemObject));
	}

	RootObject->SetArrayField(TEXT("items"), ItemArray);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(RootObject, Writer);

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BuildUrl(InAccountId));
	Request->SetVerb(TEXT("PUT"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(Body);

	Request->OnProcessRequestComplete().BindLambda(
		[InCallback](FHttpRequestPtr InRequest, FHttpResponsePtr InResponse, bool bConnectedSuccessfully)
		{
			const bool bSuccess = bConnectedSuccessfully && InResponse.IsValid() && InResponse->GetResponseCode() == 200;

			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("Loadout: save request failed"));
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("Loadout: saved"));
			}

			InCallback.ExecuteIfBound(bSuccess);
		});

	Request->ProcessRequest();
}