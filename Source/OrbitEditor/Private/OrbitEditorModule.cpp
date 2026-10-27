#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "OrbitGravitySource.h"
#include "OrbitGravitySourceComponent.h"
#include "OrbitGravityOrientationComponent.h"
#include "OrbitSettings.h"
#include "OrbitGravityReceiverComponent.h"
#include "OrbitGravityFrame.h"
#include "OrbitGravityTypes.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "Framework/Commands/UIAction.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

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
        StyleSet->Set(TEXT("ClassIcon.OrbitGravitySourceComponent"), new FSlateVectorImageBrush(
            StyleSet->RootToContentDir(TEXT("Icons/Orbit-Icon"), TEXT(".svg")), Icon16));
        StyleSet->Set(TEXT("ClassIcon.OrbitGravityOrientationComponent"), new FSlateVectorImageBrush(
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

        // Extend UE's native actor context menu after ToolMenus is initialized.
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOrbitEditorModule::RegisterMenus));
    }

    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        if (StyleSet.IsValid())
        {
            FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
            StyleSet.Reset();
        }
    }

private:
    static TArray<AActor*> GetSelectedEditorActors()
    {
        TArray<AActor*> Actors;
        if (!GEditor || !GEditor->GetSelectedActors()) { return Actors; }
        for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
        {
            AActor* Actor = Cast<AActor>(*It);
            // Don't accidentally rotate source axes while orienting platforms.
            if (IsValid(Actor) && Actor->GetWorld() &&
                !Actor->GetWorld()->IsGameWorld() &&
                !Actor->IsA<AOrbitGravitySource>())
            {
                Actors.Add(Actor);
            }
        }
        return Actors;
    }

    static bool CanOrientSelectedActors()
    {
        return GetSelectedEditorActors().Num() > 0;
    }

    static void OrientSelectedActorsToGravity()
    {
        const TArray<AActor*> Actors = GetSelectedEditorActors();
        if (Actors.IsEmpty()) { return; }
        FScopedTransaction Transaction(NSLOCTEXT("OrbitEditor", "OrientActorsTransaction",
            "Orient Actors to Orbit Gravity"));
        int32 Changed = 0;
        for (AActor* Actor : Actors)
        {
            UWorld* World = Actor->GetWorld();
            if (!World) { continue; }
            TArray<FOrbitFieldDefinition> Fields;
            for (TActorIterator<AOrbitGravitySource> It(World); It; ++It)
            {
                if (IsValid(*It) && It->bEnabled)
                {
                    FOrbitFieldDefinition& Field = Fields.AddDefaulted_GetRef();
                    It->BuildDefinition(Field);
                }
            }
            // Include fields embedded in ordinary actors as well as dedicated sources.
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                TArray<UOrbitGravitySourceComponent*> Components;
                It->GetComponents<UOrbitGravitySourceComponent>(Components);
                for (UOrbitGravitySourceComponent* Source : Components)
                {
                    if (IsValid(Source) && Source->bEnabled)
                    {
                        FOrbitFieldDefinition& Field = Fields.AddDefaulted_GetRef();
                        Source->BuildDefinition(Field);
                    }
                }
            }
            uint8 ReceiverMask = 1u;
            if (const UOrbitGravityReceiverComponent* Receiver =
                Actor->FindComponentByClass<UOrbitGravityReceiverComponent>())
            {
                ReceiverMask = Receiver->GravityChannels.ToMask();
            }
            const FOrbitGravitySample Sample = FOrbitFieldEvaluator::Combine(
                Fields, Actor->GetActorLocation(), EOrbitGravityChannel::General, ReceiverMask,
                GetDefault<UOrbitSettings>()->OverrideResolution);
            if (!Sample.bHasInfluence || !Sample.bHasValidDirection) { continue; }
            FQuat Rotation;
            if (!FOrbitGravityFrame::MakeAlignedRotation(Sample.Up, Actor->GetActorQuat(), Rotation))
            {
                continue;
            }
            if (Actor->GetActorQuat().Equals(Rotation, 0.0001)) { continue; }
            Actor->Modify();
            Actor->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
            Actor->PostEditMove(true);
            ++Changed;
        }
        if (Changed == 0) { Transaction.Cancel(); }
    }

    void RegisterMenus()
    {
        FToolMenuOwnerScoped OwnerScoped(this);
        UToolMenu* ActorMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.ActorContextMenu"));
        if (!ActorMenu) { return; }
        FToolMenuSection& Section = ActorMenu->FindOrAddSection(
            TEXT("OrbitGravityActions"), NSLOCTEXT("OrbitEditor", "GravityActions", "Orbit"));
        Section.AddMenuEntry(
            TEXT("OrbitOrientSelectedActorsToGravity"),
            NSLOCTEXT("OrbitEditor", "OrientToGravity", "Orient to Gravity"),
            NSLOCTEXT("OrbitEditor", "OrientToGravityToolTip",
                "Align selected actors' local +Z to the effective Orbit gravity Up at each actor's pivot. "
                "Keeps their heading as closely as possible; supports Undo. Requires an active, matching gravity source."),
            FSlateIcon(TEXT("OrbitEditorStyle"), TEXT("Orbit.Icon16")),
            FUIAction(FExecuteAction::CreateStatic(&FOrbitEditorModule::OrientSelectedActorsToGravity),
                FCanExecuteAction::CreateStatic(&FOrbitEditorModule::CanOrientSelectedActors)));
    }

    TSharedPtr<FSlateStyleSet> StyleSet;
};

IMPLEMENT_MODULE(FOrbitEditorModule, OrbitEditor)
