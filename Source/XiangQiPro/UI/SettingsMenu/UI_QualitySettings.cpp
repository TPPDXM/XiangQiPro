#include "UI_QualitySettings.h"
#include "Components/ListView.h"
#include "Misc/ConfigCacheIni.h"

// 单一画质档位同步所有效果组；移动端原生分辨率约束由SaveAndApply统一落实。
void UUI_QualitySettings::NativeConstruct()
{
    Super::NativeConstruct();
    if (!IsValid(UserSettings) || !IsValid(QualitySettingsList)) return;
    QualitySettingsList->ClearListItems();
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
            WeakThis->SaveAndApply();
            GConfig->SetInt(TEXT("XQP.Graphics"), TEXT("OverallQuality"), FMath::Clamp(Index, 0, 3), GGameUserSettingsIni);
            GConfig->Flush(false, GGameUserSettingsIni);
        });
    QualitySettingsList->AddItem(Quality);
}
