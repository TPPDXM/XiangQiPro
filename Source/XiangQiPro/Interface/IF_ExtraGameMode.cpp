// Copyright 2026 Ultimate Player All Rights Reserved.


#include "IF_ExtraGameMode.h"

// Add default functionality here for any IIF_ExtraGameMode functions that are not pure virtual.

void IIF_ExtraGameMode::OnSoloRideDefeat(UObject* OwnerObject, int32 Score)
{
	if (OwnerObject)
	{
		Execute_OnSoloRideDefeat(OwnerObject, Score); // Call the function in your blueprint.
	}
}
