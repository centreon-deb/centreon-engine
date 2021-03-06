/*
** Copyright 2011-2015 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/commands/connector.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/connector.hh"
#include "com/centreon/engine/configuration/applier/globals.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostdependency.hh"
#include "com/centreon/engine/configuration/applier/logging.hh"
#include "com/centreon/engine/configuration/applier/macros.hh"
#include "com/centreon/engine/configuration/applier/scheduler.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/servicedependency.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/configuration/command.hh"
#include "com/centreon/engine/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/objects.hh"
#include "com/centreon/engine/retention/applier/state.hh"
#include "com/centreon/engine/retention/state.hh"
#include "com/centreon/engine/xsddefault.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::logging;

static bool            has_already_been_loaded(false);
static applier::state* _instance(NULL);

/**
 *  Apply new configuration.
 *
 *  @param[in] new_cfg        The new configuration.
 *  @param[in] waiting_thread True to wait thread after calulate differencies.
 */
void applier::state::apply(configuration::state& new_cfg, bool waiting_thread) {
  configuration::state save(*config);
  try {
    _processing_state = state_ready;
    _processing(new_cfg, waiting_thread);
  }
  catch (std::exception const& e) {
    // If is the first time to load configuration, we don't
    // have a valid configuration to restore.
    if (!has_already_been_loaded)
      throw;

    // If is not the first time, we can restore the old one.
    logger(log_config_error, basic)
      << "Error: Could not apply new configuration: " << e.what();

    // Check if we need to restore old configuration.
    if (_processing_state == state_error) {
      logger(dbg_config, more)
        << "configuration: try to restore old configuration";
      _processing(save, waiting_thread);
    }
  }

  // wake up waiting thread.
  if (waiting_thread) {
    concurrency::locker lock(&_lock);
    _cv_lock.wake_one();
  }
  return ;
}

/**
 *  Apply new configuration.
 *
 *  @param[in] new_cfg        The new configuration.
 *  @param[in] state          The retention to use.
 *  @param[in] waiting_thread True to wait thread after calulate differencies.
 */
void applier::state::apply(
       configuration::state& new_cfg,
       retention::state& state,
       bool waiting_thread) {
  configuration::state save(*config);
  try {
    _processing_state = state_ready;
    _processing(new_cfg, waiting_thread, &state);
  }
  catch (std::exception const& e) {
    // If is the first time to load configuration, we don't
    // have a valid configuration to restore.
    if (!has_already_been_loaded)
      throw;

    // If is not the first time, we can restore the old one.
    logger(log_config_error, basic)
      << "Cannot apply new configuration: " << e.what();

    // Check if we need to restore old configuration.
    if (_processing_state == state_error) {
      logger(dbg_config, more)
        << "configuration: try to restore old configuration";
      _processing(save, waiting_thread, &state);
    }
  }

  // wake up waiting thread.
  if (waiting_thread) {
    concurrency::locker lock(&_lock);
    _cv_lock.wake_one();
  }
  return ;
}

/**
 *  Get the singleton instance of state applier.
 *
 *  @return Singleton instance.
 */
applier::state& applier::state::instance() {
  return (*_instance);
}

/**
 *  Load state applier singleton.
 */
void applier::state::load() {
  if (!_instance) {
    _instance = new applier::state;
  }
  return ;
}

/**
 *  Unload state applier singleton.
 */
void applier::state::unload() {
  delete _instance;
  _instance = NULL;
  return ;
}

/**
 *  Default constructor.
 */
applier::state::state()
  : _config(NULL),
    _processing_state(state_ready) {
  applier::logging::load();
  applier::globals::load();
  applier::macros::load();
  applier::scheduler::load();
}

/**
 *  Destructor.
 */
applier::state::~state() throw() {
  applier::scheduler::unload();
  applier::macros::unload();
  applier::globals::unload();
  applier::logging::unload();
}

/**
 *  Get the current commands.
 *
 *  @return The current commands.
 */
umap<std::string, shared_ptr<command_struct> > const& applier::state::commands() const throw () {
  return (_commands);
}

/**
 *  Get the current commands.
 *
 *  @return The current commands.
 */
umap<std::string, shared_ptr<command_struct> >& applier::state::commands() throw () {
  return (_commands);
}

/**
 *  Find a command from its key.
 *
 *  @param[in] k Command name.
 *
 *  @return Iterator to the element if found, commands().end()
 *          otherwise.
 */
umap<std::string, shared_ptr<command_struct> >::const_iterator applier::state::commands_find(configuration::command::key_type const& k) const {
  return (_commands.find(k));
}

/**
 *  Find a command from its key.
 *
 *  @param[in] k Command name.
 *
 *  @return Iterator to the element if found, commands().end()
 *          otherwise.
 */
umap<std::string, shared_ptr<command_struct> >::iterator applier::state::commands_find(configuration::command::key_type const& k) {
  return (_commands.find(k));
}

/**
 *  Find a connector from its key.
 *
 *  @param[in] k Connector name.
 *
 *  @return Iterator to the element if found, connectors().end()
 *          otherwise.
 */
umap<std::string, shared_ptr<commands::connector> >::const_iterator applier::state::connectors_find(configuration::connector::key_type const& k) const {
  return (_connectors.find(k));
}

/**
 *  Find a connector from its key.
 *
 *  @param[in] k Connector name.
 *
 *  @return Iterator to the element if found, connectors().end()
 *          otherwise.
 */
umap<std::string, shared_ptr<commands::connector> >::iterator applier::state::connectors_find(configuration::connector::key_type const& k) {
  return (_connectors.find(k));
}

/**
 *  Get the current connectors.
 *
 *  @return The current connectors.
 */
umap<std::string, shared_ptr<commands::connector> > const& applier::state::connectors() const throw () {
  return (_connectors);
}

/**
 *  Get the current connectors.
 *
 *  @return The current connectors.
 */
umap<std::string, shared_ptr<commands::connector> >& applier::state::connectors() throw () {
  return (_connectors);
}

/**
 *  Get the current hosts.
 *
 *  @return The current hosts.
 */
umap<std::string, shared_ptr<host_struct> > const& applier::state::hosts() const throw () {
  return (_hosts);
}

/**
 *  Get the current hosts.
 *
 *  @return The current hosts.
 */
umap<std::string, shared_ptr<host_struct> >& applier::state::hosts() throw () {
  return (_hosts);
}

/**
 *  Find a host from its key.
 *
 *  @param[in] k Host key (host name).
 *
 *  @return Iterator to the host object if found, hosts().end() if it
 *          was not.
 */
umap<std::string, shared_ptr<host_struct> >::const_iterator applier::state::hosts_find(configuration::host::key_type const& k) const {
  return (_hosts.find(k));
}

/**
 *  Find a host from its key.
 *
 *  @param[in] k Host key (host name).
 *
 *  @return Iterator to the host object if found, hosts().end() if it
 *          was not.
 */
umap<std::string, shared_ptr<host_struct> >::iterator applier::state::hosts_find(configuration::host::key_type const& k) {
  return (_hosts.find(k));
}

/**
 *  Get the current hostdependencies.
 *
 *  @return The current hostdependencies.
 */
umultimap<std::string, shared_ptr<hostdependency_struct> > const& applier::state::hostdependencies() const throw () {
  return (_hostdependencies);
}

/**
 *  Get the current hostdependencies.
 *
 *  @return The current hostdependencies.
 */
umultimap<std::string, shared_ptr<hostdependency_struct> >& applier::state::hostdependencies() throw () {
  return (_hostdependencies);
}

/**
 *  Get a host dependency from its key.
 *
 *  @param[in] k Host dependency key.
 *
 *  @return Iterator to the element if found, hostdependencies().end()
 *          otherwise.
 */
umultimap<std::string, shared_ptr<hostdependency_struct> >::const_iterator applier::state::hostdependencies_find(configuration::hostdependency::key_type const& k) const {
  return (const_cast<state*>(this)->hostdependencies_find(k));
}

/**
 *  Get a host dependency from its key.
 *
 *  @param[in] k Host dependency key.
 *
 *  @return Iterator to the element if found, hostdependencies().end()
 *          otherwise.
 */
umultimap<std::string, shared_ptr<hostdependency_struct> >::iterator applier::state::hostdependencies_find(configuration::hostdependency::key_type const& k) {
  typedef umultimap<std::string, shared_ptr<hostdependency_struct> > collection;
  std::pair<collection::iterator, collection::iterator> p;
  p = _hostdependencies.equal_range(k.dependent_hosts().front());
  while (p.first != p.second) {
    configuration::hostdependency current;
    current.configuration::object::operator=(k);
    current.dependent_hosts().push_back(
                                p.first->second->dependent_host_name);
    current.hosts().push_back(p.first->second->host_name);
    current.dependency_period(p.first->second->dependency_period
                              ? p.first->second->dependency_period
                              : "");
    current.inherits_parent(p.first->second->inherits_parent);
    unsigned int options(
      (p.first->second->fail_on_up
       ? configuration::hostdependency::up
       : 0)
      | (p.first->second->fail_on_down
         ? configuration::hostdependency::down
         : 0)
      | (p.first->second->fail_on_unreachable
         ? configuration::hostdependency::unreachable
         : 0)
      | (p.first->second->fail_on_pending
         ? configuration::hostdependency::pending
         : 0));
    current.failure_options(options);
    if (current == k)
      break ;
    ++p.first;
  }
  return ((p.first == p.second) ? _hostdependencies.end() : p.first);
}

/**
 *  Get the current services.
 *
 *  @return The current services.
 */
umap<std::pair<std::string, std::string>, shared_ptr<service_struct> > const& applier::state::services() const throw () {
  return (_services);
}

/**
 *  Get the current services.
 *
 *  @return The current services.
 */
umap<std::pair<std::string, std::string>, shared_ptr<service_struct> >& applier::state::services() throw () {
  return (_services);
}

/**
 *  Find a service by its key.
 *
 *  @param[in] k Pair of host name / service description.
 *
 *  @return Iterator to the element if found, services().end()
 *          otherwise.
 */
umap<std::pair<std::string, std::string>, shared_ptr<service_struct> >::const_iterator applier::state::services_find(configuration::service::key_type const& k) const {
  return (_services.find(k));
}

/**
 *  Find a service by its key.
 *
 *  @param[in] k Pair of host name / service description.
 *
 *  @return Iterator to the element if found, services().end()
 *          otherwise.
 */
umap<std::pair<std::string, std::string>, shared_ptr<service_struct> >::iterator applier::state::services_find(configuration::service::key_type const& k) {
  return (_services.find(k));
}

/**
 *  Get the current servicedependencies.
 *
 *  @return The current servicedependencies.
 */
umultimap<std::pair<std::string, std::string>, shared_ptr<servicedependency_struct> > const& applier::state::servicedependencies() const throw () {
  return (_servicedependencies);
}

/**
 *  Get the current servicedependencies.
 *
 *  @return The current servicedependencies.
 */
umultimap<std::pair<std::string, std::string>, shared_ptr<servicedependency_struct> >& applier::state::servicedependencies() throw () {
  return (_servicedependencies);
}

/**
 *  Find a service dependency from its key.
 *
 *  @param[in] k The service dependency configuration.
 *
 *  @return Iterator to the element if found,
 *          servicedependencies().end() otherwise.
 */
umultimap<std::pair<std::string, std::string>, shared_ptr<servicedependency_struct> >::const_iterator applier::state::servicedependencies_find(configuration::servicedependency::key_type const& k) const {
  return (const_cast<state*>(this)->servicedependencies_find(k));
}

/**
 *  Find a service dependency from its key.
 *
 *  @param[in] k The service dependency configuration.
 *
 *  @return Iterator to the element if found,
 *          servicedependencies().end() otherwise.
 */
umultimap<std::pair<std::string, std::string>, shared_ptr<servicedependency_struct> >::iterator applier::state::servicedependencies_find(configuration::servicedependency::key_type const& k) {
  typedef umultimap<std::pair<std::string, std::string>, shared_ptr<servicedependency_struct> > collection;
  std::pair<collection::iterator, collection::iterator> p;
  p = _servicedependencies.equal_range(std::make_pair(k.dependent_hosts().front(), k.dependent_service_description().front()));
  while (p.first != p.second) {
    configuration::servicedependency current;
    current.configuration::object::operator=(k);
    current.dependent_hosts().push_back(
                                p.first->second->dependent_host_name);
    current.dependent_service_description().push_back(
              p.first->second->dependent_service_description);
    current.hosts().push_back(p.first->second->host_name);
    current.service_description().push_back(
              p.first->second->service_description);
    current.dependency_period(p.first->second->dependency_period
                              ? p.first->second->dependency_period
                              : "");
    current.inherits_parent(p.first->second->inherits_parent);
    unsigned int options(
                   (p.first->second->fail_on_ok
                    ? configuration::servicedependency::ok
                    : 0)
                   | (p.first->second->fail_on_warning
                      ? configuration::servicedependency::warning
                      : 0)
                   | (p.first->second->fail_on_unknown
                      ? configuration::servicedependency::unknown
                      : 0)
                   | (p.first->second->fail_on_critical
                      ? configuration::servicedependency::critical
                      : 0)
                   | (p.first->second->fail_on_pending
                      ? configuration::servicedependency::pending
                      : 0));
    current.failure_options(options);
    if (current == k)
      break ;
    ++p.first;
  }
  return ((p.first == p.second) ? _servicedependencies.end() : p.first);
}

/**
 *  Get the current timeperiods.
 *
 *  @return The current timeperiods.
 */
umap<std::string, shared_ptr<timeperiod_struct> > const& applier::state::timeperiods() const throw () {
  return (_timeperiods);
}

/**
 *  Get the current timeperiods.
 *
 *  @return The current timeperiods.
 */
umap<std::string, shared_ptr<timeperiod_struct> >& applier::state::timeperiods() throw () {
  return (_timeperiods);
}

/**
 *  Find a time period from its key.
 *
 *  @param[in] k Time period name.
 *
 *  @return Iterator to the element if found, timeperiods().end()
 *          otherwise.
 */
umap<std::string, shared_ptr<timeperiod_struct> >::const_iterator applier::state::timeperiods_find(configuration::timeperiod::key_type const& k) const {
  return (_timeperiods.find(k));
}

/**
 *  Find a time period from its key.
 *
 *  @param[in] k Time period name.
 *
 *  @return Iterator to the element if found, timeperiods().end()
 *          otherwise.
 */
umap<std::string, shared_ptr<timeperiod_struct> >::iterator applier::state::timeperiods_find(configuration::timeperiod::key_type const& k) {
  return (_timeperiods.find(k));
}

/**
 *  Try to lock.
 */
void applier::state::try_lock() {
  concurrency::locker lock(&_lock);
  if (_processing_state == state_waiting) {
    _cv_lock.wake_one();
    _cv_lock.wait(&_lock);
  }
}

/*
 *  Update all new globals.
 *
 *  @param[in]  new_cfg The new configuration state.
 */
void applier::state::_apply(configuration::state const& new_cfg) {
  // Check variables should not be change after the first execution.
  if (has_already_been_loaded) {
    if (config->broker_module() != new_cfg.broker_module()) {
      logger(log_config_warning, basic)
        << "Warning: Broker modules cannot be changed nor reloaded";
      ++config_warnings;
    }
    if (config->broker_module_directory()
        != new_cfg.broker_module_directory()) {
      logger(log_config_warning, basic)
        << "Warning: Broker module directory cannot be changed";
      ++config_warnings;
    }
    if (config->command_file() != new_cfg.command_file()) {
      logger(log_config_warning, basic)
        << "Warning: Command file cannot be changed";
      ++config_warnings;
    }
    if (config->external_command_buffer_slots()
        != new_cfg.external_command_buffer_slots()) {
      logger(log_config_warning, basic)
        << "Warning: External command buffer slots cannot be changed";
      ++config_warnings;
    }
    if (config->use_timezone() != new_cfg.use_timezone()) {
      logger(log_config_warning, basic)
        << "Warning: Timezone can not be changed";
      ++config_warnings;
    }
  }

  // Initialize status file.
  bool modify_status(false);
  if (!has_already_been_loaded
      || config->status_file() != new_cfg.status_file())
    modify_status = true;

  // Cleanup.
  if (modify_status)
    xsddefault_cleanup_status_data(true);

  // Set new values.
  config->additional_freshness_latency(new_cfg.additional_freshness_latency());
  config->cached_host_check_horizon(new_cfg.cached_host_check_horizon());
  config->cached_service_check_horizon(new_cfg.cached_service_check_horizon());
  config->cfg_main(new_cfg.cfg_main());
  config->check_host_freshness(new_cfg.check_host_freshness());
  config->check_reaper_interval(new_cfg.check_reaper_interval());
  config->check_service_freshness(new_cfg.check_service_freshness());
  config->command_check_interval(new_cfg.command_check_interval());
  config->debug_file(new_cfg.debug_file());
  config->debug_level(new_cfg.debug_level());
  config->debug_verbosity(new_cfg.debug_verbosity());
  config->enable_event_handlers(new_cfg.enable_event_handlers());
  config->enable_flap_detection(new_cfg.enable_flap_detection());
  config->enable_predictive_host_dependency_checks(new_cfg.enable_predictive_host_dependency_checks());
  config->enable_predictive_service_dependency_checks(new_cfg.enable_predictive_service_dependency_checks());
  config->event_broker_options(new_cfg.event_broker_options());
  config->event_handler_timeout(new_cfg.event_handler_timeout());
  config->global_host_event_handler(new_cfg.global_host_event_handler());
  config->global_service_event_handler(new_cfg.global_service_event_handler());
  config->high_host_flap_threshold(new_cfg.high_host_flap_threshold());
  config->high_service_flap_threshold(new_cfg.high_service_flap_threshold());
  config->host_check_timeout(new_cfg.host_check_timeout());
  config->host_freshness_check_interval(new_cfg.host_freshness_check_interval());
  config->illegal_object_chars(new_cfg.illegal_object_chars());
  config->illegal_output_chars(new_cfg.illegal_output_chars());
  config->log_event_handlers(new_cfg.log_event_handlers());
  config->log_external_commands(new_cfg.log_external_commands());
  config->log_file(new_cfg.log_file());
  config->log_host_retries(new_cfg.log_host_retries());
  config->log_initial_states(new_cfg.log_initial_states());
  config->log_passive_checks(new_cfg.log_passive_checks());
  config->log_service_retries(new_cfg.log_service_retries());
  config->low_host_flap_threshold(new_cfg.low_host_flap_threshold());
  config->low_service_flap_threshold(new_cfg.low_service_flap_threshold());
  config->max_debug_file_size(new_cfg.max_debug_file_size());
  config->max_log_file_size(new_cfg.max_log_file_size());
  config->max_parallel_service_checks(new_cfg.max_parallel_service_checks());
  config->obsess_over_hosts(new_cfg.obsess_over_hosts());
  config->obsess_over_services(new_cfg.obsess_over_services());
  config->ochp_command(new_cfg.ochp_command());
  config->ochp_timeout(new_cfg.ochp_timeout());
  config->ocsp_command(new_cfg.ocsp_command());
  config->ocsp_timeout(new_cfg.ocsp_timeout());
  config->retention_update_interval(new_cfg.retention_update_interval());
  config->service_check_timeout(new_cfg.service_check_timeout());
  config->service_freshness_check_interval(new_cfg.service_freshness_check_interval());
  config->sleep_time(new_cfg.sleep_time());
  config->soft_state_dependencies(new_cfg.soft_state_dependencies());
  config->state_retention_file(new_cfg.state_retention_file());
  config->status_file(new_cfg.status_file());
  config->time_change_threshold(new_cfg.time_change_threshold());
  config->use_setpgid(new_cfg.use_setpgid());
  config->use_syslog(new_cfg.use_syslog());
  config->user(new_cfg.user());

  // Set this variable just the first time.
  if (!has_already_been_loaded) {
    config->broker_module(new_cfg.broker_module());
    config->broker_module_directory(new_cfg.broker_module_directory());
    config->command_file(new_cfg.command_file());
    config->external_command_buffer_slots(new_cfg.external_command_buffer_slots());
    config->use_timezone(new_cfg.use_timezone());
  }

  // Initialize.
  if (modify_status)
    xsddefault_initialize_status_data();

  // Check global event handler commands...
  if (verify_config)
    logger(log_info_message, basic)
      << "Checking global event handlers...";
  if (!config->global_host_event_handler().empty()) {
    // Check the event handler command.
    std::string temp_command_name(config->global_host_event_handler().substr(
                                    0,
                                    config->global_host_event_handler().find_first_of('!')));
    command_struct* temp_command(::find_command(temp_command_name.c_str()));
    if (!temp_command) {
      logger(log_verification_error, basic)
        << "Error: Global host event handler command '"
        << temp_command_name << "' is not defined anywhere!";
      ++config_errors;
    }

    // Save the pointer to the command for later.
    global_host_event_handler_ptr = temp_command;
  }
  if (!config->global_service_event_handler().empty()) {
    // Check the event handler command.
    std::string temp_command_name(config->global_service_event_handler().substr(
                                    0,
                                    config->global_service_event_handler().find_first_of('!')));
    command_struct* temp_command(::find_command(temp_command_name.c_str()));
    if (!temp_command) {
      logger(log_verification_error, basic)
        << "Error: Global service event handler command '"
        << temp_command_name << "' is not defined anywhere!";
      ++config_errors;
    }

    // Save the pointer to the command for later.
    global_service_event_handler_ptr = temp_command;
  }

  // Check obsessive processor commands...
  if (verify_config)
    logger(log_info_message, basic)
      << "Checking obsessive compulsive processor commands...";
  if (!config->ocsp_command().empty()) {
    std::string temp_command_name(config->ocsp_command().substr(
                                    0,
                                    config->ocsp_command().find_first_of('!')));
    command_struct* temp_command(::find_command(temp_command_name.c_str()));
    if (!temp_command) {
      logger(log_verification_error, basic)
        << "Error: Obsessive compulsive service processor command '"
        << temp_command_name << "' is not defined anywhere!";
      ++config_errors;
    }

    // Save the pointer to the command for later.
    ocsp_command_ptr = temp_command;
  }
  if (!config->ochp_command().empty()) {
    std::string temp_command_name(config->ochp_command().substr(
                                    0,
                                    config->ochp_command().find_first_of('!')));
    command_struct* temp_command(::find_command(temp_command_name.c_str()));
    if (!temp_command) {
      logger(log_verification_error, basic)
        << "Error: Obsessive compulsive host processor command '"
        << temp_command_name << "' is not defined anywhere!";
      ++config_errors;
    }

    // Save the pointer to the command for later.
    ochp_command_ptr = temp_command;
  }
}

/**
 *  @brief Apply configuration of a specific object type.
 *
 *  This method will perform a diff on cur_cfg and new_cfg to create the
 *  three element sets : added, modified and removed. The type applier
 *  will then be called to 1) remove old objects 2) create new objects
 *  3) modify existing objects, in this order.
 *
 *  @param[in] cur_cfg Current configuration set.
 *  @param[in] new_cfg New configuration set.
 */
template <typename ConfigurationType, typename ApplierType>
void applier::state::_apply(
       difference<std::set<shared_ptr<ConfigurationType> > > const& diff) {
  // Type alias.
  typedef std::set<shared_ptr<ConfigurationType> > cfg_set;

  /*
  ** Configuration application.
  */

  // Applier.
  ApplierType aplyr;

  // Erase objects.
  for (typename cfg_set::const_iterator
         it_delete(diff.deleted().begin()),
         end_delete(diff.deleted().end());
       it_delete != end_delete;
       ++it_delete) {
    if (!verify_config)
      aplyr.remove_object(*it_delete);
    else {
      try {
        aplyr.remove_object(*it_delete);
      }
      catch (std::exception const& e) {
        ++config_errors;
        logger(log_info_message, basic)
          << e.what();
      }
    }
  }

  // Add objects.
  for (typename cfg_set::const_iterator
         it_create(diff.added().begin()),
         end_create(diff.added().end());
       it_create != end_create;
       ++it_create) {
    if (!verify_config)
      aplyr.add_object(*it_create);
    else {
      try {
        aplyr.add_object(*it_create);
      }
      catch (std::exception const& e) {
        ++config_errors;
        logger(log_info_message, basic)
          << e.what();
      }
    }
  }

  // Modify objects.
  for (typename cfg_set::const_iterator
         it_modify(diff.modified().begin()),
         end_modify(diff.modified().end());
       it_modify != end_modify;
       ++it_modify) {
    if (!verify_config)
      aplyr.modify_object(*it_modify);
    else {
      try {
        aplyr.modify_object(*it_modify);
      }
      catch (std::exception const& e) {
        ++config_errors;
        logger(log_info_message, basic)
          << e.what();
      }
    }
  }

  return ;
}

/**
 *  Apply retention.
 *
 *  @param[in] new_cfg New configuration set.
 *  @param[in] state   The retention state to use.
 */
void applier::state::_apply(
       configuration::state& new_cfg,
       retention::state& state) {
  retention::applier::state app_state;
  if (!verify_config)
    app_state.apply(new_cfg, state);
  else {
    try {
      app_state.apply(new_cfg, state);
    }
    catch (std::exception const& e) {
      ++config_errors;
      logger(log_info_message, basic)
        << e.what();
    }
  }
}

/**
 *  Expand objects.
 *
 *  @param[in,out] new_state New configuration state.
 *  @param[in,out] cfg       Configuration objects.
 */
template <typename ConfigurationType, typename ApplierType>
void applier::state::_expand(
                       configuration::state& new_state,
                       std::set<shared_ptr<ConfigurationType> >& cfg) {
  ApplierType aplyr;
  for (typename std::set<shared_ptr<ConfigurationType> >::iterator
         it(cfg.begin()),
         end(cfg.end());
       it != end;) {
    typename std::set<shared_ptr<ConfigurationType> >::iterator
      to_expand(it++);
    if (!verify_config)
      aplyr.expand_object(*to_expand, new_state);
    else {
      try {
        aplyr.expand_object(*to_expand, new_state);
      }
      catch (std::exception const& e) {
        ++config_errors;
        logger(log_info_message, basic)
          << e.what();
      }
    }
  }
  return ;
}

/**
 *  Process new configuration and apply it.
 *
 *  @param[in] new_cfg        The new configuration.
 *  @param[in] waiting_thread True to wait thread after calulate differencies.
 *  @param[in] state          The retention to use.
 */
void applier::state::_processing(
       configuration::state& new_cfg,
       bool waiting_thread,
       retention::state* state) {
  // Timing.
  struct timeval tv[5];

  // Call prelauch broker event the first time to run applier state.
  if (!has_already_been_loaded)
    broker_program_state(
      NEBTYPE_PROCESS_PRELAUNCH,
      NEBFLAG_NONE,
      NEBATTR_NONE,
      NULL);

  //
  // Expand all objects.
  //
  gettimeofday(tv, NULL);

  // Expand timeperiods.
  _expand<configuration::timeperiod, applier::timeperiod>(
    new_cfg,
    new_cfg.timeperiods());

  // Expand connectors.
  _expand<configuration::connector, applier::connector>(
    new_cfg,
    new_cfg.connectors());

  // Expand commands.
  _expand<configuration::command, applier::command>(
    new_cfg,
    new_cfg.commands());

  // Expand hosts.
  _expand<configuration::host, applier::host>(
    new_cfg,
    new_cfg.hosts());

  // Expand services.
  _expand<configuration::service, applier::service>(
    new_cfg,
    new_cfg.services());

  // Expand hostdependencies.
  _expand<configuration::hostdependency, applier::hostdependency>(
    new_cfg,
    new_cfg.hostdependencies());

  // Expand servicedependencies.
  _expand<configuration::servicedependency, applier::servicedependency>(
    new_cfg,
    new_cfg.servicedependencies());

  //
  //  Build difference for all objects.
  //

  // Build difference for timeperiods.
  difference<set_timeperiod> diff_timeperiods;
  diff_timeperiods.parse(
    config->timeperiods(),
    new_cfg.timeperiods());

  // Build difference for connectors.
  difference<set_connector> diff_connectors;
  diff_connectors.parse(
    config->connectors(),
    new_cfg.connectors());

  // Build difference for commands.
  difference<set_command> diff_commands;
  diff_commands.parse(
    config->commands(),
    new_cfg.commands());

  // Build difference for hosts.
  difference<set_host> diff_hosts;
  diff_hosts.parse(
    config->hosts(),
    new_cfg.hosts());

  // Build difference for services.
  difference<set_service> diff_services;
  diff_services.parse(
    config->services(),
    new_cfg.services());

  // Build difference for hostdependencies.
  difference<set_hostdependency> diff_hostdependencies;
  diff_hostdependencies.parse(
    config->hostdependencies(),
    new_cfg.hostdependencies());

  // Build difference for servicedependencies.
  difference<set_servicedependency> diff_servicedependencies;
  diff_servicedependencies.parse(
    config->servicedependencies(),
    new_cfg.servicedependencies());

  // Timing.
  gettimeofday(tv + 1, NULL);

  if (waiting_thread && _processing_state == state_ready) {
    concurrency::locker lock(&_lock);
    _processing_state = state_waiting;
    // Wait to stop engine before apply configuration.
    _cv_lock.wait(&_lock);
    _processing_state = state_apply;
  }

  try {
    // Apply logging configurations.
    applier::logging::instance().apply(new_cfg);

    // Apply globals configurations.
    applier::globals::instance().apply(new_cfg);

    // Apply macros configurations.
    applier::macros::instance().apply(new_cfg);

    // Timing.
    gettimeofday(tv + 2, NULL);

    //
    //  Apply and resolve all objects.
    //

    // Apply timeperiods.
    _apply<configuration::timeperiod, applier::timeperiod>(
      diff_timeperiods);
    _resolve<configuration::timeperiod, applier::timeperiod>(
      config->timeperiods());

    // Apply connectors.
    _apply<configuration::connector, applier::connector>(
      diff_connectors);
    _resolve<configuration::connector, applier::connector>(
      config->connectors());

    // Apply commands.
    _apply<configuration::command, applier::command>(
      diff_commands);
    _resolve<configuration::command, applier::command>(
      config->commands());

    // Apply hosts.
    _apply<configuration::host, applier::host>(
      diff_hosts);

    // Apply services.
    _apply<configuration::service, applier::service>(
      diff_services);

    // Resolve hosts and services.
    _resolve<configuration::host, applier::host>(
      config->hosts());
    _resolve<configuration::service, applier::service>(
      config->services());

    // Apply host dependencies.
    _apply<configuration::hostdependency, applier::hostdependency>(
      diff_hostdependencies);
    _resolve<configuration::hostdependency, applier::hostdependency>(
      config->hostdependencies());

    // Apply service dependencies.
    _apply<configuration::servicedependency, applier::servicedependency>(
      diff_servicedependencies);
    _resolve<configuration::servicedependency, applier::servicedependency>(
      config->servicedependencies());

    // Load retention.
    if (state)
      _apply(new_cfg, *state);

    // Apply scheduler.
    if (!verify_config)
      applier::scheduler::instance().apply(
        new_cfg,
        diff_hosts,
        diff_services);

    // Apply new global on the current state.
    if (!verify_config)
      _apply(new_cfg);
    else {
      try {
        _apply(new_cfg);
      }
      catch (std::exception const& e) {
        ++config_errors;
        logger(log_info_message, basic)
          << e.what();
      }
    }

    // Timing.
    gettimeofday(tv + 3, NULL);

    // Check for circular paths between hosts.
    pre_flight_circular_check(&config_warnings, &config_errors);

    // Call start broker event the first time to run applier state.
    if (!has_already_been_loaded) {
      neb_load_all_modules();

      broker_program_state(
        NEBTYPE_PROCESS_START,
        NEBFLAG_NONE,
        NEBATTR_NONE,
        NULL);
    }

    // Timing.
    gettimeofday(tv + 4, NULL);
    if (test_scheduling) {
      double runtimes[5];
      runtimes[4] = 0.0;
      for (unsigned int i(0);
           i < (sizeof(runtimes) / sizeof(*runtimes) - 1);
           ++i) {
        runtimes[i] = tv[i + 1].tv_sec - tv[i].tv_sec
          + (tv[i + 1].tv_usec - tv[i].tv_usec) / 1000000.0;
        runtimes[4] += runtimes[i];
      }
      logger(log_info_message, basic)
        << "\nTiming information on configuration verification is listed below.\n\n"
        << "CONFIG VERIFICATION TIMES          (* = Potential for speedup with -x option)\n"
        << "----------------------------------\n"
        << "Template Resolutions: " << runtimes[0] << " sec\n"
        << "Object Relationships: " << runtimes[2] << " sec\n"
        << "Circular Paths:       " << runtimes[3] << " sec  *\n"
        << "Misc:                 " << runtimes[1] << " sec\n"
        << "                      ============\n"
        << "TOTAL:                " << runtimes[4]
        << " sec  * = " << runtimes[3] << " sec ("
        << (runtimes[3] * 100.0 / runtimes[4]) << "%) estimated savings\n";
    }
  }
  catch (...) {
    _processing_state = state_error;
    throw;
  }

  has_already_been_loaded = true;
  _processing_state = state_ready;
}

/**
 *  Resolve objects.
 *
 *  @param[in] cfg Configuration objects.
 */
template <typename ConfigurationType, typename ApplierType>
void applier::state::_resolve(
       std::set<shared_ptr<ConfigurationType> >& cfg) {
  ApplierType aplyr;
  for (typename std::set<shared_ptr<ConfigurationType> >::const_iterator
         it(cfg.begin()),
         end(cfg.end());
       it != end;
       ++it) {
    if (!verify_config)
      aplyr.resolve_object(*it);
    else {
      try {
        aplyr.resolve_object(*it);
      }
      catch (std::exception const& e) {
        ++config_errors;
        logger(log_info_message, basic)
          << e.what();
      }
    }
  }
  return ;
}
