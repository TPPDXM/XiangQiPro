#pragma once

#include "CoreMinimal.h"

class SWidget;

// Slate keeps the loading animation alive while map loading blocks the game thread.
TSharedRef<SWidget> MakeXQPLoadingScreen();
