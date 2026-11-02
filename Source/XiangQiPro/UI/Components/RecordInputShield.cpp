#include "RecordInputShield.h"
#include "Components/BorderSlot.h"
#include "Widgets/Layout/SBorder.h"

class SRecordInputShield : public SBorder
{
public:
    FSimpleDelegate OnPressed;
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override { OnPressed.ExecuteIfBound(); return FReply::Handled().CaptureMouse(SharedThis(this)); }
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return FReply::Handled().ReleaseMouseCapture(); }
    virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnTouchStarted(const FGeometry&, const FPointerEvent&) override { OnPressed.ExecuteIfBound(); return FReply::Handled().CaptureMouse(SharedThis(this)); }
    virtual FReply OnTouchMoved(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent&) override { return FReply::Handled().ReleaseMouseCapture(); }
};

TSharedRef<SWidget> URecordInputShield::RebuildWidget()
{
    const auto Shield = SNew(SRecordInputShield);
    Shield->OnPressed = FSimpleDelegate::CreateWeakLambda(this, [this]() { OnPressed.ExecuteIfBound(); });
    MyBorder = Shield;
    if (GetChildrenCount() > 0) CastChecked<UBorderSlot>(GetContentSlot())->BuildSlot(MyBorder.ToSharedRef());
    return MyBorder.ToSharedRef();
}

bool URecordInputShield::HasPointerCapture() const
{
    return MyBorder.IsValid() && MyBorder->HasMouseCapture();
}
