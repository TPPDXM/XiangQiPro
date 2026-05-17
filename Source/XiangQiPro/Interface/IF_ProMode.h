// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "InterfaceCombinations.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IF_ProMode.generated.h"

#define EXEC_ONSOLORIDEDEFEAT(SCORE) CALL_INTERFACE_EVENT_PARAM(IF_ProMode, OnSoloRideDefeat, SCORE)

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UIF_ProMode : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class XIANGQIPRO_API IIF_ProMode
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	virtual void OnSoloRideDefeat(UObject* OwnerObject, int32 Score);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnSoloRideDefeat(int32 Score);
};
