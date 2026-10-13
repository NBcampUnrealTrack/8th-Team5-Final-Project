#include "UI/Map/HB_MapLevelEntry.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UHB_MapLevelEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Btn_Entry)
	{
		Btn_Entry->OnClicked.AddUniqueDynamic(this, &UHB_MapLevelEntry::HandleClicked);
	}
}

void UHB_MapLevelEntry::Setup(int32 InIndex, FName InCode, const FText& InName, FHBMapEntryClicked InHandler)
{
	Index = InIndex;
	Code = InCode;
	Handler = InHandler;
	if (Text_Name)
	{
		Text_Name->SetText(InName);
	}
	if (Text_Code)
	{
		Text_Code->SetText(FText::FromName(InCode));
	}
}

void UHB_MapLevelEntry::SetState(bool bInGenerated, bool bInSelected)
{
	OnStateChanged(bInGenerated, bInSelected);
}

void UHB_MapLevelEntry::OnStateChanged_Implementation(bool bGenerated, bool bSelected)
{
	if (Img_Dot)
	{
		Img_Dot->SetColorAndOpacity(bGenerated ? DotOnColor : DotOffColor);
	}
	if (Text_Name)
	{
		Text_Name->SetColorAndOpacity(FSlateColor(bGenerated ? NameOnColor : NameOffColor));
	}
	if (Btn_Entry)
	{
		Btn_Entry->SetBackgroundColor(bSelected ? SelectedTint : NormalTint);
	}
}

void UHB_MapLevelEntry::HandleClicked()
{
	Handler.ExecuteIfBound(Index);
}
