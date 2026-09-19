/**
 ******************************************************************************
 * @file    boomlink_storage_port.c
 ******************************************************************************
 */
#include "boomlink_storage_port.h"

/* Deliberately no #include of boomlink_config_store.h here: unlike
   linkengine/boomlink_port.c reaching "down" into boomlink_linkframe.h (a
   module linkengine depends on but which never depends back on it), that
   header depends on THIS one (it embeds boomlink_storage_port_t) - including
   it back from here would make the two libraries include each other. This
   file only validates what is generic to any port, not the config-store
   wrapper's own format constants; boomlink_config_store.c checks its
   format-specific minimums (e.g. region_size large enough for its header)
   itself, against the port it was actually given. */

bool boomlink_storage_port_is_valid(const boomlink_storage_port_t *port) {
  return port != NULL && port->erase != NULL && port->write != NULL && port->read != NULL &&
         port->region_size > 0u && port->write_granularity > 0u &&
         (port->region_size % port->write_granularity) == 0u;
}
