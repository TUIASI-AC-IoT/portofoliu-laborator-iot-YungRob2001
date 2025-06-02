#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define OUT_PIN 4
#define IN_PIN 2

static QueueHandle_t q;
static int cnt = 0, last = 0;

static void IRAM_ATTR isr(void *arg) {
    uint32_t pin = (uint32_t)arg;
    xQueueSendFromISR(q, &pin, NULL);
}

static void task(void *arg) {
    uint32_t pin;
    while(1) {
        if(xQueueReceive(q, &pin, portMAX_DELAY)) {
            int level = gpio_get_level(pin);
            if(level != last) {
                last = level;
                printf("count: %d\n", ++cnt);
            }
        }
    }
}

void app_main() {
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << OUT_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
    
    cfg.pin_bit_mask = 1ULL << IN_PIN;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_down_en = 1;
    cfg.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&cfg);
    
    q = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(task, "task", 2048, NULL, 10, NULL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(IN_PIN, isr, (void*)IN_PIN);
    
    int i = 0;
    while(1) {
        gpio_set_level(OUT_PIN, i % 2);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(OUT_PIN, 1);
        vTaskDelay(750 / portTICK_PERIOD_MS);
        gpio_set_level(OUT_PIN, i % 2);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(OUT_PIN, 1);
        vTaskDelay(250 / portTICK_PERIOD_MS);
        i++;
    }
}