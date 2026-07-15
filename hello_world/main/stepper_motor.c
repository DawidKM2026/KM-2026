// 1 obrót = około 12.5 mm
/*
 * Timer GPTimer:
 * resolution_hz = 1 000 000 Hz
 *
 * STEP jest przełączany przy każdym alarmie:
 * step_state = !step_state;
 *
 * 1 krok silnika = 2 alarmy
 * Silnik: 1.8° = 200 kroków/obrót
 * Śruba: 12.5 mm/obrót
 
 alarm_count | RPS  | RPM | mm/s
------------+------+-----+-------
    10000   | 0.25 |  15 |   3.1
     5000   | 0.50 |  30 |   6.3
     2500   | 1.00 |  60 |  12.5
     2000   | 1.25 |  75 |  15.6
     1250   | 2.00 | 120 |  25.0
     1000   | 2.50 | 150 |  31.3
      625   | 4.00 | 240 |  50.0 
      500   | 5.00 | 300 |  62.5
      250   |10.00 | 600 | 125.0
 
 * Wzory:
 * kroki_s     = 1000000 / (2 * alarm_count)
 * RPM         = kroki_s * 60 / 200
 * predkosc_mm = RPM * 12.5 / 60
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"

#include "esp_err.h"

#include "gpio_config.h"
#include "esp_now_comm.h"
#include "stepper_motor.h"

static gptimer_handle_t timer = NULL;
static volatile  bool step_state = false;
static bool enabled = false;
static bool last_state = true;
static bool enabled_direction = true;
static int last_state_direction = 1;
extern int32_t current_x;
extern int32_t current_y;
static TaskHandle_t motor_task_handle = NULL;
static volatile bool stop_request = false;
static bool timer_running = false;


//Funkcje pomocnicze, zabezpieczające przed uruchamianiem włączonego timera i wyłączaniem wyłączonego timera
static esp_err_t motor_timer_start(void)
{
    if (timer_running)
    {
        return ESP_OK;
    }

    esp_err_t err = gptimer_start(timer);

    if (err == ESP_OK)
    {
        timer_running = true;
    }

    return err;
}

static esp_err_t motor_timer_stop(void)
{
    if (!timer_running)
    {
        return ESP_OK;
    }

    esp_err_t err = gptimer_stop(timer);

    if (err == ESP_OK)
    {
        timer_running = false;
    }

    return err;
}

void motor_task(void *pvParameters)
{
    while (1)
    {
        // Czekaj aż ktoś wyśle powiadomienie
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        printf("Start ruchu\n");

        motor_move_to(50, 0);

        printf("Koniec ruchu\n");
    }
}

//Zatrzymanie silnika
static void motor_stop(void)
{
    esp_err_t err = motor_timer_stop();

    if (err != ESP_OK)
    {
        printf("Blad zatrzymania timera: %s\n",
               esp_err_to_name(err));
    }

    step_state = false;
    gpio_set_level(STEP_PIN, 0);
    gpio_set_level(EN_PIN, 1);
}

//Wywoływanie alarmu po upływie określonego czasu
static bool IRAM_ATTR step_timer_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx)
{
    step_state = !step_state;
    gpio_set_level(STEP_PIN, step_state);

    return false;
}


//Ustawianie prędkości obrotowej silnika
void motor_set_speed(uint32_t motor_rpm)
{
    if(motor_rpm!=0){
    
            gpio_set_level(EN_PIN, 0);
    uint32_t alarm_count =(150000/(motor_rpm*16)); //Mnożenie razy 16 wynika ze step mode 1/16
    
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = alarm_count,
        .flags.auto_reload_on_alarm = true,
    };
    
    ESP_ERROR_CHECK(
        gptimer_set_alarm_action(
            timer,
            &alarm_config));
    }else{
        
            gpio_set_level(EN_PIN, 1);
    }
    printf("RPM = %" PRIu32 "\n", motor_rpm);

}

//Inicjalizacja timera
void init_stepper_motor_timer(void)
{

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };

    ESP_ERROR_CHECK(
        gptimer_new_timer(
            &timer_config,
            &timer));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = step_timer_callback,
    };

    ESP_ERROR_CHECK(
        gptimer_register_event_callbacks(
            timer,
            &callbacks,
            NULL));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 5000,
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(
        gptimer_set_alarm_action(
            timer,
            &alarm_config));
    
    gpio_set_level(EN_PIN, 0);

    ESP_ERROR_CHECK(gptimer_enable(timer));
    xTaskCreate(
    motor_task,
    "motor_task",
    4096,
    NULL,
    5,
    &motor_task_handle);
    }

    //Wybierz kierunek ruchu
    void motor_set_direction(motor_t motor,bool direction){
    switch(motor){

        default:
        printf("Zadano błędną wartość"
        );
        break;

        //Surge
        case MOTOR_SURGE:
        gpio_set_level(/* SURGE_1_DIR_PIN */DIR_PIN, direction);
        /* gpio_set_level( SURGE_2_DIR_PIN , !direction); */
        printf("Kierunek: %s\n", direction ? "przód" : "tył");
        break;
        
        //Sway
        case MOTOR_SWAY:
        gpio_set_level(/* SWAY_DIR_PIN */DIR_PIN, direction);
        printf("Kierunek: %s\n", direction ? "przód" : "tył");
        break;
    }
}

//Przesunięcie platformy w osi X i osi Y do zadanego punktu
void motor_move_to(int32_t x, int32_t y)
{
    x=x*64; //(800/12.5) //800 impulsów enkodera na obrót. Jeden obrót to około 12,5mm.
    y=y*64; //(800/12.5)

    
    int32_t start_position_x = motor_encoder_get_count();

    
    printf("Start: %"PRId32 "\n", start_position_x);
    printf("Cel: %"PRId32 "\n", x);

    // Surge Forward
    if (x-start_position_x > 0) //Trzeba przeliczyć na uklad związany ze statkiek
    {
        motor_set_direction(MOTOR_SURGE,0);
        
        motor_set_speed(30);
        ESP_ERROR_CHECK(motor_timer_start());

        while (motor_encoder_get_count() < x)
        {
            if (stop_request)
                break;
            current_x = motor_encoder_get_count();
            
            
        printf("Cel: %"PRId32 "\n", x);
        printf("Aktualne: %"PRId32 "\n", current_x);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        motor_stop();
    }

    // Surge Backward
    else if (x-start_position_x < 0)
    {
        motor_set_direction(MOTOR_SURGE,1);

        motor_set_speed(30);
        ESP_ERROR_CHECK(motor_timer_start());

        while (motor_encoder_get_count() > x)
        {
            if (stop_request)
                break;
            current_x = motor_encoder_get_count();
            printf("Cel: %"PRId32 "\n", x);
            printf("Aktualne: %"PRId32 "\n", current_x);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        motor_stop();
    }

    /* // Sway Forward
    if (y > 0)
    {
        motor_set_direction(MOTOR_SURGE,0);

        printf("Start: %"PRId32 "\n", start_position);
        int32_t target_position = abs(x-start_position);
        printf("Cel: %"PRId32 "\n", y);
        
        ESP_ERROR_CHECK(motor_timer_start());
        motor_set_speed(30);

        while (motor_encoder_get_count() < y)
        {
            if (stop_request)
                break;
            current_x = motor_encoder_get_count();
            
        printf("Cel: %"PRId32 "\n", y);
        
        printf("Aktualne: %"PRId32 "\n", current_x);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        motor_stop();
    }

    // Sway Backward
    else if (y < 0)
    {
        motor_set_direction(MOTOR_SURGE,1);

        printf("Start: %"PRId32 "\n", start_position);
        int32_t target_position = abs(x-start_position);
        printf("Cel: %"PRId32 "\n", y);
        
        ESP_ERROR_CHECK(motor_timer_start());
        motor_set_speed(30);

        while (motor_encoder_get_count() > y)
        {
            if (stop_request)
                break;
            current_x = motor_encoder_get_count();
            
        printf("Cel: %"PRId32 "\n", y);
        
        printf("Aktualne: %"PRId32 "\n", current_x);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
motor_stop();
    } */
} 

//Ręczne przesuwanie platformy w osi X i osi Y
void motor_move_by(int32_t wychylenie_x, int32_t wychylenie_y)
{
    
    int32_t start_position_x = motor_encoder_get_count();
    printf("Start: %"PRId32 "\n", start_position_x);

    //Joystick nieruchomo
    if (wychylenie_x == 0 && wychylenie_y == 0)
    {
        motor_stop();
        return;
    }

    // Surge Forward
    if (wychylenie_x > 0)
    {
        motor_set_direction(MOTOR_SURGE,0);
        motor_set_speed((wychylenie_x*45)/100);   
        ESP_ERROR_CHECK(motor_timer_start());
    }

    // Surge Backward
    else if (wychylenie_x < 0)
    {
        motor_set_direction(MOTOR_SURGE,1);
        motor_set_speed((abs(wychylenie_x)*45)/100);
        ESP_ERROR_CHECK(motor_timer_start());
    }

    // Sway Forward
    if (wychylenie_y > 0)
    {
        motor_set_direction(MOTOR_SWAY,1);
        motor_set_speed((wychylenie_y*45)/100);
        ESP_ERROR_CHECK(motor_timer_start());
    }

    // Sway Backward
    else if (wychylenie_y < 0)
    {
        motor_set_direction(MOTOR_SWAY,0);
        motor_set_speed((abs(wychylenie_y)*45)/100);   
        ESP_ERROR_CHECK(motor_timer_start());
}
}
/* //Bazowanie statku
bool motor_homing(void){
    motor_set_direction(MOTOR_SURGE,1);
    motor_set_direction(MOTOR_SWAY,1);
    motor_set_speed(SURGE,15);
    motor_set_speed(SWAY,15);
    while(krańcówka_x==1 OR krańcówka_y==1){
        if(krańcówka_x==0){
            motor_set_speed(SURGE,0);
        }
        if(krańcówka_y==0){
            motor_set_speed(SWAY,0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    current_x=-100;
    current_y=-100;
    motor_move_to(0,0);
    motor_encoder_reset_position
    current_x=0;
    current_y=0;
    printf("Bazowanie zakończone");
} */

//Switch do właczania/wyłączania silnika 
void motor_button_on_off(void)
{
    bool state = gpio_get_level(BUTTON_PIN);

    if (last_state == 1 && state == 0)
    {
        enabled = !enabled;

        if (enabled)
        {
            stop_request=false;
            gpio_set_level(EN_PIN, 0);
            printf("Silnik ON\n");
            xTaskNotifyGive(motor_task_handle);
            
        }
        else
        {
            stop_request=true;
            gpio_set_level(EN_PIN, 1);
            printf("Silnik OFF\n");
            ESP_ERROR_CHECK(motor_timer_stop());
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    last_state = state;
}


void motor_test(void)
{
    printf("TEST: uruchamianie silnika\n");

    stop_request = false;

    /* Ustawienie kierunku */
    motor_set_direction(MOTOR_SURGE, 0);

    /* Najpierw konfiguracja prędkości */
    motor_set_speed(30);

    /* Następnie uruchomienie timera */
    esp_err_t err = motor_timer_start();

    if (err != ESP_OK)
    {
        printf(
            "Blad uruchomienia timera: %s\n",
            esp_err_to_name(err)
        );

        motor_stop();
        return;
    }

    printf(
        "Silnik uruchomiony | EN=%d | DIR=%d\n",
        gpio_get_level(EN_PIN),
        gpio_get_level(DIR_PIN)
    );

    /* Silnik pracuje przez 5 sekund */
    vTaskDelay(pdMS_TO_TICKS(5000));

    motor_stop();

    printf(
        "Silnik zatrzymany | EN=%d | STEP=%d\n",
        gpio_get_level(EN_PIN),
        gpio_get_level(STEP_PIN)
    );
}





















//------------------------------------ Encoder ---------------------------------------------------
#include <math.h>
#include "driver/pulse_cnt.h"

#define ENCODER_A_GPIO GPIO_NUM_7
#define ENCODER_B_GPIO GPIO_NUM_8

#define ENCODER_CPR             200
#define ENCODER_COUNTS_PER_REV (ENCODER_CPR * 4)

static pcnt_unit_handle_t encoder_unit = NULL;

void motor_encoder_init(void)

{
    
    printf("Encoder init\n");

    pcnt_unit_config_t unit_config = {
        .low_limit = -32768,
        .high_limit = 32767,
    };

    ESP_ERROR_CHECK(
        pcnt_new_unit(&unit_config, &encoder_unit));

    pcnt_channel_handle_t chan_a = NULL;
    pcnt_channel_handle_t chan_b = NULL;

    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num = ENCODER_A_GPIO,
        .level_gpio_num = ENCODER_B_GPIO,
    };

    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num = ENCODER_B_GPIO,
        .level_gpio_num = ENCODER_A_GPIO,
    };

    ESP_ERROR_CHECK(
        pcnt_new_channel(encoder_unit, &chan_a_cfg, &chan_a));

    ESP_ERROR_CHECK(
        pcnt_new_channel(encoder_unit, &chan_b_cfg, &chan_b));

    // kanał A
    pcnt_channel_set_edge_action(
        chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE);

    pcnt_channel_set_level_action(
        chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // kanał B
    pcnt_channel_set_edge_action(
        chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE);

    pcnt_channel_set_level_action(
        chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    ESP_ERROR_CHECK(pcnt_unit_enable(encoder_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(encoder_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(encoder_unit));
    printf("encoder_unit=%p\n", encoder_unit);

int cnt = 0;
esp_err_t err = pcnt_unit_get_count(encoder_unit, &cnt);

printf("PCNT TEST err=%s cnt=%d\n",
       esp_err_to_name(err),
       cnt);
}

int32_t motor_encoder_get_count(void)
{
    int count = 0;

    if (encoder_unit != NULL)
    {
        esp_err_t err = pcnt_unit_get_count(encoder_unit, &count);

        if (err != ESP_OK)
        {
            printf("PCNT ERROR: %s\n", esp_err_to_name(err));
        }
    }
    else
    {
        printf("encoder_unit == NULL\n");
    }

    return count;
}

float motor_encoder_get_angle(void)
{
    int count = motor_encoder_get_count();

    float angle =
        ((float)count * 360.0f) /
        (float)ENCODER_COUNTS_PER_REV;

    angle = fmodf(angle, 360.0f);

    if (angle < 0)
    {
        angle += 360.0f;
    }

    return angle;
}

void motor_encoder_reset_position(void)
{
    if (encoder_unit != NULL)
    {
        pcnt_unit_clear_count(encoder_unit);
    }
}