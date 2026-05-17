// Copyright 2026 Ultimate Player All Rights Reserved.


#include "IF_ProMode.h"

// Add default functionality here for any IIF_ProMode functions that are not pure virtual.

void IIF_ProMode::OnSoloRideDefeat(UObject* OwnerObject, int32 Score)
{
	if (OwnerObject)
	{
		Execute_OnSoloRideDefeat(OwnerObject, Score); // Call the function in your blueprint.
	}
}
