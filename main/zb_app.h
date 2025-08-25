#pragma once

#include "zb_config.h"

void appzb_init();
bool appzb_is_connected();
void appzb_wait_until_connected();
void appzb_factory_reset();