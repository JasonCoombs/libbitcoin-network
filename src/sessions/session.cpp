/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/network/sessions/session.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/network/acceptor.hpp>
#include <bitcoin/network/channel.hpp>
#include <bitcoin/network/connector.hpp>
#include <bitcoin/network/p2p.hpp>
#include <bitcoin/network/proxy.hpp>
#include <bitcoin/network/protocols/protocol_version_31402.hpp>
#include <bitcoin/network/protocols/protocol_version_70002.hpp>
#include <bitcoin/network/settings.hpp>

namespace libbitcoin {
namespace network {

#define CLASS session
#define NAME "session"

using namespace std::placeholders;

session::session(p2p& network, bool notify_on_connect)
  : stopped_(true),
    notify_on_connect_(notify_on_connect),
    network_(network),
    dispatch_(network.thread_pool(), NAME),
    pool_(network.thread_pool()),
    settings_(network.network_settings())
{
}

session::~session()
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " ~session()";
    
    BITCOIN_ASSERT_MSG(stopped(), "The session was not stopped.");
}

// Properties.
// ----------------------------------------------------------------------------
// protected

size_t session::address_count() const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::address_count()";
    
    return network_.address_count();
}

size_t session::connection_count() const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::connection_count()";
    
    return network_.connection_count();
}

code session::fetch_address(address& out_address) const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::fetch_address()";
    
    return network_.fetch_address(out_address);
}

bool session::blacklisted(const authority& authority) const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::blacklisted()";
    
    const auto ip_compare = [&](const config::authority& blocked)
    {
        return authority.ip() == blocked.ip();
    };

    const auto& list = settings_.blacklists;
    return std::any_of(list.begin(), list.end(), ip_compare);
}

bool session::stopped() const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::stopped() 1";
    
    return stopped_;
}

bool session::stopped(const code& ec) const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::stopped() 2";
    
    return stopped() || ec == error::service_stopped;
}

// Socket creators.
// ----------------------------------------------------------------------------

acceptor::ptr session::create_acceptor()
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::create_acceptor()";
    
    return std::make_shared<acceptor>(pool_, settings_);
}

connector::ptr session::create_connector()
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::create_connector()";
    
    return std::make_shared<connector>(pool_, settings_);
}

// Pending connect.
// ----------------------------------------------------------------------------

code session::pend(connector::ptr connector)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::pend()";
    
    return network_.pend(connector);
}

void session::unpend(connector::ptr connector)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::unpend()";
    
    network_.unpend(connector);
}

// Pending handshake.
// ----------------------------------------------------------------------------

code session::pend(channel::ptr channel)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::pend()";
    
    return network_.pend(channel);
}

void session::unpend(channel::ptr channel)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::unpend()";
    
    network_.unpend(channel);
}

bool session::pending(uint64_t version_nonce) const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::pending()";
    
    return network_.pending(version_nonce);
}

// Start sequence.
// ----------------------------------------------------------------------------
// Must not change context before subscribing.

void session::start(result_handler handler)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::start()";
    
    if (!stopped())
    {
        handler(error::operation_failed);
        return;
    }

    stopped_ = false;
    subscribe_stop(BIND1(handle_stop, _1));

    // This is the end of the start sequence.
    handler(error::success);
}

void session::handle_stop(const code& )
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::handle_stop()";
    
    // This signals the session to stop creating connections, but does not
    // close the session. Channels stop, resulting in session loss of scope.
    stopped_ = true;
}

// Subscribe Stop.
// ----------------------------------------------------------------------------

void session::subscribe_stop(result_handler handler)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::subscribe_stop()";
    
    network_.subscribe_stop(handler);
}

// Registration sequence.
// ----------------------------------------------------------------------------
// Must not change context in start or stop sequences (use bind).

void session::register_channel(channel::ptr channel,
    result_handler handle_started, result_handler handle_stopped)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::register_channel()";
    
    if (stopped())
    {
        handle_started(error::service_stopped);
        handle_stopped(error::service_stopped);
        return;
    }

    start_channel(channel,
        BIND4(handle_start, _1, channel, handle_started, handle_stopped));
}

void session::start_channel(channel::ptr channel,
    result_handler handle_started)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::start_channel()";
    
    channel->set_notify(notify_on_connect_);
    channel->set_nonce(pseudo_random::next(1, max_uint64));

    // The channel starts, invokes the handler, then starts the read cycle.
    channel->start(
        BIND3(handle_starting, _1, channel, handle_started));
}

void session::handle_starting(const code& ec, channel::ptr channel,
    result_handler handle_started)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::handle_starting()";
    
    if (ec)
    {
        LOG_DEBUG(LOG_NETWORK)
            << "Channel failed to start [" << channel->authority() << "] "
            << ec.message();
        handle_started(ec);
        return;
    }

    attach_handshake_protocols(channel,
        BIND3(handle_handshake, _1, channel, handle_started));
}

void session::attach_handshake_protocols(channel::ptr channel,
    result_handler handle_started)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::attach_handshake_protocols()";
    
    // Reject messages are not handled until bip61 (70002).
    // The negotiated_version is initialized to the configured maximum.
    if (channel->negotiated_version() >= message::version::level::bip61)
        attach<protocol_version_70002>(channel)->start(handle_started);
    else
        attach<protocol_version_31402>(channel)->start(handle_started);
}

void session::handle_handshake(const code& ec, channel::ptr channel,
    result_handler handle_started)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::handle_handshake()";
    
    if (ec)
    {
        LOG_DEBUG(LOG_NETWORK)
            << "Failure in handshake with [" << channel->authority()
            << "] " << ec.message();

        handle_started(ec);
        return;
    }

    handshake_complete(channel, handle_started);
}

void session::handshake_complete(channel::ptr channel,
    result_handler handle_started)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::handshake_complete()";
    
    // This will fail if the IP address or nonce is already connected.
    handle_started(network_.store(channel));
}

void session::handle_start(const code& ec, channel::ptr channel,
    result_handler handle_started, result_handler handle_stopped)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::handle_start()";
    
    // Must either stop or subscribe the channel for stop before returning.
    // All closures must eventually be invoked as otherwise it is a leak.
    // Therefore upon start failure expect start failure and stop callbacks.
    if (ec)
    {
        channel->stop(ec);
        handle_stopped(ec);
    }
    else
    {
        channel->subscribe_stop(
            BIND3(handle_remove, _1, channel, handle_stopped));
    }

    // This is the end of the registration sequence.
    handle_started(ec);
}

void session::handle_remove(const code& , channel::ptr channel,
    result_handler handle_stopped)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " session::handle_remove()";
    
    network_.remove(channel);
    handle_stopped(error::success);
}

} // namespace network
} // namespace libbitcoin
