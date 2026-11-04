// Copyright 2026 Ultimate Player All Rights Reserved.
#include "XQPFeedbackAudioSubsystem.h"
#include "XQPAudioSettingsSubsystem.h"
#include "XiangQiPro/Util/AssetPaths.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UXQPFeedbackAudioSubsystem::UXQPFeedbackAudioSubsystem()
{
    // 硬引用让 Cook 能追踪正式资源，并预热短音效，避免首次交互时加载。
    static ConstructorHelpers::FObjectFinder<USoundBase> Click(PATH_AUDIO_UI_CLICK);
    static ConstructorHelpers::FObjectFinder<USoundBase> Selection(PATH_AUDIO_UI_SELECT);
    static ConstructorHelpers::FObjectFinder<USoundBase> Countdown(PATH_AUDIO_COUNTDOWN);
    static ConstructorHelpers::FObjectFinder<USoundBase> Victory(PATH_AUDIO_MATCH_VICTORY);
    static ConstructorHelpers::FObjectFinder<USoundBase> Defeat(PATH_AUDIO_MATCH_DEFEAT);
    static ConstructorHelpers::FObjectFinder<USoundBase> Draw(PATH_AUDIO_MATCH_DRAW);
    ClickSound = Click.Object;
    SelectionSound = Selection.Object;
    CountdownSound = Countdown.Object;
    VictorySound = Victory.Object;
    DefeatSound = Defeat.Object;
    DrawSound = Draw.Object;
}

void UXQPFeedbackAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // 音效开关和音量必须先就绪，首个界面交互就使用已保存的混音设置。
    Collection.InitializeDependency<UXQPAudioSettingsSubsystem>();
    Super::Initialize(Collection);
}

void UXQPFeedbackAudioSubsystem::ConfigureFor(UUserWidget* Widget)
{
    if (!IsValid(Widget) || Widget->IsDesignTime()) return;
    UGameInstance* Game = Widget->GetGameInstance();
    if (auto* Feedback = Game ? Game->GetSubsystem<UXQPFeedbackAudioSubsystem>() : nullptr)
        Feedback->ConfigureWidgetTree(Widget);
}

void UXQPFeedbackAudioSubsystem::ConfigureWidgetTree(UUserWidget* Widget)
{
    if (!Widget->WidgetTree) return;
    FSlateSound Click;
    Click.SetResourceObject(ClickSound);
    FSlateSound Selection;
    Selection.SetResourceObject(SelectionSound);
    Widget->WidgetTree->ForEachWidgetAndDescendants([this, &Click, &Selection](UWidget* Child)
    {
        if (auto* Button = Cast<UButton>(Child))
        {
            FButtonStyle Style = Button->GetStyle();
            // UE 5.8 的 Clicked 槽只响应有效点击，不在按下和抬起时各响一次。
            Style.SetPressedSound(FSlateSound()).SetHoveredSound(FSlateSound()).SetClickedSound(Click);
            Button->SetStyle(Style);
        }
        else if (auto* Combo = Cast<UComboBoxString>(Child))
        {
            FComboBoxStyle Style = Combo->GetWidgetStyle();
            Style.SetPressedSound(FSlateSound()).SetSelectionChangeSound(FSlateSound());
            Style.ComboButtonStyle.ButtonStyle.SetPressedSound(FSlateSound())
                .SetClickedSound(FSlateSound()).SetHoveredSound(FSlateSound());
            Combo->SetWidgetStyle(Style);
            // Slate 默认选择音也响应 SetSelectedIndex。改用过滤 Direct 的委托，
            // AddUnique 保证重复构造不会叠加，恢复默认值或网络刷新时保持安静。
            Combo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleSelectionChanged);
            // SComboBox 把声音缓存于构造时，运行中更换 Style 不会刷新该缓存；
            // 打开音使用稳定的 UMG 回调，避免桌面/竖屏首次显示时漏音。
            Combo->OnOpening.AddUniqueDynamic(this, &ThisClass::HandleComboOpening);
        }
        else if (auto* Check = Cast<UCheckBox>(Child))
        {
            FCheckBoxStyle Style = Check->GetWidgetStyle();
            Style.SetCheckedSound(Selection).SetUncheckedSound(Selection).SetHoveredSound(FSlateSound());
            Check->SetWidgetStyle(Style);
        }
    });
}

void UXQPFeedbackAudioSubsystem::HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectionType != ESelectInfo::Direct && !SelectedItem.IsEmpty()) PlayFeedback(SelectionSound);
}

void UXQPFeedbackAudioSubsystem::HandleComboOpening() { PlayFeedback(ClickSound); }

void UXQPFeedbackAudioSubsystem::PlayFeedback(USoundBase* Sound)
{
    UWorld* World = GetWorld();
    if (!Sound || !World || World->GetNetMode() == NM_DedicatedServer) return;
    if (auto* Settings = GetGameInstance()->GetSubsystem<UXQPAudioSettingsSubsystem>()) Settings->ApplyToWorld(World);
    // 2D UI 声音与相机距离无关，也能在暂停的结算/设置菜单中播放。
    // 音量交给 SC_XQPEffects；静音不积压，恢复后不会补播旧提示。
    UGameplayStatics::PlaySound2D(World, Sound);
}

void UXQPFeedbackAudioSubsystem::PlayCountdownTick() { PlayFeedback(CountdownSound); }

void UXQPFeedbackAudioSubsystem::PlayMatchResult(bool bWon, bool bDraw)
{
    PlayFeedback(bDraw ? DrawSound.Get() : bWon ? VictorySound.Get() : DefeatSound.Get());
}
