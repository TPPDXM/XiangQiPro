#pragma once

#include "CoreMinimal.h"
#include "XiangQiPro/Util/ChessInfo.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "UI_ProResult.generated.h"

class UProBattleComponent;

UCLASS()
class XIANGQIPRO_API UUI_ProResult : public UUI_CommonDialog
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    void ShowBattle(const UProBattleComponent* Battle, const FText& RedName, const FText& BlackName, EChessColor Winner);
private:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> RedSummary;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> BlackSummary;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> RedPlayerName;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> BlackPlayerName;
};
