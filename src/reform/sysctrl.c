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
#define REFORM_SYSCTRL_THREAD_STACK_SIZE 2046

#define UART_DEVICE_NODE DT_CHOSEN(mnt_reform_sysctrl)

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, REFORM_SYSCTRL_MSG_SIZE, 10, 4);

K_THREAD_STACK_DEFINE(reform_sysctrl_work_stack_area,
                      REFORM_SYSCTRL_THREAD_STACK_SIZE);

static struct k_work_q reform_sysctrl_work_q;

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

static char rx_buf[REFORM_SYSCTRL_MSG_SIZE];
static int rx_buf_pos;

// TODO display logic doesn't feel right here
static struct character_matrix character_matrix;

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

// public api for sysctrl
int reform_sysctrl_cmd(const char *cmd, char *res_buf) {
  // TODO do this properly (sysctrl thread, timeouts)
  print_uart(cmd);
  while (k_msgq_get(&uart_msgq, res_buf, K_FOREVER) == 0) {
    // res_buf will have response
    return 1;
  }

  return 0;
}

int reform_sysctrl_power_on() {
  char res_buf[REFORM_SYSCTRL_MSG_SIZE];
  return reform_sysctrl_cmd("1p\r", res_buf);
}

int reform_sysctrl_power_off() {
  char res_buf[REFORM_SYSCTRL_MSG_SIZE];
  return reform_sysctrl_cmd("0p\r", res_buf);
}

int reform_sysctrl_status() {
  char res_buf[REFORM_SYSCTRL_MSG_SIZE];
  int ret = reform_sysctrl_cmd("s\r", res_buf);
  if (ret) {
    matrix_clear(&character_matrix);
    matrix_write_P(&character_matrix, res_buf);
    matrix_write_P(&character_matrix, "\nmnt ");
    matrix_write_P(&character_matrix, MNT_GIT_SHA);
    matrix_write_P(&character_matrix, "\nzmk ");
    matrix_write_P(&character_matrix, ZMK_GIT_SHA);
    matrix_write_P(&character_matrix, "\nzephyr ");
    matrix_write_P(&character_matrix, ZEPHYR_GIT_SHA);
    matrix_render(&character_matrix, buffer_get(), -1);
    buffer_show();
  }
  return ret;
}

int reform_sysctrl_battery() {
  char res_buf[REFORM_SYSCTRL_MSG_SIZE];
  int ret = reform_sysctrl_cmd("c\r", res_buf);
  if (ret) {
    matrix_clear(&character_matrix);
    matrix_write_P(&character_matrix, res_buf);
    matrix_render(&character_matrix, buffer_get(), -1);
    buffer_show();
  }
  return ret;
}

int reform_sysctrl_wake() {
  char res_buf[REFORM_SYSCTRL_MSG_SIZE];
  return reform_sysctrl_cmd("1w\r", res_buf);
}
