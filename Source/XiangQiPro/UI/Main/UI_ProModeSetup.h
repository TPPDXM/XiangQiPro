#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "UI_ProModeSetup.generated.h"

class USizeBox;
class UUI_LANLobby;

// Pro 玩法共用的介绍、开局和联机房间窗口。
UCLASS()
class XIANGQIPRO_API UUI_ProModeSetup : public UUI_CommonDialog
{
    GENERATED_BODY()
public:
    void SetMode(int32 Id) { ModeId = Id; }
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    int32 ModeId = 6;
    bool bLobbyVisible = false;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USizeBox> LobbyHost;
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UUI_LANLobby> LobbyClass;
    void StartGame();
    void ToggleLobby();
    void ConfigureMode();
};
