#include "OrbitSourceDirectionDetails.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravitySource.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

TSharedRef<IDetailCustomization> FOrbitSourceDirectionDetails::MakeInstance()
{
    return MakeShared<FOrbitSourceDirectionDetails>();
}

void FOrbitSourceDirectionDetails::CustomizeDetails(IDetailLayoutBuilder& Builder)
{
    // A single editor-only presentation for both the Actor and Component.
    // Use property handles: they retain native serialization, conditions, undo/redo,
    // reset-to-default, multi-selection, and Blueprint instance overrides.
    ShapeHandle = Builder.GetProperty(TEXT("Shape"));
    DirectionHandle = Builder.GetProperty(TEXT("Direction"));
    VolumeDirectionModeHandle = Builder.GetProperty(TEXT("VolumeDirectionMode"));
    if (!ShapeHandle.IsValid() || !DirectionHandle.IsValid() || !DirectionHandle->IsValidHandle())
    {
        return;
    }

    // A single collapsible Orbit category with native, collapsible detail groups.
    // Do not create categories named Orbit|Gravity etc: Unreal presents those
    // as separate top-level panels in the Details view.
    IDetailCategoryBuilder& Orbit = Builder.EditCategory(TEXT("Orbit"),
        FText::FromString(TEXT("Orbit")), ECategoryPriority::Important);

    auto AddIfPresent = [&Builder](IDetailGroup& Group, const TCHAR* Name)
    {
        const TSharedRef<IPropertyHandle> Handle = Builder.GetProperty(FName(Name));
        if (Handle->IsValidHandle())
        {
            Group.AddPropertyRow(Handle);
        }
    };
    auto MakeGroup = [&Orbit](const TCHAR* Id, const TCHAR* Label, bool bExpanded = false) -> IDetailGroup&
    {
        return Orbit.AddGroup(FName(Id), FText::FromString(Label), false, bExpanded);
    };

    IDetailGroup& Gravity = MakeGroup(TEXT("Gravity"), TEXT("Gravity"), true);
    AddIfPresent(Gravity, TEXT("bEnabled"));
    AddIfPresent(Gravity, TEXT("Shape"));

    Builder.HideProperty(DirectionHandle);
    Gravity.AddWidgetRow()
        .NameContent()[DirectionHandle->CreatePropertyNameWidget()]
        .ValueContent()
        .MinDesiredWidth(190.0f)
        [
            SNew(SComboButton)
            .ToolTipText(FText::FromString(TEXT("Direction of gravity. Along/Reverse Spline Tangent are available only for Spline fields. Volume Direction Mode overrides this control unless set to Directional (Legacy).")))
            .IsEnabled(this, &FOrbitSourceDirectionDetails::IsDirectionEditable)
            .OnGetMenuContent(this, &FOrbitSourceDirectionDetails::MakeDirectionMenu)
            .ButtonContent()
            [SNew(STextBlock).Text(this, &FOrbitSourceDirectionDetails::GetDirectionLabel)]
        ];
    AddIfPresent(Gravity, TEXT("Strength"));
    AddIfPresent(Gravity, TEXT("Falloff"));
    AddIfPresent(Gravity, TEXT("MaxDistance"));
    AddIfPresent(Gravity, TEXT("InverseSquareReferenceDistance"));

    IDetailGroup& Targets = MakeGroup(TEXT("AffectedObjects"), TEXT("Affected Objects"), true);
    AddIfPresent(Targets, TEXT("bAffectCharacters"));
    AddIfPresent(Targets, TEXT("bAffectPhysics"));
    AddIfPresent(Targets, TEXT("GravityChannels"));

    IDetailGroup& Blending = MakeGroup(TEXT("OverlappingFields"), TEXT("Overlapping Fields"));
    AddIfPresent(Blending, TEXT("Combine"));
    AddIfPresent(Blending, TEXT("Priority"));

    IDetailGroup& Spline = MakeGroup(TEXT("SplineShape"), TEXT("Spline Shape"));
    // Actor owns its native spline in the component tree; embedded components
    // instead expose an editable reference to the target spline.
    if (Builder.GetProperty(TEXT("GravitySpline"))->IsValidHandle() &&
        !Builder.GetProperty(TEXT("SceneRoot"))->IsValidHandle())
    {
        AddIfPresent(Spline, TEXT("GravitySpline"));
    }
    AddIfPresent(Spline, TEXT("SplineSampleSpacing"));
    AddIfPresent(Spline, TEXT("SplineMaxSegments"));
    AddIfPresent(Spline, TEXT("SplineUpdateMode"));
    AddIfPresent(Spline, TEXT("SplineCheckInterval"));

    IDetailGroup& Volume = MakeGroup(TEXT("VolumeShape"), TEXT("Volume Shape"));
    AddIfPresent(Volume, TEXT("VolumeShape"));
    AddIfPresent(Volume, TEXT("VolumeDirectionMode"));
    AddIfPresent(Volume, TEXT("VolumeRadius"));
    AddIfPresent(Volume, TEXT("VolumeHalfHeight"));

    IDetailGroup& Box = MakeGroup(TEXT("BoxLimit"), TEXT("Additional Box Limit"));
    AddIfPresent(Box, TEXT("bLimitToBox"));
    AddIfPresent(Box, TEXT("BoxCornerA"));
    AddIfPresent(Box, TEXT("BoxCornerB"));

    IDetailGroup& Vortex = MakeGroup(TEXT("Vortex"), TEXT("Vortex"));
    AddIfPresent(Vortex, TEXT("VortexSpin"));
    AddIfPresent(Vortex, TEXT("VortexPhysicsMode"));

    IDetailGroup& Capture = MakeGroup(TEXT("SpiralCapture"), TEXT("Vortex - Spiral Capture"));
    AddIfPresent(Capture, TEXT("VortexConvergenceRate"));
    AddIfPresent(Capture, TEXT("VortexAngularRate"));
    AddIfPresent(Capture, TEXT("VortexResponseRate"));
    AddIfPresent(Capture, TEXT("VortexMaxFlowSpeed"));
    AddIfPresent(Capture, TEXT("VortexMaxAccelerationFactor"));

    IDetailGroup& Visualization = MakeGroup(TEXT("ViewportVisualization"), TEXT("Viewport Visualization"));
    for (const TCHAR* Name : {TEXT("bShowDebugInGame"), TEXT("bShowGravityPreview"),
         TEXT("bShowVolumeBounds"), TEXT("bShowMaxDistanceBoundary"),
         TEXT("bFitPreviewToMaxDistance"), TEXT("PreviewDensity"),
         TEXT("PreviewMaxArrows"), TEXT("PreviewExtent"),
         TEXT("PreviewArrowLength"), TEXT("bScalePreviewByStrength")})
    {
        AddIfPresent(Visualization, Name);
    }

    IDetailGroup& Bounds = MakeGroup(TEXT("BoundaryAppearance"), TEXT("Boundary Appearance"));
    for (const TCHAR* Name : {TEXT("LimitBoxColor"), TEXT("LimitBoxThickness"),
         TEXT("MaxDistanceColor"), TEXT("MaxDistanceThickness")})
    {
        AddIfPresent(Bounds, Name);
    }

    IDetailGroup& Arrows = MakeGroup(TEXT("ArrowAppearance"), TEXT("Arrow Appearance"));
    AddIfPresent(Arrows, TEXT("PreviewArrowColor"));
    AddIfPresent(Arrows, TEXT("PreviewArrowThickness"));

    IDetailGroup& Streamlines = MakeGroup(TEXT("StreamlineAppearance"), TEXT("Streamline Appearance"));
    for (const TCHAR* Name : {TEXT("bShowStreamlines"), TEXT("StreamlineColor"),
         TEXT("StreamlineDensity"), TEXT("StreamlineMaxLines"),
         TEXT("StreamlineSteps"), TEXT("StreamlineLength"),
         TEXT("StreamlineMaxSegments"), TEXT("bShowStreamlineDirectionMarkers"),
         TEXT("StreamlineThickness")})
    {
        AddIfPresent(Streamlines, Name);
    }

    IDetailGroup& Diagnostics = MakeGroup(TEXT("PIEDiagnostics"), TEXT("PIE Diagnostics"));
    AddIfPresent(Diagnostics, TEXT("bMirrorEditorChangesToPIE"));
    AddIfPresent(Diagnostics, TEXT("LastEditorPIEMirrorCount"));

    IDetailGroup& Components = MakeGroup(TEXT("InternalComponents"), TEXT("Internal Components"));
    for (const TCHAR* Name : {TEXT("SceneRoot"), TEXT("GravitySpline"),
         TEXT("DirectionArrow"), TEXT("OrbitBillboard"), TEXT("BoundarySelection")})
    {
        AddIfPresent(Components, Name);
    }

}

bool FOrbitSourceDirectionDetails::IsDirectionEditable() const
{
    if (!ShapeHandle.IsValid() || !DirectionHandle.IsValid() ||
        !ShapeHandle->IsValidHandle() || !DirectionHandle->IsValidHandle()) { return false; }
    uint8 Shape = 0;
    if (ShapeHandle->GetValue(Shape) != FPropertyAccess::Success) { return false; }
    if (static_cast<EOrbitGravityShape>(Shape) != EOrbitGravityShape::Volume) { return true; }
    uint8 Mode = 0;
    return VolumeDirectionModeHandle.IsValid() &&
        VolumeDirectionModeHandle->GetValue(Mode) == FPropertyAccess::Success &&
        static_cast<EOrbitVolumeDirectionMode>(Mode) == EOrbitVolumeDirectionMode::Directional;
}

bool FOrbitSourceDirectionDetails::IsSplineShape() const
{
    uint8 Shape = 0;
    return ShapeHandle.IsValid() && ShapeHandle->GetValue(Shape) == FPropertyAccess::Success &&
        static_cast<EOrbitGravityShape>(Shape) == EOrbitGravityShape::Spline;
}

FText FOrbitSourceDirectionDetails::GetDirectionLabel() const
{
    uint8 Value = 0;
    if (!DirectionHandle.IsValid() || DirectionHandle->GetValue(Value) != FPropertyAccess::Success)
    {
        return FText::FromString(TEXT("Multiple Values"));
    }
    const UEnum* Enum = StaticEnum<EOrbitGravityDirection>();
    return Enum ? Enum->GetDisplayNameTextByValue(Value) : FText::GetEmpty();
}

TSharedRef<SWidget> FOrbitSourceDirectionDetails::MakeDirectionMenu()
{
    FMenuBuilder MenuBuilder(true, nullptr);
    const UEnum* Enum = StaticEnum<EOrbitGravityDirection>();
    if (!Enum || !DirectionHandle.IsValid()) { return MenuBuilder.MakeWidget(); }

    const bool bSpline = IsSplineShape();
    for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
    {
        const int64 EnumValue = Enum->GetValueByIndex(Index);
        if (EnumValue < 0 || EnumValue > MAX_uint8 || Enum->HasMetaData(TEXT("Hidden"), Index))
        {
            continue;
        }
        const EOrbitGravityDirection Option = static_cast<EOrbitGravityDirection>(EnumValue);
        if (!bSpline && OrbitIsSplineTangentDirection(Option)) { continue; }
        const TSharedPtr<IPropertyHandle> Handle = DirectionHandle;
        MenuBuilder.AddMenuEntry(Enum->GetDisplayNameTextByIndex(Index), FText::GetEmpty(), FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([Handle, EnumValue]()
            {
                if (Handle.IsValid()) { Handle->SetValue(static_cast<uint8>(EnumValue)); }
            })));
    }
    return MenuBuilder.MakeWidget();
}
