/*
** Copyright 2011-2012 Merethis
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

#ifndef CCE_MOD_WS_SERVER_SCHEDULE_OBJECT_HH
#  define CCE_MOD_WS_SERVER_SCHEDULE_OBJECT_HH

#  include "com/centreon/engine/objects/host.hh"
#  include "com/centreon/engine/objects/service.hh"

namespace       com {
  namespace     centreon {
    namespace   engine {
      namespace modules {
        void    schedule_host(host* hst);
        void    schedule_service(service* svc);
      }
    }
  }
}

#endif // !CCE_MOD_WS_SERVER_SCHEDULE_OBJECT_HH