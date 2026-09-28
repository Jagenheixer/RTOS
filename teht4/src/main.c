 /*
 * TEHTÄVÄ 5 - LIIKENNEVALO-OHJELMA & YKSIKKÖTESTAUS
 * 
 * TAVOITELTAVA PISTEMÄÄRÄ: 3p / 4p
 * 
 * PERUSTELUT:
 * 1. Perussuoritus (1p):
 *    - TimeParser-kirjasto rakennettu ja yksikkötestattu GoogleTestillä.
 *    - Mukana raja-arvotestejä (Hours 0-23, Minutes 0-59, Seconds 0-59).
 *    - Integroitu RTOS-liikennevalokoodiin: UARTista saatu aikamerkkijono 
 *      parsitaan ja käynnistetään k_timer-ajastin saadun sekuntimäärän päähän.
 * 
 * 2. Lisätestikeissit (+1p):
 *    - GoogleTestissä toteutettu 4 lisätestiä:
 *      * Tasan 6 merkin pituustarkistus (InvalidLength)
 *      * Numeerisuustarkistus (NonDigitCharacters)
 *      * NULL-osoittimen tarkistus (NullPointerCheck)
 *      * Nollasijainnin hylkäys (ZeroTimeNotAllowed)
 *    - Jokaiselle määritetty omat negatiiviset virhekoodit.
 * 
 * 3. Oma lisäominaisuus (+1p):
 *    - UART Security Lockout & virhelaskuri:
 *    - 3 peräkkäisestä virheellisestä syötteestä UART-syöte lukitaan 10 sekunniksi.
 *    - Hyväksytty validi aika nollaa virhelaskurin takaisin 0/3:een.
 */
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/__assert.h>
#include <stdio.h>
#include <stdarg.h>
#include "TimeParser.h"

#define STACKSIZE 1024
#define PRIO_LIGHTS 5
#define PRIO_UART 6
#define PRIO_DEBUG 10

#define RX_BUF_SIZE 16

static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_yellow = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

static volatile bool debug_enabled = true;
static uint32_t total_sequences_run;
static uint32_t total_uart_commands;

// --- VIIKKOTEHTÄVÄ 5: LUKITUSMUUTTUJAT & AJASTIN ---
static int failed_attempts = 0;
static int64_t lockout_until = 0;

void timer_expiry_function(struct k_timer *timer_id)
{
	printk("\n[AJASTINKESKEYTYS] k_timer aika kulu loppuun\n");
}

K_TIMER_DEFINE(my_timer, timer_expiry_function, NULL);

// --- DEBUG-VIESTIEN FIFO ---
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

// --- VIIKKOTEHTÄVÄ 5: AIKASYÖTE TARKISTUS & RATE LIMITING ---
static void process_uart_time_input(char *input_str)
{
	int64_t now = k_uptime_get();

	// 1. Tarkistus onko lukitustila päällä (10 sekunnin rangaistus)
	if (now < lockout_until) {
		int left_sec = (int)((lockout_until - now) / 1000) + 1;
		send_debug_msg("[SECURITY ALERT] Jarjestelma lukittu Odota %d sekuntia.", left_sec);
		return;
	}

	// 2. TDD-parsinta
	int parsed_seconds = time_parse(input_str);

	if (parsed_seconds < 0) {
		failed_attempts++;
		send_debug_msg("[ERROR] Virheellinen syote (%s), virhekoodi: %d. Virheet: %d/3",
			       input_str, parsed_seconds, failed_attempts);

		if (failed_attempts >= 3) {
			lockout_until = k_uptime_get() + 10000; // 10s lukitus
			failed_attempts = 0;
			send_debug_msg("[SECURITY LOCKOUT] 3 virheellista syotetta! UART LUKITTU 10s.");
		}
	} else {
		failed_attempts = 0; // Nollataan virhelaskuri onnistuneen syötteen jälkeen
		send_debug_msg("[SUCCESS] Validiaika: %d sekuntia.", parsed_seconds);
		send_debug_msg("Kaynnistetaan k_timer %d sekunnin paahan...", parsed_seconds);

		// Käynnistetään Zephyr k_timer
		k_timer_start(&my_timer, K_SECONDS(parsed_seconds), K_NO_WAIT);
	}
}

// --- LIIKENNEVALO-TASKI ---
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

// --- UART-TASKI (Kokoaa syötemerkkijonon puskuriin) ---
static void uart_rx_task(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	__ASSERT(device_is_ready(uart_dev), "UART-laite ei ole valmis");

	char rx_buf[RX_BUF_SIZE];
	int rx_idx = 0;

	while (1) {
		unsigned char recv_char;

		if (uart_poll_in(uart_dev, &recv_char) == 0) {
			// Kun painetaan Enteriä (\r tai \n)
			if (recv_char == '\r' || recv_char == '\n') {
				if (rx_idx > 0) {
					rx_buf[rx_idx] = '\0'; // Päätetään merkkijono

					total_uart_commands++;

					// Tarkistetaan onko kyseessä 'D'-komento vai aikasymboli
					if ((rx_idx == 1) && (rx_buf[0] == 'D' || rx_buf[0] == 'd')) {
						debug_enabled = !debug_enabled;
						printk("DEBUG TILA: %s\n", debug_enabled ? "PAALLA" : "POIS");
					} else {
						process_uart_time_input(rx_buf);
					}

					rx_idx = 0; // Nollataan puskuri seuraavaa syötettä varten
				}
				continue;
			}

			// Kerätään merkki puskuriin jos mahtuu
			if (rx_idx < (RX_BUF_SIZE - 1)) {
				rx_buf[rx_idx++] = recv_char;
			}
		}
		k_msleep(10);
	}
}

K_THREAD_DEFINE(uart_tid, STACKSIZE, uart_rx_task, NULL, NULL, NULL, PRIO_UART, 0, 0);

int main(void)
{
	printk("Liikennevalojen ohjausohjelma + TimeParser V5\n");
	printk("Syota 'D' debug-tilan vaihtamiseksi tai aika muodossa HHMMSS (esim. 000005).\n");
	return 0;
}