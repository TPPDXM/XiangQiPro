// Copyright 2026 Ultimate Player All Rights Reserved.

#include "XQPLANSubsystem.h"
#include "Dom/JsonObject.h"
#include "XiangQiPro/GameMode/XQPGameStateBase.h"
#include "XiangQiPro/GameObject/ChessBoard2P.h"
#include "XiangQiPro/UI/Components/UI_CommonDialog.h"
#include "XiangQiPro/UI/UIManager.h"
#include "XiangQiPro/UI/XQPMobile.h"
#include "XiangQiPro/Util/AssetPaths.h"

namespace
{
    TSharedRef<FJsonObject> UndoPacket(const TCHAR* Type)
    {
        auto M = MakeShared<FJsonObject>();
        M->SetStringField(TEXT("type"), Type);
        return M;
    }

    // 网络数字一律检查有限值、整数和取值范围，再转换到 C++ 类型。
    bool UndoInt(const TSharedPtr<FJsonObject>& M, const TCHAR* Key, int32& Out, int32 Min, int32 Max)
    {
        double Value;
        if (!M->TryGetNumberField(Key, Value) || !FMath::IsFinite(Value) || Value < Min || Value > Max ||
            FMath::FloorToDouble(Value) != Value) return false;
        Out = int32(Value);
        return true;
    }

    bool ReadUndoBoard(const TSharedPtr<FJsonObject>& M, FChessPosition& Board, EChessColor& Turn, int32& Red, int32& Black, int32& Plies)
    {
        const TArray<TSharedPtr<FJsonValue>>* Squares;
        int32 Side, RedUndo, BlackUndo;
        FString Hash;
        if (!M->TryGetArrayField(TEXT("board"), Squares) || Squares->Num() != 90 ||
            !UndoInt(M, TEXT("turn"), Side, 0, 1) || !UndoInt(M, TEXT("red_score"), Red, -100000000, 100000000) ||
            !UndoInt(M, TEXT("black_score"), Black, -100000000, 100000000) || !UndoInt(M, TEXT("plies"), Plies, 1, 2) ||
            !M->TryGetStringField(TEXT("hash"), Hash) || !UndoInt(M, TEXT("red_undo"), RedUndo, 0, 5) ||
            !UndoInt(M, TEXT("black_undo"), BlackUndo, 0, 5)) return false;
        for (int32 I = 0; I < 90; ++I)
        {
            double Value;
            if (!(*Squares)[I]->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value < 0 || Value > 23 ||
                FMath::FloorToDouble(Value) != Value || (int32(Value) & 15) > 7) return false;
            Board.Squares[I] = {static_cast<EChessType>(int32(Value) & 15), static_cast<EChessColor>(int32(Value) >> 4)};
        }
        Turn = static_cast<EChessColor>(Side);
        return Hash == FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Board.Hash(Turn)));
    }
}

bool UXQPLANSubsystem::CanRequestUndo() const
{
    const auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    return GetRemainingUndoCount(GetLocalColor()) > 0 && !bSpectator && GameType == EXQPGameType::Classic && IsBattleReady() && !IsUndoPending() &&
        !bMoveRequested && PendingMoves.IsEmpty() && GS && GS->GetClassicUndoPlies(GetLocalColor()) > 0;
}

void UXQPLANSubsystem::ResetUndo()
{
    if (UndoDialog.IsValid()) UndoDialog->Dismiss();
    UndoDialog.Reset();
    bUndoPending = bUndoAwaitingHost = bUndoIncoming = false;
    PendingUndoSnapshot.Reset();
    PendingUndoPlayedPlies = 0;
    UndoDeadline = 0.;
}

bool UXQPLANSubsystem::RequestUndo()
{
    if (!CanRequestUndo()) return false;
    if (bHost) return StartUndo(GetLocalColor(), Sequence);
    auto M = UndoPacket(TEXT("undo_request"));
    M->SetNumberField(TEXT("seq"), Sequence);
    M->SetNumberField(TEXT("request"), ++UndoClientSerial);
    bUndoAwaitingHost = true;
    UndoDeadline = FPlatformTime::Seconds() + 40.;
    Send(M); // 只给房主对手，绝不调用 BroadcastSpectators。
    return true;
}

bool UXQPLANSubsystem::StartUndo(EChessColor Requester, int32 ExpectedSequence)
{
    const auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    if (GetRemainingUndoCount(Requester) <= 0 || !bHost || bSpectator || GameType != EXQPGameType::Classic || !IsBattleReady() || IsUndoPending() ||
        bMoveRequested || !PendingMoves.IsEmpty() || Sequence != ExpectedSequence || !GS || GS->GetClassicUndoPlies(Requester) == 0) return false;
    bUndoPending = true;
    UndoRequester = Requester;
    UndoSequence = Sequence;
    UndoId = ++UndoSerial;
    UndoDeadline = FPlatformTime::Seconds() + 30.;
    bUndoIncoming = Requester != GetLocalColor();
    auto Offer = UndoPacket(TEXT("undo_offer"));
    Offer->SetNumberField(TEXT("id"), UndoId);
    Offer->SetNumberField(TEXT("seq"), Sequence);
    Offer->SetNumberField(TEXT("requester"), static_cast<uint8>(Requester));
    Send(Offer);
    ShowUndoDialog();
    return true;
}

void UXQPLANSubsystem::ShowUndoDialog()
{
    // 不依赖对手当前是否打开暂停菜单；直接在其本地 UI 栈上展示申请。
    // 观战端永远不会创建协商弹窗，即使收到了非预期的协商消息。
    if (bSpectator || !bUndoPending) return;
    if (UndoDialog.IsValid()) UndoDialog->Dismiss();
    auto Class = LoadClass<UUI_CommonDialog>(nullptr, PATH_UI_COMMON_DIALOG);
    auto* Dialog = XQPMobile::CreateWidget<UUI_CommonDialog>(GetWorld()->GetFirstPlayerController(), Class);
    if (!Dialog) return; // 无 UI 的服务器仍会按超时自动拒绝，不可默认同意。
    UndoDialog = Dialog;
    Dialog->Configure(FText::FromString(bUndoIncoming ? TEXT("对方申请悔棋") : TEXT("已申请悔棋")),
        FText::FromString(bUndoIncoming ? TEXT("是否同意撤回对方上一手及随后应手？同意后由对方重新走棋。") : TEXT("等待对方同意，申请将在 30 秒后自动取消。")),
        FText::FromString(bUndoIncoming ? TEXT("同意") : TEXT("取消申请")),
        bUndoIncoming ? FText::FromString(TEXT("拒绝")) : FText::GetEmpty());
    Dialog->OnPrimaryAction = FSimpleDelegate::CreateWeakLambda(this, [this]() { ReplyToUndo(bUndoIncoming); });
    Dialog->OnSecondaryAction = FSimpleDelegate::CreateWeakLambda(this, [this]() { ReplyToUndo(false); });
    GetGameInstance()->GetSubsystem<UUIManager>()->AddUI(Dialog);
}

void UXQPLANSubsystem::ReplyToUndo(bool bAccept)
{
    if (bSpectator || !bUndoPending || (bAccept && !bUndoIncoming)) return;
    if (bHost) { ResolveUndo(bAccept); return; }
    auto Reply = UndoPacket(TEXT("undo_reply"));
    Reply->SetNumberField(TEXT("id"), UndoId);
    Reply->SetNumberField(TEXT("seq"), UndoSequence);
    Reply->SetBoolField(TEXT("accept"), bAccept);
    Send(Reply);
    if (UndoDialog.IsValid()) UndoDialog->SetPrimaryAction(FText::FromString(TEXT("等待同步…")), false);
}

void UXQPLANSubsystem::ResolveUndo(bool bAccept)
{
    if (!bHost || !bUndoPending) return;
    auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    const int32 Plies = GS ? GS->GetClassicUndoPlies(UndoRequester) : 0;
    const int32 ResolvedId = UndoId;
    const bool bApplied = bAccept && GetRemainingUndoCount(UndoRequester) > 0 && State == EXQPRoomState::Playing && Sequence == UndoSequence && PendingMoves.IsEmpty() &&
        Plies > 0 && GS && GS->UndoClassicMoves(Plies);
    if (bApplied)
    {
        --UndoRemaining[static_cast<uint8>(UndoRequester)];
        AuthoritativeBoard = GS->board2P->CreateSnapshot();
        Turn = GS->GetTurnColor();
        // 传输序号永不倒退，避免旧落子或旧答复在悔棋后重新命中同一个序号。
        auto StateMessage = UndoPacket(TEXT("undo_state"));
        StateMessage->SetNumberField(TEXT("seq"), ++Sequence);
        StateMessage->SetNumberField(TEXT("id"), ResolvedId);
        StateMessage->SetNumberField(TEXT("turn"), static_cast<uint8>(Turn));
        StateMessage->SetNumberField(TEXT("red_score"), GS->GetScore1());
        StateMessage->SetNumberField(TEXT("black_score"), GS->GetScore2());
        StateMessage->SetNumberField(TEXT("plies"), Plies);
        StateMessage->SetNumberField(TEXT("red_undo"), UndoRemaining[0]);
        StateMessage->SetNumberField(TEXT("black_undo"), UndoRemaining[1]);
        StateMessage->SetStringField(TEXT("hash"), FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(AuthoritativeBoard.Hash(Turn))));
        TArray<TSharedPtr<FJsonValue>> Squares;
        for (const auto& Piece : AuthoritativeBoard.Squares)
            Squares.Add(MakeShared<FJsonValueNumber>(static_cast<uint8>(Piece.Type) | (static_cast<uint8>(Piece.Color) << 4)));
        StateMessage->SetArrayField(TEXT("board"), Squares);
        Send(StateMessage);
        // 观战者只接收已生效的棋盘状态，不收到申请、等待、同意或拒绝的弹窗消息。
        BroadcastSpectators(StateMessage);
    }
    else
    {
        auto Result = UndoPacket(TEXT("undo_resolved"));
        Result->SetNumberField(TEXT("id"), ResolvedId);
        Send(Result);
    }
    ResetUndo();
    Status = bApplied ? TEXT("悔棋完成，申请方重新走棋") : TEXT("悔棋申请已拒绝或取消");
    OnRoomUpdated.Broadcast();
}

bool UXQPLANSubsystem::HandleUndoMessage(const TSharedPtr<FJsonObject>& M, const FString& Type)
{
    if (!Type.StartsWith(TEXT("undo_"))) return false;
    // 协商包必须来自棋手 Peer；服务端观战 Socket 入口另有白名单，根本不会转发到这里。
    if (GameType != EXQPGameType::Classic || (bSpectator && Type != TEXT("undo_state"))) return true;
    if (Type == TEXT("undo_request") && bHost)
    {
        int32 Seq, Request;
        if (!UndoInt(M, TEXT("request"), Request, 1, 1000000)) return true;
        if (!UndoInt(M, TEXT("seq"), Seq, 0, 1000000) || !StartUndo(FChessPosition::Opponent(HostColor), Seq))
        {
            auto Denied = UndoPacket(TEXT("undo_denied"));
            Denied->SetNumberField(TEXT("request"), Request);
            Send(Denied);
        }
        return true;
    }
    if (Type == TEXT("undo_denied") && !bHost)
    {
        int32 Request;
        if (bUndoAwaitingHost && UndoInt(M, TEXT("request"), Request, 1, 1000000) && Request == UndoClientSerial)
        {
            ResetUndo();
            Status = TEXT("当前无法悔棋，请等待落子完成后重试");
        }
        return true;
    }
    if (Type == TEXT("undo_offer") && !bHost && State == EXQPRoomState::Playing)
    {
        int32 Id, Seq, Requester;
        if (!UndoInt(M, TEXT("id"), Id, 1, 1000000) || !UndoInt(M, TEXT("seq"), Seq, 0, 1000000) || Seq != Sequence ||
            !UndoInt(M, TEXT("requester"), Requester, 0, 1)) return true;
        ResetUndo();
        UndoId = Id;
        UndoSequence = Seq;
        UndoRequester = static_cast<EChessColor>(Requester);
        bUndoPending = true;
        bUndoIncoming = UndoRequester != GetLocalColor();
        UndoDeadline = FPlatformTime::Seconds() + 40.;
        ShowUndoDialog();
        return true;
    }
    if (Type == TEXT("undo_reply") && bHost)
    {
        int32 Id, Seq;
        bool Accept;
        if (bUndoPending && UndoInt(M, TEXT("id"), Id, 1, 1000000) && Id == UndoId &&
            UndoInt(M, TEXT("seq"), Seq, 0, 1000000) && Seq == UndoSequence && M->TryGetBoolField(TEXT("accept"), Accept) &&
            (!Accept || UndoRequester == GetLocalColor())) ResolveUndo(Accept);
        return true;
    }
    if (Type == TEXT("undo_resolved") && !bHost)
    {
        int32 Id;
        if (bUndoPending && UndoInt(M, TEXT("id"), Id, 1, 1000000) && Id == UndoId)
        {
            ResetUndo();
            Status = TEXT("悔棋申请已拒绝或取消");
            OnRoomUpdated.Broadcast();
        }
        return true;
    }
    if (Type == TEXT("undo_state") && !bHost && State == EXQPRoomState::Playing)
    {
        FChessPosition Board;
        EChessColor Side;
        int32 Seq, Red, Black, Plies;
        if (!UndoInt(M, TEXT("seq"), Seq, 1, 1000000) || Seq != Sequence + 1 || !ReadUndoBoard(M, Board, Side, Red, Black, Plies))
        {
            Disconnect(TEXT("悔棋同步数据无效"));
            return true;
        }
        ResetUndo();
        Sequence = Seq;
        UndoInt(M, TEXT("red_undo"), UndoRemaining[0], 0, 5);
        UndoInt(M, TEXT("black_undo"), UndoRemaining[1], 0, 5);
        AuthoritativeBoard = Board;
        Turn = Side;
        bMoveRequested = false;
        // 尚未播放的旧走法不在 HUD 棋谱中，不能把它们再次从已显示棋谱扣除。
        PendingUndoPlayedPlies = FMath::Max(0, Plies - PendingMoves.Num());
        PendingMoves.Reset(); // 此快照已经覆盖尚未播放的旧落子；之后收到的新走法排在快照之后。
        PendingUndoSnapshot = M;
        Status = TEXT("悔棋完成，申请方重新走棋");
        return true;
    }
    return true; // 过期或重复的答复不改变局面，也不误断开正在继续的对局。
}

void UXQPLANSubsystem::TickUndo(double Now)
{
    if (bUndoPending && Now >= UndoDeadline)
    {
        if (bHost) ResolveUndo(false);
        else { ResetUndo(); Status = TEXT("悔棋申请已超时"); }
    }
    else if (bUndoAwaitingHost && Now >= UndoDeadline)
    {
        ResetUndo();
        Status = TEXT("悔棋申请未获响应");
    }
    if (!PendingUndoSnapshot.IsValid() || !bLocalBoardReady) return;
    auto* GS = GetWorld() ? GetWorld()->GetGameState<AXQPGameStateBase>() : nullptr;
    if (!GS || !GS->CanApplyNetworkMove()) return; // 等当前已开始的动画结束后，才替换 Actor。
    FChessPosition Board;
    EChessColor Side;
    int32 Red, Black, Plies;
    if (!ReadUndoBoard(PendingUndoSnapshot, Board, Side, Red, Black, Plies) || !GS->ApplyClassicUndoSnapshot(Board, Side, Red, Black, PendingUndoPlayedPlies))
    {
        Disconnect(TEXT("无法恢复悔棋局面"));
        return;
    }
    PendingUndoSnapshot.Reset();
    OnRoomUpdated.Broadcast();
}
