/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>

LOG_MODULE_REGISTER(main);

#define EVENT_DATA_AVAILABLE (1 << 0)
#define EVENT_TX_COMPLETE (1 << 1)

static const struct device *i2c = NULL;
static const struct device *uart = NULL;

RING_BUF_DECLARE(buffer, 512);
K_THREAD_STACK_DEFINE(uart_worker_stack, 512);
static struct k_thread uart_worker_thread;
static K_EVENT_DEFINE(event);

#if defined(CONFIG_UART_ASYNC_API)
static void uart_callback(const struct device *dev, struct uart_event *evt,
			  void *user_data) {
	if (evt->type != UART_TX_DONE)
		return;

	ring_buf_get_finish(&buffer, evt->data.tx.len);
	k_event_post(&event, EVENT_TX_COMPLETE);
}
#endif

static void uart_worker(void) {
	uint8_t *data;

	while (true) {
		k_event_wait(&event, EVENT_DATA_AVAILABLE, true, K_FOREVER);
		uint32_t len = ring_buf_get_claim(&buffer, &data, sizeof(_ring_buffer_data_buffer));
		if (len == 0)
			continue;
#if defined(CONFIG_UART_ASYNC_API)
		int ret = uart_tx(uart, data, len, SYS_FOREVER_US);
		if (ret < 0) {
			ring_buf_get_finish(&buffer, len);
			continue;
		}
		k_event_wait(&event, EVENT_TX_COMPLETE, true, K_FOREVER);
#else
		for (uint32_t i = 0; i < len; i++)
			uart_poll_out(uart, data[i]);
		ring_buf_get_finish(&buffer, len);
#endif
	}
}

static int write_requested(struct i2c_target_config *config) {
	return 0;
}

static int write_received(struct i2c_target_config *config, uint8_t val) {
static uint8_t lf = '\r';

	if (ring_buf_put(&buffer, &val, 1) < 0) {
		LOG_ERR("ring buffer is full");
		return 0;
	}
	if (val == '\n')
		ring_buf_put(&buffer, &lf, 1);
	k_event_post(&event, EVENT_DATA_AVAILABLE);
	return 0;
}

static int read_requested(struct i2c_target_config *config, uint8_t *val) {
	return -1;
}

static int read_processed(struct i2c_target_config *config, uint8_t *val) {
	return -1;
}

static int stop(struct i2c_target_config *config) {
	return 0;
}

static const struct i2c_target_callbacks callbacks = {
	.write_requested = write_requested,
	.write_received = write_received,
	.read_requested = read_requested,
	.read_processed = read_processed,
	.stop = stop,
};


static struct i2c_target_config cfg = {
	.flags = 0,
	.address = 0x76,
	.callbacks = &callbacks,
};

static const struct uart_config uart_config = {
	.baudrate = 115200,
	.data_bits = UART_CFG_DATA_BITS_8,
	.stop_bits = UART_CFG_STOP_BITS_1,
	.parity = UART_CFG_PARITY_NONE,
	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
};

int main(void)
{
	int ret;

	i2c = DEVICE_DT_GET(DT_CHOSEN(dasharo_debug_i2c));
	uart = DEVICE_DT_GET(DT_CHOSEN(dasharo_output_uart));

	if (!device_is_ready(i2c)) {
		LOG_ERR("i2c controller not ready");
		return 1;
	}

	if (!device_is_ready(uart)) {
		LOG_ERR("uart not ready");
		return 1;
	}

	ret = uart_configure(uart, &uart_config);
	if (ret < 0) {
		LOG_ERR("uart_configure failed: %d", ret);
		return 1;
	}
	
#if defined(CONFIG_UART_ASYNC_API)
	ret = uart_callback_set(uart, uart_callback, NULL);
	if (ret < 0) {
		LOG_ERR("uart_callback_set failed: %d", ret);
		return 1;
	}
#endif

	ret = i2c_target_register(i2c, &cfg);
	if (ret < 0) {
		LOG_ERR("i2c_target_register failed: %d", ret);
		return 1;
	}

	k_thread_create(&uart_worker_thread, uart_worker_stack,
		sizeof(uart_worker_stack), (k_thread_entry_t)uart_worker, NULL,
		NULL, NULL, 5, 0, K_NO_WAIT);

	LOG_INF("startup ok");

	return 0;
}
