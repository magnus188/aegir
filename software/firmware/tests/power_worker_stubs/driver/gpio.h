#pragma once
#include <esp_err.h>
using gpio_num_t=int;
enum {GPIO_MODE_OUTPUT_OD,GPIO_MODE_OUTPUT,GPIO_MODE_INPUT,GPIO_FLOATING,GPIO_INTR_NEGEDGE};
esp_err_t gpio_set_level(gpio_num_t,int);
int gpio_get_level(gpio_num_t);
esp_err_t gpio_set_direction(gpio_num_t,int);
esp_err_t gpio_set_pull_mode(gpio_num_t,int);
esp_err_t gpio_install_isr_service(int);
esp_err_t gpio_set_intr_type(gpio_num_t,int);
esp_err_t gpio_isr_handler_add(gpio_num_t,void(*)(void *),void *);
esp_err_t gpio_isr_handler_remove(gpio_num_t);
