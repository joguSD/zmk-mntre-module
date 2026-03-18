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

static void battery_render_callback(uint8_t *buffer) {
  matrix_clear(&character_matrix);
  matrix_write_P(&character_matrix, battery_response);
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
    if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
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
    // res_buf will have response
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
  int ret = reform_sysctrl_cmd("s\r", status_response);
  if (ret) {
    display_request_render(status_render_callback);
  }
}

static void battery_work_handler(struct k_work *work) {
  int ret = reform_sysctrl_cmd("c\r", battery_response);
  if (ret) {
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
