#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#define GPIO_OUTPUT_IO 4
#define GPIO_OUTPUT_PIN_SEL (1ULL<<GPIO_OUTPUT_IO)
#define GPIO_BUTTON_IO 2
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_BUTTON_IO)

static volatile int button_press_count = 0;

void button_task(void *arg) {
    gpio_set_direction(GPIO_BUTTON_IO, GPIO_MODE_INPUT);  

    while (1) {
         
        int button_state = gpio_get_level(GPIO_BUTTON_IO);
        
       
        if (button_state == 1) {
            vTaskDelay(50 / portTICK_PERIOD_MS);  
            if (gpio_get_level(GPIO_BUTTON_IO) == 1) {  
                button_press_count++;  
                printf("Apasare buton: %d\n", button_press_count);
                while (gpio_get_level(GPIO_BUTTON_IO) == 1) {
                    vTaskDelay(10 / portTICK_PERIOD_MS); 
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);  
    }
}

void app_main() {
    xTaskCreate(button_task, "Button Task", 2048, NULL, 10, NULL);
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    
    

    while(1) {
     
        gpio_set_level(GPIO_OUTPUT_IO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

         
        gpio_set_level(GPIO_OUTPUT_IO, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);

      
        gpio_set_level(GPIO_OUTPUT_IO, 1);
        vTaskDelay(250 / portTICK_PERIOD_MS);

         
        gpio_set_level(GPIO_OUTPUT_IO, 0);
        vTaskDelay(750 / portTICK_PERIOD_MS);
    }
}


/*
• Ce rol are functia gpio config?
gpio config este un api care poate fi folosit pentru configurarea modurilor i/o, internal pull-up,pull-down resistors, etc.
Este un api care face overwrite la toate configurarile anterioare.
•
ˆIn codul exemplu, pinul GPIO4 este configurat ca ies¸ire. Care sunt celelalte
moduri ˆın care poate fi configurat un pin GPIO?
input,output

*/