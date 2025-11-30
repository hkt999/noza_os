#pragma once

#include <stdint.h>
#include "noza_irq_defs.h"

int irq_service_subscribe(uint32_t irq_id);
int irq_service_unsubscribe(uint32_t irq_id);
