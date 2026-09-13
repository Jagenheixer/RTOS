/*
 * TAVOITELTU PISTEMÄÄRÄ: 4 / 4 p
 * 
 * PERUSTELUT:
 * 1p - UART-vastaanotto & FIFO-puskuri dispatcherille.
 * +1p - Sekvenssin ajastus (esim. R,1000 / Y,500 / G,1000).
 * +1p - Refaktorointi (valotaski toimii ilman superlooppeja semaforeilla).
 * +1p - Sekvenssin toisto ('T').
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <stdlib.h>
#include <string.h>

#define STACKSIZE 1024
#define PRIORITY 5
#define MSG_LEN 20

// Ledit (nRF5340 Audio DK)
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// Sarjaportti
static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));

// FIFO-puskuri UART -> Dispatcher
K_FIFO_DEFINE(dispatcher_fifo);

struct data_t {
    void *fifo_reserved;
    char msg[MSG_LEN];
};

// Synkronointi-semaforit
static struct k_sem light_sem;
static struct k_sem release_sem;

// Nykyisen valotaskin tiedot
static char current_color = 'R';
static int current_time_ms = 1000;

// Tallennetut komennot toistoa ('T') varten
static char saved_sequence[50][MSG_LEN];
static int saved_cmd_count = 0;

// --- VALOTASKI ---
void light_task(void *p1, void *p2, void *p3) {
    while (1) {
        // Odotetaan komentoa dispatcheriltä
        k_sem_take(&light_sem, K_FOREVER);

        // Sytytetään pyydetty valo
        if (current_color == 'R' || current_color == 'r') {
            gpio_pin_set_dt(&led_red, 1);
            gpio_pin_set_dt(&led_green, 0);
        } else if (current_color == 'G' || current_color == 'g') {
            gpio_pin_set_dt(&led_red, 0);
            gpio_pin_set_dt(&led_green, 1);
        } else if (current_color == 'Y' || current_color == 'y') {
            gpio_pin_set_dt(&led_red, 1);
            gpio_pin_set_dt(&led_green, 1);
        }

        // Pidetään valo päällä haluttu aika
        k_msleep(current_time_ms);

        // Sammutetaan valot
        gpio_pin_set_dt(&led_red, 0);
        gpio_pin_set_dt(&led_green, 0);

        // Kuitataan dispatcherille että suoritus on valmis
        k_sem_give(&release_sem);
    }
}

// Suoritetaan yksi komento (esim. R,1000)
void execute_command(const char *cmd) {
    current_color = cmd[0];
    current_time_ms = 1000; // Oletusaika 1s

    // Parsitaan aika jos se on annettu (esim. R,500)
    const char *comma = strchr(cmd, ',');
    if (comma != NULL) {
        current_time_ms = atoi(comma + 1);
        if (current_time_ms <= 0) current_time_ms = 1000;
    }

    printk("Suoritetaan: %c, %d ms\n", current_color, current_time_ms);
    
    k_sem_give(&light_sem);             // Käynnistetään valotaski
    k_sem_take(&release_sem, K_FOREVER); // Odotetaan että valotaski valmistuu
}

// --- DISPATCHER TASK ---
static void dispatcher_task(void *p1, void *p2, void *p3) {
    while (true) {
        // Luetaan FIFO-puskuria
        struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
        char cmd[MSG_LEN];
        strncpy(cmd, rec_item->msg, MSG_LEN);
        k_free(rec_item);

        // Jos saatiin 'T', toistetaan kaikki tallennetut komennot
        if (cmd[0] == 'T' || cmd[0] == 't') {
            printk("Toistetaan sekvenssi...\n");
            for (int i = 0; i < saved_cmd_count; i++) {
                execute_command(saved_sequence[i]);
            }
        } else {
            // Tallennetaan muistiin ja suoritetaan
            if (saved_cmd_count < 50) {
                strncpy(saved_sequence[saved_cmd_count++], cmd, MSG_LEN);
            }
            execute_command(cmd);
        }
    }
}

// --- UART TASK ---
static void uart_task(void *p1, void *p2, void *p3) {
    char rc = 0;
    char msg[MSG_LEN];
    int cnt = 0;
    memset(msg, 0, MSG_LEN);

    while (true) {
        // Luetaan merkkejä sarjaportista
        if (uart_poll_in(uart_dev, &rc) == 0) {
            if (rc != '\r' && rc != '\n') {
                if (cnt < MSG_LEN - 1) msg[cnt++] = rc;
            } else if (cnt > 0) {
                msg[cnt] = '\0';

                // Lähetetään merkkijono FIFO-puskuriin
                struct data_t *buf = k_malloc(sizeof(struct data_t));
                if (buf != NULL) {
                    strncpy(buf->msg, msg, MSG_LEN);
                    k_fifo_put(&dispatcher_fifo, buf);
                }
                cnt = 0;
                memset(msg, 0, MSG_LEN);
            }
        }
        k_msleep(10);
    }
}

// Luodaan säikeet
K_THREAD_DEFINE(light_thread, STACKSIZE, light_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dis_thread, STACKSIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(uart_thread, STACKSIZE, uart_task, NULL, NULL, NULL, PRIORITY, 0, 0);

int main(void) {
    // Alustetaan semaforit
    k_sem_init(&light_sem, 0, 1);
    k_sem_init(&release_sem, 0, 1);

    // Tarkistetaan laitteet
    if (!device_is_ready(led_red.port) || !device_is_ready(led_green.port) || !device_is_ready(uart_dev)) {
        return -1;
    }

    // Alustetaan pinnit
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);

    printk("RTOS Liikennevalo kaynnistetty.\n");
    return 0;
}