#include "DNGMatchWidget.h"

#include "../Gameplay/Player/DNGPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Fonts/SlateFontInfo.h"

namespace
{
	UTextBlock* MakeMatchLabel(UWidgetTree* WidgetTree, const FString& Text, int32 FontSize = 14)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo FontInfo = Label->GetFont();
		FontInfo.Size = FontSize;
		Label->SetFont(FontInfo);
		return Label;
	}

	UButton* MakeMatchButton(UWidgetTree* WidgetTree, const FString& Text)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* Label = MakeMatchLabel(WidgetTree, Text, 16);
		Label->SetJustification(ETextJustify::Center);
		Button->AddChild(Label);
		return Button;
	}
}

// Binds button handlers and creates the fallback HUD when no Blueprint tree exists.
void UDNGMatchWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
}

// Polls the owning controller each frame so the HUD always reflects replicated state.
void UDNGMatchWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromController();
}

// Builds the prototype HUD entirely in C++ for projects that do not yet use Blueprint UI.
void UDNGMatchWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetPadding(FMargin(18.0f));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.78f));

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	PanelSlot->SetPosition(FVector2D(24.0f, 24.0f));
	PanelSlot->SetSize(FVector2D(430.0f, 560.0f));

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Panel->SetContent(Content);

	PhaseText = MakeMatchLabel(WidgetTree, TEXT("Phase:"), 22);
	RoleText = MakeMatchLabel(WidgetTree, TEXT("Role:"), 18);
	ScoreText = MakeMatchLabel(WidgetTree, TEXT("Score:"), 15);
	PromptText = MakeMatchLabel(WidgetTree, TEXT("Prompt:"), 14);
	ResultText = MakeMatchLabel(WidgetTree, TEXT("Result:"), 14);
	InstructionText = MakeMatchLabel(WidgetTree, TEXT(""), 14);
	SaveStatusText = MakeMatchLabel(WidgetTree, TEXT(""), 12);
	GuessInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("GuessInput"));
	GuessInput->SetHintText(FText::FromString(TEXT("Type your guess")));

	PencilButton = MakeMatchButton(WidgetTree, TEXT("Pencil"));
	EraserButton = MakeMatchButton(WidgetTree, TEXT("Eraser"));
	FinishButton = MakeMatchButton(WidgetTree, TEXT("Finish Drawing"));
	SubmitButton = MakeMatchButton(WidgetTree, TEXT("Submit Guess"));
	NextRoundButton = MakeMatchButton(WidgetTree, TEXT("Next Round"));
	SaveButton = MakeMatchButton(WidgetTree, TEXT("Save Board"));

	Content->AddChildToVerticalBox(PhaseText);
	Content->AddChildToVerticalBox(RoleText);
	Content->AddChildToVerticalBox(ScoreText);
	Content->AddChildToVerticalBox(PromptText);
	Content->AddChildToVerticalBox(InstructionText);
	Content->AddChildToVerticalBox(PencilButton);
	Content->AddChildToVerticalBox(EraserButton);
	Content->AddChildToVerticalBox(FinishButton);
	Content->AddChildToVerticalBox(GuessInput);
	Content->AddChildToVerticalBox(SubmitButton);
	Content->AddChildToVerticalBox(ResultText);
	Content->AddChildToVerticalBox(SaveButton);
	Content->AddChildToVerticalBox(SaveStatusText);
	Content->AddChildToVerticalBox(NextRoundButton);

	PencilButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandlePencilClicked);
	EraserButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleEraserClicked);
	FinishButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleFinishClicked);
	SubmitButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleSubmitClicked);
	NextRoundButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleNextRoundClicked);
	SaveButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleSaveClicked);
}

// Refreshes text and button states from the owning player controller.
void UDNGMatchWidget::RefreshFromController()
{
	ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>();
	if (!Controller)
	{
		return;
	}

	PhaseText->SetText(FText::FromString(FString::Printf(TEXT("Phase: %s"), *Controller->GetPhaseDescription())));
	RoleText->SetText(FText::FromString(Controller->GetRoleDescription()));
	ScoreText->SetText(FText::FromString(Controller->GetScoreDescription()));
	PromptText->SetText(FText::FromString(Controller->GetPromptDescription()));
	ResultText->SetText(FText::FromString(Controller->GetResultDescription()));
	InstructionText->SetText(FText::FromString(Controller->GetInstructionDescription()));
	SaveStatusText->SetText(FText::FromString(Controller->GetSaveStatusDescription()));

	const bool bCanDraw = Controller->CanUseDrawingControls();
	const bool bCanGuess = Controller->CanSubmitGuess();
	const bool bCanAdvance = Controller->CanAdvanceRound();

	PencilButton->SetIsEnabled(bCanDraw);
	EraserButton->SetIsEnabled(bCanDraw);
	FinishButton->SetIsEnabled(bCanDraw);
	GuessInput->SetIsEnabled(bCanGuess);
	SubmitButton->SetIsEnabled(bCanGuess);
	NextRoundButton->SetIsEnabled(bCanAdvance);
	SaveButton->SetIsEnabled(true);
}

// Switches to the pencil tool.
void UDNGMatchWidget::HandlePencilClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetTool(EDNGDrawTool::Pencil);
	}
}

// Switches to the eraser tool.
void UDNGMatchWidget::HandleEraserClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetTool(EDNGDrawTool::Eraser);
	}
}

// Requests the end of the drawing phase.
void UDNGMatchWidget::HandleFinishClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestFinishDrawing();
	}
}

// Requests submission of the current guess text.
void UDNGMatchWidget::HandleSubmitClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSubmitGuess(GuessInput ? GuessInput->GetText().ToString() : FString());
	}
}

// Requests that the next round begins.
void UDNGMatchWidget::HandleNextRoundClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestNextRound();
	}
}

// Saves the current board image locally.
void UDNGMatchWidget::HandleSaveClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSaveBoard();
	}
}
