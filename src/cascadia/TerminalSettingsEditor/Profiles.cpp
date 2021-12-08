// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles.h"

#include "PreviewConnection.h"
#include "Profiles.g.cpp"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles::Profiles() :
        _previewControl{ Control::TermControl(Model::TerminalSettings{}, make<PreviewConnection>()) }
    {
        InitializeComponent();

        const auto startingDirCheckboxTooltip{ ToolTipService::GetToolTip(StartingDirectoryUseParentCheckbox()) };
        Automation::AutomationProperties::SetFullDescription(StartingDirectoryUseParentCheckbox(), unbox_value<hstring>(startingDirCheckboxTooltip));

        Automation::AutomationProperties::SetName(DeleteButton(), RS_(L"Profile_DeleteButton/Text"));

        _previewControl.IsEnabled(false);
        _previewControl.AllowFocusWhenDisabled(false);
        ControlPreview().Child(_previewControl);
    }

    void Profiles::OnNavigatedTo(const NavigationEventArgs& e)
    {
        auto state{ e.Parameter().as<Editor::ProfilePageNavigationState>() };
        _Profile = state.Profile();

        // generate the font list, if we don't have one
        if (!ProfileViewModel::CompleteFontList() || !ProfileViewModel::MonospaceFontList())
        {
            ProfileViewModel::UpdateFontList();
        }

        // Check the use parent directory box if the starting directory is empty
        if (_Profile.StartingDirectory().empty())
        {
            StartingDirectoryUseParentCheckbox().IsChecked(true);
        }

        // Subscribe to some changes in the view model
        // These changes should force us to update our own set of "Current<Setting>" members,
        // and propagate those changes to the UI
        _ViewModelChangedRevoker = _Profile.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"AntialiasingMode")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAntiAliasingMode" });
            }
            else if (settingName == L"CloseOnExit")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCloseOnExitMode" });
            }
            else if (settingName == L"BellStyle")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsBellStyleFlagSet" });
            }
            else if (settingName == L"ScrollState")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentScrollState" });
            }
            _previewControl.Settings(_Profile.TermSettings());
            _previewControl.UpdateSettings();
        });

        // The Appearances object handles updating the values in the settings UI, but
        // we still need to listen to the changes here just to update the preview control
        _AppearanceViewModelChangedRevoker = _Profile.DefaultAppearance().PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& /*args*/) {
            _previewControl.Settings(_Profile.TermSettings());
            _previewControl.UpdateSettings();
        });

        // Navigate to the pivot in the provided navigation state
        ProfilesPivot().SelectedIndex(static_cast<int>(ProfileViewModel::LastActivePivot()));

        _previewControl.Settings(_State.Profile().TermSettings());
        // There is a possibility that the control has not fully initialized yet,
        // so wait for it to initialize before updating the settings (so we know
        // that the renderer is set up)
        _previewControl.Initialized([&](auto&& /*s*/, auto&& /*e*/) {
            _previewControl.UpdateSettings();
        });
    }

    void Profiles::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
        _AppearanceViewModelChangedRevoker.revoke();
    }

    void Profiles::DeleteConfirmation_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        winrt::get_self<ProfileViewModel>(_Profile)->DeleteProfile();
    }

    void Profiles::CreateUnfocusedAppearance_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _Profile.CreateUnfocusedAppearance();
    }

    void Profiles::DeleteUnfocusedAppearance_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _Profile.DeleteUnfocusedAppearance();
    }

    fire_and_forget Profiles::Icon_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(winrt::get_self<ProfileViewModel>(_Profile)->WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            _Profile.Icon(file);
        }
    }

    fire_and_forget Profiles::Commandline_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        static constexpr COMDLG_FILTERSPEC supportedFileTypes[] = {
            { L"Executable Files (*.exe, *.cmd, *.bat)", L"*.exe;*.cmd;*.bat" },
            { L"All Files (*.*)", L"*.*" }
        };

        static constexpr winrt::guid clientGuidExecutables{ 0x2E7E4331, 0x0800, 0x48E6, { 0xB0, 0x17, 0xA1, 0x4C, 0xD8, 0x73, 0xDD, 0x58 } };
        const auto parentHwnd{ reinterpret_cast<HWND>(winrt::get_self<ProfileViewModel>(_Profile)->WindowRoot().GetHostingWindow()) };
        auto path = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
            THROW_IF_FAILED(dialog->SetClientGuid(clientGuidExecutables));
            try
            {
                auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr) };
                dialog->SetDefaultFolder(folderShellItem.get());
            }
            CATCH_LOG(); // non-fatal
            THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedFileTypes), supportedFileTypes));
            THROW_IF_FAILED(dialog->SetFileTypeIndex(1)); // the array is 1-indexed
            THROW_IF_FAILED(dialog->SetDefaultExtension(L"exe;cmd;bat"));
        });

        if (!path.empty())
        {
            _Profile.Commandline(path);
        }
    }

    fire_and_forget Profiles::StartingDirectory_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        const auto parentHwnd{ reinterpret_cast<HWND>(winrt::get_self<ProfileViewModel>(_Profile)->WindowRoot().GetHostingWindow()) };
        auto folder = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
            static constexpr winrt::guid clientGuidFolderPicker{ 0xAADAA433, 0xB04D, 0x4BAE, { 0xB1, 0xEA, 0x1E, 0x6C, 0xD1, 0xCD, 0xA6, 0x8B } };
            THROW_IF_FAILED(dialog->SetClientGuid(clientGuidFolderPicker));
            try
            {
                auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr) };
                dialog->SetDefaultFolder(folderShellItem.get());
            }
            CATCH_LOG(); // non-fatal

            DWORD flags{};
            THROW_IF_FAILED(dialog->GetOptions(&flags));
            THROW_IF_FAILED(dialog->SetOptions(flags | FOS_PICKFOLDERS)); // folders only
        });

        if (!folder.empty())
        {
            _Profile.StartingDirectory(folder);
        }
    }

    void Profiles::Pivot_SelectionChanged(Windows::Foundation::IInspectable const& /*sender*/,
                                          RoutedEventArgs const& /*e*/)
    {
        ProfileViewModel::LastActivePivot(static_cast<Editor::ProfilesPivots>(ProfilesPivot().SelectedIndex()));
    }
}
