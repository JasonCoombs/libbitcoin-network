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
#include <bitcoin/network/channel.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/network/proxy.hpp>
#include <bitcoin/network/settings.hpp>

namespace libbitcoin {
namespace network {

using namespace bc::message;
using namespace std::placeholders;

// Factory for deadline timer pointer construction.
static deadline::ptr alarm(threadpool& pool, const asio::duration& duration)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " alarm()";

    return std::make_shared<deadline>(pool, pseudo_random::duration(duration));
}

channel::channel(threadpool& pool, socket::ptr socket,
    const settings& settings)
  : proxy(pool, socket, settings),
    notify_(false),
    nonce_(0),
    expiration_(alarm(pool, settings.channel_expiration())),
    inactivity_(alarm(pool, settings.channel_inactivity())),
    CONSTRUCT_TRACK(channel)
{
}

// Talk sequence.
// ----------------------------------------------------------------------------

// public:
void channel::start(result_handler handler)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::start()";

    proxy::start(
        std::bind(&channel::do_start,
            shared_from_base<channel>(), _1, handler));
}

// Don't start the timers until the socket is enabled.
void channel::do_start(const code& , result_handler handler)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::do_start()";

    start_expiration();
    start_inactivity();
    handler(error::success);
}

// Properties.
// ----------------------------------------------------------------------------

bool channel::notify() const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::notify()";

    return notify_;
}

void channel::set_notify(bool value)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::set_notify()";

    notify_ = value;
}

uint64_t channel::nonce() const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::nonce()";

    return nonce_;
}

void channel::set_nonce(uint64_t value)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::set_nonce()";

    nonce_.store(value);
}

version_const_ptr channel::peer_version() const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::peer_version()";

    const auto version = peer_version_.load();
    BITCOIN_ASSERT_MSG(version, "Read peer version before set.");
    return version;
}

void channel::set_peer_version(version_const_ptr value)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::set_peer_version()";

    peer_version_.store(value);
}

// Proxy pure virtual protected and ordered handlers.
// ----------------------------------------------------------------------------

// It is possible that this may be called multiple times.
void channel::handle_stopping()
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::handle_stopping()";

    expiration_->stop();
    inactivity_->stop();
}

void channel::signal_activity()
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::signal_activity()";

    start_inactivity();
}

bool channel::stopped(const code& ec) const
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::stopped()";

    return proxy::stopped() || ec == error::channel_stopped ||
        ec == error::service_stopped;
}

// Timers (these are inherent races, requiring stranding by stop only).
// ----------------------------------------------------------------------------

void channel::start_expiration()
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::start_expiration()";

    if (proxy::stopped())
        return;

    expiration_->start(
        std::bind(&channel::handle_expiration,
            shared_from_base<channel>(), _1));
}

void channel::handle_expiration(const code& ec)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::handle_expiration()";

    if (stopped(ec))
        return;

    LOG_DEBUG(LOG_NETWORK)
        << "Channel lifetime expired [" << authority() << "]";

    stop(error::channel_timeout);
}

void channel::start_inactivity()
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::start_inactivity()";

    if (proxy::stopped())
        return;

    inactivity_->start(
        std::bind(&channel::handle_inactivity,
            shared_from_base<channel>(), _1));
}

void channel::handle_inactivity(const code& ec)
{
    const auto this_id = boost::this_thread::get_id();
    LOG_VERBOSE(LOG_NETWORK)
    << this_id
    << " channel::handle_inactivity()";

    if (stopped(ec))
        return;

    LOG_DEBUG(LOG_NETWORK)
        << "Channel inactivity timeout [" << authority() << "]";

    stop(error::channel_timeout);
}

} // namespace network
} // namespace libbitcoin
