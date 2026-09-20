/*
 * TAVOITELTU PISTEMAARA: 3 / 3 p (Viikkotehtava 4)
 *
 * PERUSTELUT (Toteutettu 5/5 lisapisteen vaatimusta):
 * 1. Ajoitukset liikennevaloihin (+1p):
 *    - Mitattu jokaisen valovaiheen suoritusaika us-tarkkuudella.
 *    - Mitattu ja raportoitu koko sekvenssin kokonaisaika.
 * 2. Erillinen Debug-taski (+1p):
 *    - Muiden taskien printk-kutsut korvattu k_fifo-puskurilla.
 *    - Debug-taski lukee FIFOa ja tulostaa viestit konsoliin.
 * 3. Lisaa ajoitustietoja / laskureita (+1p).
 * 4. Debugin asetus paalle/pois 'D'-komennolla (+1p).
 * 5. Assert-tarkistukset (+1p).
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/__assert.h>
#include <stdio.h>
#include <stdarg.h>

#define STACKSIZE 1024
#define PRIO_LIGHTS 5
#define PRIO_UART 6
#define PRIO_DEBUG 10

static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_yellow = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

static volatile bool debug_enabled = true;
static uint32_t total_sequences_run;
static uint32_t total_uart_commands;

struct debug_msg_t {
        void *fifo_reserved;
        char msg[128];
};

K_FIFO_DEFINE(debug_fifo);

static void send_debug_msg(const char *fmt, ...)
{
        if (!debug_enabled) {
                return;
        }

        struct debug_msg_t *msg = k_malloc(sizeof(*msg));
        __ASSERT(msg != NULL, "Debug-viestin muistinvaraus epaonnistui");

        if (msg != NULL) {
                va_list args;
                va_start(args, fmt);
                vsnprintf(msg->msg, sizeof(msg->msg), fmt, args);
                va_end(args);
                k_fifo_put(&debug_fifo, msg);
        }
}

static void debug_task(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1);
        ARG_UNUSED(p2);
        ARG_UNUSED(p3);

        while (1) {
                struct debug_msg_t *rx_msg = k_fifo_get(&debug_fifo, K_FOREVER);
                if (rx_msg != NULL) {
                        printk("[DEBUG] %s\n", rx_msg->msg);
                        k_free(rx_msg);
                }
        }
}

K_THREAD_DEFINE(debug_tid, STACKSIZE, debug_task, NULL, NULL, NULL, PRIO_DEBUG, 0, 0);

static void traffic_light_task(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1);
        ARG_UNUSED(p2);
        ARG_UNUSED(p3);

        __ASSERT(gpio_is_ready_dt(&led_red), "Punainen LED ei ole valmis");
        __ASSERT(gpio_is_ready_dt(&led_yellow), "Keltainen LED ei ole valmis");
        __ASSERT(gpio_is_ready_dt(&led_green), "Vihrea LED ei ole valmis");

        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_yellow, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);

        while (1) {
                uint32_t start_cycles;
                uint32_t end_cycles;
                uint32_t red_us;
                uint32_t yellow_us;
                uint32_t green_us;
                uint32_t total_us;

                total_sequences_run++;

                start_cycles = k_cycle_get_32();
                gpio_pin_set_dt(&led_red, 1);
                k_msleep(1000);
                gpio_pin_set_dt(&led_red, 0);
                end_cycles = k_cycle_get_32();
                red_us = k_cyc_to_us_near32(end_cycles - start_cycles);

                start_cycles = k_cycle_get_32();
                gpio_pin_set_dt(&led_yellow, 1);
                k_msleep(500);
                gpio_pin_set_dt(&led_yellow, 0);
                end_cycles = k_cycle_get_32();
                yellow_us = k_cyc_to_us_near32(end_cycles - start_cycles);

                start_cycles = k_cycle_get_32();
                gpio_pin_set_dt(&led_green, 1);
                k_msleep(1000);
                gpio_pin_set_dt(&led_green, 0);
                end_cycles = k_cycle_get_32();
                green_us = k_cyc_to_us_near32(end_cycles - start_cycles);

                total_us = red_us + yellow_us + green_us;
                __ASSERT(total_us > 0, "Sekvenssin suoritusaika on virheellinen");

                send_debug_msg("--- Sekvenssi #%u ---", total_sequences_run);
                send_debug_msg("  Punainen valo:  %u us", red_us);
                send_debug_msg("  Keltainen valo: %u us", yellow_us);
                send_debug_msg("  Vihrea valo:    %u us", green_us);
                send_debug_msg("  KOKONAISAISAIKA: %u us", total_us);
                send_debug_msg("  UART-komentoja: %u", total_uart_commands);

                k_msleep(2000);
        }
}

K_THREAD_DEFINE(light_tid, STACKSIZE, traffic_light_task, NULL, NULL, NULL, PRIO_LIGHTS, 0, 0);

static void uart_rx_task(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1);
        ARG_UNUSED(p2);
        ARG_UNUSED(p3);

        __ASSERT(device_is_ready(uart_dev), "UART-laite ei ole valmis");

        while (1) {
                unsigned char recv_char;

                if (uart_poll_in(uart_dev, &recv_char) == 0) {
                        // Hypataan Enter- / rivinvaihtomerkkien (\r ja \n) yli
                        if (recv_char == '\r' || recv_char == '\n') {
                                continue;
                        }

                        // Tarkistetaan vain tulostettavat ASCII-merkit (32-126)
                        __ASSERT(recv_char >= 32 && recv_char <= 126,
                                 "Luettu merkki ei ole validi ASCII-merkki");

                        total_uart_commands++;

                        if (recv_char == 'D' || recv_char == 'd') {
                                debug_enabled = !debug_enabled;
                                printk("DEBUG TILA: %s\n", debug_enabled ? "PAALLA" : "POIS");
                        }
                }
                k_msleep(100);
        }
}

K_THREAD_DEFINE(uart_tid, STACKSIZE, uart_rx_task, NULL, NULL, NULL, PRIO_UART, 0, 0);

int main(void)
{
        printk("Liikennevalojen ohjausohjelma\n");
        printk("Syota D tai d debug-tilan vaihtamiseksi.\n");
        return 0;
}
