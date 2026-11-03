#include "UI_QualitySettings.h"
#include "Components/ListView.h"
#include "Misc/ConfigCacheIni.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/Chess/ChessMoveEffectsComponent.h"

// PC 保留八项独立设置；移动端合并档位，清晰度由 SaveAndApply 统一落实。
void UUI_QualitySettings::NativeConstruct()
{
    Super::NativeConstruct();
    if (!IsValid(UserSettings) || !IsValid(QualitySettingsList)) return;
    QualitySettingsList->ClearListItems();

    // 两个平台的蓝图共用此原生列表入口，开关独立于画质档位并立即持久化。
    if (!XQPMobile::IsPortrait(this))
    {
        // 回调只修改自己的效果组，不读取移动端 OverallQuality，也不覆盖其他 PC 自定义选项。
        const auto AddOption = [this](const TCHAR* Name, int32 Value, void (UGameUserSettings::*Setter)(int32))
        {
            auto* Item = NewObject<UQualitySettingObject>(this);
            Item->Init(FMath::Clamp(Value, 0, 3), FText::FromString(Name),
                [WeakThis = TWeakObjectPtr<UUI_QualitySettings>(this), Setter](int32 Index)
                {
                    if (!WeakThis.IsValid() || !IsValid(WeakThis->UserSettings)) return;
                    (WeakThis->UserSettings->*Setter)(FMath::Clamp(Index, 0, 3));
                    WeakThis->SaveAndApply();
                });
            QualitySettingsList->AddItem(Item);
        };
        AddOption(TEXT("后期效果"), UserSettings->GetPostProcessingQuality(), &UGameUserSettings::SetPostProcessingQuality);
        AddOption(TEXT("抗锯齿"), UserSettings->GetAntiAliasingQuality(), &UGameUserSettings::SetAntiAliasingQuality);
        AddOption(TEXT("阴影质量"), UserSettings->GetShadowQuality(), &UGameUserSettings::SetShadowQuality);
        AddOption(TEXT("反射质量"), UserSettings->GetReflectionQuality(), &UGameUserSettings::SetReflectionQuality);
        AddOption(TEXT("全局光照质量"), UserSettings->GetGlobalIlluminationQuality(), &UGameUserSettings::SetGlobalIlluminationQuality);
        AddOption(TEXT("贴图质量"), UserSettings->GetTextureQuality(), &UGameUserSettings::SetTextureQuality);
        AddOption(TEXT("细节质量"), UserSettings->GetFoliageQuality(), &UGameUserSettings::SetFoliageQuality);
        AddOption(TEXT("特效质量"), UserSettings->GetVisualEffectQuality(), &UGameUserSettings::SetVisualEffectQuality);
    }
    else
    {
        auto* Quality = NewObject<UQualitySettingObject>(this);
        int32 Selected = FMath::Clamp(UserSettings->GetOverallScalabilityLevel(), 0, 3);
        // 移动端保留清晰贴图后，各组不再完全相等，因此单独持久化玩家选择的整体档位。
        GConfig->GetInt(TEXT("XQP.Graphics"), TEXT("OverallQuality"), Selected, GGameUserSettingsIni);
        Selected = FMath::Clamp(Selected, 0, 3);
        Quality->Init(Selected, FText::FromString(TEXT("画面质量")),
            [WeakThis = TWeakObjectPtr<UUI_QualitySettings>(this)](int32 Index)
            {
                if (!WeakThis.IsValid() || !IsValid(WeakThis->UserSettings)) return;
                WeakThis->UserSettings->SetOverallScalabilityLevel(FMath::Clamp(Index, 0, 3));
                GConfig->SetInt(TEXT("XQP.Graphics"), TEXT("OverallQuality"), FMath::Clamp(Index, 0, 3), GGameUserSettingsIni);
                WeakThis->SaveAndApply();
                GConfig->Flush(false, GGameUserSettingsIni);
            });

        QualitySettingsList->AddItem(Quality);
    }

    auto* Effects = NewObject<UQualitySettingObject>(this);
    Effects->Options = { TEXT("关闭"), TEXT("开启") };
    Effects->Init(UChessMoveEffectsComponent::AreChessEffectsEnabled() ? 1 : 0,
        FText::FromString(TEXT("棋子特效")), [](int32 Index)
        {
            UChessMoveEffectsComponent::SetChessEffectsEnabled(Index == 1);
        });
    QualitySettingsList->AddItem(Effects);
}
