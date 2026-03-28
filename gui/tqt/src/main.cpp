#include <libnotify/notify.h>
#include "daemon_client.h"
#include "main_window.h"
#include <kuniqueapplication.h>
#include <tdeaboutdata.h>
#include <tdecmdlineargs.h>

int main(int argc, char** argv) {
    TDEAboutData about("tdenordgui", "tdenordgui", "0.1");
    TDECmdLineArgs::init(argc, argv, &about);
    
    if (!KUniqueApplication::start()) {
        // App is already running, abort secondary instance
        return 0;
    }
    
    KUniqueApplication app;

    DaemonClient* daemonClient = new DaemonClient(nullptr);
    daemonClient->connectToDaemon();

    MainWindow* window = new MainWindow(daemonClient);
    
    // Now that MainWindow is listening to signals, request initial state
    daemonClient->requestInitialState();
    
    app.setMainWidget(window);
    window->show();

    int ret = app.exec();
    
    // Release library handles safely
    notify_uninit();
    
    // Let kernel drop GUI heap cleanly to avoid TDE WDestructiveClose races
    // But we MUST tear down backend threads properly before glibc exits
    delete daemonClient;
    return ret;
}
