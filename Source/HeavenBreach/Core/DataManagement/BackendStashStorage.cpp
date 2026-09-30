#include "BackendStashStorage.h"

#include "Engine/DataTable.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Item/ItemEngraving.h"

bool FBackendStashStorage::Initialize()
{
	return true;
}

void FBackendStashStorage::Shutdown()
{
}

FString FBackendStashStorage::BuildUrl(const FString& InAccountId) const
{
	return BaseUrl / InAccountId;
}

void FBackendStashStorage::LoadStashAsync(const FString& InAccountId, FOnStashLoaded InCallback)
{
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BuildUrl(InAccountId));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	Request->OnProcessRequestComplete().BindLambda(
		[InCallback](FHttpRequestPtr InRequest, FHttpResponsePtr InResponse, bool bConnectedSuccessfully)
		{
			TArray<FInventoryEntry> Entries;

			if (!bConnectedSuccessfully || !InResponse.IsValid() || InResponse->GetResponseCode() != 200)
			{
				UE_LOG(LogTemp, Warning, TEXT("Stash: load request failed"));
				InCallback.ExecuteIfBound(false, Entries);
				return;
			}

			TSharedPtr<FJsonObject> RootObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InResponse->GetContentAsString());

			if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("Stash: load response parse failed"));
				InCallback.ExecuteIfBound(false, Entries);
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

					FInventoryEntry Entry;

					FGuid::Parse((*ItemObject)->GetStringField(TEXT("instanceId")), Entry.Item.InstanceId);
					FGuid::Parse((*ItemObject)->GetStringField(TEXT("originalOwnerId")), Entry.Item.OriginalOwnerId);

					const FString TablePath = (*ItemObject)->GetStringField(TEXT("tablePath"));
					Entry.Item.ItemRow.DataTable = LoadObject<UDataTable>(nullptr, *TablePath);
					Entry.Item.ItemRow.RowName = FName(*(*ItemObject)->GetStringField(TEXT("rowName")));

					Entry.Item.StackCount = (*ItemObject)->GetIntegerField(TEXT("stackCount"));
					Entry.Item.bRotated = (*ItemObject)->GetBoolField(TEXT("rotated"));

					double LoadedDurability = -1.0;
					(*ItemObject)->TryGetNumberField(TEXT("currentDurability"), LoadedDurability);
					Entry.Item.CurrentDurability = static_cast<float>(LoadedDurability);

					// 각인. 모양은 ItemEngraving 이 정한다 (로드아웃 저장과 같이 쓴다)
					ItemEngraving::ReadJson(*ItemObject, Entry.Item.Engraving);

					Entry.Position = FIntPoint(
						(*ItemObject)->GetIntegerField(TEXT("posX")),
						(*ItemObject)->GetIntegerField(TEXT("posY")));

					Entries.Add(Entry);
				}
			}

			UE_LOG(LogTemp, Log, TEXT("Stash: loaded %d entries"), Entries.Num());

			InCallback.ExecuteIfBound(true, Entries);
		});

	Request->ProcessRequest();
}

void FBackendStashStorage::SaveStashAsync(const FString& InAccountId, const TArray<FInventoryEntry>& InEntries, FOnStashSaved InCallback)
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("accountId"), InAccountId);

	TArray<TSharedPtr<FJsonValue>> ItemArray;

	for (const FInventoryEntry& Entry : InEntries)
	{
		const TSharedRef<FJsonObject> ItemObject = MakeShared<FJsonObject>();

		ItemObject->SetStringField(TEXT("instanceId"), Entry.Item.InstanceId.ToString());
		ItemObject->SetStringField(TEXT("originalOwnerId"), Entry.Item.OriginalOwnerId.ToString());

		const FString TablePath = Entry.Item.ItemRow.DataTable != nullptr
			? Entry.Item.ItemRow.DataTable->GetPathName()
			: FString();

		ItemObject->SetStringField(TEXT("tablePath"), TablePath);
		ItemObject->SetStringField(TEXT("rowName"), Entry.Item.ItemRow.RowName.ToString());
		ItemObject->SetNumberField(TEXT("stackCount"), Entry.Item.StackCount);
		ItemObject->SetBoolField(TEXT("rotated"), Entry.Item.bRotated);
		ItemObject->SetNumberField(TEXT("currentDurability"), Entry.Item.CurrentDurability);
		ItemEngraving::WriteJson(Entry.Item.Engraving, ItemObject);
		ItemObject->SetNumberField(TEXT("posX"), Entry.Position.X);
		ItemObject->SetNumberField(TEXT("posY"), Entry.Position.Y);

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
				UE_LOG(LogTemp, Warning, TEXT("Stash: save request failed"));
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("Stash: saved"));
			}

			InCallback.ExecuteIfBound(bSuccess);
		});

	Request->ProcessRequest();
}