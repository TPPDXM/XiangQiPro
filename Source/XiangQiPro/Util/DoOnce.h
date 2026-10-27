// Copyright 2026 Ultimate Player All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

template <typename FunctionType>
struct FDoOnce;

// 游戏线程内的一次性调用器，统一支持有返回值和void函数。
template <typename Ret, typename... ParamTypes>
struct FDoOnce<Ret(ParamTypes...)>
{
public:
    // 创建未绑定工作函数的一次性调用器。
    FDoOnce() = default;

    // 保存待执行工作函数。
    explicit FDoOnce(TFunction<Ret(ParamTypes...)> Function)
        : WorkFunction(MoveTemp(Function))
    {
    }

    // 替换工作函数，保留当前是否允许执行的状态。
    void SetWorkFunction(TFunction<Ret(ParamTypes...)> Function)
    {
        WorkFunction = MoveTemp(Function);
    }

    // 清除函数并恢复可执行标志。
    void Reset()
    {
        bDoOnce = true;
        WorkFunction = nullptr;
    }

    // 设置是否允许再次执行。
    void SetExecutable(bool bNewDoOnce)
    {
        bDoOnce = bNewDoOnce;
    }

    // 查询当前是否允许执行。
    bool IsExecutable() const
    {
        return bDoOnce;
    }

    // 执行前关闭入口，并保留闭包副本防止回调清理调用器自身。
    Ret Execute(ParamTypes... Params)
    {
        if (!bDoOnce || !WorkFunction) return Ret();
        bDoOnce = false;
        const TFunction<Ret(ParamTypes...)> Function = WorkFunction;
        return Function(Forward<ParamTypes>(Params)...);
    }

private:
    bool bDoOnce = true;
    TFunction<Ret(ParamTypes...)> WorkFunction;
};
