#include "UI_LANPause.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/Network/XQPLANSubsystem.h"
#include "XiangQiPro/GameMode/XQPGameInstance.h"
#include "XiangQiPro/UI/Components/XQPButtonBinding.h"
#include "Kismet/GameplayStatics.h"

void UUI_LANPause::NativeConstruct()
{
    Super::NativeConstruct();
    // 只连接UMG资产中的按钮，本机菜单不会暂停网络对局。
    if (ResumeButton)
        XQPButtonBinding::GetButton(ResumeButton)->OnClicked.AddUniqueDynamic(this, &UUI_LANPause::Resume);
    if (LeaveRoomButton)
        XQPButtonBinding::GetButton(LeaveRoomButton)->OnClicked.AddUniqueDynamic(this, &UUI_LANPause::Leave);
}
void UUI_LANPause::Resume()
{
    GetGameInstance()->GetSubsystem<UUIManager>()->RemoveUI(this);
}
void UUI_LANPause::Leave()
{
    // 发出离房通知后切回主菜单，释放旧连接和端口。
    GetGameInstance()->GetSubsystem<UXQPLANSubsystem>()->LeaveRoom();
    GetGameInstance<UXQPGameInstance>()->SetGameMode(EXQPGameMode::Default);
    UXQPGameInstance::OpenGameLevel(this, TEXT("/Game/Map/Main"));
}
