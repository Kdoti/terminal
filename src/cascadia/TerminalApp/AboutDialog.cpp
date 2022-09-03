
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AboutDialog.h"
#include "AboutDialog.g.cpp"

#include <LibraryResources.h>
#include <WtExeUtils.h>

#include <winrt/Windows.Services.Store.h>

#include "../../types/inc/utils.hpp"
#include "Utils.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal;
using namespace ::TerminalApp;
using namespace std::chrono_literals;

namespace winrt
{
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    AboutDialog::AboutDialog()
    {
        InitializeComponent();
    }

    winrt::hstring AboutDialog::ApplicationDisplayName()
    {
        return CascadiaSettings::ApplicationDisplayName();
    }

    winrt::hstring AboutDialog::ApplicationVersion()
    {
        return CascadiaSettings::ApplicationVersion();
    }

    void AboutDialog::_SendFeedbackOnClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs& /*eventArgs*/)
    {
#if defined(WT_BRANDING_RELEASE)
        ShellExecute(nullptr, nullptr, L"https://go.microsoft.com/fwlink/?linkid=2125419", nullptr, nullptr, SW_SHOW);
#else
        ShellExecute(nullptr, nullptr, L"https://go.microsoft.com/fwlink/?linkid=2204904", nullptr, nullptr, SW_SHOW);
#endif
    }

    void AboutDialog::_ThirdPartyNoticesOnClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        std::filesystem::path currentPath{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
        currentPath.replace_filename(L"NOTICE.html");
        ShellExecute(nullptr, nullptr, currentPath.c_str(), nullptr, nullptr, SW_SHOW);
    }

    bool AboutDialog::UpdatesAvailable() const
    {
        return !_pendingUpdateVersion.empty();
    }

    winrt::hstring AboutDialog::PendingUpdateVersion() const
    {
        return _pendingUpdateVersion;
    }

    void AboutDialog::_SetPendingUpdateVersion(const winrt::hstring& version)
    {
        _pendingUpdateVersion = version;
        _PropertyChangedHandlers(*this, WUX::Data::PropertyChangedEventArgs{ L"PendingUpdateVersion" });
        _PropertyChangedHandlers(*this, WUX::Data::PropertyChangedEventArgs{ L"UpdatesAvailable" });
    }

    winrt::fire_and_forget AboutDialog::_QueueUpdateCheck()
    {
        auto strongThis = get_strong();
        auto now{ std::chrono::system_clock::now() };
        if (now - _lastUpdateCheck < std::chrono::days{ 1 })
        {
            co_return;
        }
        _lastUpdateCheck = now;

        if (!IsPackaged())
        {
            co_return;
        }

        co_await wil::resume_foreground(strongThis->Dispatcher());
        _SetPendingUpdateVersion({});
        CheckingForUpdates(true);

        try
        {
#ifdef WT_BRANDING_DEV
            // **DEV BRANDING**: Always sleep for three seconds and then report that
            // there is an update available. This lets us test the system.
            co_await winrt::resume_after(std::chrono::seconds{ 3 });
            co_await wil::resume_foreground(strongThis->Dispatcher());
            _SetPendingUpdateVersion(L"X.Y.Z");
#else // release build, likely has a store context
            auto storeContext = winrt::Windows::Services::Store::StoreContext::GetDefault();
            auto updates = co_await storeContext.GetAppAndOptionalStorePackageUpdatesAsync();
            co_await wil::resume_foreground(strongThis->Dispatcher());
            if (updates.Size() > 0)
            {
                auto version = updates.GetAt(0).Package().Id().Version();
                _SetPendingUpdateVersion(fmt::format(L"{0}.{1}.{2}", version.Major, version.Minor, version.Revision));
            }
#endif
        }
        catch (...)
        {
            // do nothing on failure
        }

        co_await wil::resume_foreground(strongThis->Dispatcher());
        CheckingForUpdates(false);
    }
}
