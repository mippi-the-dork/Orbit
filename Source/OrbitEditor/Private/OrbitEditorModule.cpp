#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Misc/Paths.h"

/**
 * Orbit's runtime module has no Slate or editor dependencies.
 * All class-icon and thumbnail registrations live in this Editor-only module.
 */
class FOrbitEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Orbit"));
        if (!Plugin.IsValid())
        {
            return;
        }

        StyleSet = MakeShared<FSlateStyleSet>(TEXT("OrbitEditorStyle"));
        StyleSet->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));

        const FVector2D Icon16(16.0f, 16.0f);
        const FVector2D Icon20(20.0f, 20.0f);
        const FVector2D Icon32(32.0f, 32.0f);
        const FVector2D Thumbnail64(64.0f, 64.0f);

        // Class icon keys omit the native A/U prefix. Unreal uses these for
        // the actor in the World Outliner and for component class pickers.
        StyleSet->Set(TEXT("ClassIcon.OrbitGravitySource"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/OrbitActor_16"), TEXT(".svg")), Icon16));
        StyleSet->Set(TEXT("ClassIcon.OrbitCharacterGravityComponent"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/Orbit-Icon"), TEXT(".svg")), Icon16));
        StyleSet->Set(TEXT("ClassIcon.OrbitCharacterControlsComponent"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/Orbit-Icon"), TEXT(".svg")), Icon16));
        StyleSet->Set(TEXT("ClassIcon.OrbitGravityReceiverComponent"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/Orbit-Icon"), TEXT(".svg")), Icon16));
        // Optional OrbitMover companion is registered by class name only; this Editor module
        // has no compile-time dependency on Epic Mover or the OrbitMover plugin.
        StyleSet->Set(TEXT("ClassIcon.OrbitMoverGravityComponent"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/Orbit-Icon"), TEXT(".svg")), Icon16));

        // A gravity source is a placeable actor, not a custom Content Browser
        // asset type. Its class/Blueprint thumbnail can still use the 64px
        // artwork; this does not register a nonexistent asset factory.
        StyleSet->Set(TEXT("ClassThumbnail.OrbitGravitySource"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/OrbitAsset_64"), TEXT(".svg")), Thumbnail64));

        // Reserved style brushes for future Orbit menus, tabs and toolbars.
        // The general icon is also used by the existing Add Component picker.
        StyleSet->Set(TEXT("Orbit.Icon16"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/Orbit-Icon"), TEXT(".svg")), Icon16));
        StyleSet->Set(TEXT("Orbit.Icon20"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/Orbit-Icon"), TEXT(".svg")), Icon20));
        StyleSet->Set(TEXT("Orbit.Icon32"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/Orbit-Icon"), TEXT(".svg")), Icon32));

        FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
    }

    virtual void ShutdownModule() override
    {
        if (StyleSet.IsValid())
        {
            FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
            StyleSet.Reset();
        }
    }

private:
    TSharedPtr<FSlateStyleSet> StyleSet;
};

IMPLEMENT_MODULE(FOrbitEditorModule, OrbitEditor)
