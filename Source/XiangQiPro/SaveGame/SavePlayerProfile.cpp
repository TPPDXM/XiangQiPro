// Copyright 2026 Ultimate Player All Rights Reserved.


#include "SavePlayerProfile.h"

void USavePlayerProfile::MigrateLegacyData()
{
    if (Version != 1) return;
    auto& Progress = ModeProgress.FindOrAdd(EXQPGameMode::AI2P);
    Progress.BestStreak = BestStreak;
    Progress.CurrentStreak = CurrentStreak;
    Progress.bRunActive = bRunActive;
    Progress.RunDifficulty = RunDifficulty;
    Progress.SavedBoard = MoveTemp(SavedBoard);
    Progress.SavedTurn = SavedTurn;
    Progress.SavedRedMoveScore = SavedRedMoveScore;
    Progress.SavedBlackMoveScore = SavedBlackMoveScore;
    auto& Learning = ModeLearning.FindOrAdd(EXQPGameMode::AI2P);
    Learning.PieceCounts = MoveTemp(PieceCounts);
    Learning.FileCounts = MoveTemp(FileCounts);
    Learning.ObservedMoves = ObservedMoves;
    Learning.CaptureMoves = CaptureMoves;
    Learning.ForwardMoves = ForwardMoves;
    Learning.CheckMoves = CheckMoves;
    Learning.PositionMoveCounts = MoveTemp(PositionMoveCounts);
    Learning.AICaution = MoveTemp(AICaution);
    Learning.HarmfulReplies = MoveTemp(HarmfulReplies);
    Learning.LastAIMoveKey = MoveTemp(LastAIMoveKey);
    Learning.LastPlayerReplyKey = MoveTemp(LastPlayerReplyKey);
    Learning.LastExpectedScore = LastExpectedScore;
    Version = 2;
}

