#ifndef DAEMON_CLIENT_H
#define DAEMON_CLIENT_H

#include <tqobject.h>
#include <tqstring.h>
#include <tqvaluelist.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/grpcpp.h>

#include "service.grpc.pb.h"

class DaemonClient : public TQObject {
    TQ_OBJECT
public:
    explicit DaemonClient(TQObject* parent = nullptr);
    virtual ~DaemonClient();

    // Initialize channel and start listening
    bool connectToDaemon(const TQString& socketPath = "/run/nordvpn/nordvpnd.sock");

protected:
    void customEvent(TQCustomEvent* e) override;

public:
    struct ServerGroup {
        TQString name;
        bool isVirtual;
        TQString code;
        TQString connectPayload;
    };
    
    TQValueList<ServerGroup> getCountries();
    TQValueList<ServerGroup> getGroups();

public slots:
    // UI driven actions
    void login(const TQString& username, const TQString& password);
    void submitLoginCallback(const TQString& url);
    void connectVpn();
    void connectToCountry(const TQString& countryName);
    void connectToGroup(const TQString& groupName);
    void disconnectVpn();
    void requestInitialState();
    void requestState();

    // Settings
    struct VpnStatus {
        bool isConnected;
        TQString serverName;
        TQString ip;
        TQString countryCode;
    };
    VpnStatus getVpnStatus();
    
    struct AdvancedSettings {
        bool killSwitch;
        bool threatProtectionLite;
        bool obfuscate;
        bool autoConnect;
        TQString autoConnectTarget;
        int technology; // config::Technology enum
        int protocol;   // config::Protocol enum
        bool lanDiscovery;
        bool firewall;
        uint32_t firewallMark;
        bool postQuantum;
        bool notify;
        
        TQValueList<TQString> customDns;
        
        struct AllowlistPort { int64_t port; bool isTcp; };
        TQValueList<AllowlistPort> allowlistPorts;
        TQValueList<TQString> allowlistSubnets;
    };
    AdvancedSettings getAdvancedSettings();
    void setKillSwitch(bool enabled);
    void setThreatProtectionLite(bool enabled);
    void setObfuscate(bool enabled);
    
    void setAutoConnect(bool enabled);
    void setAutoConnectTarget(const TQString& countryOrCity, const TQString& group);
    void setTechnologyAndProtocol(int technology, int protocol);
    void setLanDiscovery(bool enabled);
    void setFirewall(bool enabled);
    void setPostQuantum(bool enabled);
    void setNotify(bool enabled);
    void setDefaults(bool noLogout = true);
    
    void setCustomDns(const TQValueList<TQString>& dnsList);
    void addAllowlistPort(int64_t portStr, bool tcp, bool udp);
    void addAllowlistSubnet(const TQString& subnet);
    void removeAllowlistPort(int64_t portStr, bool tcp, bool udp);
    void removeAllowlistSubnet(const TQString& subnet);
    void clearAllowlist();
    
    void logout();

    TQString getVpnStateString();
    
    struct AccountInfo {
        TQString email;
        TQString createdOn;
        TQString subscriptionStatus;
    };
    AccountInfo getAccountInfo();

signals:
    // Daemon state updates to UI
    void stateChanged(const TQString& state);
    void connectionError(const TQString& error);

private:
    std::shared_ptr<grpc::Channel> m_channel;
    std::unique_ptr<pb::Daemon::Stub> m_stub;
    class DaemonStatePoller* m_poller;
};

#endif // DAEMON_CLIENT_H
