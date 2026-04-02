#include "DNGMainMenuWidget.h"

#include "../DNGGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* WidgetTree, const FString& Text, int32 FontSize = 16)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo FontInfo = Label->GetFont();
		FontInfo.Size = FontSize;
		Label->SetFont(FontInfo);
		return Label;
	}

	UButton* MakeButton(UWidgetTree* WidgetTree, const FString& Text)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();

		UTextBlock* Label = MakeLabel(WidgetTree, Text, 18);
		Label->SetJustification(ETextJustify::Center);
		Button->AddChild(Label);
		return Button;
	}
}

void UDNGMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
		LoadSavedValues();
	}
}

void UDNGMainMenuWidget::BuildWidgetTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetPadding(FMargin(24.0f));
	Panel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.88f));

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Panel->SetContent(Content);

	Content->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("Draw N Guess Prototype"), 26));
	Content->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("Set prompt prefixes locally, host on one machine, then join by IP from another."), 14));

	Content->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("Positive prompt prefix")));
	PositivePrefixInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("PositivePrefixInput"));
	Content->AddChildToVerticalBox(PositivePrefixInput);

	Content->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("Negative prompt prefix")));
	NegativePrefixInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("NegativePrefixInput"));
	Content->AddChildToVerticalBox(NegativePrefixInput);

	Content->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("Join address")));
	JoinAddressInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("JoinAddressInput"));
	Content->AddChildToVerticalBox(JoinAddressInput);

	SaveButton = MakeButton(WidgetTree, TEXT("Save Prompt"));
	HostButton = MakeButton(WidgetTree, TEXT("Host Game"));
	JoinButton = MakeButton(WidgetTree, TEXT("Join Game"));
	StatusText = MakeLabel(WidgetTree, TEXT(""), 14);

	Content->AddChildToVerticalBox(SaveButton);
	Content->AddChildToVerticalBox(HostButton);
	Content->AddChildToVerticalBox(JoinButton);
	Content->AddChildToVerticalBox(StatusText);

	if (SaveButton)
	{
		SaveButton->OnClicked.AddDynamic(this, &UDNGMainMenuWidget::HandleSaveClicked);
	}

	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UDNGMainMenuWidget::HandleHostClicked);
	}

	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UDNGMainMenuWidget::HandleJoinClicked);
	}
}

void UDNGMainMenuWidget::LoadSavedValues()
{
	if (const UDNGGameInstance* GameInstance = Cast<UDNGGameInstance>(GetGameInstance()))
	{
		const FDNGPromptSettings& Settings = GameInstance->GetPromptSettings();
		PositivePrefixInput->SetText(FText::FromString(Settings.PositivePrefix));
		NegativePrefixInput->SetText(FText::FromString(Settings.NegativePrefix));
		JoinAddressInput->SetText(FText::FromString(Settings.LastJoinAddress));
	}
}

void UDNGMainMenuWidget::PersistInputs() const
{
	if (UDNGGameInstance* GameInstance = Cast<UDNGGameInstance>(GetGameInstance()))
	{
		GameInstance->UpdatePromptSettings(
			PositivePrefixInput ? PositivePrefixInput->GetText().ToString() : FString(),
			NegativePrefixInput ? NegativePrefixInput->GetText().ToString() : FString(),
			JoinAddressInput ? JoinAddressInput->GetText().ToString() : FString());
	}
}

void UDNGMainMenuWidget::HandleHostClicked()
{
	PersistInputs();

	if (UDNGGameInstance* GameInstance = Cast<UDNGGameInstance>(GetGameInstance()))
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("Reopening the current map as a listen server...")));
		}

		GameInstance->HostGame();
	}
}

void UDNGMainMenuWidget::HandleJoinClicked()
{
	PersistInputs();

	if (UDNGGameInstance* GameInstance = Cast<UDNGGameInstance>(GetGameInstance()))
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("Connecting to host...")));
		}

		GameInstance->JoinGame(JoinAddressInput ? JoinAddressInput->GetText().ToString() : FString());
	}
}

void UDNGMainMenuWidget::HandleSaveClicked()
{
	PersistInputs();

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("Prompt settings saved locally.")));
	}
}
