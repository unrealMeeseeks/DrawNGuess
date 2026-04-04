#include "DNGMainMenuWidget.h"

#include "../Gameplay/Flow/DNGGameInstance.h"
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

// Binds the fallback widget setup and loads locally cached values into the text boxes.
void UDNGMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	BindActions();
	LoadSavedValues();
}

// Builds a minimal menu entirely in C++ when no Blueprint layout exists.
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

	Content->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("DeepSeek API key")));
	DeepSeekApiKeyInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DeepSeekApiKeyInput"));
	Content->AddChildToVerticalBox(DeepSeekApiKeyInput);

	Content->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("DeepSeek base URL")));
	DeepSeekBaseUrlInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DeepSeekBaseUrlInput"));
	Content->AddChildToVerticalBox(DeepSeekBaseUrlInput);

	Content->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("DeepSeek model")));
	DeepSeekModelInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DeepSeekModelInput"));
	Content->AddChildToVerticalBox(DeepSeekModelInput);

	SaveButton = MakeButton(WidgetTree, TEXT("Save Prompt"));
	SaveAgentConfigButton = MakeButton(WidgetTree, TEXT("Save DeepSeek Config"));
	HostButton = MakeButton(WidgetTree, TEXT("Host Game"));
	JoinButton = MakeButton(WidgetTree, TEXT("Join Game"));
	StatusText = MakeLabel(WidgetTree, TEXT(""), 14);

	Content->AddChildToVerticalBox(SaveButton);
	Content->AddChildToVerticalBox(SaveAgentConfigButton);
	Content->AddChildToVerticalBox(HostButton);
	Content->AddChildToVerticalBox(JoinButton);
	Content->AddChildToVerticalBox(StatusText);
}

void UDNGMainMenuWidget::BindActions()
{
	if (SaveButton)
	{
		SaveButton->OnClicked.RemoveDynamic(this, &UDNGMainMenuWidget::HandleSaveClicked);
		SaveButton->OnClicked.AddDynamic(this, &UDNGMainMenuWidget::HandleSaveClicked);
	}

	if (HostButton)
	{
		HostButton->OnClicked.RemoveDynamic(this, &UDNGMainMenuWidget::HandleHostClicked);
		HostButton->OnClicked.AddDynamic(this, &UDNGMainMenuWidget::HandleHostClicked);
	}

	if (JoinButton)
	{
		JoinButton->OnClicked.RemoveDynamic(this, &UDNGMainMenuWidget::HandleJoinClicked);
		JoinButton->OnClicked.AddDynamic(this, &UDNGMainMenuWidget::HandleJoinClicked);
	}

	if (SaveAgentConfigButton)
	{
		SaveAgentConfigButton->OnClicked.RemoveDynamic(this, &UDNGMainMenuWidget::HandleSaveAgentConfigClicked);
		SaveAgentConfigButton->OnClicked.AddDynamic(this, &UDNGMainMenuWidget::HandleSaveAgentConfigClicked);
	}
}

// Restores saved prompt values and join address from the local GameInstance.
void UDNGMainMenuWidget::LoadSavedValues()
{
	if (const UDNGGameInstance* GameInstance = Cast<UDNGGameInstance>(GetGameInstance()))
	{
		const FDNGPromptSettings& Settings = GameInstance->GetPromptSettings();

		if (PositivePrefixInput)
		{
			PositivePrefixInput->SetText(FText::FromString(Settings.PositivePrefix));
		}

		if (NegativePrefixInput)
		{
			NegativePrefixInput->SetText(FText::FromString(Settings.NegativePrefix));
		}

		if (JoinAddressInput)
		{
			JoinAddressInput->SetText(FText::FromString(Settings.LastJoinAddress));
		}

		const FDNGDeepSeekConfig& DeepSeekConfig = GameInstance->GetDeepSeekConfig();

		if (DeepSeekApiKeyInput)
		{
			DeepSeekApiKeyInput->SetText(FText::FromString(DeepSeekConfig.ApiKey));
		}

		if (DeepSeekBaseUrlInput)
		{
			DeepSeekBaseUrlInput->SetText(FText::FromString(DeepSeekConfig.BaseUrl));
		}

		if (DeepSeekModelInput)
		{
			DeepSeekModelInput->SetText(FText::FromString(DeepSeekConfig.Model));
		}
	}
}

// Pushes the current text box values back into the GameInstance save flow.
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

// Saves current values and starts a listen server.
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

// Saves current values and joins the typed server address.
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

// Saves current values without changing maps.
void UDNGMainMenuWidget::HandleSaveClicked()
{
	PersistInputs();

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("Prompt settings saved locally.")));
	}
}

void UDNGMainMenuWidget::HandleSaveAgentConfigClicked()
{
	if (UDNGGameInstance* GameInstance = Cast<UDNGGameInstance>(GetGameInstance()))
	{
		GameInstance->UpdateDeepSeekConfig(
			DeepSeekApiKeyInput ? DeepSeekApiKeyInput->GetText().ToString() : FString(),
			DeepSeekBaseUrlInput ? DeepSeekBaseUrlInput->GetText().ToString() : FString(),
			DeepSeekModelInput ? DeepSeekModelInput->GetText().ToString() : FString(),
			true);

		if (StatusText)
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("DeepSeek config saved to %s"), *GameInstance->GetDeepSeekConfigPath())));
		}
	}
}
