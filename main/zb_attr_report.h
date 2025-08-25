#pragma once

#include "zb_config.h"

void zbattr_send_attribute_report(uint16_t cluster_id, uint16_t attr_id, void *value);