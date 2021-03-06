// Copyright 2010-2012 RethinkDB, all rights reserved.
#include "clustering/administration/servers/network_logger.hpp"

#include "errors.hpp"
#include <boost/bind.hpp>

network_logger_t::network_logger_t(
        peer_id_t our_peer_id,
        watchable_map_t<peer_id_t, cluster_directory_metadata_t> *dv) :
    us(our_peer_id),
    directory_view(dv),
    directory_subscription(
        directory_view,
        std::bind(&network_logger_t::on_change, this, ph::_1, ph::_2),
        initial_call_t::YES)
    { }

void network_logger_t::on_change(
        const peer_id_t &peer_id, const cluster_directory_metadata_t *value) {
    if (value != nullptr) {
        const server_id_t &server_id = value->server_id;
        local_connections.set_key(server_id, empty_value_t());
        switch (value->peer_type) {
            case SERVER_PEER: {
                const name_string_t &name = value->server_config.config.name;
                if (peer_id != us && connected_servers.count(peer_id) == 0) {
                    logNTC("Connected to server \"%s\" %s",
                        name.c_str(), server_id.print().c_str());
                }
                connected_servers[peer_id] = std::make_pair(server_id, name);
                break;
            }
            case PROXY_PEER: {
                if (peer_id != us && connected_proxies.count(peer_id) == 0) {
                    logNTC("Connected to proxy %s", server_id.print().c_str());
                }
                connected_proxies[peer_id] = server_id;
                break;
            }
            default: unreachable();
        }
    } else {
        auto it = connected_servers.find(peer_id);
        if (it != connected_servers.end()) {
            if (peer_id != us) {
                logNTC("Disconnected from server \"%s\" %s",
                       it->second.second.c_str(),
                       it->second.first.print().c_str());
            }
            local_connections.delete_key(it->second.first);
            connected_servers.erase(it);
        } else {
            auto jt = connected_proxies.find(peer_id);
            if (jt != connected_proxies.end()) {
                if (peer_id != us) {
                    logNTC("Disconnected from proxy %s",
                           jt->second.print().c_str());
                }
                local_connections.delete_key(jt->second);
                connected_proxies.erase(jt);
            }
        }
    }
}

