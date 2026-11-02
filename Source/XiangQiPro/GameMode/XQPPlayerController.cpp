// Copyright 2026 Ultimate Player All Rights Reserved.


#include "XQPPlayerController.h"
#include "XiangQiPro/Util/Logger.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XQPGameStateBase.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/Camera/MobileMenuCamera.h"
#include "XiangQiPro/Chess/Chesses.h"
#include "XiangQiPro/GameObject/ChessBoard2P.h"
#include "XiangQiPro/GameObject/SettingPoint.h"
#include "Blueprint/WidgetLayoutLibrary.h"

// 进入场景后初始化本对象依赖。
void AXQPPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 显示鼠标、启用点击事件、启用滑动事件
	this->bEnableClickEvents = true;
	this->bEnableMouseOverEvents = true;
	this->bShowMouseCursor = !XQPMobile::IsPortrait(this);
	this->bEnableTouchEvents = true;
	this->bEnableTouchOverEvents = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
}

// 绑定可在暂停期间使用的退出操作。
void AXQPPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 绑定Esc按键
	if (IsValid(InputComponent)) InputComponent->BindAction(TEXT("Escape"), IE_Pressed, this, &AXQPPlayerController::OnEscapePressed).bExecuteWhenPaused = true;
}

// 切换顶层界面或暂停菜单。
void AXQPPlayerController::OnEscapePressed()
{
	if (!GetGameInstance()) return;
    CancelMobileBoardGesture();
	ULogger::Log(TEXT("AXQPPlayerController::OnEscapePressed"));
	if (UUIManager* UIManager = GetGameInstance()->GetSubsystem<UUIManager>())
	{
		// 图标和Esc共用菜单栈；GameState在联机模式下不会暂停棋盘。
		UIManager->FinishUI();
	}
}


bool AXQPPlayerController::CanUseMobileBoardInput() const
{
    if (!GetWorld() || GetWorld()->IsPaused() || !GetGameInstance()) return false;
    const auto* Manager = GetGameInstance()->GetSubsystem<UUIManager>();
    if (!Manager || Manager->HasOverlayUI()) return false;
    const UUserWidget* Basic = Manager->GetBasicUI();
    return Basic && Basic->IsVisible() && Basic->GetIsEnabled();
}

void AXQPPlayerController::SetHoveredDropPoint(ASettingPoint* Point)
{
    if (HoveredDropPoint.Get() == Point) return;
    if (HoveredDropPoint.IsValid()) HoveredDropPoint->HandleHoverEnd(nullptr);
    HoveredDropPoint = Point;
    if (Point) Point->HandleHoverStart(nullptr);
}

void AXQPPlayerController::CancelMobileBoardGesture(bool bAllowMenuInertia)
{
    SetHoveredDropPoint(nullptr);
    ActiveBoardContact.Reset();
    TouchChess.Reset();
    if (TouchMenu.IsValid()) TouchMenu->EndBoardInteraction(bAllowMenuInertia);
    TouchMenu.Reset();
    TouchSource = FIntPoint(-1, -1);
    bBoardTouchMoved = bSelectedBeforeTouch = bTouchBeganOnPiece = false;
    // 只取消手势，不取消棋子选择。拖到无效位置或被界面打断后，仍可继续用第二次点击落子。
}

ASettingPoint* AXQPPlayerController::FindTouchDropPoint(const FVector2D& ScreenPosition) const
{
    const auto* State = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    const UChessBoard2P* Board = State ? State->board2P.Get() : nullptr;
    if (!Board || !TouchChess.IsValid() || Board->BoardLocs.Num() != 10 ||
        Board->BoardLocs[0].Num() != 9 || Board->BoardLocs[1].Num() != 9) return nullptr;
    FVector Origin, Direction;
    if (!DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, Origin, Direction)) return nullptr;
    const FVector Base = Board->BoardLocs[0][0];
    const FVector RowStep = Board->BoardLocs[1][0] - Base;
    const FVector ColumnStep = Board->BoardLocs[0][1] - Base;
    const FVector Normal = FVector::CrossProduct(RowStep, ColumnStep).GetSafeNormal();
    const double Denominator = FVector::DotProduct(Direction, Normal);
    if (RowStep.IsNearlyZero() || ColumnStep.IsNearlyZero() || FMath::IsNearlyZero(Denominator)) return nullptr;
    const double RayDistance = FVector::DotProduct(Base - Origin, Normal) / Denominator;
    if (RayDistance < 0.) return nullptr;
    const FVector Local = Origin + Direction * RayDistance - Base;
    const double Row = FVector::DotProduct(Local, RowStep) / RowStep.SizeSquared();
    const double Column = FVector::DotProduct(Local, ColumnStep) / ColumnStep.SizeSquared();
    // 格子边界保留少量空隙，不把滑在两格之间或棋盘外的释放吸附到相邻落点。
    if (Row < -.48 || Row > 9.48 || Column < -.48 || Column > 8.48) return nullptr;
    const int32 X = FMath::RoundToInt(Row), Y = FMath::RoundToInt(Column);
    if (FMath::Abs(Row - X) > .48 || FMath::Abs(Column - Y) > .48 ||
        !Board->SettingPoints.IsValidIndex(X) || !Board->SettingPoints[X].IsValidIndex(Y)) return nullptr;
    ASettingPoint* Point = Board->SettingPoints[X][Y].Get();
    return Point && Point->IsActiveForMove() && Point->GetTargetChess() == TouchChess ? Point : nullptr;
}

void AXQPPlayerController::ProcessTouchHitResult(FTouchId TouchId, ETouchType::Type Type,
    const FVector2D& TouchLocation, const FHitResult& HitResult, bool bHit)
{
    auto* Menu = Cast<AMobileMenuCamera>(GetViewTarget());
    const auto* State = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    if (!XQPMobile::IsPortrait(this) || (!Menu && (!State || !State->board2P)))
    {
        Super::ProcessTouchHitResult(TouchId, Type, TouchLocation, HitResult, bHit);
        return;
    }

    // 只有已经经过 Slate 且到达游戏视口的触摸才进入此入口，按钮继续使用原 UMG 事件。
    // 这里不再分发棋子/落点的旧 TouchEnd，防止按下选中后抬手又切换选择或重复落子。
    if (Type == ETouchType::Began)
    {
        BoardContacts.Add(TouchId);
        if (BoardContacts.Num() != 1)
        {
            bRejectBoardContacts = true;
            CancelMobileBoardGesture();
            return;
        }
        CancelMobileBoardGesture();
        if (bRejectBoardContacts || !CanUseMobileBoardInput()) return;
        ActiveBoardContact = TouchId;
        TouchStart = PreviousBoardTouch = TouchLocation;
        if (Menu)
        {
            if (Menu->IsBoardTouchPosition(TouchLocation))
            {
                TouchMenu = Menu;
                Menu->BeginBoardInteraction();
            }
            return;
        }
        if (!State->IsMyTurn()) return;
        if (auto* Chess = bHit ? Cast<AChesses>(HitResult.GetActor()) : nullptr)
        {
            bSelectedBeforeTouch = Chess->IsSelectedForMove();
            if (Chess->BeginTouchMoveSelection())
            {
                TouchChess = Chess;
                TouchSource = Chess->GetPosition();
                bTouchBeganOnPiece = true;
            }
        }
        else if (auto* Point = bHit ? Cast<ASettingPoint>(HitResult.GetActor()) : nullptr)
        {
            // 第二次点击落点也走同一个提交入口，按下时不提交，抬手时再次检查实时合法性。
            if (Point->IsActiveForMove() && Point->GetTargetChess().IsValid())
            {
                TouchChess = Point->GetTargetChess();
                TouchSource = TouchChess->GetPosition();
                SetHoveredDropPoint(Point);
            }
        }
        if (!TouchChess.IsValid())
        {
            // 第二次点击吃子点时，手指射线可能先命中对方棋子的表面。
            // 从当前合法落点找回原选中棋子，再按格子判定，避免要求用户精确点中小球碰撞体。
            for (const auto& Row : State->board2P->SettingPoints)
            {
                for (const auto& Candidate : Row)
                {
                    if (!Candidate.IsValid() || !Candidate->IsActiveForMove() || !Candidate->GetTargetChess().IsValid()) continue;
                    TouchChess = Candidate->GetTargetChess();
                    TouchSource = TouchChess->GetPosition();
                    if (auto* Point = FindTouchDropPoint(TouchLocation)) SetHoveredDropPoint(Point);
                    else { TouchChess.Reset(); TouchSource = FIntPoint(-1, -1); }
                    return;
                }
            }
        }
        return;
    }

    if (Type == ETouchType::Ended)
    {
        const bool WasActive = ActiveBoardContact.IsSet() && ActiveBoardContact.GetValue() == TouchId;
        const float Slop = 8.f * FMath::Max(.5f, UWidgetLayoutLibrary::GetViewportScale(this));
        if (WasActive && FVector2D::DistSquared(TouchLocation, TouchStart) >= FMath::Square(Slop)) bBoardTouchMoved = true;
        BoardContacts.Remove(TouchId);
        if (WasActive && !bRejectBoardContacts && CanUseMobileBoardInput() && TouchChess.IsValid() &&
            State && State->IsMyTurn() && !TouchChess->IsDead() && !TouchChess->IsRealtimeMoving() &&
            TouchChess->IsSelectedForMove() && TouchChess->GetPosition() == TouchSource)
        {
            if (ASettingPoint* Point = FindTouchDropPoint(TouchLocation))
            {
                // 与两次点击、局域网及烽火弈共用原始入口，最终权限和规则仍由 GameState 校验。
                Point->HandleClick(nullptr, EKeys::LeftMouseButton);
            }
            else if (bTouchBeganOnPiece && bSelectedBeforeTouch && !bBoardTouchMoved)
            {
                // 已选棋子的原地短按沿用旧的再次点击取消选择；首次按下则保留选中和提示。
                TouchChess->HandleClick();
            }
        }
        if (WasActive) CancelMobileBoardGesture(!bRejectBoardContacts && CanUseMobileBoardInput());
        if (BoardContacts.IsEmpty()) bRejectBoardContacts = false;
        return;
    }

    if (!ActiveBoardContact.IsSet() || ActiveBoardContact.GetValue() != TouchId || bRejectBoardContacts) return;
    if (!CanUseMobileBoardInput()) { CancelMobileBoardGesture(); return; }
    const float Slop = 8.f * FMath::Max(.5f, UWidgetLayoutLibrary::GetViewportScale(this));
    if (!bBoardTouchMoved && FVector2D::DistSquared(TouchLocation, TouchStart) >= FMath::Square(Slop))
        bBoardTouchMoved = true;
    if (TouchMenu.IsValid() && bBoardTouchMoved) TouchMenu->RotateBoard(TouchLocation - PreviousBoardTouch);
    if (TouchChess.IsValid()) SetHoveredDropPoint(FindTouchDropPoint(TouchLocation));
    PreviousBoardTouch = TouchLocation;
}

void AXQPPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (BoardContacts.IsEmpty()) return;
    // 手指在 UI 上抬起、应用失焦或弹窗打断时，释放可能不会再次经过游戏拾取。
    // 每帧清理已结束的接触，只取消手势，绝不把中断补做成一次落子。
    for (auto It = BoardContacts.CreateIterator(); It; ++It)
    {
        float X = 0.f, Y = 0.f; bool Pressed = false;
        GetInputTouchState(It->GetIndex(), X, Y, Pressed);
        if (!Pressed)
        {
            if (ActiveBoardContact.IsSet() && ActiveBoardContact.GetValue() == *It) CancelMobileBoardGesture();
            It.RemoveCurrent();
        }
    }
    if (ActiveBoardContact.IsSet() && !CanUseMobileBoardInput()) CancelMobileBoardGesture();
    if (BoardContacts.IsEmpty()) bRejectBoardContacts = false;
}

void AXQPPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelMobileBoardGesture();
    BoardContacts.Reset();
    Super::EndPlay(EndPlayReason);
}
