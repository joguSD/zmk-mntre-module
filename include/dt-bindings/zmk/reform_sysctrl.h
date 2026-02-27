#pragma once

#define RSC_POWER_CMD 0x70  // 'p'
#define RSC_STATUS_CMD 0x73 // 's'
#define RSC_CHARGE_CMD 0x63 // 'c'
#define RSC_WAKE_CMD 0x77   // 'w'

#define RSC_POWER_OFF_ARG 0
#define RSC_POWER_ON_ARG 1

#define R_PWR_OFF RSC_POWER_CMD RSC_POWER_OFF_ARG
#define R_PWR_ON RSC_POWER_CMD RSC_POWER_ON_ARG
#define R_STATUS RSC_STATUS_CMD 0
#define R_BATT RSC_CHARGE_CMD 0
#define R_WAKE RSC_WAKE_CMD 1
