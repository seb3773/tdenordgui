#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <tqmainwindow.h>
#include <tqwidgetstack.h>
#include <tqpushbutton.h>
#include <tqlabel.h>
#include <tqevent.h>
#include <tqlistbox.h>
#include <tqlineedit.h>
#include <ksystemtray.h>
#include <tdepopupmenu.h>
#include "daemon_client.h"

class TQListBoxItem;
class TQButtonGroup;
class TQRadioButton;
class TQLabel;
class TQWidget;

class MainWindow : public TQMainWindow {
    TQ_OBJECT
public:
    explicit MainWindow(DaemonClient* client, TQWidget* parent = nullptr);
    virtual ~MainWindow();

protected:
    bool eventFilter(TQObject *obj, TQEvent *event);
    void customEvent(TQCustomEvent* event);
    virtual void closeEvent(TQCloseEvent *e);

public slots:
    void onConnectionStateChanged(const TQString& state);
    void onConnectionError(const TQString& error);

private slots:
    void quitApplication();
    void updateSystemTrayMenu();
    void handleLoginClicked();
    void handleCallbackSubmit();
    void showDashboard();
    void showSettings();
    void showCountriesTab();
    void showSpecialtyTab();
    void onSearchClicked();
    void onSearchTextChanged(const TQString& text);
    
    void showAcCountriesTab();
    void showAcSpecialtyTab();
    void onAcSearchClicked();
    void onAcSearchTextChanged(const TQString& text);
    void onAcApplyClicked();
    
    void onServerSelected(TQListBoxItem* item);
    void onAcServerSelected(TQListBoxItem* item);
    void onQuickConnectClicked();
    void onTrayConnectClicked();
    void applyAdvancedSettings(const DaemonClient::AdvancedSettings& settings);
    void onThemeChanged(int id);
    void onResetAppClicked();
    void onKillSwitchToggled(bool);
    void onThreatProtectionLiteToggled(bool);
    void onObfuscateToggled(bool);
    
    void onAutoConnectToggled(bool);
    void onLanDiscoveryToggled(bool);
    void onFirewallToggled(bool);
    void onPostQuantumToggled(bool);
    void onNotifyToggled(bool);
    void onProtocolChanged(int);
    void onLogoutClicked();
    void applyAccountInfo(const DaemonClient::AccountInfo& info);
    void onTermsLinkClicked(const TQString& link);
    void onAboutClicked();

    void onAllowlistToggle(bool checked);
    void onCustomDnsToggle(bool checked);
    void onAllowlistAddClicked();
    void onCustomDnsAddClicked();
    
    // UI Visual List Helpers
    void addVisualAllowlistRow(const TQString& port, const TQString& proto);
    void addVisualDnsRow(const TQString& ip);
    void sendDnsUpdateToDaemon();
    void onAllowlistDeleteClicked();
    void onCustomDnsDeleteClicked();

private:
    void setupUi();
    void setupSystemTray();
    void setupSidebar();
    void showNotification(const TQString& title, const TQString& message);
    void setupLoginView();
    void setupDashboardView();
    void setupSettingsView();
    void setupSettingsVpnConnection();
    void setupSettingsSecurity();
    void setupSettingsThreatProtection();
    void setupSettingsGeneral();
    void setupSettingsTerms();
    void setupSettingsAccount();
    void setupSettingsAllowlist();
    void setupSettingsCustomDns();
    void setupSettingsAutoConnectServer();
    
    bool checkThemeFileExists();
    void setThemeFileExists(bool exist);
    TQWidget* createSettingsCategory(const TQString& id, const TQString& title, const TQString& subtitle, const unsigned char* iconData, unsigned int iconLen);
    void populateServerLists();
    void updateListWithFilter(TQListBox* list, const TQValueList<DaemonClient::ServerGroup>& data, const TQString& filter, bool addFastestItem = false);

    DaemonClient* m_client;
    TQWidget* m_sidebarWidget;
    TQWidget* m_navVpnBtn;
    TQWidget* m_navSettingsBtn;
    TQWidget* m_navAboutBtn;
    TQWidget* m_navQuitBtn;
    TQLabel* m_navVpnIconLabel;
    TQLabel* m_navSettingsIconLabel;
    TQLabel* m_navAboutIconLabel;
    TQLabel* m_navQuitIconLabel;

    TQWidgetStack* m_viewStack;
    
    // Components
    KSystemTray* m_tray;
    TDEPopupMenu* m_trayMenu;
    int m_trayConnectId;
    
    TQButtonGroup* m_bgApp;
    TQRadioButton* m_rbLight;
    TQRadioButton* m_rbDark;
    
    TQWidget* m_loginWidget;
    TQPushButton* m_loginBtn;
    class TQLineEdit* m_callbackInput;
    TQPushButton* m_submitCallbackBtn;
    
    // Main VPN UI elements
    TQWidget* m_dashboardWidget;
    TQLabel* m_statusLabel;
    TQPushButton* m_quickConnectBtn;
    
    TQPushButton* m_tabCountriesBtn;
    TQPushButton* m_tabSpecialtyBtn;
    TQPushButton* m_searchBtn;
    TQLineEdit* m_searchEdit;
    
    TQWidgetStack* m_listStack;
    TQWidget* m_countriesWidget;
    TQListBox* m_countriesList;
    
    TQWidget* m_specialtyWidget;
    TQListBox* m_specialtyList;

    TQValueList<DaemonClient::ServerGroup> m_cachedCountries;
    TQValueList<DaemonClient::ServerGroup> m_cachedGroups;
    TQMap<TQString, TQString> m_countryPayloadMap;

    TQWidgetStack* m_settingsWidget; // The stack containing the list or details
    TQWidget* m_settingsListWidget;
    
    // Sub-pages
    TQWidget* m_pageVpnConnection;
    TQWidget* m_pageSecurityPrivacy;
    TQWidget* m_pageThreatProtection;
    TQWidget* m_pageGeneral;
    TQWidget* m_pageTerms;
    TQWidget* m_pageAccount;
    
    // Notification state tracker
    TQString m_lastNotificationState;
    bool m_isFirstStateReceived;
    TQWidget* m_pageAutoConnectServer;
    TQWidget* m_pageAllowlist;
    TQWidget* m_pageCustomDns;
    
    class TQCheckBox* m_chkAutoConnect;
    class TQCheckBox* m_chkKillSwitch;
    class TQCheckBox* m_chkThreatProtection;
    class TQCheckBox* m_chkObfuscate;
    class TQCheckBox* m_chkLan;
    class TQCheckBox* m_chkFirewall;
    class TQCheckBox* m_chkPostQuantum;
    class TQCheckBox* m_chkNotifications;
    
    class TQRadioButton* m_rbNordLynx;
    class TQRadioButton* m_rbNordWhisper;
    class TQRadioButton* m_rbOpenVpnTcp;
    class TQRadioButton* m_rbOpenVpnUdp;
    
    class TQLabel* m_lblAutoConnectTo;
    
    class TQLabel* m_lblFwm;
    class TQLineEdit* m_edtFwm;
    class TQPushButton* m_btnFwmSave;
    
    class    TQLabel* m_lblSubscriptionStatus;
    TQLabel* m_lblAccountInfo;
    
    TQPushButton* m_acTabCountriesBtn;
    TQPushButton* m_acTabSpecialtyBtn;
    TQPushButton* m_acSearchBtn;
    TQLineEdit* m_acSearchEdit;
    TQWidgetStack* m_acListStack;
    TQListBox* m_acCountriesList;
    TQListBox* m_acSpecialtyList;
    
    TQWidget* m_allowlistContainer;
    TQVBoxLayout* m_allowlistEntriesLayout;
    class TQLineEdit* m_allowlistEdtVal;
    class TQComboBox* m_allowlistCboProto;
    class TQButtonGroup* m_allowlistBgType;
    class TQCheckBox* m_chkAllowlist;
    class TQFrame* m_allowlistBox;
    
    TQWidget* m_customDnsContainer;
    TQVBoxLayout* m_customDnsEntriesLayout;
    class TQLineEdit* m_customDnsEdtBase;
    class TQCheckBox* m_chkCustomDns;
    class TQFrame* m_customDnsBox;
    
    bool showConfirmOffDialog(const TQString& title, const TQString& message);
};

#endif // MAIN_WINDOW_H
