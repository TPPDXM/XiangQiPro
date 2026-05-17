// Copyright 2026 Ultimate Player All Rights Reserved.


#include "UI_ProModeMenu.h"
#include "Components/TileView.h"

#include "XiangQiPro/Util/ProModeLibrary.h"
#include "ProModeData.h"

void UUI_ProModeMenu::NativeConstruct()
{
	Super::NativeConstruct();
	auto Arr = UProModeLibrary::GetProModeInfos();
	for (const auto& info : Arr)
	{
		UProModeData* item = NewObject<UProModeData>();
		if (item)
		{
			item->Init(info->Title, info->Covers, info->GameModeID);
			ModeList->AddItem(item);
		}
	}
}
