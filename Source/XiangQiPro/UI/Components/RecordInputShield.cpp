#include "RecordInputShield.h"
#include "Components/BorderSlot.h"
#include "Widgets/Layout/SBorder.h"

class SRecordInputShield : public SBorder
{
public:
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override { return FReply::Handled().CaptureMouse(SharedThis(this)); }
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return FReply::Handled().ReleaseMouseCapture(); }
    virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnTouchStarted(const FGeometry&, const FPointerEvent&) override { return FReply::Handled().CaptureMouse(SharedThis(this)); }
    virtual FReply OnTouchMoved(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent&) override { return FReply::Handled().ReleaseMouseCapture(); }
};

TSharedRef<SWidget> URecordInputShield::RebuildWidget()
{
    MyBorder = SNew(SRecordInputShield);
    if (GetChildrenCount() > 0) CastChecked<UBorderSlot>(GetContentSlot())->BuildSlot(MyBorder.ToSharedRef());
    return MyBorder.ToSharedRef();
}
