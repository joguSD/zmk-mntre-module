/*
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright 2026 joguSD
*/
#include <string.h>

#include <zephyr/app_version.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <reform/display.h>
#include <reform/matrix.h>

LOG_MODULE_DECLARE(mnt, CONFIG_MNT_LOG_LEVEL);

#define REFORM_SYSCTRL_MSG_SIZE 128
#define REFORM_SYSCTRL_THREAD_PRIORITY 3
#define REFORM_SYSCTRL_THREAD_STACK_SIZE 2048

#define UART_DEVICE_NODE DT_CHOSEN(mnt_reform_sysctrl)

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, REFORM_SYSCTRL_MSG_SIZE, 10, 4);

K_THREAD_STACK_DEFINE(reform_sysctrl_work_stack_area,
                      REFORM_SYSCTRL_THREAD_STACK_SIZE);

static struct k_work_q reform_sysctrl_work_q;

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

static char rx_buf[REFORM_SYSCTRL_MSG_SIZE];
static int rx_buf_pos;

static struct character_matrix character_matrix;
static char status_response[REFORM_SYSCTRL_MSG_SIZE];
static char battery_response[REFORM_SYSCTRL_MSG_SIZE];

static void status_render_callback(uint8_t *buffer) {
  matrix_clear(&character_matrix);
  matrix_write_P(&character_matrix, status_response);
  matrix_write_P(&character_matrix, "\nmnt ");
  matrix_write_P(&character_matrix, MNT_GIT_SHA);
  matrix_write_P(&character_matrix, "\nzmk ");
  matrix_write_P(&character_matrix, ZMK_GIT_SHA);
  matrix_write_P(&character_matrix, "\nzephyr ");
  matrix_write_P(&character_matrix, ZEPHYR_GIT_SHA);
  matrix_render(&character_matrix, buffer, -1);
}

#define BAT_ICON_BASE 128 // glyph index 4*32
#define BAT_ICON_EMPTY 0
#define BAT_ICON_25 2
#define BAT_ICON_50 4
#define BAT_ICON_75 6
#define BAT_ICON_FULL 8

static void insert_bat_icon(char *str, int x, double v) {
  uint8_t level = BAT_ICON_EMPTY;
  if (v >= 3.3) {
    level = BAT_ICON_FULL;
  } else if (v >= 3.1) {
    level = BAT_ICON_75;
  } else if (v >= 3.0) {
    level = BAT_ICON_50;
  } else if (v >= 2.9) {
    level = BAT_ICON_25;
  }
  str[x] = BAT_ICON_BASE + level;
  str[x + 1] = BAT_ICON_BASE + level + 1;
}

// Parse sysctrl 'c' response and render 4-row battery display with icons
// Response format: "39 39 39 39 39 39 39 39 mA-1234mV12345 069% P1"
//                   0  3  6  9  12 15 18 21 24 26    33    39   44
static void battery_render_callback(uint8_t *buffer) {
  char *r = battery_response;
  double voltages[8];

  for (int i = 0; i < 8; i++) {
    voltages[i] = ((r[i * 3] - '0') * 10 + (r[i * 3 + 1] - '0')) / 10.0;
    if (voltages[i] < 0)
      voltages[i] = 0;
    if (voltages[i] >= 10)
      voltages[i] = 9.9;
  }

  int amps_offset = 3 * 8 + 2;
  int amps_sign = 1;
  int ai = amps_offset;
  if (r[ai] == '-') {
    amps_sign = -1;
    ai++;
  }
  int amps_raw = 0;
  while (ai < amps_offset + 5 && r[ai] >= '0' && r[ai] <= '9') {
    amps_raw = amps_raw * 10 + (r[ai++] - '0');
  }
  double bat_amps = (amps_sign * amps_raw) / 1000.0;

  int volts_offset = amps_offset + 5 + 2;
  int volts_raw = 0;
  for (int i = volts_offset; i < volts_offset + 5; i++) {
    if (r[i] >= '0' && r[i] <= '9')
      volts_raw = volts_raw * 10 + (r[i] - '0');
  }
  double bat_volts = volts_raw / 1000.0;

  int gauge_offset = volts_offset + 5 + 1;
  char bat_gauge[5];
  memcpy(bat_gauge, &r[gauge_offset], 4);
  bat_gauge[4] = '\0';

  int syspower_offset = gauge_offset + 5;
  const char *power_str = "   ";
  if (r[syspower_offset + 1] == '1')
    power_str = " On";
  else if (r[syspower_offset + 1] == '0')
    power_str = "Off";

  matrix_clear(&character_matrix);
  char str[22];

  // [##] 3.9  [##] 3.9   069%
  snprintf(str, 22, "[] %.1f  [] %.1f   %s", voltages[0], voltages[4],
           bat_gauge);
  insert_bat_icon(str, 0, voltages[0]);
  insert_bat_icon(str, 8, voltages[4]);
  matrix_poke_str(&character_matrix, 0, 0, str);

  // [##] 3.9  [##] 3.9     On
  snprintf(str, 22, "[] %.1f  [] %.1f    %s", voltages[1], voltages[5],
           power_str);
  insert_bat_icon(str, 0, voltages[1]);
  insert_bat_icon(str, 8, voltages[5]);
  matrix_poke_str(&character_matrix, 0, 1, str);

  // [##] 3.9  [##] 3.9  0.256A
  if (bat_amps >= 0)
    snprintf(str, 22, "[] %.1f  [] %.1f %2.3fA", voltages[2], voltages[6],
             bat_amps);
  else
    snprintf(str, 22, "[] %.1f  [] %.1f %2.2fA", voltages[2], voltages[6],
             bat_amps);
  insert_bat_icon(str, 0, voltages[2]);
  insert_bat_icon(str, 8, voltages[6]);
  matrix_poke_str(&character_matrix, 0, 2, str);

  // [##] 3.9  [##] 3.9  12.34V
  snprintf(str, 22, "[] %.1f  [] %.1f %2.2fV", voltages[3], voltages[7],
           bat_volts);
  insert_bat_icon(str, 0, voltages[3]);
  insert_bat_icon(str, 8, voltages[7]);
  matrix_poke_str(&character_matrix, 0, 3, str);

  matrix_render(&character_matrix, buffer, -1);
}

static void error_render_callback(uint8_t *buffer) {
  matrix_clear(&character_matrix);
  matrix_write_P(&character_matrix, "sysctrl timeout!");
  matrix_render(&character_matrix, buffer, -1);
}

void serial_cb(const struct device *dev, void *user_data) {
  uint8_t c;

  if (!uart_irq_update(uart_dev)) {
    return;
  }

  if (!uart_irq_rx_ready(uart_dev)) {
    return;
  }

  /* read until FIFO empty */
  while (uart_fifo_read(uart_dev, &c, 1) == 1) {
    if (c == '\n') {
      continue;
    }
    if (c == '\r' && rx_buf_pos > 0) {
      /* terminate string */
      rx_buf[rx_buf_pos] = '\0';

      /* if queue is full, message is silently dropped */
      k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

      /* reset the buffer (it was copied to the msgq) */
      rx_buf_pos = 0;
    } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
      rx_buf[rx_buf_pos++] = c;
    }
    /* else: characters beyond buffer size are dropped */
  }
}

/*
 * Print a null-terminated string character by character to the UART interface
 */
void print_uart(const char *buf) {
  int msg_len = strlen(buf);

  for (int i = 0; i < msg_len; i++) {
    uart_poll_out(uart_dev, buf[i]);
  }
}

void initialize_reform_sysctrl(struct k_work *work) {
  if (!device_is_ready(uart_dev)) {
    LOG_ERR("Failed to find sysctrl uart device");
    return;
  }

  /* configure interrupt and callback to receive data */
  int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
  if (ret < 0) {
    if (ret == -ENOTSUP) {
      LOG_ERR("Interrupt-driven UART API support not enabled\n");
    } else if (ret == -ENOSYS) {
      LOG_ERR("UART device does not support interrupt-driven API\n");
    } else {
      LOG_ERR("Error setting UART callback: %d\n", ret);
    }
    return;
  }

  uart_irq_rx_enable(uart_dev);

  LOG_INF("Reform System Controller Initialized!");
}

K_WORK_DEFINE(reform_sysctrl_init_work, initialize_reform_sysctrl);

static int reform_sysctrl_init(void) {
  k_work_queue_start(&reform_sysctrl_work_q, reform_sysctrl_work_stack_area,
                     K_THREAD_STACK_SIZEOF(reform_sysctrl_work_stack_area),
                     REFORM_SYSCTRL_THREAD_PRIORITY, NULL);

  k_work_submit_to_queue(&reform_sysctrl_work_q, &reform_sysctrl_init_work);

  LOG_INF("Reform System Controller Thread Initialized!");
  return 0;
}

SYS_INIT(reform_sysctrl_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static void reform_sysctrl_flush() {
  char discard[REFORM_SYSCTRL_MSG_SIZE];
  while (k_msgq_get(&uart_msgq, discard, K_NO_WAIT) == 0) {
  }
}

static int reform_sysctrl_cmd(const char *cmd, char *res_buf) {
  reform_sysctrl_flush();
  print_uart(cmd);
  if (k_msgq_get(&uart_msgq, res_buf, K_MSEC(200)) == 0) {
    return 1;
  }

  LOG_WRN("Sysctrl command timed out: %s", cmd);
  display_request_render(error_render_callback);
  return 0;
}

static void power_on_work_handler(struct k_work *work) {
  char res_buf[REFORM_SYSCTRL_MSG_SIZE];
  reform_sysctrl_cmd("1p\r", res_buf);
}

static void power_off_work_handler(struct k_work *work) {
  char res_buf[REFORM_SYSCTRL_MSG_SIZE];
  reform_sysctrl_cmd("0p\r", res_buf);
}

static void status_work_handler(struct k_work *work) {
#ifndef CONFIG_MNT_REFORM_STANDALONE
  int ret = reform_sysctrl_cmd("s\r", status_response);
  if (ret) {
    display_request_render(status_render_callback);
  }
#else
  strncpy(status_response, "STANDALONE", REFORM_SYSCTRL_MSG_SIZE);
  display_request_render(status_render_callback);
#endif
}

static void battery_work_handler(struct k_work *work) {
  int ret = reform_sysctrl_cmd("c\r", battery_response);
  if (ret) {
    LOG_DBG("battery response: %s", battery_response);
    display_request_render(battery_render_callback);
  }
}

static void wake_work_handler(struct k_work *work) {
  char res_buf[REFORM_SYSCTRL_MSG_SIZE];
  reform_sysctrl_cmd("1w\r", res_buf);
}

#define SYSCTRL_CMD(sysctrl_cmd)                                               \
  K_WORK_DEFINE(sysctrl_cmd##_work, sysctrl_cmd##_work_handler);               \
  void reform_sysctrl_##sysctrl_cmd() {                                        \
    k_work_submit_to_queue(&reform_sysctrl_work_q, &sysctrl_cmd##_work);       \
  }

SYSCTRL_CMD(power_on);
SYSCTRL_CMD(power_off);
SYSCTRL_CMD(status);
SYSCTRL_CMD(battery);
SYSCTRL_CMD(wake);
