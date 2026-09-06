#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

// --- LEDIEN MÄÄRITTELYT ---
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// --- NAPPIEN MÄÄRITTELYT ---
static const struct gpio_dt_spec btn1 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
static const struct gpio_dt_spec btn2 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw1), gpios, {0});
static const struct gpio_dt_spec btn3 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw2), gpios, {0});
static const struct gpio_dt_spec btn4 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw3), gpios, {0});
static const struct gpio_dt_spec btn5 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw4), gpios, {0});

static struct gpio_callback cb1, cb2, cb3, cb4, cb5;

// --- GLOBAALIT MUUTTUJAT ---
// 0 = Punainen, 1 = Keltainen, 2 = Vihreä, 3 = Vilkkuvinkeltainen, 4 = Pause, 5 = Manuaali
volatile int led_state = 0;
volatile int old_state = 0;

// Manuaalivalojen tilat
volatile bool man_r = false, man_y = false, man_g = false;

// --- TASKI MÄÄRITTELYT ---
#define STACKSIZE 500
#define PRIORITY 5

void red_task(void *, void *, void *);
void yellow_task(void *, void *, void *);
void green_task(void *, void *, void *);

K_THREAD_DEFINE(t_red, STACKSIZE, red_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(t_yellow, STACKSIZE, yellow_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(t_green, STACKSIZE, green_task, NULL, NULL, NULL, PRIORITY, 0, 0);

// --- NAPPIEN KESKEYTYKSEN KÄSITTELIJÄT ---

// Nappi 1: Pause on/off
void btn1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("Button pressed: Pause\n");
    if (led_state != 4) {
        old_state = led_state;
        led_state = 4;
    } else {
        led_state = old_state;
    }
}

// Nappi 2: Manuaalinen Punainen
void btn2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("Button pressed: Manual Red\n");
    led_state = 5;
    man_r = !man_r;
}

// Nappi 3: Manuaalinen Keltainen
void btn3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("Button pressed: Manual Yellow\n");
    led_state = 5;
    man_y = !man_y;
}

// Nappi 4: Manuaalinen Vihreä
void btn4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("Button pressed: Manual Green\n");
    led_state = 5;
    man_g = !man_g;
}

// Nappi 5: Vilkkuva keltainen on/off
void btn5_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("Button pressed: Flashing Yellow\n");
    if (led_state != 3) {
        old_state = led_state;
        led_state = 3;
    } else {
        led_state = 0; // Palataan alkuun
    }
}

// --- PÄÄOHJELMA ---
int main(void) {
    // Alustetaan LEDit
    gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE);

    // Alustetaan Napit & Keskeytykset
    gpio_pin_configure_dt(&btn1, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&btn1, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb1, btn1_handler, BIT(btn1.pin));
    gpio_add_callback(btn1.port, &cb1);

    gpio_pin_configure_dt(&btn2, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&btn2, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb2, btn2_handler, BIT(btn2.pin));
    gpio_add_callback(btn2.port, &cb2);

    gpio_pin_configure_dt(&btn3, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&btn3, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb3, btn3_handler, BIT(btn3.pin));
    gpio_add_callback(btn3.port, &cb3);

    gpio_pin_configure_dt(&btn4, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&btn4, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb4, btn4_handler, BIT(btn4.pin));
    gpio_add_callback(btn4.port, &cb4);

    gpio_pin_configure_dt(&btn5, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&btn5, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb5, btn5_handler, BIT(btn5.pin));
    gpio_add_callback(btn5.port, &cb5);

    // Main pyörittää manuaaliohjausta (Tila 5)
    while (1) {
        if (led_state == 5) {
            gpio_pin_set_dt(&red, man_r || man_y ? 1 : 0);
            gpio_pin_set_dt(&green, man_g || man_y ? 1 : 0);
        }
        k_msleep(100);
    }
    return 0;
}

// --- TASKIFUNKTIOT ---

// Punainen task
void red_task(void *, void *, void *) {
    while (1) {
        if (led_state == 0) {
            gpio_pin_set_dt(&red, 1);
            printk("Red ON\n");
            k_msleep(1000);

            gpio_pin_set_dt(&red, 0);
            printk("Red OFF\n");

            if (led_state == 0) led_state = 1; // Siirrytään keltaiseen
        }
        k_msleep(50);
    }
}

// Keltainen task (Keltainen = Punainen + Vihreä)
void yellow_task(void *, void *, void *) {
    while (1) {
        // Normaali keltainen sekvenssissä
        if (led_state == 1) {
            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 1);
            printk("Yellow ON\n");
            k_msleep(1000);

            gpio_pin_set_dt(&red, 0);
            gpio_pin_set_dt(&green, 0);
            printk("Yellow OFF\n");

            if (led_state == 1) led_state = 2; // Siirrytään vihreään
        } 
        // Vilkkuva keltainen (Nappi 5)
        else if (led_state == 3) {
            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 1);
            k_msleep(500);
            gpio_pin_set_dt(&red, 0);
            gpio_pin_set_dt(&green, 0);
            k_msleep(500);
        } else {
            k_msleep(50);
        }
    }
}

// Vihreä task
void green_task(void *, void *, void *) {
    while (1) {
        if (led_state == 2) {
            gpio_pin_set_dt(&green, 1);
            printk("Green ON\n");
            k_msleep(1000);

            gpio_pin_set_dt(&green, 0);
            printk("Green OFF\n");

            if (led_state == 2) led_state = 0; // Palataan punaiseen
        }
        k_msleep(50);
    }
}