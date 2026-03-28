#include "daemon_client.h"
#include <iostream>
#include <tqapplication.h>
#include <tqthread.h>
#include <tqevent.h>
#include <stdlib.h>
#include <thread>

#include "service.grpc.pb.h"
#include <vector>
#include <algorithm>
#include "state.pb.h"
#include "common.pb.h"
#include "set.pb.h"
#include "settings.pb.h"
#include "defaults.pb.h"
#include "account.pb.h"
#include "config/technology.pb.h"
#include "config/protocol.pb.h"

// Custom event ID for state updates
#define EVENT_APP_STATE (TQEvent::User + 1)

class DaemonStateEvent : public TQCustomEvent {
public:
    DaemonStateEvent(const pb::AppState& state) 
        : TQCustomEvent(EVENT_APP_STATE), m_state(state) {}
    
    pb::AppState m_state;
};

class DaemonStatePoller : public TQThread {
public:
    DaemonStatePoller(std::shared_ptr<grpc::Channel> channel, TQObject* receiver)
        : m_channel(channel), m_receiver(receiver), m_running(true) {
        m_stub = pb::Daemon::NewStub(m_channel);
    }
    
    void stop() {
        m_running = false;
        m_context.TryCancel();
    }
    
protected:
    void run() override {
        pb::Empty request;
        std::unique_ptr<grpc::ClientReader<pb::AppState>> reader(
            m_stub->SubscribeToStateChanges(&m_context, request));
            
        pb::AppState state;
        while (m_running && reader->Read(&state)) {
            DaemonStateEvent* event = new DaemonStateEvent(state);
            TQApplication::postEvent(m_receiver, event);
        }
        
        grpc::Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "SubscribeToStateChanges error: " << status.error_message() << std::endl;
        }
    }
    
private:
    std::shared_ptr<grpc::Channel> m_channel;
    std::unique_ptr<pb::Daemon::Stub> m_stub;
    TQObject* m_receiver;
    grpc::ClientContext m_context;
    bool m_running;
};

DaemonClient::DaemonClient(TQObject* parent)
    : TQObject(parent), m_poller(nullptr)
{
}

DaemonClient::~DaemonClient()
{
    if (m_poller) {
        m_poller->stop();
        m_poller->wait(); // wait for thread to finish
        delete m_poller;
    }
}

bool DaemonClient::connectToDaemon(const TQString& socketPath)
{
    TQString target = "unix://" + socketPath;
    m_channel = grpc::CreateChannel(target.ascii(), grpc::InsecureChannelCredentials());
    m_stub = pb::Daemon::NewStub(m_channel);
    
    // Start listening to state changes
    m_poller = new DaemonStatePoller(m_channel, this);
    m_poller->start();
    
    std::cout << "Connected to daemon gRPC socket at " << target.ascii() << std::endl;
    return true;
}

void DaemonClient::customEvent(TQCustomEvent* e)
{
    if (e->type() == EVENT_APP_STATE) {
        DaemonStateEvent* stateEvent = static_cast<DaemonStateEvent*>(e);
        const auto& state = stateEvent->m_state;
        
        // Handle the incoming state from GUI thread
        if (state.has_connection_status()) {
            TQString statusStr = "Unknown";
            switch (state.connection_status().state()) {
                case pb::DISCONNECTED: statusStr = "Disconnected"; break;
                case pb::CONNECTING: statusStr = "Connecting..."; break;
                case pb::CONNECTED: statusStr = "Connected"; break;
                default: break;
            }
            std::cout << "GUI Thread: New Connection Status: " << statusStr.ascii() << std::endl;
            emit stateChanged(statusStr);
        }
        
        if (state.has_login_event()) {
            std::cout << "GUI Thread: New Login Event: " 
                      << state.login_event().type() << std::endl;
            if (state.login_event().type() == pb::LOGIN) {
                emit stateChanged("LoggedIn");
            } else if (state.login_event().type() == pb::LOGOUT) {
                emit stateChanged("LoggedOut");
            }
        }
    }
}

void DaemonClient::login(const TQString& /*username*/, const TQString& /*password*/)
{
    // Make the LoginOAuth2 gRPC call
    pb::LoginOAuth2Request request;
    // Set type to LOGIN (1) based on login.proto enum
    request.set_type(pb::LoginType_LOGIN);
    
    pb::LoginOAuth2Response response;
    grpc::ClientContext context;
    
    grpc::Status status = m_stub->LoginOAuth2(&context, request, &response);
    
    if (status.ok()) {
        std::cout << "LoginOAuth2 URL: " << response.url() << std::endl;
        std::cout << "Status code: " << response.status() << std::endl;
        
        // Print instruction to terminal for debugging
        std::cout << "\n>>> Please open the URL in your browser to log in <<<\n" << std::endl;
        
        // Launch browser automatically
        std::string cmd = "xdg-open '" + response.url() + "' &";
        system(cmd.c_str());
    } else {
        std::cerr << "LoginOAuth2 failed: " << status.error_message() << std::endl;
    }
}

void DaemonClient::submitLoginCallback(const TQString& url)
{
    pb::LoginOAuth2CallbackRequest request;
    request.set_token(url.ascii());
    request.set_type(pb::LoginType_LOGIN);
    
    pb::LoginOAuth2CallbackResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = m_stub->LoginOAuth2Callback(&context, request, &response);
    
    if (status.ok()) {
        std::cout << "Login callback submitted successfully! Status code: " << response.status() << std::endl;
    } else {
        std::cerr << "Login callback failed: " << status.error_message() << std::endl;
    }
}

TQValueList<DaemonClient::ServerGroup> DaemonClient::getCountries()
{
    TQValueList<ServerGroup> list;
    if (!m_stub) return list;
    
    pb::Empty req;
    pb::ServersResponse res;
    grpc::ClientContext ctx;
    
    grpc::Status status = m_stub->GetServers(&ctx, req, &res);
    if (status.ok() && res.has_servers()) {
        const pb::ServersMap& map = res.servers();
        for (int i = 0; i < map.servers_by_country_size(); i++) {
            const pb::ServerCountry& c = map.servers_by_country(i);
            
            for (int j = 0; j < c.cities_size(); j++) {
                const pb::ServerCity& city = c.cities(j);
                
                ServerGroup sg;
                TQString cityName(city.city_name().c_str());
                sg.name = TQString(c.country_name().c_str()) + " - " + cityName;
                
                sg.isVirtual = false;
                if (city.servers_size() > 0) {
                    sg.isVirtual = city.servers(0).virtual_();
                }
                
                if (sg.isVirtual) {
                    sg.name += " (Virtual)";
                }
                
                sg.code = c.country_name().c_str(); // Use country name for flag lookup
                sg.connectPayload = cityName; // Connect directly to the city
                list.append(sg);
            }
        }
    } else {
        std::cerr << "Failed to fetch Servers bulk map: " << status.error_message() << std::endl;
    }
    
    std::vector<ServerGroup> vec;
    for (TQValueList<ServerGroup>::const_iterator it = list.begin(); it != list.end(); ++it) {
        vec.push_back(*it);
    }
    std::sort(vec.begin(), vec.end(), [](const ServerGroup& a, const ServerGroup& b) {
        return a.name < b.name;
    });
    list.clear();
    for (size_t i = 0; i < vec.size(); i++) {
        list.append(vec[i]);
    }
    
    return list;
}

TQValueList<DaemonClient::ServerGroup> DaemonClient::getGroups()
{
    TQValueList<ServerGroup> list;
    if (!m_stub) return list;
    
    pb::Empty req;
    pb::ServerGroupsList res;
    grpc::ClientContext ctx;
    
    grpc::Status status = m_stub->Groups(&ctx, req, &res);
    if (status.ok()) {
        for (int i = 0; i < res.servers_size(); i++) {
            const pb::ServerGroup& s = res.servers(i);
            ServerGroup sg;
            sg.name = s.name().c_str();
            sg.code = s.name().c_str();
            sg.connectPayload = s.name().c_str();
            sg.isVirtual = s.virtuallocation();
            list.append(sg);
        }
    } else {
        std::cerr << "Failed to fetch Groups: " << status.error_message() << std::endl;
    }
    
    std::vector<ServerGroup> vec;
    for (TQValueList<ServerGroup>::const_iterator it = list.begin(); it != list.end(); ++it) {
        vec.push_back(*it);
    }
    std::sort(vec.begin(), vec.end(), [](const ServerGroup& a, const ServerGroup& b) {
        return a.name < b.name;
    });
    list.clear();
    for (size_t i = 0; i < vec.size(); i++) {
        list.append(vec[i]);
    }
    
    return list;
}

TQString DaemonClient::getVpnStateString()
{
    if (!m_stub) return "Not connected to a VPN server";
    
    pb::Empty req;
    pb::StatusResponse res;
    grpc::ClientContext ctx;
    
    grpc::Status status = m_stub->Status(&ctx, req, &res);
    if (status.ok()) {
        if (res.state() == pb::CONNECTED) {
            TQString serverName = res.name().c_str();
            TQString country = res.country().c_str();
            if (!serverName.isEmpty()) {
                return "Connected to: " + serverName + (country.isEmpty() ? "" : " (" + country + ")");
            }
            return "Connected";
        } else if (res.state() == pb::CONNECTING) {
            return "Connecting...";
        }
    }
    
    return "Not connected to a VPN server";
}

DaemonClient::VpnStatus DaemonClient::getVpnStatus()
{
    VpnStatus s;
    s.isConnected = false;
    if (!m_stub) return s;
    
    pb::Empty req;
    pb::StatusResponse res;
    grpc::ClientContext ctx;
    
    grpc::Status status = m_stub->Status(&ctx, req, &res);
    if (status.ok() && res.state() == pb::CONNECTED) {
        s.isConnected = true;
        s.serverName = res.name().c_str();
        s.ip = res.ip().c_str();
        s.countryCode = res.country_code().c_str();
    }
    return s;
}

void DaemonClient::connectVpn()
{
    printf("TRACER: DaemonClient::connectVpn() CALLED\\n");
    if (!m_stub) return;
    std::thread([this]() {
        pb::ConnectRequest req;
        grpc::ClientContext ctx;
        std::unique_ptr<grpc::ClientReader<pb::Payload>> reader(m_stub->Connect(&ctx, req));
        
        pb::Payload payload;
        while (reader->Read(&payload)) {}
    }).detach();
}

void DaemonClient::connectToCountry(const TQString& countryName)
{
    printf("TRACER: DaemonClient::connectToCountry(%s) CALLED\\n", countryName.utf8().data());
    if (!m_stub) return;
    std::string tag = countryName.ascii();
    
    std::thread([this, tag]() {
        pb::ConnectRequest req;
        req.set_server_tag(tag);
        grpc::ClientContext ctx;
        std::unique_ptr<grpc::ClientReader<pb::Payload>> reader(m_stub->Connect(&ctx, req));
        
        pb::Payload payload;
        while (reader->Read(&payload)) {}
        
        grpc::Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "ConnectToCountry failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::connectToGroup(const TQString& groupName)
{
    printf("TRACER: DaemonClient::connectToGroup(%s) CALLED\\n", groupName.utf8().data());
    if (!m_stub) return;
    std::string group = groupName.ascii();
    
    std::thread([this, group]() {
        pb::ConnectRequest req;
        req.set_server_group(group);
        grpc::ClientContext ctx;
        std::unique_ptr<grpc::ClientReader<pb::Payload>> reader(m_stub->Connect(&ctx, req));
        
        pb::Payload payload;
        while (reader->Read(&payload)) {}
        
        grpc::Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "ConnectToGroup failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::disconnectVpn()
{
    printf("TRACER: DaemonClient::disconnectVpn() CALLED\\n");
    if (!m_stub) return;
    
    std::thread([this]() {
        pb::Empty req;
        grpc::ClientContext ctx;
        std::unique_ptr<grpc::ClientReader<pb::Payload>> reader(m_stub->Disconnect(&ctx, req));
        
        pb::Payload payload;
        while (reader->Read(&payload)) {}
    }).detach();
}

DaemonClient::AdvancedSettings DaemonClient::getAdvancedSettings() {
    AdvancedSettings s;
    s.killSwitch = false;
    s.threatProtectionLite = false;
    s.obfuscate = false;
    s.autoConnect = false;
    s.technology = 0;
    s.protocol = 0;
    s.lanDiscovery = false;
    s.firewall = false;
    s.firewallMark = 0;
    s.postQuantum = false;
    s.notify = false;
    
    pb::Empty request;
    pb::SettingsResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = m_stub->Settings(&context, request, &response);
    if (status.ok()) {
        s.killSwitch = response.data().kill_switch();
        s.threatProtectionLite = response.data().threat_protection_lite();
        s.obfuscate = response.data().obfuscate();
        if (response.data().has_auto_connect_data()) {
            s.autoConnect = response.data().auto_connect_data().enabled();
            if (response.data().auto_connect_data().server_group() != config::UNDEFINED) {
                s.autoConnectTarget = TQString::fromUtf8(config::ServerGroup_Name(response.data().auto_connect_data().server_group()).c_str()) + " (Quick connect)";
            } else if (!response.data().auto_connect_data().city().empty()) {
                s.autoConnectTarget = TQString::fromUtf8(response.data().auto_connect_data().city().c_str()) + " (Quick connect)";
            } else if (!response.data().auto_connect_data().country().empty()) {
                s.autoConnectTarget = TQString::fromUtf8(response.data().auto_connect_data().country().c_str()) + " (Quick connect)";
            } else {
                s.autoConnectTarget = "Fastest (Quick connect)";
            }
        }
        s.technology = response.data().technology();
        s.protocol = response.data().protocol();
        s.lanDiscovery = response.data().lan_discovery();
        s.firewall = response.data().firewall();
        s.firewallMark = response.data().fwmark();
        s.postQuantum = response.data().postquantum_vpn();
        if (response.data().has_user_settings()) {
            s.notify = response.data().user_settings().notify();
        }
        for (int i = 0; i < response.data().dns_size(); ++i) {
            s.customDns.append(TQString::fromUtf8(response.data().dns(i).c_str()));
        }
        if (response.data().has_allowlist()) {
            for (int i = 0; i < response.data().allowlist().subnets_size(); ++i) {
                s.allowlistSubnets.append(TQString::fromUtf8(response.data().allowlist().subnets(i).c_str()));
            }
            if (response.data().allowlist().has_ports()) {
                for (int i = 0; i < response.data().allowlist().ports().tcp_size(); ++i) {
                    s.allowlistPorts.append({response.data().allowlist().ports().tcp(i), true});
                }
                for (int i = 0; i < response.data().allowlist().ports().udp_size(); ++i) {
                    s.allowlistPorts.append({response.data().allowlist().ports().udp(i), false});
                }
            }
        }
    } else {
        std::cerr << "Failed to fetch settings: " << status.error_message() << std::endl;
    }
    
    return s;
}

void DaemonClient::setKillSwitch(bool enabled)
{
    if (!m_stub) return;
    std::thread([this, enabled]() {
        grpc::ClientContext ctx;
        pb::SetKillSwitchRequest req;
        req.set_kill_switch(enabled);
        pb::Payload res;
        grpc::Status status = m_stub->SetKillSwitch(&ctx, req, &res);
        if (!status.ok()) std::cerr << "KillSwitch error: " << status.error_message() << std::endl;
    }).detach();
}

void DaemonClient::setThreatProtectionLite(bool enabled)
{
    if (!m_stub) return;
    std::thread([this, enabled]() {
        grpc::ClientContext ctx;
        pb::SetThreatProtectionLiteRequest req;
        req.set_threat_protection_lite(enabled);
        pb::SetThreatProtectionLiteResponse res;
        grpc::Status status = m_stub->SetThreatProtectionLite(&ctx, req, &res);
        if (!status.ok()) std::cerr << "ThreatProtection error: " << status.error_message() << std::endl;
    }).detach();
}

void DaemonClient::setObfuscate(bool enabled)
{
    if (!m_stub) return;
    std::thread([this, enabled]() {
        pb::SetGenericRequest req;
        req.set_enabled(enabled);
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetObfuscate(&context, req, &resp);
        if (!status.ok()) {
            std::cerr << "SetObfuscate failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::setAutoConnect(bool enabled) {
    if (!m_stub) return;
    std::thread([this, enabled]() {
        pb::SetAutoconnectRequest req;
        req.set_enabled(enabled);
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetAutoConnect(&context, req, &resp);
        if (!status.ok()) {
            std::cerr << "SetAutoConnect failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::setAutoConnectTarget(const TQString& countryOrCity, const TQString& group) {
    if (!m_stub) return;
    std::thread([this, countryOrCity, group]() {
        pb::SetAutoconnectRequest req;
        req.set_enabled(true);
        if (!group.isEmpty()) req.set_server_group(group.utf8().data());
        if (!countryOrCity.isEmpty()) req.set_server_tag(countryOrCity.utf8().data());
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetAutoConnect(&context, req, &resp);
        if (!status.ok()) {
            std::cerr << "SetAutoConnectTarget failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::setTechnologyAndProtocol(int technology, int protocol) {
    if (!m_stub) return;
    std::thread([this, technology, protocol]() {
        if (technology != 0) {
            pb::SetTechnologyRequest techReq;
            techReq.set_technology(static_cast<config::Technology>(technology));
            pb::Payload techResp;
            grpc::ClientContext techCtx;
            grpc::Status status = m_stub->SetTechnology(&techCtx, techReq, &techResp);
            if (!status.ok()) {
                std::cerr << "SetTechnology failed: " << status.error_message() << std::endl;
            }
        }
        
        if (protocol != 0 && static_cast<config::Technology>(technology) == config::OPENVPN) {
            pb::SetProtocolRequest protoReq;
            protoReq.set_protocol(static_cast<config::Protocol>(protocol));
            pb::SetProtocolResponse protoResp;
            grpc::ClientContext protoCtx;
            grpc::Status status = m_stub->SetProtocol(&protoCtx, protoReq, &protoResp);
            if (!status.ok()) {
                std::cerr << "SetProtocol failed: " << status.error_message() << std::endl;
            }
        }
    }).detach();
}

void DaemonClient::setLanDiscovery(bool enabled) {
    if (!m_stub) return;
    std::thread([this, enabled]() {
        pb::SetLANDiscoveryRequest req;
        req.set_enabled(enabled);
        pb::SetLANDiscoveryResponse resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetLANDiscovery(&context, req, &resp);
        if (!status.ok()) {
            std::cerr << "SetLANDiscovery failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::setFirewall(bool enabled) {
    if (!m_stub) return;
    std::thread([this, enabled]() {
        pb::SetGenericRequest req;
        req.set_enabled(enabled);
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetFirewall(&context, req, &resp);
        if (!status.ok()) {
            std::cerr << "SetFirewall failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::logout() {
    if (!m_stub) return;
    std::thread([this]() {
        pb::LogoutRequest req;
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->Logout(&context, req, &resp);
        if (!status.ok()) {
            std::cerr << "Logout failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

DaemonClient::AccountInfo DaemonClient::getAccountInfo() {
    AccountInfo info;
    info.subscriptionStatus = "Active until (Unknown)";
    if (!m_stub) return info;
    
    pb::AccountRequest req;
    req.set_full(true); // request full user details
    pb::AccountResponse resp;
    grpc::ClientContext context;
    
    grpc::Status status = m_stub->AccountInfo(&context, req, &resp);
    if (status.ok()) {
        if (!resp.subscription_expires_at().empty()) {
            info.subscriptionStatus = TQString("Active until %1").arg(resp.subscription_expires_at().c_str());
        } else {
            info.subscriptionStatus = "Subscription Information Unavailable";
        }
        info.email = resp.email().c_str();
        info.createdOn = resp.created_on().c_str();
    } else {
        std::cerr << "AccountInfo failed: " << status.error_message() << std::endl;
    }
    
    return info;
}

void DaemonClient::setPostQuantum(bool enabled) {
    if (!m_stub) return;
    std::thread([this, enabled]() {
        pb::SetGenericRequest req;
        req.set_enabled(enabled);
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetPostQuantum(&context, req, &resp);
        if (!status.ok()) {
            std::cerr << "SetPostQuantum failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::setNotify(bool enabled) {
    if (!m_stub) return;
    std::thread([this, enabled]() {
        pb::SetNotifyRequest req;
        req.set_notify(enabled);
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetNotify(&context, req, &resp);
        if (!status.ok()) {
            std::cerr << "SetNotify failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::setDefaults(bool noLogout) {
    if (!m_stub) return;
    std::thread([this, noLogout]() {
        pb::SetDefaultsRequest req;
        req.set_no_logout(noLogout);
        req.set_off_killswitch(false);
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetDefaults(&context, req, &resp);
        if (!status.ok()) {
            std::cerr << "SetDefaults failed: " << status.error_message() << std::endl;
        }
    }).detach();
}

void DaemonClient::setCustomDns(const TQValueList<TQString>& dnsList) {
    if (!m_stub) return;
    std::thread([this, dnsList]() {
        pb::SetDNSRequest req;
        for (TQValueList<TQString>::const_iterator it = dnsList.begin(); it != dnsList.end(); ++it) {
            req.add_dns((*it).utf8().data());
        }
        pb::SetDNSResponse resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetDNS(&context, req, &resp);
        if (!status.ok()) std::cerr << "SetDNS failed: " << status.error_message() << std::endl;
    }).detach();
}

void DaemonClient::addAllowlistPort(int64_t port, bool tcp, bool udp) {
    if (!m_stub) return;
    std::thread([this, port, tcp, udp]() {
        pb::SetAllowlistRequest req;
        pb::SetAllowlistPortsRequest* pr = req.mutable_set_allowlist_ports_request();
        pr->set_is_tcp(tcp);
        pr->set_is_udp(udp);
        pb::PortRange* pr2 = pr->mutable_port_range();
        pr2->set_start_port(port);
        pr2->set_end_port(port);
        
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetAllowlist(&context, req, &resp);
        if (!status.ok()) std::cerr << "addAllowlistPort failed: " << status.error_message() << std::endl;
    }).detach();
}

void DaemonClient::addAllowlistSubnet(const TQString& subnet) {
    if (!m_stub) return;
    std::thread([this, subnet]() {
        pb::SetAllowlistRequest req;
        pb::SetAllowlistSubnetRequest* sr = req.mutable_set_allowlist_subnet_request();
        sr->set_subnet(subnet.utf8().data());
        
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->SetAllowlist(&context, req, &resp);
        if (!status.ok()) std::cerr << "addAllowlistSubnet failed: " << status.error_message() << std::endl;
    }).detach();
}

void DaemonClient::removeAllowlistPort(int64_t port, bool tcp, bool udp) {
    if (!m_stub) return;
    std::thread([this, port, tcp, udp]() {
        pb::SetAllowlistRequest req;
        pb::SetAllowlistPortsRequest* pr = req.mutable_set_allowlist_ports_request();
        pr->set_is_tcp(tcp);
        pr->set_is_udp(udp);
        pb::PortRange* pr2 = pr->mutable_port_range();
        pr2->set_start_port(port);
        pr2->set_end_port(port);
        
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->UnsetAllowlist(&context, req, &resp);
        if (!status.ok()) std::cerr << "removeAllowlistPort failed: " << status.error_message() << std::endl;
    }).detach();
}

void DaemonClient::removeAllowlistSubnet(const TQString& subnet) {
    if (!m_stub) return;
    std::thread([this, subnet]() {
        pb::SetAllowlistRequest req;
        pb::SetAllowlistSubnetRequest* sr = req.mutable_set_allowlist_subnet_request();
        sr->set_subnet(subnet.utf8().data());
        
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->UnsetAllowlist(&context, req, &resp);
        if (!status.ok()) std::cerr << "removeAllowlistSubnet failed: " << status.error_message() << std::endl;
    }).detach();
}

void DaemonClient::clearAllowlist() {
    if (!m_stub) return;
    std::thread([this]() {
        pb::Empty req;
        pb::Payload resp;
        grpc::ClientContext context;
        grpc::Status status = m_stub->UnsetAllAllowlist(&context, req, &resp);
        if (!status.ok()) std::cerr << "clearAllowlist failed: " << status.error_message() << std::endl;
    }).detach();
}

void DaemonClient::requestState()
{
    // TODO: Make a streaming or unary call to get connection state
}

void DaemonClient::requestInitialState()
{
    pb::Empty req;
    
    pb::StatusResponse statusRes;
    grpc::ClientContext ctx1;
    grpc::Status s = m_stub->Status(&ctx1, req, &statusRes);
    if (!s.ok()) {
        emit connectionError("Could not reach daemon status endpoint: " + TQString(s.error_message().c_str()));
        return;
    }
    
    if (statusRes.state() == pb::CONNECTED) {
        emit stateChanged("Connected");
        return;
    }
    
    pb::IsLoggedInResponse loginRes;
    grpc::ClientContext ctx2;
    if (m_stub->IsLoggedIn(&ctx2, req, &loginRes).ok()) {
        if (loginRes.is_logged_in()) {
            emit stateChanged("LoggedIn");
            return;
        }
    }
}
