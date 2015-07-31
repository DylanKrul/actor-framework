/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_ACTOR_REGISTRY_HPP
#define CAF_DETAIL_ACTOR_REGISTRY_HPP

#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <condition_variable>

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/abstract_actor.hpp"

#include "caf/detail/shared_spinlock.hpp"
#include "caf/detail/singleton_mixin.hpp"

namespace caf {
namespace detail {

class singletons;

class actor_registry {
public:
  ~actor_registry();

  /// A registry entry consists of a pointer to the actor and an
  /// exit reason. An entry with a nullptr means the actor has finished
  /// execution for given reason.
  using value_type = std::pair<abstract_actor_ptr, uint32_t>;

  value_type get_entry(actor_id key) const;

  // return nullptr if the actor wasn't put *or* finished execution
  inline abstract_actor_ptr get(actor_id key) const {
    return get_entry(key).first;
  }

  // return nullptr if the actor wasn't put *or* finished execution
  inline actor_addr get_addr(actor_id key) const {
    auto ptr = get_entry(key).first;
    return ptr ? ptr->address() : invalid_actor_addr;
  }

  void put(actor_id key, const abstract_actor_ptr& value);

  void put(actor_id key, const actor_addr& value);

  void erase(actor_id key, uint32_t reason);

  // gets the next free actor id
  actor_id next_id();

  // increases running-actors-count by one
  void inc_running();

  // decreases running-actors-count by one
  void dec_running();

  size_t running() const;

  // blocks the caller until running-actors-count becomes `expected`
  void await_running_count_equal(size_t expected);

  actor get_named(atom_value key) const;

  std::vector<std::pair<atom_value, actor>> named_actors() const;

  static actor_registry* create_singleton();

  void dispose();

  void stop();

  void initialize();

private:
  using named_entries = std::unordered_map<atom_value, actor>;

  using entries = std::unordered_map<actor_id, value_type>;

  actor_registry();

  std::atomic<size_t> running_;

  std::mutex running_mtx_;
  std::condition_variable running_cv_;

  mutable detail::shared_spinlock instances_mtx_;
  entries entries_;

  named_entries named_entries_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_ACTOR_REGISTRY_HPP
