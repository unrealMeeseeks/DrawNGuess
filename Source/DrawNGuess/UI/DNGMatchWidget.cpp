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

	BindActions();
}

// Polls the owning controller each frame so the HUD always reflects replicated state.
void UDNGMatchWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromController();
}

void UDNGMatchWidget::BindActions()
{
	if (PencilButton)
	{
		PencilButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandlePencilClicked);
		PencilButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandlePencilClicked);
	}

	if (EraserButton)
	{
		EraserButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleEraserClicked);
		EraserButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleEraserClicked);
	}

	if (BlackButton)
	{
		BlackButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleBlackClicked);
		BlackButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleBlackClicked);
	}

	if (RedButton)
	{
		RedButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleRedClicked);
		RedButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleRedClicked);
	}

	if (BlueButton)
	{
		BlueButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleBlueClicked);
		BlueButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleBlueClicked);
	}

	if (GreenButton)
	{
		GreenButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleGreenClicked);
		GreenButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleGreenClicked);
	}

	if (PencilSmallButton)
	{
		PencilSmallButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandlePencilSmallClicked);
		PencilSmallButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandlePencilSmallClicked);
	}

	if (PencilMediumButton)
	{
		PencilMediumButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandlePencilMediumClicked);
		PencilMediumButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandlePencilMediumClicked);
	}

	if (PencilLargeButton)
	{
		PencilLargeButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandlePencilLargeClicked);
		PencilLargeButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandlePencilLargeClicked);
	}

	if (EraserSmallButton)
	{
		EraserSmallButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleEraserSmallClicked);
		EraserSmallButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleEraserSmallClicked);
	}

	if (EraserMediumButton)
	{
		EraserMediumButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleEraserMediumClicked);
		EraserMediumButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleEraserMediumClicked);
	}

	if (EraserLargeButton)
	{
		EraserLargeButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleEraserLargeClicked);
		EraserLargeButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleEraserLargeClicked);
	}

	if (FinishButton)
	{
		FinishButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleFinishClicked);
		FinishButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleFinishClicked);
	}

	if (SubmitButton)
	{
		SubmitButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleSubmitClicked);
		SubmitButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleSubmitClicked);
	}

	if (NextRoundButton)
	{
		NextRoundButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleNextRoundClicked);
		NextRoundButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleNextRoundClicked);
	}

	if (SaveButton)
	{
		SaveButton->OnClicked.RemoveDynamic(this, &UDNGMatchWidget::HandleSaveClicked);
		SaveButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleSaveClicked);
	}
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
	BrushText = MakeMatchLabel(WidgetTree, TEXT("Brush:"), 14);
	SaveStatusText = MakeMatchLabel(WidgetTree, TEXT(""), 12);
	GuessInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("GuessInput"));
	GuessInput->SetHintText(FText::FromString(TEXT("Type your guess")));

	PencilButton = MakeMatchButton(WidgetTree, TEXT("Pencil"));
	EraserButton = MakeMatchButton(WidgetTree, TEXT("Eraser"));
	BlackButton = MakeMatchButton(WidgetTree, TEXT("Black"));
	RedButton = MakeMatchButton(WidgetTree, TEXT("Red"));
	BlueButton = MakeMatchButton(WidgetTree, TEXT("Blue"));
	GreenButton = MakeMatchButton(WidgetTree, TEXT("Green"));
	PencilSmallButton = MakeMatchButton(WidgetTree, TEXT("Pencil S"));
	PencilMediumButton = MakeMatchButton(WidgetTree, TEXT("Pencil M"));
	PencilLargeButton = MakeMatchButton(WidgetTree, TEXT("Pencil L"));
	EraserSmallButton = MakeMatchButton(WidgetTree, TEXT("Eraser S"));
	EraserMediumButton = MakeMatchButton(WidgetTree, TEXT("Eraser M"));
	EraserLargeButton = MakeMatchButton(WidgetTree, TEXT("Eraser L"));
	FinishButton = MakeMatchButton(WidgetTree, TEXT("Finish Drawing"));
	SubmitButton = MakeMatchButton(WidgetTree, TEXT("Submit Guess"));
	NextRoundButton = MakeMatchButton(WidgetTree, TEXT("Next Round"));
	SaveButton = MakeMatchButton(WidgetTree, TEXT("Save Board"));
	BlackButton->SetBackgroundColor(FLinearColor::Black);
	RedButton->SetBackgroundColor(FLinearColor(0.75f, 0.15f, 0.15f, 1.0f));
	BlueButton->SetBackgroundColor(FLinearColor(0.15f, 0.35f, 0.9f, 1.0f));
	GreenButton->SetBackgroundColor(FLinearColor(0.15f, 0.65f, 0.25f, 1.0f));

	Content->AddChildToVerticalBox(PhaseText);
	Content->AddChildToVerticalBox(RoleText);
	Content->AddChildToVerticalBox(ScoreText);
	Content->AddChildToVerticalBox(PromptText);
	Content->AddChildToVerticalBox(InstructionText);
	Content->AddChildToVerticalBox(BrushText);
	Content->AddChildToVerticalBox(PencilButton);
	Content->AddChildToVerticalBox(EraserButton);
	Content->AddChildToVerticalBox(BlackButton);
	Content->AddChildToVerticalBox(RedButton);
	Content->AddChildToVerticalBox(BlueButton);
	Content->AddChildToVerticalBox(GreenButton);
	Content->AddChildToVerticalBox(PencilSmallButton);
	Content->AddChildToVerticalBox(PencilMediumButton);
	Content->AddChildToVerticalBox(PencilLargeButton);
	Content->AddChildToVerticalBox(EraserSmallButton);
	Content->AddChildToVerticalBox(EraserMediumButton);
	Content->AddChildToVerticalBox(EraserLargeButton);
	Content->AddChildToVerticalBox(FinishButton);
	Content->AddChildToVerticalBox(GuessInput);
	Content->AddChildToVerticalBox(SubmitButton);
	Content->AddChildToVerticalBox(ResultText);
	Content->AddChildToVerticalBox(SaveButton);
	Content->AddChildToVerticalBox(SaveStatusText);
	Content->AddChildToVerticalBox(NextRoundButton);

	PencilButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandlePencilClicked);
	EraserButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleEraserClicked);
	BlackButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleBlackClicked);
	RedButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleRedClicked);
	BlueButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleBlueClicked);
	GreenButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleGreenClicked);
	PencilSmallButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandlePencilSmallClicked);
	PencilMediumButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandlePencilMediumClicked);
	PencilLargeButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandlePencilLargeClicked);
	EraserSmallButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleEraserSmallClicked);
	EraserMediumButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleEraserMediumClicked);
	EraserLargeButton->OnClicked.AddDynamic(this, &UDNGMatchWidget::HandleEraserLargeClicked);
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

	if (PhaseText)
	{
		PhaseText->SetText(FText::FromString(FString::Printf(TEXT("Phase: %s"), *Controller->GetPhaseDescription())));
	}

	if (RoleText)
	{
		RoleText->SetText(FText::FromString(Controller->GetRoleDescription()));
	}

	if (ScoreText)
	{
		ScoreText->SetText(FText::FromString(Controller->GetScoreDescription()));
	}

	if (PromptText)
	{
		PromptText->SetText(FText::FromString(Controller->GetPromptDescription()));
	}

	if (ResultText)
	{
		ResultText->SetText(FText::FromString(Controller->GetResultDescription()));
	}

	if (InstructionText)
	{
		InstructionText->SetText(FText::FromString(Controller->GetInstructionDescription()));
	}

	if (BrushText)
	{
		BrushText->SetText(FText::FromString(Controller->GetBrushDescription()));
	}

	if (SaveStatusText)
	{
		SaveStatusText->SetText(FText::FromString(Controller->GetSaveStatusDescription()));
	}

	const bool bCanDraw = Controller->CanUseDrawingControls();
	const bool bCanGuess = Controller->CanSubmitGuess();
	const bool bCanAdvance = Controller->CanAdvanceRound();

	if (PencilButton) { PencilButton->SetIsEnabled(bCanDraw); }
	if (EraserButton) { EraserButton->SetIsEnabled(bCanDraw); }
	if (BlackButton) { BlackButton->SetIsEnabled(bCanDraw); }
	if (RedButton) { RedButton->SetIsEnabled(bCanDraw); }
	if (BlueButton) { BlueButton->SetIsEnabled(bCanDraw); }
	if (GreenButton) { GreenButton->SetIsEnabled(bCanDraw); }
	if (PencilSmallButton) { PencilSmallButton->SetIsEnabled(bCanDraw); }
	if (PencilMediumButton) { PencilMediumButton->SetIsEnabled(bCanDraw); }
	if (PencilLargeButton) { PencilLargeButton->SetIsEnabled(bCanDraw); }
	if (EraserSmallButton) { EraserSmallButton->SetIsEnabled(bCanDraw); }
	if (EraserMediumButton) { EraserMediumButton->SetIsEnabled(bCanDraw); }
	if (EraserLargeButton) { EraserLargeButton->SetIsEnabled(bCanDraw); }
	if (FinishButton) { FinishButton->SetIsEnabled(bCanDraw); }
	if (GuessInput) { GuessInput->SetIsEnabled(bCanGuess); }
	if (SubmitButton) { SubmitButton->SetIsEnabled(bCanGuess); }
	if (NextRoundButton) { NextRoundButton->SetIsEnabled(bCanAdvance); }
	if (SaveButton) { SaveButton->SetIsEnabled(true); }
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

// Selects the black pencil preset.
void UDNGMatchWidget::HandleBlackClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetPencilColor(FLinearColor::Black);
		Controller->RequestSetTool(EDNGDrawTool::Pencil);
	}
}

// Selects the red pencil preset.
void UDNGMatchWidget::HandleRedClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetPencilColor(FLinearColor(0.85f, 0.0f, 0.0f, 1.0f));
		Controller->RequestSetTool(EDNGDrawTool::Pencil);
	}
}

// Selects the blue pencil preset.
void UDNGMatchWidget::HandleBlueClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetPencilColor(FLinearColor(0.15f, 0.35f, 0.95f, 1.0f));
		Controller->RequestSetTool(EDNGDrawTool::Pencil);
	}
}

// Selects the green pencil preset.
void UDNGMatchWidget::HandleGreenClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetPencilColor(FLinearColor(0.15f, 0.7f, 0.25f, 1.0f));
		Controller->RequestSetTool(EDNGDrawTool::Pencil);
	}
}

// Selects the small pencil thickness preset.
void UDNGMatchWidget::HandlePencilSmallClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetPencilThickness(4.0f);
		Controller->RequestSetTool(EDNGDrawTool::Pencil);
	}
}

// Selects the medium pencil thickness preset.
void UDNGMatchWidget::HandlePencilMediumClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetPencilThickness(7.0f);
		Controller->RequestSetTool(EDNGDrawTool::Pencil);
	}
}

// Selects the large pencil thickness preset.
void UDNGMatchWidget::HandlePencilLargeClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetPencilThickness(12.0f);
		Controller->RequestSetTool(EDNGDrawTool::Pencil);
	}
}

// Selects the small eraser thickness preset.
void UDNGMatchWidget::HandleEraserSmallClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetEraserThickness(12.0f);
		Controller->RequestSetTool(EDNGDrawTool::Eraser);
	}
}

// Selects the medium eraser thickness preset.
void UDNGMatchWidget::HandleEraserMediumClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetEraserThickness(18.0f);
		Controller->RequestSetTool(EDNGDrawTool::Eraser);
	}
}

// Selects the large eraser thickness preset.
void UDNGMatchWidget::HandleEraserLargeClicked()
{
	if (ADNGPlayerController* Controller = GetOwningPlayer<ADNGPlayerController>())
	{
		Controller->RequestSetEraserThickness(28.0f);
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
