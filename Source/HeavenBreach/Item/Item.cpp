#include "Item.h"

bool CanEquipAccessory(const FItemData& Item)
{
    // Accessory 타입이면서 Normal 등급이 아닐 때만 장착 가능
    return Item.ItemType == TEXT("Accessory") && Item.Grade != EItemGrade::Normal;
}