// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "UI_Item_ProMode.generated.h"

/**
 * 
 */
UCLASS()
class XIANGQIPRO_API UUI_Item_ProMode : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	class UTextBlock* ItemText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	class UImage* ItemCover;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int GameModeID = 3;

public:
	virtual void NativeConstruct() override;

	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	
};
