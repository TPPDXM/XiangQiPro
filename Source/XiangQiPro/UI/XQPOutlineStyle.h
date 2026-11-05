#pragma once
class UUserWidget;
namespace XQPUIStyle
{
    /** 覆盖整棵嵌套UMG树的移动端浅色描边规则，重复构造不累乘。 */
    void ApplyMobileOutlines(UUserWidget* Widget);
}
