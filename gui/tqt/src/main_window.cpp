#include <libnotify/notify.h>
#include "main_window.h"
#include <tqlayout.h>
#include <tqfont.h>
#include <tqlineedit.h>
#include <tqcheckbox.h>
#include <tqgroupbox.h>
#include <tqpixmap.h>
#include <tqimage.h>
#include <tqcursor.h>
#include <tqmessagebox.h>
#include <tqradiobutton.h>
#include <tqtooltip.h>
#include "delete_png.h"
#include <tqbuttongroup.h>
#include "settings_icons.h"
#include "flags_icons.h"
#include <tqcombobox.h>
#include <tqdialog.h>
#include <tqapplication.h>
#include <tqobjectlist.h>
#include <tqcolor.h>
#include <thread>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <tqpainter.h>
static const int LOAD_SETTINGS_EVENT = 1001;

class RichServerItem : public TQListBoxItem {
public:
    RichServerItem(TQListBox* listbox, const TQPixmap& pm, const TQString& text)
        : TQListBoxItem(listbox), m_pm(pm), m_text(text) {}

    int height(const TQListBox* lb) const override {
        return TQMAX(lb->fontMetrics().lineSpacing() + 4, m_pm.height() + 4);
    }
    
    int width(const TQListBox* lb) const override {
        int w = 6;
        if (!m_pm.isNull()) w += m_pm.width() + 4;
        
        int dashIdx = m_text.find(" - ");
        if (dashIdx != -1) {
            TQString p1 = m_text.left(dashIdx + 3); // Includes " - "
            TQString p2 = m_text.mid(dashIdx + 3);
            w += lb->fontMetrics().width(p1);
            TQFont f = lb->font();
            f.setItalic(true);
            TQFontMetrics fm(f);
            w += fm.width(p2);
        } else {
            w += lb->fontMetrics().width(m_text);
        }
        return w + 6;
    }
    
    TQString text() const override { return m_text; }
    
    void paint(TQPainter* p) override {
        int x = 3;
        int h = height(listBox());
        
        if (!m_pm.isNull()) {
            p->drawPixmap(x, (h - m_pm.height()) / 2, m_pm);
            x += m_pm.width() + 4;
        }
        
        int y = (h - p->fontMetrics().height()) / 2 + p->fontMetrics().ascent();
        
        int dashIdx = m_text.find(" - ");
        if (dashIdx != -1) {
            TQString p1 = m_text.left(dashIdx + 3); // "France - "
            TQString p2 = m_text.mid(dashIdx + 3);  // "Paris"
            
            p->drawText(x, y, p1);
            x += p->fontMetrics().width(p1);
            
            TQFont oldFont = p->font();
            TQFont italicFont = oldFont;
            italicFont.setItalic(true);
            p->setFont(italicFont);
            
            p->drawText(x, y, p2);
            
            p->setFont(oldFont);
        } else {
            p->drawText(x, y, m_text);
        }
    }
private:
    TQPixmap m_pm;
    TQString m_text;
};

const TQColor bgBody(245, 246, 247);
const TQColor bgCard(255, 255, 255);
const TQColor bgDarkCard(248, 248, 248);
const TQColor fgGray(150, 150, 150);
const TQColor fgMain(0, 0, 0);

static TQColor text_color;
static TQColor bg_color;
static TQColor bg_color2;
static TQColor bg_color3;
static bool is_dark_mode = false;
const bool dark = false;

static TQPixmap getScaledIcon(const unsigned char* data, unsigned int len, int size, bool invertColor = false);

class SettingsLoadedEvent : public TQCustomEvent {
public:
    DaemonClient::AdvancedSettings settings;
    DaemonClient::AccountInfo account;
    SettingsLoadedEvent(const DaemonClient::AdvancedSettings& s, const DaemonClient::AccountInfo& a)
        : TQCustomEvent(LOAD_SETTINGS_EVENT), settings(s), account(a) {}
};

static std::string getThemeFilePath() {
    const char* home = getenv("HOME");
    if (!home) return "";
    std::string configDir = std::string(home) + "/.configtde";
    std::string appDir = configDir + "/tdenordgui";
    mkdir(configDir.c_str(), 0755);
    mkdir(appDir.c_str(), 0755);
    return appDir + "/dark";
}

bool MainWindow::checkThemeFileExists() {
    std::string path = ::getThemeFilePath();
    if (path.empty()) return false;
    return (access(path.c_str(), F_OK) == 0);
}

void MainWindow::setThemeFileExists(bool exist) {
    std::string path = ::getThemeFilePath();
    if (path.empty()) return;
    if (exist) {
        FILE* f = fopen(path.c_str(), "w");
        if (f) fclose(f);
    } else {
        unlink(path.c_str());
    }
}

MainWindow::MainWindow(DaemonClient* client, TQWidget* parent)
    : TQMainWindow(parent, "MainWindow", 0),
      m_client(client), m_isFirstStateReceived(true)
{
    is_dark_mode = checkThemeFileExists();
    if (is_dark_mode) {
        text_color = TQColor(255, 255, 255);
        bg_color = TQColor(0, 0, 0);
        bg_color2 = TQColor(21, 21, 21); // #151515
        bg_color3 = TQColor(30, 30, 30); // #1e1e1e
    } else {
        text_color = TQColor(0, 0, 0);
        bg_color = TQColor(255, 255, 255);
        bg_color2 = TQColor(245, 246, 247); // #f5f6f7
        bg_color3 = TQColor(225, 225, 225); // #e1e1e1
    }
    
    // UI SetupCaption("tdeNordgui");
    resize(380, 650);
    setWFlags(getWFlags() | WType_TopLevel);

    TQPixmap appIcon;
    appIcon.loadFromData(icon_nordvpn_png, icon_nordvpn_png_len);
    setIcon(appIcon);

    // Initialize libnotify
    notify_init("tdenordgui");
    FILE* f = fopen("/tmp/tdenordgui_icon.png", "wb");
    if (f) {
        fwrite(icon_nordvpn_png, 1, icon_nordvpn_png_len, f);
        fclose(f);
    }

    setupUi();

    // Preload settings to avoid lag on first 'Settings' tab click
    std::thread([this]() {    // Settings loading
    DaemonClient::AdvancedSettings settings = m_client->getAdvancedSettings();
        DaemonClient::AccountInfo a = m_client->getAccountInfo();
        TQApplication::postEvent(this, new SettingsLoadedEvent(settings, a));
    }).detach();

    // Wiring up DaemonClient signals
    connect(m_client, SIGNAL(stateChanged(const TQString&)), 
            this, SLOT(onConnectionStateChanged(const TQString&)));
    connect(m_client, SIGNAL(connectionError(const TQString&)), 
            this, SLOT(onConnectionError(const TQString&)));
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{

    TQWidget* centralWidget = new TQWidget(this);
    setCentralWidget(centralWidget);
    
    TQHBoxLayout* rootLayout = new TQHBoxLayout(centralWidget);
    rootLayout->setMargin(0);
    rootLayout->setSpacing(0);
    
    // Set global background color for a modern feel
    centralWidget->setPaletteBackgroundColor(bg_color2); // Base App background
    
    // 1. Sidebar (pushed to root layout)
    m_sidebarWidget = new TQWidget(centralWidget);
    m_sidebarWidget->setFixedWidth(180);
    m_sidebarWidget->setPaletteBackgroundColor(bg_color); // Sidebar variable background
    rootLayout->addWidget(m_sidebarWidget);
    setupSidebar();
    
    // 2. View Stack (pushed to root layout)
    m_viewStack = new TQWidgetStack(centralWidget);
    rootLayout->addWidget(m_viewStack, 1);
    
    // Build the views
    setupLoginView();
    setupDashboardView();
    setupSettingsView();
    
    m_viewStack->addWidget(m_loginWidget, 0);
    m_viewStack->addWidget(m_dashboardWidget, 1);
    m_viewStack->addWidget(m_settingsWidget, 2);
    
    // Start with sidebar hidden, show login
    m_sidebarWidget->hide();
    m_viewStack->raiseWidget(m_loginWidget);
    
    setupSystemTray();
}

class NordSysTray : public KSystemTray {
public:
    TDEPopupMenu* customMenu;
    NordSysTray(TQWidget* parent) : KSystemTray(parent, "systray") {
        customMenu = new TDEPopupMenu(parent);
    }
protected:
    void contextMenuAboutToShow(TDEPopupMenu*) override {}
    
    void mousePressEvent(TQMouseEvent* e) override {
        // Suppress default right-click down
        if (e->button() == TQt::RightButton) return;
        KSystemTray::mousePressEvent(e);
    }
    
    void mouseReleaseEvent(TQMouseEvent* e) override {
        // Intercept right-click to show our 100% custom clean menu instead of KSystemTray's injected one
        if (e->button() == TQt::RightButton) {
            customMenu->popup(e->globalPos());
            return;
        }
        KSystemTray::mouseReleaseEvent(e);
    }
};

void MainWindow::setupSystemTray()
{
    NordSysTray* customTray = new NordSysTray(this);
    m_tray = customTray;
    m_trayMenu = customTray->customMenu;
    
    // Wire pre-render callback so menu is physically updated with current state
    connect(m_trayMenu, SIGNAL(aboutToShow()), this, SLOT(updateSystemTrayMenu()));
    updateSystemTrayMenu(); // bootstrap initial load
    
    TQImage img;
    img.loadFromData(tray_off_png, tray_off_png_len);
    m_tray->setPixmap(TQPixmap(img));
    m_tray->show();
}

void MainWindow::updateSystemTrayMenu()
{
    if (!m_trayMenu) return;
    
    m_trayMenu->clear();
    DaemonClient::VpnStatus status = m_client->getVpnStatus();
    
    // Temporarily force Small icon size to 32x32 strictly for the tray menu
    TQIconSet::setIconSize(TQIconSet::Small, TQSize(32, 32));
    
    if (status.isConnected) {
        TQString country = status.countryCode.lower().replace("_", " ");
        static TQMap<TQString, FlagIconData> flagsMap = getFlagsMap();
        TQPixmap flagIcon;
        if (flagsMap.contains(country)) {
            flagIcon = getScaledIcon(flagsMap[country].data, flagsMap[country].len, 32, false);
        } else {
            flagIcon = getScaledIcon(noflag_png, noflag_png_len, 32, is_dark_mode);
        }
        
        m_trayMenu->insertItem(TQIconSet(flagIcon), "Connected");
        m_trayMenu->insertItem("  " + status.serverName);
        m_trayMenu->insertItem("  IP: " + status.ip);
        
        m_trayMenu->insertSeparator();
        
        TQPixmap dcIcon = getScaledIcon(close_search_png, close_search_png_len, 32, is_dark_mode);
        m_trayConnectId = m_trayMenu->insertItem(TQIconSet(dcIcon), "Disconnect", this, SLOT(onTrayConnectClicked()));
        
    } else {
        TQPixmap ncIcon = getScaledIcon(not_connected_png, not_connected_png_len, 32, is_dark_mode);
        m_trayMenu->insertItem(TQIconSet(ncIcon), "Not connected");
        
        m_trayMenu->insertSeparator();
        
        TQPixmap cIcon = getScaledIcon(fastest_server_png, fastest_server_png_len, 32, is_dark_mode);
        m_trayConnectId = m_trayMenu->insertItem(TQIconSet(cIcon), "Connect", this, SLOT(onTrayConnectClicked()));
    }
    
    m_trayMenu->insertSeparator();
    
    TQPixmap quitIcon = getScaledIcon(quit_png, quit_png_len, 32, is_dark_mode);
    m_trayMenu->insertItem(TQIconSet(quitIcon), "Quit", this, SLOT(quitApplication()));
}

void MainWindow::onTrayConnectClicked()
{
    printf("TRACER: MainWindow::onTrayConnectClicked called\\n");
    TQString vpnState = m_client->getVpnStateString();
    bool isConnected = (vpnState.contains("Connected to:") || vpnState == "Connected" || vpnState.startsWith("Connected"));
    
    if (isConnected) {
        if (m_client) m_client->disconnectVpn();
    } else { // If not connected, try to quick connect
        onQuickConnectClicked();
    }
}

void MainWindow::closeEvent(TQCloseEvent *e)
{
    hide();
    e->ignore();
}

void MainWindow::showNotification(const TQString& title, const TQString& message) {
    if (!m_client) return;
    DaemonClient::AdvancedSettings settings = m_client->getAdvancedSettings();
    if (!settings.notify) return;

    NotifyNotification * n = notify_notification_new(title.utf8().data(), message.utf8().data(), "/tmp/tdenordgui_icon.png");
    notify_notification_set_timeout(n, 3000); 
    notify_notification_show(n, NULL);
    g_object_unref(G_OBJECT(n));
}

static void manualInvert(TQImage& img) {
    if (img.depth() != 32) img = img.convertDepth(32);
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            TQRgb p = img.pixel(x, y);
            int r = 255 - ((p >> 16) & 0xff);
            int g = 255 - ((p >> 8) & 0xff);
            int b = 255 - (p & 0xff);
            int a = (p >> 24) & 0xff;
            img.setPixel(x, y, (a << 24) | (r << 16) | (g << 8) | b);
        }
    }
}

static TQPixmap getRawIconInverted(const unsigned char* data, unsigned int len, bool invert) {
    TQImage img;
    img.loadFromData(data, len);
    if (invert) manualInvert(img);
    TQPixmap pm; pm.convertFromImage(img);
    return pm;
}

static TQPixmap getScaledIcon(const unsigned char* data, unsigned int len, int size, bool invertColor) {
    TQImage img;
    img.loadFromData(data, len);
    if (invertColor) manualInvert(img);
    if (size > 0) img = img.smoothScale(size, size);
    
    // Add transparent padding for TQListBoxItem visual spacing (width + 10px right, height 36)
    int outW = size + 10;
    int outH = size + 16; 
    TQImage padded(outW, outH, 32);
    padded.setAlphaBuffer(true);
    
    int offsetY = (outH - size) / 2;
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            if (x < size && y >= offsetY && y < offsetY + size) {
                padded.setPixel(x, y, img.pixel(x, y - offsetY));
            } else {
                padded.setPixel(x, y, 0x00000000); // Transparent black
            }
        }
    }
    
    TQPixmap pm;
    pm.convertFromImage(padded);
    return pm;
}

void MainWindow::setupSidebar()
{
    TQVBoxLayout* layout = new TQVBoxLayout(m_sidebarWidget, 20, 10);
    layout->setAlignment(TQt::AlignTop);
    
    // Logo / Title
    TQLabel* title = new TQLabel("<h2>NordVPN</h2>", m_sidebarWidget);
    title->setAlignment(TQt::AlignTop | TQt::AlignHCenter);
    title->setPaletteForegroundColor(text_color);
    layout->addWidget(title);
    
    layout->addSpacing(30);
    
    // VPN Navigation Button (Custom Widget)
    m_navVpnBtn = new TQWidget(m_sidebarWidget);
    m_navVpnBtn->setFixedHeight(45);
    m_navVpnBtn->setPaletteBackgroundColor(bg_color);
    m_navVpnBtn->setCursor(TQCursor(TQt::PointingHandCursor));
    m_navVpnBtn->installEventFilter(this);
    
    TQHBoxLayout* vpnLayout = new TQHBoxLayout(m_navVpnBtn, 10, 10);
    m_navVpnIconLabel = new TQLabel(m_navVpnBtn);
    m_navVpnIconLabel->setPixmap(getScaledIcon(home_on_png, home_on_png_len, 32, is_dark_mode));
    vpnLayout->addWidget(m_navVpnIconLabel);
    
    TQLabel* vpnText = new TQLabel("VPN", m_navVpnBtn);
    TQFont f = vpnText->font(); f.setPointSize(12); f.setBold(true);
    vpnText->setFont(f);
    vpnText->setPaletteForegroundColor(text_color);
    vpnLayout->addWidget(vpnText);
    vpnLayout->addStretch(1);
    
    layout->addWidget(m_navVpnBtn);
    
    layout->addSpacing(10);
    
    // Settings Navigation Button (Custom Widget)
    m_navSettingsBtn = new TQWidget(m_sidebarWidget);
    m_navSettingsBtn->setFixedHeight(45);
    m_navSettingsBtn->setPaletteBackgroundColor(bg_color);
    m_navSettingsBtn->setCursor(TQCursor(TQt::PointingHandCursor));
    m_navSettingsBtn->installEventFilter(this);
    
    TQHBoxLayout* settingsLayout = new TQHBoxLayout(m_navSettingsBtn, 10, 10);
    m_navSettingsIconLabel = new TQLabel(m_navSettingsBtn);
    m_navSettingsIconLabel->setPixmap(getScaledIcon(settings_navigation_off_png, settings_navigation_off_png_len, 32, is_dark_mode));
    settingsLayout->addWidget(m_navSettingsIconLabel);
    
    TQLabel* settingsText = new TQLabel("Settings", m_navSettingsBtn);
    settingsText->setFont(f);
    settingsText->setPaletteForegroundColor(text_color);
    settingsLayout->addWidget(settingsText);
    settingsLayout->addStretch(1);
    
    layout->addWidget(m_navSettingsBtn);
    
    layout->addSpacing(10);
    
    // About Navigation Button (Custom Widget)
    m_navAboutBtn = new TQWidget(m_sidebarWidget);
    m_navAboutBtn->setFixedHeight(45);
    m_navAboutBtn->setPaletteBackgroundColor(bg_color);
    m_navAboutBtn->setCursor(TQCursor(TQt::PointingHandCursor));
    m_navAboutBtn->setName("btn_about_sidebar");
    m_navAboutBtn->installEventFilter(this);
    
    TQHBoxLayout* aboutLayout = new TQHBoxLayout(m_navAboutBtn, 10, 10);
    m_navAboutIconLabel = new TQLabel(m_navAboutBtn);
    m_navAboutIconLabel->setPixmap(getScaledIcon(about_png, about_png_len, 32, is_dark_mode));

    m_navAboutIconLabel->setName("btn_about_sidebar");
    m_navAboutIconLabel->installEventFilter(this);
    aboutLayout->addWidget(m_navAboutIconLabel);
    
    TQLabel* aboutText = new TQLabel("About", m_navAboutBtn);
    aboutText->setFont(f);
    aboutText->setPaletteForegroundColor(text_color);
    aboutText->setName("btn_about_sidebar");
    aboutText->installEventFilter(this);
    aboutLayout->addWidget(aboutText);
    aboutLayout->addStretch(1);
    
    // Quit Navigation Button (Custom Widget)
    m_navQuitBtn = new TQWidget(m_sidebarWidget);
    m_navQuitBtn->setFixedHeight(45);
    m_navQuitBtn->setPaletteBackgroundColor(bg_color);
    m_navQuitBtn->setCursor(TQCursor(TQt::PointingHandCursor));
    m_navQuitBtn->setName("btn_quit_sidebar");
    m_navQuitBtn->installEventFilter(this);
    
    TQHBoxLayout* quitLayout = new TQHBoxLayout(m_navQuitBtn, 10, 10);
    m_navQuitIconLabel = new TQLabel(m_navQuitBtn);
    m_navQuitIconLabel->setPixmap(getScaledIcon(quit_png, quit_png_len, 32, is_dark_mode));

    m_navQuitIconLabel->setName("btn_quit_sidebar");
    m_navQuitIconLabel->installEventFilter(this);
    quitLayout->addWidget(m_navQuitIconLabel);
    
    TQLabel* quitText = new TQLabel("Quit", m_navQuitBtn);
    quitText->setFont(f);
    quitText->setPaletteForegroundColor(text_color);
    quitText->setName("btn_quit_sidebar");
    quitText->installEventFilter(this);
    quitLayout->addWidget(quitText);
    quitLayout->addStretch(1);
    
    layout->addStretch(1);
    layout->addWidget(m_navAboutBtn);
    layout->addWidget(m_navQuitBtn);
}

void MainWindow::showDashboard()
{
    m_navVpnIconLabel->setPixmap(getScaledIcon(home_on_png, home_on_png_len, 32, is_dark_mode));
    m_navSettingsIconLabel->setPixmap(getScaledIcon(settings_navigation_off_png, settings_navigation_off_png_len, 32, is_dark_mode));
    m_navAboutIconLabel->setPixmap(getScaledIcon(about_png, about_png_len, 32, is_dark_mode));
    
    m_viewStack->raiseWidget(m_dashboardWidget);
}

void MainWindow::showSettings()
{
    m_navVpnIconLabel->setPixmap(getScaledIcon(home_off_png, home_off_png_len, 32, is_dark_mode));
    m_navSettingsIconLabel->setPixmap(getScaledIcon(settings_navigation_on_png, settings_navigation_on_png_len, 32, is_dark_mode));
    m_navAboutIconLabel->setPixmap(getScaledIcon(about_png, about_png_len, 32, is_dark_mode));
    
    m_viewStack->raiseWidget(m_settingsWidget);
    
    if (m_settingsWidget) {
        ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_settingsListWidget);
    }
}

void MainWindow::setupLoginView()
{
    m_loginWidget = new TQWidget(m_viewStack);
    
    TQHBoxLayout* mainLayout = new TQHBoxLayout(m_loginWidget, 40, 20); // margins, spacing
    
    // Left side panel
    TQVBoxLayout* leftPanel = new TQVBoxLayout(mainLayout);
    leftPanel->addStretch(1);
    
    TQLabel* titleLabel = new TQLabel("<b>tdeNordgui</b>", m_loginWidget);
    titleLabel->setPaletteForegroundColor(text_color);
    TQFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleLabel->setFont(titleFont);
    leftPanel->addWidget(titleLabel);
    
    leftPanel->addSpacing(30);
    
    m_loginBtn = new TQPushButton("Log in", m_loginWidget);
    m_loginBtn->setMinimumSize(150, 40);
    m_loginBtn->setPaletteForegroundColor(text_color);
    m_loginBtn->setPaletteBackgroundColor(bg_color3);
    leftPanel->addWidget(m_loginBtn);
    
    TQPushButton* createBtn = new TQPushButton("Create account", m_loginWidget);
    createBtn->setMinimumSize(150, 40);
    createBtn->setPaletteForegroundColor(text_color);
    createBtn->setPaletteBackgroundColor(bg_color3);
    leftPanel->addWidget(createBtn);
    
    leftPanel->addSpacing(30);
    TQLabel* fallbackLabel = new TQLabel("If browser is stuck, paste the 'Continue' link here:", m_loginWidget);
    fallbackLabel->setPaletteForegroundColor(text_color);
    leftPanel->addWidget(fallbackLabel);
    
    TQHBoxLayout* inputLayout = new TQHBoxLayout(leftPanel);
    m_callbackInput = new TQLineEdit(m_loginWidget);
    m_callbackInput->setPaletteBackgroundColor(bg_color3);
    m_callbackInput->setPaletteForegroundColor(text_color);
    inputLayout->addWidget(m_callbackInput, 1);
    
    TQPushButton* pasteBtn = new TQPushButton(m_loginWidget);
    pasteBtn->setPixmap(getRawIconInverted(paste_png, paste_png_len, is_dark_mode));
    pasteBtn->setFixedSize(30, 30);
    pasteBtn->setFlat(true);
    pasteBtn->setPaletteBackgroundColor(bg_color);
    pasteBtn->setCursor(TQt::PointingHandCursor);
    inputLayout->addWidget(pasteBtn, 0);
    connect(pasteBtn, SIGNAL(clicked()), m_callbackInput, SLOT(paste()));
    
    m_submitCallbackBtn = new TQPushButton("Submit Link", m_loginWidget);
    m_submitCallbackBtn->setPaletteForegroundColor(text_color);
    m_submitCallbackBtn->setPaletteBackgroundColor(bg_color3);
    leftPanel->addWidget(m_submitCallbackBtn);
    
    leftPanel->addStretch(1);
    
    // Right side graphic
    TQLabel* graphicLabel = new TQLabel(m_loginWidget);
    TQPixmap pmIcon;
    pmIcon.loadFromData(icon_nordvpn_png, icon_nordvpn_png_len);
    graphicLabel->setPixmap(pmIcon);
    graphicLabel->setAlignment(TQt::AlignCenter);
    mainLayout->addWidget(graphicLabel, 1); // stretch factor 1
    
    connect(m_loginBtn, SIGNAL(clicked()), this, SLOT(handleLoginClicked()));
    connect(m_submitCallbackBtn, SIGNAL(clicked()), this, SLOT(handleCallbackSubmit()));
}

void MainWindow::setupDashboardView()
{
    m_dashboardWidget = new TQWidget(m_viewStack);
    TQVBoxLayout* layout = new TQVBoxLayout(m_dashboardWidget, 20, 15);
    
    // Top Status Card
    TQWidget* statusCard = new TQWidget(m_dashboardWidget);
    statusCard->setPaletteBackgroundColor(bg_color);
    TQVBoxLayout* statusLayout = new TQVBoxLayout(statusCard, 20, 10);
    
    m_statusLabel = new TQLabel("Not connected\nConnect to VPN", statusCard);
    m_statusLabel->setAlignment(TQt::AlignCenter);
    m_statusLabel->setPaletteForegroundColor(text_color);
    m_statusLabel->setPaletteBackgroundColor(bg_color);
    m_statusLabel->setBackgroundMode(TQt::PaletteBackground);
    statusLayout->addWidget(m_statusLabel);
    
    TQHBoxLayout* qcbLayout = new TQHBoxLayout(statusLayout);
    qcbLayout->addStretch(1);
    
    m_quickConnectBtn = new TQPushButton("Quick Connect", statusCard);
    m_quickConnectBtn->setMinimumHeight(40);
    m_quickConnectBtn->setFixedWidth(180);
    
    // Make button blue
    TQPalette pal = m_quickConnectBtn->palette();
    pal.setColor(TQColorGroup::Button, TQColor(60, 90, 250));
    pal.setColor(TQColorGroup::ButtonText, TQt::white);
    m_quickConnectBtn->setPalette(pal);
    m_quickConnectBtn->installEventFilter(this);
    
    qcbLayout->addWidget(m_quickConnectBtn);
    qcbLayout->addStretch(1);
    
    layout->addWidget(statusCard);
    
    // Bottom List Card
    TQWidget* listCard = new TQWidget(m_dashboardWidget);
    listCard->setPaletteBackgroundColor(bg_color);
    TQVBoxLayout* listLayout = new TQVBoxLayout(listCard, 10, 10);
    
    // Tabs header
    TQHBoxLayout* tabLayout = new TQHBoxLayout(listLayout);
    
    m_tabCountriesBtn = new TQPushButton("Countries", listCard);
    m_tabCountriesBtn->setFlat(true);
    m_tabCountriesBtn->setPaletteBackgroundColor(bg_color3);
    TQFont boldFont = m_tabCountriesBtn->font();
    boldFont.setBold(true);
    m_tabCountriesBtn->setFont(boldFont);
    TQPalette tcbPal = m_tabCountriesBtn->palette();
    tcbPal.setColor(TQColorGroup::ButtonText, text_color);
    m_tabCountriesBtn->setPalette(tcbPal);
    
    m_tabSpecialtyBtn = new TQPushButton("Specialty servers", listCard);
    m_tabSpecialtyBtn->setFlat(true);
    m_tabSpecialtyBtn->setPaletteBackgroundColor(bg_color3);
    TQPalette tsbPal = m_tabSpecialtyBtn->palette();
    tsbPal.setColor(TQColorGroup::ButtonText, text_color);
    m_tabSpecialtyBtn->setPalette(tsbPal);
    
    m_searchBtn = new TQPushButton(listCard);
    m_searchBtn->setPaletteBackgroundColor(bg_color);
    TQPixmap pmSearch = getRawIconInverted(search_png, search_png_len, is_dark_mode);
    m_searchBtn->setPixmap(pmSearch);
    m_searchBtn->setFlat(true);
    m_searchBtn->setFixedSize(32, 32);
    
    tabLayout->addWidget(m_tabCountriesBtn);
    tabLayout->addWidget(m_tabSpecialtyBtn);
    tabLayout->addStretch(1);
    
    m_searchEdit = new TQLineEdit(listCard);
    m_searchEdit->hide(); // Initially hidden
    tabLayout->addWidget(m_searchEdit);
    
    tabLayout->addWidget(m_searchBtn);
    
    // List Stack
    m_listStack = new TQWidgetStack(listCard);
    
    m_countriesWidget = new TQWidget(m_listStack);
    TQVBoxLayout* cLayout = new TQVBoxLayout(m_countriesWidget);
    m_countriesList = new TQListBox(m_countriesWidget);
    m_countriesList->setFrameStyle(TQFrame::NoFrame);
    m_countriesList->setPaletteBackgroundColor(bg_color);
    m_countriesList->setPaletteForegroundColor(text_color);
    cLayout->addWidget(m_countriesList);
    
    m_specialtyWidget = new TQWidget(m_listStack);
    TQVBoxLayout* sLayout = new TQVBoxLayout(m_specialtyWidget);
    m_specialtyList = new TQListBox(m_specialtyWidget);
    m_specialtyList->setFrameStyle(TQFrame::NoFrame);
    m_specialtyList->setPaletteBackgroundColor(bg_color);
    m_specialtyList->setPaletteForegroundColor(text_color);
    sLayout->addWidget(m_specialtyList);
    
    m_listStack->addWidget(m_countriesWidget, 0);
    m_listStack->addWidget(m_specialtyWidget, 1);
    
    listLayout->addWidget(m_listStack, 1);
    layout->addWidget(listCard, 1);
    
    connect(m_tabCountriesBtn, SIGNAL(clicked()), this, SLOT(showCountriesTab()));
    connect(m_tabSpecialtyBtn, SIGNAL(clicked()), this, SLOT(showSpecialtyTab()));
    connect(m_searchBtn, SIGNAL(clicked()), this, SLOT(onSearchClicked()));
    connect(m_searchEdit, SIGNAL(textChanged(const TQString&)), this, SLOT(onSearchTextChanged(const TQString&)));
    
    connect(m_countriesList, SIGNAL(doubleClicked(TQListBoxItem*)), this, SLOT(onServerSelected(TQListBoxItem*)));
    connect(m_specialtyList, SIGNAL(doubleClicked(TQListBoxItem*)), this, SLOT(onServerSelected(TQListBoxItem*)));
    connect(m_quickConnectBtn, SIGNAL(clicked()), this, SLOT(onQuickConnectClicked()));
}

void MainWindow::populateServerLists()
{
    m_cachedCountries = m_client->getCountries();
    m_cachedGroups = m_client->getGroups();
    
    updateListWithFilter(m_countriesList, m_cachedCountries, "");
    updateListWithFilter(m_specialtyList, m_cachedGroups, "");
    
    if (m_acCountriesList) updateListWithFilter(m_acCountriesList, m_cachedCountries, "", true);
    if (m_acSpecialtyList) updateListWithFilter(m_acSpecialtyList, m_cachedGroups, "");
}

void MainWindow::updateListWithFilter(TQListBox* list, const TQValueList<DaemonClient::ServerGroup>& data, const TQString& filter, bool addFastestItem)
{
    list->clear();
    
    if (addFastestItem && filter.isEmpty()) {
        TQPixmap pm = getScaledIcon(fastest_server_png, fastest_server_png_len, 24, is_dark_mode);
        new RichServerItem(list, pm, "Fastest");
    }
    
    TQString lowerFilter = filter.lower();
    
    static TQMap<TQString, FlagIconData> flagsMap = getFlagsMap();
    
    for (TQValueList<DaemonClient::ServerGroup>::const_iterator it = data.begin(); it != data.end(); ++it) {
        if (filter.isEmpty() || (*it).name.lower().contains(lowerFilter)) {
            m_countryPayloadMap.insert((*it).name, (*it).connectPayload);
            TQString code = (*it).code.lower().replace("_", " ");
            if (!code.isEmpty() && flagsMap.contains(code)) {
                const FlagIconData& fid = flagsMap[code];
                TQPixmap pm = getScaledIcon(fid.data, fid.len, 24, false); // Explicitly do NOT invert flags
                new RichServerItem(list, pm, (*it).name);
            } else if ((*it).name == "Dedicated_IP") {
                TQPixmap pm = getScaledIcon(dedicated_ip_png, dedicated_ip_png_len, 24, is_dark_mode);
                new RichServerItem(list, pm, (*it).name);
            } else if ((*it).name == "Double_VPN") {
                TQPixmap pm = getScaledIcon(double_vpn_png, double_vpn_png_len, 24, is_dark_mode);
                new RichServerItem(list, pm, (*it).name);
            } else if ((*it).name == "Onion_Over_VPN") {
                TQPixmap pm = getScaledIcon(onion_over_vpn_png, onion_over_vpn_png_len, 24, is_dark_mode);
                new RichServerItem(list, pm, (*it).name);
            } else if ((*it).name == "P2P") {
                TQPixmap pm = getScaledIcon(p2p_png, p2p_png_len, 24, is_dark_mode);
                new RichServerItem(list, pm, (*it).name);
            } else {
                TQPixmap pm = getScaledIcon(noflag_png, noflag_png_len, 24, is_dark_mode);
                new RichServerItem(list, pm, (*it).name);
            }
        }
    }
}

void MainWindow::showCountriesTab()
{
    TQFont boldFont = m_tabCountriesBtn->font();
    boldFont.setBold(true);
    m_tabCountriesBtn->setFont(boldFont);
    
    TQFont normalFont = m_tabSpecialtyBtn->font();
    normalFont.setBold(false);
    m_tabSpecialtyBtn->setFont(normalFont);
    
    m_listStack->raiseWidget(m_countriesWidget);
}

void MainWindow::showSpecialtyTab()
{
    TQFont normalFont = m_tabCountriesBtn->font();
    normalFont.setBold(false);
    m_tabCountriesBtn->setFont(normalFont);
    
    TQFont boldFont = m_tabSpecialtyBtn->font();
    boldFont.setBold(true);
    m_tabSpecialtyBtn->setFont(boldFont);
    
    m_listStack->raiseWidget(m_specialtyWidget);
}

void MainWindow::onSearchClicked()
{
    if (m_searchEdit->isHidden()) {
        m_searchEdit->show();
        m_searchEdit->setFocus();
        TQPixmap pmClose = getRawIconInverted(close_search_png, close_search_png_len, is_dark_mode);
        m_searchBtn->setPixmap(pmClose);
    } else {
        m_searchEdit->hide();
        m_searchEdit->clear(); // Will automatically trigger onSearchTextChanged to clear filter
        TQPixmap pmSearch = getRawIconInverted(search_png, search_png_len, is_dark_mode);
        m_searchBtn->setPixmap(pmSearch);
    }
}

void MainWindow::onSearchTextChanged(const TQString& text)
{
    updateListWithFilter(m_countriesList, m_cachedCountries, text);
    updateListWithFilter(m_specialtyList, m_cachedGroups, text);
}

bool MainWindow::eventFilter(TQObject *obj, TQEvent *event)
{
    if (event->type() == TQEvent::MouseButtonRelease) {
        if (obj == m_lblAutoConnectTo) {
            if (m_lblAutoConnectTo->isEnabled() && m_settingsWidget) {
                ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageAutoConnectServer);
            }
            return true;
        }
        TQString name = obj->name();
        if (name == "btn_vpn") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageVpnConnection);
            return true;
        } else if (name == "btn_security") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageSecurityPrivacy);
            return true;
        } else if (name == "btn_threat") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageThreatProtection);
            return true;
        } else if (name == "btn_general") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageGeneral);
            return true;
        } else if (name == "btn_terms") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageTerms);
            return true;
        } else if (name == "btn_about_sidebar") {
            onAboutClicked();
            return true;
        } else if (name == "btn_quit_sidebar") {
            quitApplication();
            return true;
        } else if (name == "btn_account") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageAccount);
            return true;
        } else if (name == "btn_back_ac") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageVpnConnection);
            return true;
        } else if (name == "nav_allowlist") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageAllowlist);
            return true;
        } else if (name == "nav_custom_dns") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageCustomDns);
            return true;
        } else if (name == "btn_back_sec_privacy") {
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageSecurityPrivacy);
            return true;
        } else if (name == "btn_back_settings") { // Back button on any page
            ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_settingsListWidget);
            return true;
        } else if (name.startsWith("link_")) {
            TQString url = name.mid(5);
            system(TQString("xdg-open \"%1\" &").arg(url).ascii());
            return true;
        }
    }
    
    if (obj == m_navVpnBtn && event->type() == TQEvent::MouseButtonRelease) {
        showDashboard();
        return true;
    }
    if (obj == m_navSettingsBtn && event->type() == TQEvent::MouseButtonRelease) {
        showSettings();
        return true;
    }
    
    if (obj == m_quickConnectBtn) {
        if (event->type() == TQEvent::Enter) {
            TQPalette pal = m_quickConnectBtn->palette();
            pal.setColor(TQColorGroup::ButtonText, TQt::black);
            m_quickConnectBtn->setPalette(pal);
        } else if (event->type() == TQEvent::Leave) {
            TQPalette pal = m_quickConnectBtn->palette();
            pal.setColor(TQColorGroup::ButtonText, TQt::white);
            m_quickConnectBtn->setPalette(pal);
        }
    }
    return TQMainWindow::eventFilter(obj, event);
}

void MainWindow::setupSettingsVpnConnection()
{
    TQVBoxLayout* layout = new TQVBoxLayout(m_pageVpnConnection, 20, 15);
    
    TQHBoxLayout* header = new TQHBoxLayout(layout);
    header->setSpacing(5);
    
    TQLabel* backBtn = new TQLabel("Settings", m_pageVpnConnection);
    backBtn->setName("btn_back_settings");
    TQFont f = backBtn->font();
    f.setPointSize(14);
    f.setBold(true);
    backBtn->setFont(f);
    backBtn->setPaletteForegroundColor(text_color);
    backBtn->setCursor(TQt::PointingHandCursor);
    backBtn->installEventFilter(this);
    
    TQLabel* sep = new TQLabel("/", m_pageVpnConnection);
    sep->setFont(f);
    sep->setPaletteForegroundColor(text_color);
    
    TQLabel* title = new TQLabel("VPN Connection", m_pageVpnConnection);
    title->setFont(f);
    title->setPaletteForegroundColor(text_color);
    
    header->addWidget(backBtn);
    header->addWidget(sep);
    header->addWidget(title);
    header->addStretch(1);
    
    layout->addSpacing(10);
    
    m_chkAutoConnect = new TQCheckBox("Auto-connect\nAutomatically connect to the fastest available server or your chosen server location\nwhen the app starts.", m_pageVpnConnection);
    m_chkAutoConnect->setPaletteForegroundColor(text_color);
    layout->addWidget(m_chkAutoConnect);
    m_lblAutoConnectTo = new TQLabel("Auto-connect to:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Fastest (Quick Connect) &gt;", m_pageVpnConnection);
    m_lblAutoConnectTo->setTextFormat(Qt::RichText); // just for &nbsp; space
    m_lblAutoConnectTo->setPaletteForegroundColor(text_color);
    m_lblAutoConnectTo->setCursor(TQCursor(TQt::PointingHandCursor));
    m_lblAutoConnectTo->installEventFilter(this);
    layout->addWidget(m_lblAutoConnectTo);
    
    TQFrame* line1 = new TQFrame(m_pageVpnConnection); line1->setFrameShape(TQFrame::HLine); line1->setPaletteForegroundColor(text_color); layout->addWidget(line1);
    
    m_chkKillSwitch = new TQCheckBox("Kill Switch\nDisable internet access if the VPN connection drops to secure your data from accidental\nexposure.", m_pageVpnConnection);
    m_chkKillSwitch->setPaletteForegroundColor(text_color);
    layout->addWidget(m_chkKillSwitch);
    
    TQFrame* line2 = new TQFrame(m_pageVpnConnection); line2->setFrameShape(TQFrame::HLine); line2->setPaletteForegroundColor(text_color); layout->addWidget(line2);
    
    TQLabel* protocolLbl = new TQLabel("VPN Protocol", m_pageVpnConnection);
    protocolLbl->setPaletteForegroundColor(text_color);
    layout->addWidget(protocolLbl);
    
    TQButtonGroup* bgProto = new TQButtonGroup(1, Qt::Horizontal, m_pageVpnConnection);
    bgProto->setFrameStyle(TQFrame::NoFrame);
    bgProto->setTitle("");
    m_rbNordLynx = new TQRadioButton("NordLynx", bgProto); m_rbNordLynx->setPaletteForegroundColor(text_color); m_rbNordLynx->setChecked(true);
    m_rbNordWhisper = new TQRadioButton("NordWhisper", bgProto); m_rbNordWhisper->setPaletteForegroundColor(text_color);
    m_rbOpenVpnTcp = new TQRadioButton("OpenVPN (TCP)", bgProto); m_rbOpenVpnTcp->setPaletteForegroundColor(text_color);
    m_rbOpenVpnUdp = new TQRadioButton("OpenVPN (UDP)", bgProto); m_rbOpenVpnUdp->setPaletteForegroundColor(text_color);
    layout->addWidget(bgProto);
    
    connect(m_chkAutoConnect, SIGNAL(toggled(bool)), this, SLOT(onAutoConnectToggled(bool)));
    connect(m_chkKillSwitch, SIGNAL(toggled(bool)), this, SLOT(onKillSwitchToggled(bool)));
    connect(bgProto, SIGNAL(clicked(int)), this, SLOT(onProtocolChanged(int)));
    
    layout->addStretch(1);
}

void MainWindow::setupSettingsSecurity()
{
    TQVBoxLayout* layout = new TQVBoxLayout(m_pageSecurityPrivacy, 20, 15);
    
    TQHBoxLayout* header = new TQHBoxLayout(layout);
    header->setSpacing(5);
    
    TQLabel* backBtn = new TQLabel("Settings", m_pageSecurityPrivacy);
    backBtn->setName("btn_back_settings");
    TQFont f = backBtn->font();
    f.setPointSize(14);
    f.setBold(true);
    backBtn->setFont(f);
    backBtn->setPaletteForegroundColor(text_color);
    backBtn->setCursor(TQt::PointingHandCursor);
    backBtn->installEventFilter(this);
    
    TQLabel* sep = new TQLabel("/", m_pageSecurityPrivacy);
    sep->setFont(f);
    sep->setPaletteForegroundColor(text_color);
    
    TQLabel* title = new TQLabel("Security and Privacy", m_pageSecurityPrivacy);
    title->setFont(f);
    title->setPaletteForegroundColor(text_color);
    
    header->addWidget(backBtn);
    header->addWidget(sep);
    header->addWidget(title);
    header->addStretch(1);
    
    layout->addSpacing(10);
    
    TQFrame* frmAllow = new TQFrame(m_pageSecurityPrivacy);
    frmAllow->setCursor(TQt::PointingHandCursor);
    frmAllow->installEventFilter(this);
    frmAllow->setName("nav_allowlist");
    TQHBoxLayout* row1 = new TQHBoxLayout(frmAllow);
    TQLabel* lblA1 = new TQLabel("Allowlist\nExclude ports, port ranges, or subnets from VPN protection.", frmAllow);
    lblA1->setName("nav_allowlist"); lblA1->installEventFilter(this);
    lblA1->setPaletteForegroundColor(text_color);
    row1->addWidget(lblA1);
    row1->addStretch(1);
    TQLabel* lblA2 = new TQLabel(">", frmAllow);
    lblA2->setName("nav_allowlist"); lblA2->installEventFilter(this);
    lblA2->setPaletteForegroundColor(text_color);
    row1->addWidget(lblA2);
    layout->addWidget(frmAllow);
    
    TQFrame* line1 = new TQFrame(m_pageSecurityPrivacy); line1->setFrameShape(TQFrame::HLine); line1->setPaletteForegroundColor(text_color); layout->addWidget(line1);
    
    TQFrame* frmDns = new TQFrame(m_pageSecurityPrivacy);
    frmDns->setCursor(TQt::PointingHandCursor);
    frmDns->installEventFilter(this);
    frmDns->setName("nav_custom_dns");
    TQHBoxLayout* row2 = new TQHBoxLayout(frmDns);
    TQLabel* lblD1 = new TQLabel("Custom DNS\nSet custom DNS server addresses to use.", frmDns);
    lblD1->setName("nav_custom_dns"); lblD1->installEventFilter(this);
    lblD1->setPaletteForegroundColor(text_color);
    row2->addWidget(lblD1);
    row2->addStretch(1);
    TQLabel* lblD2 = new TQLabel(">", frmDns);
    lblD2->setName("nav_custom_dns"); lblD2->installEventFilter(this);
    lblD2->setPaletteForegroundColor(text_color);
    row2->addWidget(lblD2);
    layout->addWidget(frmDns);
    
    TQFrame* line2 = new TQFrame(m_pageSecurityPrivacy); line2->setFrameShape(TQFrame::HLine); line2->setPaletteForegroundColor(text_color); layout->addWidget(line2);

    m_chkLan = new TQCheckBox("LAN discovery\nMake your device visible to other devices on your local network while connected to the\nVPN. Access printers, TVs, and other LAN devices.", m_pageSecurityPrivacy);
    m_chkLan->setPaletteForegroundColor(text_color);
    layout->addWidget(m_chkLan);
    
    TQFrame* line3 = new TQFrame(m_pageSecurityPrivacy); line3->setFrameShape(TQFrame::HLine); line3->setPaletteForegroundColor(text_color); layout->addWidget(line3);

    m_chkFirewall = new TQCheckBox("Firewall\nAllow the use of the system firewall. When enabled, you can attach a firewall mark to\nVPN packets for custom firewall rules.", m_pageSecurityPrivacy);
    m_chkFirewall->setPaletteForegroundColor(text_color);
    layout->addWidget(m_chkFirewall);
    
    TQHBoxLayout* fwLayout = new TQHBoxLayout(layout);
    fwLayout->addSpacing(30);
    m_lblFwm = new TQLabel("Firewall mark", m_pageSecurityPrivacy);
    m_lblFwm->setPaletteForegroundColor(text_color);
    fwLayout->addWidget(m_lblFwm);
    m_edtFwm = new TQLineEdit("0xe1f1", m_pageSecurityPrivacy);
    m_edtFwm->setFixedWidth(100);
    fwLayout->addWidget(m_edtFwm);
    m_btnFwmSave = new TQPushButton("Save", m_pageSecurityPrivacy);
    fwLayout->addWidget(m_btnFwmSave);
    
    m_edtFwm->setEnabled(false);
    m_btnFwmSave->setEnabled(false);
    
    fwLayout->addStretch(1);
    
    TQFrame* line4 = new TQFrame(m_pageSecurityPrivacy); line4->setFrameShape(TQFrame::HLine); line4->setPaletteForegroundColor(text_color); layout->addWidget(line4);
    
    m_chkPostQuantum = new TQCheckBox("Post-quantum encryption\nActivate next-generation encryption that protects your data from threats posed by\nquantum computing.", m_pageSecurityPrivacy);
    m_chkPostQuantum->setPaletteForegroundColor(text_color);
    layout->addWidget(m_chkPostQuantum);
    
    m_chkObfuscate = new TQCheckBox("Obfuscated Servers\nBypass network restrictions.", m_pageSecurityPrivacy);
    m_chkObfuscate->setPaletteForegroundColor(text_color);
    layout->addWidget(m_chkObfuscate);
    m_chkObfuscate->hide(); // Hide it, actually it isn't in screenshot but daemon supports it. Let's keep it functionally but invisible unless we add it to the bottom.
    
    connect(m_chkLan, SIGNAL(toggled(bool)), this, SLOT(onLanDiscoveryToggled(bool)));
    connect(m_chkFirewall, SIGNAL(toggled(bool)), this, SLOT(onFirewallToggled(bool)));
    connect(m_chkPostQuantum, SIGNAL(toggled(bool)), this, SLOT(onPostQuantumToggled(bool)));
    connect(m_chkObfuscate, SIGNAL(toggled(bool)), this, SLOT(onObfuscateToggled(bool)));
    
    layout->addStretch(1);
}

void MainWindow::setupSettingsThreatProtection()
{
    TQVBoxLayout* layout = new TQVBoxLayout(m_pageThreatProtection, 20, 15);
    
    TQHBoxLayout* header = new TQHBoxLayout(layout);
    header->setSpacing(5);
    
    TQLabel* backBtn = new TQLabel("Settings", m_pageThreatProtection);
    backBtn->setName("btn_back_settings");
    TQFont f = backBtn->font();
    f.setPointSize(14);
    f.setBold(true);
    backBtn->setFont(f);
    
    backBtn->setPaletteForegroundColor(text_color);
    backBtn->setCursor(TQt::PointingHandCursor);
    backBtn->installEventFilter(this);
    
    TQLabel* sep = new TQLabel("/", m_pageThreatProtection);
    sep->setFont(f);
    sep->setPaletteForegroundColor(text_color);
    
    TQLabel* title = new TQLabel("Threat Protection", m_pageThreatProtection);
    title->setFont(f);
    title->setPaletteForegroundColor(text_color);
    
    header->addWidget(backBtn);
    header->addWidget(sep);
    header->addWidget(title);
    header->addStretch(1);
    
    layout->addSpacing(10);
    
    m_chkThreatProtection = new TQCheckBox("Threat Protection Lite (Blocks dangerous websites and flashy ads at the domain level.)", m_pageThreatProtection);
    m_chkThreatProtection->setPaletteForegroundColor(text_color);
    layout->addWidget(m_chkThreatProtection);
    connect(m_chkThreatProtection, SIGNAL(toggled(bool)), this, SLOT(onThreatProtectionLiteToggled(bool)));
    layout->addStretch(1);
}

void MainWindow::setupSettingsGeneral()
{
    TQVBoxLayout* layout = new TQVBoxLayout(m_pageGeneral, 20, 15);
    
    TQHBoxLayout* header = new TQHBoxLayout(layout);
    header->setSpacing(5);
    
    TQLabel* backBtn = new TQLabel("Settings", m_pageGeneral);
    backBtn->setName("btn_back_settings");
    TQFont f = backBtn->font();
    f.setPointSize(14);
    f.setBold(true);
    backBtn->setFont(f);
    
    backBtn->setPaletteForegroundColor(text_color);
    backBtn->setCursor(TQCursor(TQt::PointingHandCursor));
    backBtn->installEventFilter(this);
    
    TQLabel* sep = new TQLabel("/", m_pageGeneral);
    sep->setFont(f);
    sep->setPaletteForegroundColor(text_color);
    
    TQLabel* title = new TQLabel("General", m_pageGeneral);
    title->setFont(f);
    title->setPaletteForegroundColor(text_color);
    
    header->addWidget(backBtn);
    header->addWidget(sep);
    header->addWidget(title);
    header->addStretch(1);
    
    layout->addSpacing(10);
    
    // Appearance
    TQHBoxLayout* appearanceLayout = new TQHBoxLayout(layout);
    TQLabel* lblAppearance = new TQLabel("Appearance", m_pageGeneral);
    lblAppearance->setPaletteForegroundColor(text_color);
    appearanceLayout->addWidget(lblAppearance);
    appearanceLayout->addStretch(1);
    
    m_bgApp = new TQButtonGroup(1, TQt::Horizontal, m_pageGeneral);
    m_bgApp->setFrameStyle(TQFrame::NoFrame);
    m_bgApp->setTitle("");
    m_rbLight = new TQRadioButton("Light", m_bgApp);
    m_rbDark = new TQRadioButton("Dark", m_bgApp);
    m_rbLight->setPaletteForegroundColor(text_color);
    m_rbDark->setPaletteForegroundColor(text_color);
    m_bgApp->insert(m_rbLight, 0);
    m_bgApp->insert(m_rbDark, 1);
    
    if (checkThemeFileExists()) m_rbDark->setChecked(true);
    else m_rbLight->setChecked(true);
    
    connect(m_bgApp, SIGNAL(clicked(int)), this, SLOT(onThemeChanged(int)));
    appearanceLayout->addWidget(m_bgApp);
    
    TQFrame* lineApp = new TQFrame(m_pageGeneral); lineApp->setFrameShape(TQFrame::HLine); lineApp->setPaletteForegroundColor(text_color); layout->addWidget(lineApp);
    
    // Notifications toggle
    m_chkNotifications = new TQCheckBox("Show notifications", m_pageGeneral);
    m_chkNotifications->setPaletteForegroundColor(text_color);
    m_chkNotifications->setChecked(true); // Default
    layout->addWidget(m_chkNotifications);
    
    connect(m_chkNotifications, SIGNAL(toggled(bool)), this, SLOT(onNotifyToggled(bool)));
    
    TQFrame* lineNotif = new TQFrame(m_pageGeneral); lineNotif->setFrameShape(TQFrame::HLine); lineNotif->setPaletteForegroundColor(text_color); layout->addWidget(lineNotif);
    
    TQHBoxLayout* resetLayout = new TQHBoxLayout(layout);
    TQLabel* lblReset = new TQLabel("Reset all app settings to default", m_pageGeneral);
    lblReset->setPaletteForegroundColor(text_color);
    resetLayout->addWidget(lblReset);
    resetLayout->addStretch(1);
    TQPushButton* btnReset = new TQPushButton("Reset", m_pageGeneral);
    TQPalette rbPal = btnReset->palette();
    rbPal.setColor(TQColorGroup::ButtonText, TQt::white);
    rbPal.setColor(TQColorGroup::Button, TQColor(70, 100, 250)); // blue
    btnReset->setPalette(rbPal);
    connect(btnReset, SIGNAL(clicked()), this, SLOT(onResetAppClicked()));
    resetLayout->addWidget(btnReset);
    
    layout->addStretch(1);
}

void MainWindow::setupSettingsTerms()
{
    TQVBoxLayout* layout = new TQVBoxLayout(m_pageTerms, 20, 15);
    TQHBoxLayout* header = new TQHBoxLayout(layout);
    header->setSpacing(5);
    
    TQLabel* backBtn = new TQLabel("Settings", m_pageTerms);
    backBtn->setName("btn_back_settings");
    TQFont f = backBtn->font(); f.setPointSize(14); f.setBold(true);
    backBtn->setFont(f);
    backBtn->setPaletteForegroundColor(text_color);
    backBtn->setCursor(TQCursor(TQt::PointingHandCursor));
    backBtn->installEventFilter(this);
    
    TQLabel* sep = new TQLabel("/", m_pageTerms); sep->setFont(f); sep->setPaletteForegroundColor(text_color);
    TQLabel* title = new TQLabel("Terms", m_pageTerms); title->setFont(f); title->setPaletteForegroundColor(text_color);
    
    header->addWidget(backBtn); header->addWidget(sep); header->addWidget(title); header->addStretch(1);
    layout->addSpacing(10);
    
    TQLabel* desc = new TQLabel("By continuing to use this app, you agree to our terms and how we handle your data.\nTo read the terms and privacy policy check the links below.", m_pageTerms);
    desc->setPaletteForegroundColor(text_color);
    layout->addWidget(desc);
    
    // Read Mores placeholder
    TQLabel* t1 = new TQLabel("<b>Terms of Service</b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='https://my.nordaccount.com/legal/terms-of-service/'>Read more</a>", m_pageTerms);
    t1->setPaletteForegroundColor(text_color);
    t1->setName("link_https://my.nordaccount.com/legal/terms-of-service/");
    t1->setCursor(TQt::PointingHandCursor);
    t1->installEventFilter(this);
    
    TQLabel* t2 = new TQLabel("<b>Auto - renewal terms</b>&nbsp;&nbsp;&nbsp;&nbsp;<a href='https://my.nordaccount.com/legal/terms-of-service/subscription/#Automatic%20renewal'>Read more</a>", m_pageTerms);
    t2->setPaletteForegroundColor(text_color);
    t2->setName("link_https://my.nordaccount.com/legal/terms-of-service/subscription/#Automatic%20renewal");
    t2->setCursor(TQt::PointingHandCursor);
    t2->installEventFilter(this);
    
    TQLabel* t3 = new TQLabel("<b>Privacy Policy</b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='https://my.nordaccount.com/legal/privacy-policy/'>Read more</a>", m_pageTerms);
    t3->setPaletteForegroundColor(text_color);
    t3->setName("link_https://my.nordaccount.com/legal/privacy-policy/");
    t3->setCursor(TQt::PointingHandCursor);
    t3->installEventFilter(this);
    layout->addWidget(t1); layout->addWidget(t2); layout->addWidget(t3);
    
    layout->addStretch(1);
}

void MainWindow::setupSettingsAccount()
{
    TQVBoxLayout* layout = new TQVBoxLayout(m_pageAccount, 20, 15);
    TQHBoxLayout* header = new TQHBoxLayout(layout);
    header->setSpacing(5);
    
    TQLabel* backBtn = new TQLabel("Settings", m_pageAccount);
    backBtn->setName("btn_back_settings");
    TQFont f = backBtn->font(); f.setPointSize(14); f.setBold(true);
    backBtn->setFont(f);
    backBtn->setPaletteForegroundColor(text_color);
    backBtn->setCursor(TQCursor(TQt::PointingHandCursor));
    backBtn->installEventFilter(this);
    
    TQLabel* sep = new TQLabel("/", m_pageAccount); sep->setFont(f); sep->setPaletteForegroundColor(text_color);
    TQLabel* title = new TQLabel("Account", m_pageAccount); title->setFont(f); title->setPaletteForegroundColor(text_color);
    
    header->addWidget(backBtn); header->addWidget(sep); header->addWidget(title); header->addStretch(1);
    layout->addSpacing(10);
    
    m_lblSubscriptionStatus = new TQLabel("Subscription\n<font color='gray'>Active until (Checking...)</font>", m_pageAccount);
    m_lblSubscriptionStatus->setTextFormat(TQt::RichText);
    m_lblSubscriptionStatus->setPaletteForegroundColor(text_color);
    
    TQHBoxLayout* subLayout = new TQHBoxLayout(layout);
    subLayout->addWidget(m_lblSubscriptionStatus);
    subLayout->addStretch(1);
    TQLabel* lnkManage = new TQLabel("<a href='https://my.nordaccount.com/billing/my-subscriptions/?utm_medium=app&utm_source=nordvpn-linux-gui&utm_campaign=settings_account-manage_subscription&nm=app&ns=nordvpn-linux-gui&nc=settings-manage_subscription'>Manage subscription</a>", m_pageAccount);
    lnkManage->setPaletteForegroundColor(text_color);
    lnkManage->setName("link_https://my.nordaccount.com/billing/my-subscriptions/?utm_medium=app&utm_source=nordvpn-linux-gui&utm_campaign=settings_account-manage_subscription&nm=app&ns=nordvpn-linux-gui&nc=settings-manage_subscription");
    lnkManage->setCursor(TQt::PointingHandCursor);
    lnkManage->installEventFilter(this);
    subLayout->addWidget(lnkManage);
    layout->addSpacing(10);
    
    m_lblAccountInfo = new TQLabel("user@domain.com\n<font color='gray'>Account created: (Checking...)</font>", m_pageAccount);
    m_lblAccountInfo->setTextFormat(TQt::RichText);
    m_lblAccountInfo->setPaletteForegroundColor(text_color);
    TQHBoxLayout* accLayout = new TQHBoxLayout(layout);
    accLayout->addWidget(m_lblAccountInfo);
    accLayout->addStretch(1);
    TQLabel* lnkPass = new TQLabel("<a href='https://my.nordaccount.com/account-settings/account-management/?utm_medium=app&utm_source=nordvpn-linux-gui&utm_campaign=settings_account-change_password&nm=app&ns=nordvpn-linux-gui&nc=settings-change_password'>Change password</a>", m_pageAccount);
    lnkPass->setPaletteForegroundColor(text_color);
    lnkPass->setName("link_https://my.nordaccount.com/account-settings/account-management/?utm_medium=app&utm_source=nordvpn-linux-gui&utm_campaign=settings_account-change_password&nm=app&ns=nordvpn-linux-gui&nc=settings-change_password");
    lnkPass->setCursor(TQt::PointingHandCursor);
    lnkPass->installEventFilter(this);
    accLayout->addWidget(lnkPass);
    layout->addSpacing(10);
    
    TQPushButton* btnLogout = new TQPushButton("Log out", m_pageAccount);
    btnLogout->setMinimumHeight(40);
    btnLogout->setFixedWidth(120);
    layout->addWidget(btnLogout);
    connect(btnLogout, SIGNAL(clicked()), this, SLOT(onLogoutClicked()));
    layout->addSpacing(20);
    
    TQLabel* lblHub = new TQLabel("Product Hub", m_pageAccount);
    lblHub->setPaletteForegroundColor(text_color);
    layout->addWidget(lblHub);
    
    TQLabel* p1 = new TQLabel("<b>NordPass</b><br><font color='gray'>Generate, store, and organize your passwords.</font><br><a href='https://nordpass.com/'>Learn more</a>", m_pageAccount);
    p1->setPaletteForegroundColor(text_color);
    p1->setName("link_https://nordpass.com/");
    p1->setCursor(TQt::PointingHandCursor);
    p1->installEventFilter(this);
    layout->addWidget(p1);
    
    TQLabel* p2 = new TQLabel("<b>NordLocker</b><br><font color='gray'>Store your files securely in our end-to-end encrypted cloud.</font><br><a href='https://nordlocker.com/'>Learn more</a>", m_pageAccount);
    p2->setPaletteForegroundColor(text_color);
    p2->setName("link_https://nordlocker.com/");
    p2->setCursor(TQt::PointingHandCursor);
    p2->installEventFilter(this);
    layout->addWidget(p2);
    
    TQLabel* p3 = new TQLabel("<b>NordLayer</b><br><font color='gray'>Get a powerful security solution for your business network.</font><br><a href='https://nordlayer.com/'>Learn more</a>", m_pageAccount);
    p3->setPaletteForegroundColor(text_color);
    p3->setName("link_https://nordlayer.com/");
    p3->setCursor(TQt::PointingHandCursor);
    p3->installEventFilter(this);
    layout->addWidget(p3);
    
    layout->addStretch(1);
}

void MainWindow::setupSettingsCustomDns()
{
    m_pageCustomDns = new TQWidget(m_settingsWidget);
    TQVBoxLayout* layout = new TQVBoxLayout(m_pageCustomDns, 20, 15);
    
    TQHBoxLayout* header = new TQHBoxLayout(layout);
    header->setSpacing(5);
    
    TQLabel* link1 = new TQLabel("Settings", m_pageCustomDns); link1->setName("btn_back_settings");
    TQFont f = link1->font(); f.setPointSize(14); f.setBold(true); link1->setFont(f);
    link1->setPaletteForegroundColor(text_color); link1->setCursor(TQt::PointingHandCursor); link1->installEventFilter(this);
    header->addWidget(link1);
    
    TQLabel* sep1 = new TQLabel("/", m_pageCustomDns); sep1->setFont(f); sep1->setPaletteForegroundColor(text_color); header->addWidget(sep1);
    
    TQLabel* link2 = new TQLabel("Security and privacy", m_pageCustomDns); link2->setName("btn_back_sec_privacy");
    link2->setFont(f); link2->setPaletteForegroundColor(text_color); link2->setCursor(TQt::PointingHandCursor); link2->installEventFilter(this);
    header->addWidget(link2);
    
    TQLabel* sep2 = new TQLabel("/", m_pageCustomDns); sep2->setFont(f); sep2->setPaletteForegroundColor(text_color); header->addWidget(sep2);
    
    TQLabel* title = new TQLabel("Custom DNS", m_pageCustomDns); title->setFont(f); title->setPaletteForegroundColor(text_color); header->addWidget(title);
    header->addStretch(1);
    
    layout->addSpacing(10);
    
    TQHBoxLayout* toggleRow = new TQHBoxLayout(layout);
    TQLabel* lblTop = new TQLabel("Use custom DNS\nAdd up to three DNS servers.", m_pageCustomDns);
    lblTop->setPaletteForegroundColor(text_color);
    toggleRow->addWidget(lblTop);
    toggleRow->addStretch(1);
    m_chkCustomDns = new TQCheckBox("", m_pageCustomDns); 
    m_chkCustomDns->setPaletteForegroundColor(text_color); 
    toggleRow->addWidget(m_chkCustomDns);
    connect(m_chkCustomDns, SIGNAL(toggled(bool)), this, SLOT(onCustomDnsToggle(bool)));
    
    layout->addSpacing(15);
    
    m_customDnsBox = new TQFrame(m_pageCustomDns);
    m_customDnsBox->setFrameShape(TQFrame::NoFrame);
    m_customDnsBox->setPaletteBackgroundColor(bg_color);
    TQVBoxLayout* boxLayout = new TQVBoxLayout(m_customDnsBox, 15, 10);
    TQLabel* lblHint = new TQLabel("Enter DNS server address:", m_customDnsBox);
    lblHint->setPaletteForegroundColor(text_color);
    boxLayout->addWidget(lblHint);
    TQHBoxLayout* inputRow = new TQHBoxLayout(boxLayout);
    m_customDnsEdtBase = new TQLineEdit("0.0.0.0", m_customDnsBox);
    m_customDnsEdtBase->setPaletteForegroundColor(text_color);
    m_customDnsEdtBase->setPaletteBackgroundColor(bg_color3);
    inputRow->addWidget(m_customDnsEdtBase);
    TQPushButton* btnAdd = new TQPushButton("Add", m_customDnsBox);
    btnAdd->setPaletteForegroundColor(text_color);
    btnAdd->setPaletteBackgroundColor(bg_color);
    connect(btnAdd, SIGNAL(clicked()), this, SLOT(onCustomDnsAddClicked()));
    inputRow->addWidget(btnAdd);
    layout->addWidget(m_customDnsBox);
    m_customDnsBox->setEnabled(false);
    
    // The list container
    m_customDnsContainer = new TQWidget(m_pageCustomDns);
    TQVBoxLayout* listContainerLayout = new TQVBoxLayout(m_customDnsContainer, 0, 10);
    TQLabel* listLbl = new TQLabel("Custom DNS", m_customDnsContainer);
    listLbl->setPaletteForegroundColor(text_color);
    TQFont listLblF = listLbl->font(); listLblF.setPointSize(listLblF.pointSize() - 1); listLbl->setFont(listLblF);
    listContainerLayout->addWidget(listLbl);
    TQFrame* listLine = new TQFrame(m_customDnsContainer); listLine->setFrameShape(TQFrame::HLine); listLine->setPaletteForegroundColor(text_color);
    listContainerLayout->addWidget(listLine);
    m_customDnsEntriesLayout = new TQVBoxLayout(listContainerLayout, 5);
    layout->addWidget(m_customDnsContainer);
    m_customDnsContainer->hide();
    
    layout->addSpacing(15);
    
    TQFrame* warnBox = new TQFrame(m_pageCustomDns);
    warnBox->setFrameShape(TQFrame::NoFrame);
    warnBox->setPaletteBackgroundColor(bg_color);
    TQHBoxLayout* warnLayout = new TQHBoxLayout(warnBox, 15, 10);
    TQLabel* warnIcon = new TQLabel("<font color='#FF5555' size='5'><b>!</b></font>", warnBox);
    warnIcon->setTextFormat(TQt::RichText);
    warnLayout->addWidget(warnIcon);
    TQLabel* warnTxt = new TQLabel("Using third-party DNS may limit website availability. For the best browsing\nexperience, use our default settings.", warnBox);
    warnTxt->setPaletteForegroundColor(text_color);
    warnLayout->addWidget(warnTxt);
    layout->addWidget(warnBox);
    
    layout->addStretch(1);
}

void MainWindow::setupSettingsAllowlist()
{
    m_pageAllowlist = new TQWidget(m_settingsWidget);
    TQVBoxLayout* layout = new TQVBoxLayout(m_pageAllowlist, 20, 15);
    
    TQHBoxLayout* header = new TQHBoxLayout(layout);
    header->setSpacing(5);
    
    TQLabel* link1 = new TQLabel("Settings", m_pageAllowlist); link1->setName("btn_back_settings");
    TQFont f = link1->font(); f.setPointSize(14); f.setBold(true); link1->setFont(f);
    link1->setPaletteForegroundColor(text_color); link1->setCursor(TQt::PointingHandCursor); link1->installEventFilter(this);
    header->addWidget(link1);
    
    TQLabel* sep1 = new TQLabel("/", m_pageAllowlist); sep1->setFont(f); sep1->setPaletteForegroundColor(text_color); header->addWidget(sep1);
    
    TQLabel* link2 = new TQLabel("Security and privacy", m_pageAllowlist); link2->setName("btn_back_sec_privacy");
    link2->setFont(f); link2->setPaletteForegroundColor(text_color); link2->setCursor(TQt::PointingHandCursor); link2->installEventFilter(this);
    header->addWidget(link2);
    
    TQLabel* sep2 = new TQLabel("/", m_pageAllowlist); sep2->setFont(f); sep2->setPaletteForegroundColor(text_color); header->addWidget(sep2);
    
    TQLabel* title = new TQLabel("Allowlist", m_pageAllowlist); title->setFont(f); title->setPaletteForegroundColor(text_color); header->addWidget(title);
    header->addStretch(1);
    
    layout->addSpacing(10);
    
    TQHBoxLayout* toggleRow = new TQHBoxLayout(layout);
    TQLabel* lblTop = new TQLabel("Use allowlist\nSpecify ports, port ranges, or subnets to exclude from VPN protection.\nAllowlisted ports may accept incoming connections from any external\nsource outside your network.", m_pageAllowlist);
    lblTop->setPaletteForegroundColor(text_color);
    toggleRow->addWidget(lblTop);
    toggleRow->addStretch(1);
    m_chkAllowlist = new TQCheckBox("", m_pageAllowlist); 
    m_chkAllowlist->setPaletteForegroundColor(text_color); 
    toggleRow->addWidget(m_chkAllowlist);
    connect(m_chkAllowlist, SIGNAL(toggled(bool)), this, SLOT(onAllowlistToggle(bool)));
    
    layout->addSpacing(15);
    
    m_allowlistBox = new TQFrame(m_pageAllowlist);
    m_allowlistBox->setFrameShape(TQFrame::NoFrame);
    m_allowlistBox->setPaletteBackgroundColor(bg_color);
    TQVBoxLayout* boxLayout = new TQVBoxLayout(m_allowlistBox, 15, 10);
    
    TQHBoxLayout* radioRow = new TQHBoxLayout(boxLayout);
    m_allowlistBgType = new TQButtonGroup(1, TQt::Horizontal, m_allowlistBox);
    m_allowlistBgType->setFrameStyle(TQFrame::NoFrame); m_allowlistBgType->setTitle("");
    TQRadioButton* rbPort = new TQRadioButton("Port", m_allowlistBgType); rbPort->setPaletteForegroundColor(text_color); rbPort->setChecked(true);
    TQRadioButton* rbRange = new TQRadioButton("Port range", m_allowlistBgType); rbRange->setPaletteForegroundColor(text_color);
    TQRadioButton* rbSubnet = new TQRadioButton("Subnet", m_allowlistBgType); rbSubnet->setPaletteForegroundColor(text_color);
    radioRow->addWidget(m_allowlistBgType);
    radioRow->addStretch(1);
    
    boxLayout->addSpacing(10);
    
    TQHBoxLayout* inputRow1 = new TQHBoxLayout(boxLayout);
    TQLabel* l1 = new TQLabel("Enter port:", m_allowlistBox); l1->setPaletteForegroundColor(text_color);
    inputRow1->addWidget(l1);
    TQLabel* l2 = new TQLabel("Select protocol:", m_allowlistBox); l2->setPaletteForegroundColor(text_color);
    inputRow1->addWidget(l2);
    
    TQHBoxLayout* inputRow2 = new TQHBoxLayout(boxLayout);
    m_allowlistEdtVal = new TQLineEdit("0", m_allowlistBox); 
    m_allowlistEdtVal->setPaletteForegroundColor(text_color);
    m_allowlistEdtVal->setPaletteBackgroundColor(bg_color3);
    inputRow2->addWidget(m_allowlistEdtVal);
    
    m_allowlistCboProto = new TQComboBox(m_allowlistBox); 
    m_allowlistCboProto->insertItem("All"); m_allowlistCboProto->insertItem("TCP"); m_allowlistCboProto->insertItem("UDP");
    m_allowlistCboProto->setPaletteForegroundColor(text_color); 
    m_allowlistCboProto->setPaletteBackgroundColor(bg_color3);
    inputRow2->addWidget(m_allowlistCboProto);
    
    TQPushButton* btnAdd = new TQPushButton("Add", m_allowlistBox);
    btnAdd->setPaletteForegroundColor(text_color);
    btnAdd->setPaletteBackgroundColor(bg_color);
    connect(btnAdd, SIGNAL(clicked()), this, SLOT(onAllowlistAddClicked()));
    inputRow2->addWidget(btnAdd);
    
    layout->addWidget(m_allowlistBox);
    m_allowlistBox->setEnabled(false);
    
    // The list container
    m_allowlistContainer = new TQWidget(m_pageAllowlist);
    TQVBoxLayout* listContainerLayout = new TQVBoxLayout(m_allowlistContainer, 0, 10);
    TQHBoxLayout* listHeaderLayout = new TQHBoxLayout(listContainerLayout);
    TQLabel* listLblPort = new TQLabel("Port", m_allowlistContainer); listLblPort->setPaletteForegroundColor(text_color); listHeaderLayout->addWidget(listLblPort);
    TQLabel* listLblProto = new TQLabel("Protocol", m_allowlistContainer); listLblProto->setPaletteForegroundColor(text_color); listHeaderLayout->addWidget(listLblProto);
    TQFrame* listLine = new TQFrame(m_allowlistContainer); listLine->setFrameShape(TQFrame::HLine); listLine->setPaletteForegroundColor(text_color);
    listContainerLayout->addWidget(listLine);
    m_allowlistEntriesLayout = new TQVBoxLayout(listContainerLayout, 5);
    layout->addWidget(m_allowlistContainer);
    m_allowlistContainer->hide();
    layout->addStretch(1);
}

TQWidget* MainWindow::createSettingsCategory(const TQString& id, const TQString& title, const TQString& subtitle, const unsigned char* iconData, unsigned int iconLen)
{
    TQFrame* frame = new TQFrame(m_settingsListWidget);
    frame->setName(id.ascii());
    frame->setFrameStyle(TQFrame::Box | TQFrame::Plain);
    frame->setLineWidth(0);
    frame->setPaletteBackgroundColor(bg_color);
    
    // Darken background slightly on hover native to trinity? Not strictly necessary, event filter handles clicks.
    frame->installEventFilter(this);
    
    TQHBoxLayout* hLayout = new TQHBoxLayout(frame, 10, 5);
    
    TQLabel* iconLabel = new TQLabel(frame);
    TQImage img;
    img.loadFromData(iconData, iconLen, "PNG");
    if (is_dark_mode) {
        manualInvert(img);
    }
    iconLabel->setPixmap(TQPixmap(img));
    hLayout->addWidget(iconLabel);
    
    TQVBoxLayout* vLayout = new TQVBoxLayout(hLayout);
    TQLabel* titleLbl = new TQLabel(TQString("<b>%1</b>").arg(title), frame);
    titleLbl->setPaletteForegroundColor(text_color);
    TQLabel* subLbl = new TQLabel(subtitle, frame);
    subLbl->setPaletteForegroundColor(text_color);
    TQFont subF = subLbl->font();
    subF.setPointSize(subF.pointSize() - 1);
    subLbl->setFont(subF);
    
    vLayout->addWidget(titleLbl);
    vLayout->addWidget(subLbl);
    hLayout->addStretch(1);
    
    return frame;
}

void MainWindow::setupSettingsView()
{
    m_settingsWidget = new TQWidgetStack(m_viewStack);
    
    m_settingsListWidget = new TQWidget(m_settingsWidget);
    TQVBoxLayout* mainLayout = new TQVBoxLayout(m_settingsListWidget, 20, 15);
    
    TQHBoxLayout* header = new TQHBoxLayout(mainLayout);
    header->setSpacing(5);
    
    TQLabel* title = new TQLabel("Settings", m_settingsListWidget);
    title->setPaletteForegroundColor(text_color);
    TQFont f = title->font();
    f.setPointSize(14);
    f.setBold(true);
    title->setFont(f);
    header->addWidget(title);
    header->addStretch(1);
    
    mainLayout->addSpacing(10);
    
    m_pageVpnConnection = new TQWidget(m_settingsWidget);
    m_pageSecurityPrivacy = new TQWidget(m_settingsWidget);
    m_pageThreatProtection = new TQWidget(m_settingsWidget);
    m_pageGeneral = new TQWidget(m_settingsWidget);
    m_pageTerms = new TQWidget(m_settingsWidget);
    m_pageAccount = new TQWidget(m_settingsWidget);
    m_pageAllowlist = new TQWidget(m_settingsWidget);
    m_pageCustomDns = new TQWidget(m_settingsWidget);
    
    setupSettingsVpnConnection();
    setupSettingsSecurity();
    setupSettingsThreatProtection();
    setupSettingsGeneral();
    setupSettingsTerms();
    setupSettingsAccount();
    setupSettingsAllowlist();
    setupSettingsCustomDns();
    
    mainLayout->addWidget(createSettingsCategory("btn_vpn", "VPN Connection", "Auto-connect, Kill Switch, protocol", vpn_connection_png, vpn_connection_png_len));
    mainLayout->addWidget(createSettingsCategory("btn_security", "Security and Privacy", "Allowlist, DNS, LAN discovery, obfuscation, firewall", security_png, security_png_len));
    mainLayout->addWidget(createSettingsCategory("btn_threat", "Threat Protection", "Blocks harmfull websites, ads, and trackers", threat_protection_png, threat_protection_png_len));
    mainLayout->addWidget(createSettingsCategory("btn_general", "General", "Appearance, notifications and analytics settings", general_png, general_png_len));
    mainLayout->addWidget(createSettingsCategory("btn_terms", "Terms", "Learn about NordVPN legal terms", terms_png, terms_png_len));
    mainLayout->addWidget(createSettingsCategory("btn_account", "Account", "Log out, subscription", account_png, account_png_len));
    
    mainLayout->addStretch(1);
    
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_settingsListWidget, 0);
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_pageVpnConnection, 1);
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_pageSecurityPrivacy, 2);
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_pageThreatProtection, 3);
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_pageGeneral, 4);
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_pageTerms, 5);
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_pageAccount, 6);
    
    setupSettingsAutoConnectServer();
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_pageAutoConnectServer, 7);
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_pageAllowlist, 8);
    ((TQWidgetStack*)m_settingsWidget)->addWidget(m_pageCustomDns, 9);
    
    m_viewStack->addWidget(m_settingsWidget, 2);
    
    connect(m_chkKillSwitch, SIGNAL(toggled(bool)), this, SLOT(onKillSwitchToggled(bool)));
    connect(m_chkThreatProtection, SIGNAL(toggled(bool)), this, SLOT(onThreatProtectionLiteToggled(bool)));
    connect(m_chkObfuscate, SIGNAL(toggled(bool)), this, SLOT(onObfuscateToggled(bool)));
}

void MainWindow::customEvent(TQCustomEvent* event)
{
    if (event->type() == LOAD_SETTINGS_EVENT) {
        SettingsLoadedEvent* sle = static_cast<SettingsLoadedEvent*>(event);
        applyAdvancedSettings(sle->settings);
        applyAccountInfo(sle->account);
    }
}

void MainWindow::applyAdvancedSettings(const DaemonClient::AdvancedSettings& settings)
{
    
    m_chkKillSwitch->blockSignals(true);
    m_chkThreatProtection->blockSignals(true);
    m_chkObfuscate->blockSignals(true);
    m_chkAutoConnect->blockSignals(true);
    m_chkLan->blockSignals(true);
    m_chkFirewall->blockSignals(true);
    m_chkPostQuantum->blockSignals(true);
    m_chkNotifications->blockSignals(true);
    m_rbNordLynx->blockSignals(true);
    m_rbNordWhisper->blockSignals(true);
    m_rbOpenVpnTcp->blockSignals(true);
    m_rbOpenVpnUdp->blockSignals(true);
    m_rbOpenVpnUdp->blockSignals(true);
    m_rbLight->blockSignals(true);
    m_rbDark->blockSignals(true);
    
    m_chkKillSwitch->setChecked(settings.killSwitch);
    m_chkThreatProtection->setChecked(settings.threatProtectionLite);
    m_chkObfuscate->setChecked(settings.obfuscate);
    m_chkAutoConnect->setChecked(settings.autoConnect);
    if (m_lblAutoConnectTo) {
        m_lblAutoConnectTo->setText(TQString("Auto-connect to:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%1 &gt;").arg(settings.autoConnectTarget));
        m_lblAutoConnectTo->setEnabled(settings.autoConnect);
    }
    
    m_chkLan->setChecked(settings.lanDiscovery);
    m_chkFirewall->setChecked(settings.firewall);
    m_chkPostQuantum->setChecked(settings.postQuantum);
    m_chkNotifications->setChecked(settings.notify);
    
    // Allowlist UI
    m_chkAllowlist->blockSignals(true);
    TQLayoutIterator itA = m_allowlistEntriesLayout->iterator();
    while (itA.current()) {
        if (TQWidget* w = itA.current()->widget()) { w->hide(); delete w; itA = m_allowlistEntriesLayout->iterator(); } else { ++itA; }
    }
    for (uint i = 0; i < settings.allowlistSubnets.count(); ++i) {
        addVisualAllowlistRow(settings.allowlistSubnets[i], "Subnet");
    }
    for (uint i = 0; i < settings.allowlistPorts.count(); ++i) {
        addVisualAllowlistRow(TQString::number(settings.allowlistPorts[i].port), settings.allowlistPorts[i].isTcp ? "TCP" : "UDP");
    }
    bool hasAllowlist = (!settings.allowlistSubnets.isEmpty() || !settings.allowlistPorts.isEmpty());
    m_chkAllowlist->setChecked(hasAllowlist);
    if (hasAllowlist) m_allowlistContainer->show(); else m_allowlistContainer->hide();
    m_chkAllowlist->blockSignals(false);
    
    // Custom DNS UI
    m_chkCustomDns->blockSignals(true);
    TQLayoutIterator itD = m_customDnsEntriesLayout->iterator();
    while (itD.current()) {
        if (TQWidget* w = itD.current()->widget()) { w->hide(); delete w; itD = m_customDnsEntriesLayout->iterator(); } else { ++itD; }
    }
    for (uint i = 0; i < settings.customDns.count(); ++i) {
        addVisualDnsRow(settings.customDns[i]);
    }
    bool hasDns = !settings.customDns.isEmpty();
    m_chkCustomDns->setChecked(hasDns);
    if (hasDns) m_customDnsContainer->show(); else m_customDnsContainer->hide();
    m_chkCustomDns->blockSignals(false);
    
    if (settings.technology == 2) {
        m_rbNordLynx->setChecked(true);
    } else if (settings.technology == 3) {
        m_rbNordWhisper->setChecked(true);
    } else if (settings.technology == 1) { // OPENVPN
        if (settings.protocol == 2) m_rbOpenVpnTcp->setChecked(true);
        else m_rbOpenVpnUdp->setChecked(true); // default UDP
    }

    // Theme is a local GUI setting, not in daemon.
    // For now it defaults to System (Light).

    m_chkKillSwitch->blockSignals(false);
    m_chkThreatProtection->blockSignals(false);
    m_chkObfuscate->blockSignals(false);
    m_chkAutoConnect->blockSignals(false);
    m_chkLan->blockSignals(false);
    m_chkFirewall->blockSignals(false);
    m_chkPostQuantum->blockSignals(false);
    m_chkNotifications->blockSignals(false);
    m_rbNordLynx->blockSignals(false);
    m_rbNordWhisper->blockSignals(false);
    m_rbOpenVpnTcp->blockSignals(false);
    m_rbOpenVpnUdp->blockSignals(false);
    m_rbOpenVpnUdp->blockSignals(false);
    m_rbLight->blockSignals(false);
    m_rbDark->blockSignals(false);
}

void MainWindow::applyAccountInfo(const DaemonClient::AccountInfo& info)
{
    if (m_lblSubscriptionStatus) {
        m_lblSubscriptionStatus->setText(TQString("Subscription\n<font color='gray'>%1</font>").arg(info.subscriptionStatus));
    }
    if (m_lblAccountInfo) {
        m_lblAccountInfo->setText(TQString("%1\n<font color='gray'>Account created: %2</font>").arg(info.email).arg(info.createdOn));
    }
}

void MainWindow::onKillSwitchToggled(bool on)
{
    m_client->setKillSwitch(on);
}

void MainWindow::onThreatProtectionLiteToggled(bool on)
{
    m_client->setThreatProtectionLite(on);
}

void MainWindow::onObfuscateToggled(bool checked)
{
    m_client->setObfuscate(checked);
}

void MainWindow::onAutoConnectToggled(bool checked)
{
    m_client->setAutoConnect(checked);
    if (m_lblAutoConnectTo) {
        m_lblAutoConnectTo->setEnabled(checked);
    }
}

void MainWindow::onLanDiscoveryToggled(bool checked)
{
    m_client->setLanDiscovery(checked);
}

void MainWindow::onFirewallToggled(bool checked)
{
    m_client->setFirewall(checked);
    if (m_lblFwm) m_lblFwm->setEnabled(checked);
    if (m_edtFwm) m_edtFwm->setEnabled(checked);
    if (m_btnFwmSave) m_btnFwmSave->setEnabled(checked);
}

void MainWindow::onPostQuantumToggled(bool checked)
{
    m_client->setPostQuantum(checked);
}

void MainWindow::onNotifyToggled(bool checked)
{
    m_client->setNotify(checked);
}

void MainWindow::onProtocolChanged(int id)
{
    // ID comes from ButtonGroup: 1=Lynx, 2=Whisper, 3=OpenVPN TCP, 4=OpenVPN UDP
    // technology enum: OPENVPN=1, NORDLYNX=2, NORDWHISPER=3
    // protocol enum: UDP=1, TCP=2
    if (m_rbNordLynx->isChecked()) m_client->setTechnologyAndProtocol(2, 0);
    else if (m_rbNordWhisper->isChecked()) m_client->setTechnologyAndProtocol(3, 0);
    else if (m_rbOpenVpnTcp->isChecked()) m_client->setTechnologyAndProtocol(1, 2);
    else if (m_rbOpenVpnUdp->isChecked()) m_client->setTechnologyAndProtocol(1, 1);
}

void MainWindow::onResetAppClicked()
{
    int ret = TQMessageBox::information(this, "Reset all custom settings to default?", 
        "This will remove your personalized configurations across the app and restore default settings.",
        "Cancel", "Reset settings", TQString::null, 0, 0);
        
    if (ret == 1) {
        if (m_client) m_client->setDefaults(true); // Don't logout
        m_rbLight->setChecked(true); // default to light
        onThemeChanged(0);
    }
}

void MainWindow::onThemeChanged(int id)
{
    // id 0 = Light, 1 = Dark
    setThemeFileExists(id == 1);
    
    int ret = TQMessageBox::information(this, "Theme", 
        "Theme preference saved.\nDo you want to restart the application now to apply the new theme?",
        "OK", "Restart now", TQString::null, 0, 0);
        
    if (ret == 1) { // User clicked 'Restart now'
        char exePath[4096];
        ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath));
        if (count != -1) {
            exePath[count] = '\0';
            std::string cmd = std::string(exePath) + " &";
            system(cmd.c_str());
        }
        tqApp->quit();
    }
}

void MainWindow::onLogoutClicked()
{
    m_client->logout();
}

void MainWindow::onTermsLinkClicked(const TQString& link)
{
    // Clickable term links launching browser
    system(TQString("xdg-open \"%1\" &").arg(link).latin1());
}

void MainWindow::quitApplication()
{
    DaemonClient::VpnStatus status = m_client->getVpnStatus();
    if (status.isConnected) {
        TQString serverName = status.serverName.isEmpty() ? "the VPN" : status.serverName;
        TQString message = TQString("You are currently connected to <b>%1</b>.<br><br>Closing the GUI application will not disconnect you from the VPN daemon.<br><br>Are you sure you want to quit?").arg(serverName);
        int ret = TQMessageBox::warning(this, "Quit Application", message, "Quit", "Cancel", TQString::null, 1, 1);
        if (ret == 1) return; // User clicked Cancel
    }
    tqApp->quit();
}

void MainWindow::onAboutClicked()
{
    TQDialog* dlg = new TQDialog(this, "about_dialog", true);
    dlg->setCaption("About tdeNordgui");
    dlg->resize(460, 320);
    dlg->setPaletteBackgroundColor(TQColor(0, 0, 0));
    
    TQVBoxLayout* outerLayout = new TQVBoxLayout(dlg, 30, 20); // 30px padding
    TQHBoxLayout* topLayout = new TQHBoxLayout(outerLayout, 15); // spacing 15
    
    // Left: Dragon
    TQLabel* imgLabel = new TQLabel(dlg);
    TQPixmap pmDragon; pmDragon.loadFromData(about_tdenordgui_png, about_tdenordgui_png_len);
    imgLabel->setPixmap(pmDragon);
    imgLabel->setAlignment(TQt::AlignLeft | TQt::AlignVCenter);
    topLayout->addWidget(imgLabel);
    
    // Right: text
    TQVBoxLayout* rightLayout = new TQVBoxLayout(topLayout, 10);
    rightLayout->addStretch(1);
    
    TQLabel* iconLabel = new TQLabel(dlg);
    TQPixmap pmIcon; pmIcon.loadFromData(icon_nordvpn_png, icon_nordvpn_png_len);
    pmIcon = getScaledIcon(icon_nordvpn_png, icon_nordvpn_png_len, 64);
    iconLabel->setPixmap(pmIcon);
    iconLabel->setAlignment(TQt::AlignHCenter);
    rightLayout->addWidget(iconLabel);
    
    TQLabel* lblTitle = new TQLabel("tdeNordgui", dlg);
    TQFont fTitle = lblTitle->font(); fTitle.setPointSize(18); fTitle.setBold(true);
    lblTitle->setFont(fTitle);
    lblTitle->setPaletteForegroundColor(TQColor(255, 255, 255));
    lblTitle->setAlignment(TQt::AlignHCenter);
    rightLayout->addWidget(lblTitle);
    
    TQLabel* lblDesc = new TQLabel("A Trinity DE graphical interface\nfor NordVPN", dlg);
    TQFont fDesc = lblDesc->font(); fDesc.setPointSize(10); fDesc.setBold(false);
    lblDesc->setFont(fDesc);
    lblDesc->setPaletteForegroundColor(TQColor(255, 255, 255));
    lblDesc->setAlignment(TQt::AlignHCenter);
    rightLayout->addWidget(lblDesc);
    
    rightLayout->addSpacing(10);
    
    TQLabel* lblAuthor = new TQLabel("by seb3773 - https://github.com/seb3773", dlg);
    TQFont fAuth = lblAuthor->font(); fAuth.setPointSize(8);
    lblAuthor->setFont(fAuth);
    lblAuthor->setPaletteForegroundColor(TQColor(238, 238, 238));
    lblAuthor->setAlignment(TQt::AlignHCenter);
    rightLayout->addWidget(lblAuthor);
    
    rightLayout->addStretch(1);
    
    // Bottom: close button
    TQHBoxLayout* bottomLayout = new TQHBoxLayout(outerLayout);
    bottomLayout->addStretch(1);
    TQPushButton* btnClose = new TQPushButton("Close", dlg);
    btnClose->setFixedWidth(100);
    connect(btnClose, SIGNAL(clicked()), dlg, SLOT(accept()));
    bottomLayout->addWidget(btnClose);
    bottomLayout->addStretch(1);
    
    dlg->exec();
    delete dlg;
}


void MainWindow::handleLoginClicked()
{
    m_client->login("", ""); // Will trigger the browser redirect!
}

void MainWindow::handleCallbackSubmit()
{
    TQString url = m_callbackInput->text().stripWhiteSpace();
    if (!url.isEmpty()) {
        // Parse exchange_token from the URL
        if (url.startsWith("nordvpn://")) {
            int tokenStart = url.find("exchange_token=");
            if (tokenStart != -1) {
                tokenStart += 15; // length of "exchange_token="
                int tokenEnd = url.find("&", tokenStart);
                TQString token;
                if (tokenEnd != -1) {
                    token = url.mid(tokenStart, tokenEnd - tokenStart);
                } else {
                    token = url.mid(tokenStart);
                }
                
                // Decode URL encoding (like %3D to =)
                token.replace("%3D", "=");
                token.replace("%3d", "=");
                
                m_client->submitLoginCallback(token);
                return;
            }
        }
        
        // Fallback: send whatever they pasted as the token
        m_client->submitLoginCallback(url);
    }
}

void MainWindow::onConnectionStateChanged(const TQString& state)
{
    std::cerr << "GUI Thread: New Connection Status: " << state.ascii() << "\n";
    TQString vpnState = m_client->getVpnStateString();
    
    if (state.startsWith("Connected") || state == "LoggedIn") {
        m_sidebarWidget->show();
        m_viewStack->raiseWidget(m_dashboardWidget);
        populateServerLists();
        m_statusLabel->setText(vpnState);
    } else if (state == "LoggedOut") {
        m_sidebarWidget->hide();
        m_viewStack->raiseWidget(m_loginWidget);
        m_statusLabel->setText("Not connected to a VPN server");
    } else {
        // Just connecting/disconnected while logged in
        if (m_sidebarWidget->isVisible()) {
            m_statusLabel->setText(vpnState);
        }
    }
    
    // Toggle button text based on real connection state
    bool isConnected = (vpnState.contains("Connected to:") || vpnState == "Connected");
    if (isConnected) {
        m_quickConnectBtn->setText("Disconnect");
    } else {
        m_quickConnectBtn->setText("Quick Connect");
    }
    
    if (m_tray) {
        TQImage img;
        if (isConnected) {
            img.loadFromData(tray_on_png, tray_on_png_len);
            if (m_trayMenu && m_trayMenu->indexOf(m_trayConnectId) != -1) {
                m_trayMenu->changeItem(m_trayConnectId, TQString("Disconnect"));
            }
        } else {
            img.loadFromData(tray_off_png, tray_off_png_len);
            if (m_trayMenu && m_trayMenu->indexOf(m_trayConnectId) != -1) {
                m_trayMenu->changeItem(m_trayConnectId, TQString("Connect"));
            }
        }
        m_tray->setPixmap(TQPixmap(img));
        
        TQToolTip::add(m_tray, vpnState);
    }
    
    // Check if we need to send a notification (state transition)
    if (m_isFirstStateReceived) {
        m_isFirstStateReceived = false;
        m_lastNotificationState = state;
    } else if (state != m_lastNotificationState) {
        if (state == "Connected") {
            TQString serverName = vpnState;
            serverName.replace("Connected to: ", "");
            showNotification(TQString("NordVPN"), TQString("Connected to ") + serverName);
        } else if (state == "Disconnected") {
            showNotification(TQString("NordVPN"), TQString("Disconnected from VPN"));
        }
        m_lastNotificationState = state;
    }
}

void MainWindow::onConnectionError(const TQString& error)
{
    int result = TQMessageBox::critical(this, 
        "NordVPN Connection Error",
        "Failed to connect to the NordVPN daemon.\\n" + error + "\\n\\nPlease make sure the nordvpnd service is running.",
        "Retry", "Quit", TQString(), 0, 1);
        
    if (result == 0) {
        if (m_client) {
            m_client->connectToDaemon();
            m_client->requestInitialState();
        }
    } else {
        quitApplication();
    }
}

void MainWindow::onServerSelected(TQListBoxItem* item)
{
    if (item) {
        TQString payload = m_countryPayloadMap.contains(item->text()) ? m_countryPayloadMap[item->text()] : item->text();
        if (item->listBox() == m_countriesList || item->listBox() == m_acCountriesList) {
            m_client->connectToCountry(payload);
        } else {
            m_client->connectToGroup(payload);
        }
    }
}

void MainWindow::onQuickConnectClicked()
{
    printf("TRACER: MainWindow::onQuickConnectClicked called\\n");
    TQString vpnState = m_client->getVpnStateString();
    bool isConnected = (vpnState.contains("Connected to:") || vpnState == "Connected" || vpnState.startsWith("Connected"));
    
    if (isConnected) {
        if (m_client) m_client->disconnectVpn();
    } else {
        // Read selected item dynamically
        if (m_listStack->id(m_listStack->visibleWidget()) == 0) { // Countries
            TQListBoxItem* sel = m_countriesList->selectedItem();
            if (sel) {
                TQString payload = m_countryPayloadMap.contains(sel->text()) ? m_countryPayloadMap[sel->text()] : sel->text();
                m_client->connectToCountry(payload);
                return;
            }
        } else { // Specialty
            TQListBoxItem* sel = m_specialtyList->selectedItem();
            if (sel) {
                m_client->connectToGroup(sel->text());
                return;
            }
        }
        
        m_client->connectVpn(); // Default if nothing selected
    }
}

void MainWindow::showAcCountriesTab()
{
    TQFont boldFont = m_acTabCountriesBtn->font(); boldFont.setBold(true);
    m_acTabCountriesBtn->setFont(boldFont);
    TQFont normalFont = m_acTabSpecialtyBtn->font(); normalFont.setBold(false);
    m_acTabSpecialtyBtn->setFont(normalFont);
    m_acListStack->raiseWidget(0);
}

void MainWindow::showAcSpecialtyTab()
{
    TQFont normalFont = m_acTabCountriesBtn->font(); normalFont.setBold(false);
    m_acTabCountriesBtn->setFont(normalFont);
    TQFont boldFont = m_acTabSpecialtyBtn->font(); boldFont.setBold(true);
    m_acTabSpecialtyBtn->setFont(boldFont);
    m_acListStack->raiseWidget(1);
}

void MainWindow::onAcSearchClicked()
{
    if (m_acSearchEdit->isHidden()) {
        m_acSearchEdit->show(); m_acSearchEdit->setFocus(); 
        TQPixmap pmClose = getRawIconInverted(close_search_png, close_search_png_len, is_dark_mode);
        m_acSearchBtn->setPixmap(pmClose);
    } else {
        m_acSearchEdit->hide(); m_acSearchEdit->clear(); 
        TQPixmap pmSearch = getRawIconInverted(search_png, search_png_len, is_dark_mode);
        m_acSearchBtn->setPixmap(pmSearch);
    }
}

void MainWindow::onAcSearchTextChanged(const TQString& text)
{
    updateListWithFilter(m_acCountriesList, m_cachedCountries, text, true);
    updateListWithFilter(m_acSpecialtyList, m_cachedGroups, text);
}

void MainWindow::onAcServerSelected(TQListBoxItem* item)
{
    if (!item) return;
    TQString payload = m_countryPayloadMap.contains(item->text()) ? m_countryPayloadMap[item->text()] : item->text();
    bool isGroup = (m_acListStack->id(m_acListStack->visibleWidget()) == 1);
    
    if (isGroup) {
        m_client->setAutoConnectTarget("", payload);
    } else {
        m_client->setAutoConnectTarget(payload, "");
    }
    
    // Update local label instantly for responsiveness
    TQString displayText = item->text() + " (Quick connect)";
    if (m_lblAutoConnectTo) {
        m_lblAutoConnectTo->setText(TQString("Auto-connect to:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%1 &gt;").arg(displayText));
    }
    
    // return to previous page
    ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageVpnConnection);
}

void MainWindow::setupSettingsAutoConnectServer()
{
    m_pageAutoConnectServer = new TQWidget(m_settingsWidget);
    TQVBoxLayout* layout = new TQVBoxLayout(m_pageAutoConnectServer, 20, 15);
    
    // Header
    TQHBoxLayout* header = new TQHBoxLayout(layout);
    header->setSpacing(5);
    TQLabel* backBtn = new TQLabel("Settings / VPN Connection", m_pageAutoConnectServer);
    backBtn->setName("btn_back_ac");
    TQFont f = backBtn->font(); f.setPointSize(14); f.setBold(true);
    backBtn->setFont(f);
    backBtn->setPaletteForegroundColor(text_color);
    backBtn->setCursor(TQCursor(TQt::PointingHandCursor));
    backBtn->installEventFilter(this);
    
    TQLabel* sep = new TQLabel("/", m_pageAutoConnectServer);
    sep->setFont(f);
    sep->setPaletteForegroundColor(text_color);
    
    TQLabel* title = new TQLabel("Auto-connect to", m_pageAutoConnectServer); 
    title->setFont(f);
    title->setPaletteForegroundColor(text_color);
    
    header->addWidget(backBtn);
    header->addWidget(sep);
    header->addWidget(title);
    header->addStretch(1);
    
    // Bottom List Card
    TQWidget* listCard = new TQWidget(m_pageAutoConnectServer);
    listCard->setPaletteBackgroundColor(bg_color);
    TQVBoxLayout* listLayout = new TQVBoxLayout(listCard, 10, 10);
    
    TQHBoxLayout* tabLayout = new TQHBoxLayout(listLayout);
    m_acTabCountriesBtn = new TQPushButton("Countries", listCard);
    m_acTabCountriesBtn->setFlat(true);
    m_acTabCountriesBtn->setPaletteBackgroundColor(bg_color3);
    TQFont boldFont = m_acTabCountriesBtn->font(); boldFont.setBold(true);
    m_acTabCountriesBtn->setFont(boldFont);
    TQPalette tcPal = m_acTabCountriesBtn->palette();
    tcPal.setColor(TQColorGroup::ButtonText, text_color);
    m_acTabCountriesBtn->setPalette(tcPal);
    
    m_acTabSpecialtyBtn = new TQPushButton("Specialty servers", listCard);
    m_acTabSpecialtyBtn->setFlat(true);
    m_acTabSpecialtyBtn->setPaletteBackgroundColor(bg_color3);
    TQPalette tsPal = m_acTabSpecialtyBtn->palette();
    tsPal.setColor(TQColorGroup::ButtonText, text_color);
    m_acTabSpecialtyBtn->setPalette(tsPal);
    
    m_acSearchBtn = new TQPushButton(listCard);
    m_acSearchBtn->setPaletteBackgroundColor(bg_color);
    TQPixmap pmAcSearch = getRawIconInverted(search_png, search_png_len, is_dark_mode);
    m_acSearchBtn->setPixmap(pmAcSearch);
    m_acSearchBtn->setFlat(true); 
    m_acSearchBtn->setFixedSize(32, 32);
    
    tabLayout->addWidget(m_acTabCountriesBtn);
    tabLayout->addWidget(m_acTabSpecialtyBtn);
    tabLayout->addStretch(1);
    
    m_acSearchEdit = new TQLineEdit(listCard);
    m_acSearchEdit->hide();
    tabLayout->addWidget(m_acSearchEdit);
    tabLayout->addWidget(m_acSearchBtn);
    
    m_acListStack = new TQWidgetStack(listCard);
    
    TQWidget* cWidget = new TQWidget(m_acListStack);
    TQVBoxLayout* cLayout = new TQVBoxLayout(cWidget);
    m_acCountriesList = new TQListBox(cWidget);
    m_acCountriesList->setFrameStyle(TQFrame::NoFrame);
    m_acCountriesList->setPaletteBackgroundColor(bg_color);
    m_acCountriesList->setPaletteForegroundColor(text_color);
    cLayout->addWidget(m_acCountriesList);
    
    TQWidget* sWidget = new TQWidget(m_acListStack);
    TQVBoxLayout* sLayout = new TQVBoxLayout(sWidget);
    m_acSpecialtyList = new TQListBox(sWidget);
    m_acSpecialtyList->setFrameStyle(TQFrame::NoFrame);
    m_acSpecialtyList->setPaletteBackgroundColor(bg_color);
    m_acSpecialtyList->setPaletteForegroundColor(text_color);
    sLayout->addWidget(m_acSpecialtyList);
    
    m_acListStack->addWidget(cWidget, 0);
    m_acListStack->addWidget(sWidget, 1);
    listLayout->addWidget(m_acListStack, 1);
    layout->addWidget(listCard, 1);
    
    connect(m_acTabCountriesBtn, SIGNAL(clicked()), this, SLOT(showAcCountriesTab()));
    connect(m_acTabSpecialtyBtn, SIGNAL(clicked()), this, SLOT(showAcSpecialtyTab()));
    connect(m_acSearchBtn, SIGNAL(clicked()), this, SLOT(onAcSearchClicked()));
    connect(m_acSearchEdit, SIGNAL(textChanged(const TQString&)), this, SLOT(onAcSearchTextChanged(const TQString&)));
    
    connect(m_acCountriesList, SIGNAL(doubleClicked(TQListBoxItem*)), this, SLOT(onAcServerSelected(TQListBoxItem*)));
    connect(m_acSpecialtyList, SIGNAL(doubleClicked(TQListBoxItem*)), this, SLOT(onAcServerSelected(TQListBoxItem*)));
    
    TQHBoxLayout* bottomLayout = new TQHBoxLayout(layout);
    bottomLayout->addStretch(1);
    TQPushButton* applyBtn = new TQPushButton("Apply", m_pageAutoConnectServer);
    applyBtn->setMinimumSize(100, 35);
    TQPalette palApply = applyBtn->palette();
    palApply.setColor(TQColorGroup::Button, TQColor(60, 90, 250));
    palApply.setColor(TQColorGroup::ButtonText, TQt::white);
    applyBtn->setPalette(palApply);
    bottomLayout->addWidget(applyBtn);
    
    connect(applyBtn, SIGNAL(clicked()), this, SLOT(onAcApplyClicked()));
}

void MainWindow::onAcApplyClicked()
{
    TQListBox* activeList = (m_acListStack->id(m_acListStack->visibleWidget()) == 0) ? m_acCountriesList : m_acSpecialtyList;
    TQListBoxItem* sel = activeList->selectedItem();
    if (sel) {
        onAcServerSelected(sel);
    } else {
        ((TQWidgetStack*)m_settingsWidget)->raiseWidget(m_pageVpnConnection);
    }
}

bool MainWindow::showConfirmOffDialog(const TQString& title, const TQString& message)
{
    // Return 1 for "Turn off" (the second button index = 1, Cancel = 0)
    int result = TQMessageBox::information(this, title, message, "Cancel", "Turn off", 0, 1, 1);
    return (result == 1);
}

void MainWindow::onAllowlistToggle(bool checked)
{
    int numWidgets = 0;
    TQLayoutIterator itA = m_allowlistEntriesLayout->iterator();
    while (itA.current()) {
        if (itA.current()->widget()) numWidgets++;
        ++itA;
    }
    
    if (!checked && numWidgets > 0) {
        bool confirm = showConfirmOffDialog("Turn off allowlist?", "Disabling the allowlist will delete all your previously added ports, port ranges, and subnets.");
        if (!confirm) {
            m_chkAllowlist->blockSignals(true);
            m_chkAllowlist->setChecked(true); // revert
            m_chkAllowlist->blockSignals(false);
            return;
        }
        
        TQLayoutIterator itClear = m_allowlistEntriesLayout->iterator();
        while (itClear.current()) {
            if (TQWidget* w = itClear.current()->widget()) {
                w->hide();
                delete w;
                itClear = m_allowlistEntriesLayout->iterator(); // safely restart iteration
            } else {
                ++itClear;
            }
        }
        numWidgets = 0;
        if (m_client) m_client->clearAllowlist();
    }
    
    m_allowlistBox->setEnabled(checked);
    
    if (checked && numWidgets > 0) {
        m_allowlistContainer->show();
    } else {
        m_allowlistContainer->hide();
    }
}

void MainWindow::onCustomDnsToggle(bool checked)
{
    int numWidgets = 0;
    TQLayoutIterator itD = m_customDnsEntriesLayout->iterator();
    while (itD.current()) {
        if (itD.current()->widget()) numWidgets++;
        ++itD;
    }
    
    if (!checked && numWidgets > 0) {
        bool confirm = showConfirmOffDialog("Turn off custom DNS?", "This will remove all your previously added DNS servers.");
        if (!confirm) {
            m_chkCustomDns->blockSignals(true);
            m_chkCustomDns->setChecked(true);
            m_chkCustomDns->blockSignals(false);
            return;
        }
        
        TQLayoutIterator itClear = m_customDnsEntriesLayout->iterator();
        while (itClear.current()) {
            if (TQWidget* w = itClear.current()->widget()) {
                w->hide();
                delete w;
                itClear = m_customDnsEntriesLayout->iterator();
            } else {
                ++itClear;
            }
        }
        numWidgets = 0;
        if (m_client) m_client->setCustomDns(TQValueList<TQString>());
    }
    
    m_customDnsBox->setEnabled(checked);
    
    if (checked && numWidgets > 0) {
        m_customDnsContainer->show();
    } else {
        m_customDnsContainer->hide();
    }
}

void MainWindow::addVisualAllowlistRow(const TQString& port, const TQString& proto)
{
    TQWidget* row = new TQWidget(m_allowlistContainer);
    row->setPaletteBackgroundColor(bg_color3);
    TQHBoxLayout* rowL = new TQHBoxLayout(row, 10, 5);
    
    TQLabel* lblPort = new TQLabel(port, row);
    lblPort->setPaletteForegroundColor(text_color);
    rowL->addWidget(lblPort, 1);
    
    TQLabel* lblProto = new TQLabel(proto, row);
    lblProto->setPaletteForegroundColor(text_color);
    rowL->addWidget(lblProto, 1);
    
    TQPushButton* btnDel = new TQPushButton(row);
    btnDel->setPixmap(getRawIconInverted(icons_delete_png, icons_delete_png_len, is_dark_mode));
    btnDel->setFlat(true);
    btnDel->setPaletteBackgroundColor(bg_color3);
    btnDel->setFixedSize(30, 30);
    btnDel->setCursor(TQt::PointingHandCursor);
    connect(btnDel, SIGNAL(clicked()), this, SLOT(onAllowlistDeleteClicked()));
    rowL->addWidget(btnDel, 0);
    
    m_allowlistEntriesLayout->addWidget(row);
    row->show();
}

void MainWindow::onAllowlistAddClicked()
{
    TQString port = m_allowlistEdtVal->text();
    if (port.isEmpty() || port == "0") return;
    
    TQString proto = m_allowlistCboProto->currentText();
    
    if (m_client) {
        if (port.contains('.') || port.contains('/')) {
            m_client->addAllowlistSubnet(port);
        } else {
            bool tcp = (proto == "TCP" || proto == "All");
            bool udp = (proto == "UDP" || proto == "All");
            m_client->addAllowlistPort(port.toLongLong(), tcp, udp);
        }
    }
    
    addVisualAllowlistRow(port, proto);
    
    m_allowlistContainer->show();
    m_allowlistEdtVal->clear();
    
    if (!m_chkAllowlist->isChecked()) {
        m_chkAllowlist->blockSignals(true);
        m_chkAllowlist->setChecked(true);
        m_chkAllowlist->blockSignals(false);
    }
}

void MainWindow::sendDnsUpdateToDaemon()
{
    TQValueList<TQString> list;
    TQLayoutIterator it = m_customDnsEntriesLayout->iterator();
    while (it.current()) {
        if (TQWidget* row = it.current()->widget()) {
            TQObjectListIt childIt(*row->children());
            while (childIt.current()) {
                if (TQLabel* lbl = dynamic_cast<TQLabel*>(childIt.current())) {
                    list.append(lbl->text());
                    break;
                }
                ++childIt;
            }
        }
        ++it;
    }
    if (m_client) m_client->setCustomDns(list);
}

void MainWindow::addVisualDnsRow(const TQString& ip)
{
    TQWidget* row = new TQWidget(m_customDnsContainer);
    row->setPaletteBackgroundColor(bg_color3);
    TQHBoxLayout* rowL = new TQHBoxLayout(row, 10, 5);
    
    TQLabel* lblIp = new TQLabel(ip, row);
    lblIp->setPaletteForegroundColor(text_color);
    rowL->addWidget(lblIp, 1);
    
    TQPushButton* btnDel = new TQPushButton(row);
    btnDel->setPixmap(getRawIconInverted(icons_delete_png, icons_delete_png_len, is_dark_mode));
    btnDel->setFlat(true);
    btnDel->setPaletteBackgroundColor(bg_color3);
    btnDel->setFixedSize(30, 30);
    btnDel->setCursor(TQt::PointingHandCursor);
    connect(btnDel, SIGNAL(clicked()), this, SLOT(onCustomDnsDeleteClicked()));
    rowL->addWidget(btnDel, 0);
    
    m_customDnsEntriesLayout->addWidget(row);
    row->show();
}

void MainWindow::onCustomDnsAddClicked()
{
    TQString ip = m_customDnsEdtBase->text();
    if (ip.isEmpty() || ip == "0.0.0.0") return;
    
    addVisualDnsRow(ip);
    
    m_customDnsContainer->show();
    m_customDnsEdtBase->setText("");
    
    if (!m_chkCustomDns->isChecked()) {
        m_chkCustomDns->blockSignals(true);
        m_chkCustomDns->setChecked(true);
        m_chkCustomDns->blockSignals(false);
    }
    
    sendDnsUpdateToDaemon();
}

void MainWindow::onAllowlistDeleteClicked()
{
    if (TQWidget* btn = (TQWidget*)sender()) {
        if (TQWidget* row = btn->parentWidget()) {
            TQString port, proto;
            TQObjectListIt childIt(*row->children());
            while (childIt.current()) {
                if (TQLabel* lbl = dynamic_cast<TQLabel*>(childIt.current())) {
                    if (port.isEmpty()) port = lbl->text();
                    else proto = lbl->text();
                }
                ++childIt;
            }
            if (port.contains('.') || port.contains('/')) {
                if (m_client) m_client->removeAllowlistSubnet(port);
            } else {
                bool tcp = (proto == "TCP" || proto == "All");
                bool udp = (proto == "UDP" || proto == "All");
                if (m_client) m_client->removeAllowlistPort(port.toLongLong(), tcp, udp);
            }
            
            row->hide();
            delete row;
            
            int numWidgets = 0;
            TQLayoutIterator it = m_allowlistEntriesLayout->iterator();
            while (it.current()) {
                if (it.current()->widget()) numWidgets++;
                ++it;
            }
            if (numWidgets == 0) m_allowlistContainer->hide();
        }
    }
}

void MainWindow::onCustomDnsDeleteClicked()
{
    if (TQWidget* btn = (TQWidget*)sender()) {
        if (TQWidget* row = btn->parentWidget()) {
            row->hide();
            delete row;
            
            int numWidgets = 0;
            TQLayoutIterator it = m_customDnsEntriesLayout->iterator();
            while (it.current()) {
                if (it.current()->widget()) numWidgets++;
                ++it;
            }
            if (numWidgets == 0) m_customDnsContainer->hide();
            
            sendDnsUpdateToDaemon();
        }
    }
}
