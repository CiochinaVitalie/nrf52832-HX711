
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "nrf_delay.h"
#include "app_error.h"
#include "app_uart.h"
#include "bsp.h"
#include "nrf_drv_ppi.h"
#include "nrf_drv_timer.h"
#include "nrf.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_gpiote.h"
#include "nrf_drv_gpiote.h"
#include "nrf_gpio.h"
#include "nrf_uart.h"
#include "nrf_rtc.h"
#include "nrf_drv_rtc.h"
#include "nrf_drv_clock.h"

#include "app_timer.h"

#define PD_SCK                    28    
#define DOUT                      31    
///////////////////////////////////////////////////
typedef enum
{
    SIMPLE_TIMER_STATE_IDLE = 0,
    SIMPLE_TIMER_STATE_INITIALIZED,
    SIMPLE_TIMER_STATE_STOPPED,
    SIMPLE_TIMER_STATE_STARTED
}simple_timer_states_t;


//////////////////////////////////////////////////
const nrf_drv_rtc_t rtc = NRF_DRV_RTC_INSTANCE(0);
static const nrf_drv_timer_t m_timer0 = NRF_DRV_TIMER_INSTANCE(0);
static simple_timer_states_t              m_simple_timer_state       = SIMPLE_TIMER_STATE_IDLE;

static nrf_ppi_channel_t m_ppi_channel1;
static nrf_ppi_channel_t m_ppi_channel2;
static nrf_ppi_channel_t ppi_channel3;

static volatile uint32_t m_counter,b_counter,c_counter,buffer;

static void timer0_event_handler(nrf_timer_event_t event_type, void * p_context)
{
    ++m_counter;
    if(m_counter==50){
        nrf_drv_timer_pause(&m_timer0);
        m_simple_timer_state = SIMPLE_TIMER_STATE_STOPPED;
        buffer = buffer ^ 0x800000;
        b_counter=1;        
        }
    if(m_counter%2 != 0 && m_counter<=48){
        buffer <<= 1;
            if(nrf_gpio_pin_read(DOUT))buffer++;
    }
}

/* Timer event handler. Not used since Timer1 and Timer2 are used only for PPI. */
static void empty_timer_handler(nrf_timer_event_t event_type, void * p_context)
{

}

/////////////////////////////////////////////////////////////////////////////////////////
/** @brief Function configuring gpio for pin toggling.
 */
static void leds_config(void)
{
    bsp_board_init(BSP_INIT_LEDS);
}

/** @brief Function starting the internal LFCLK XTAL oscillator.
 */
static void lfclk_config(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);

    nrf_drv_clock_lfclk_request(NULL);
}


static void gpiote_evt_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    nrf_drv_gpiote_in_event_disable(DOUT);
}

static void led_blinking_setup()
{

    ret_code_t err_code;
    nrf_drv_gpiote_out_config_t config = GPIOTE_CONFIG_OUT_TASK_TOGGLE(false);
    /////////////////////////////////////////////////////////////////////////////////
    nrf_drv_gpiote_in_config_t  gpiote_config = GPIOTE_CONFIG_IN_SENSE_LOTOHI(false);
    nrf_gpio_cfg_input(DOUT, NRF_GPIO_PIN_NOPULL);
    err_code = nrf_drv_gpiote_in_init(DOUT, &gpiote_config, gpiote_evt_handler);
    APP_ERROR_CHECK(err_code);
    ////////////////////////////////////////////////////////////////////////////////
    err_code = nrf_drv_gpiote_out_init(PD_SCK, &config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_ppi_channel_alloc(&ppi_channel3);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_ppi_channel_assign(ppi_channel3,
                                            nrf_drv_timer_event_address_get(&m_timer0, NRF_TIMER_EVENT_COMPARE0),
                                            nrf_drv_gpiote_out_task_addr_get(PD_SCK));
    APP_ERROR_CHECK(err_code);


    err_code = nrf_drv_ppi_channel_enable(ppi_channel3);
    APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_out_task_enable(PD_SCK);
    nrf_drv_gpiote_in_event_enable(DOUT, true);
}

/** @brief Function for Timer 0 initialization.
 *  @details Timer 0 will be stopped and started by Timer 1 and Timer 2 respectively using PPI.
 *           It is configured to generate an interrupt every 100ms.
 */
static void timer0_init(void)
{
    // Check TIMER0 configuration for details.
    nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;
    timer_cfg.frequency = NRF_TIMER_FREQ_1MHz;

    ret_code_t err_code = nrf_drv_timer_init(&m_timer0, &timer_cfg, timer0_event_handler);
    APP_ERROR_CHECK(err_code);

    nrf_drv_timer_extended_compare(&m_timer0,
                                   NRF_TIMER_CC_CHANNEL0,
                                   nrf_drv_timer_us_to_ticks(&m_timer0,
                                                             10),
                                   NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
                                   true);
}


/**
 * @brief Function for application main entry.
 */
APP_TIMER_DEF(m_repeated_timer_id);
/**@brief Timeout handler for the repeated timer.
 */
static void repeated_timer_handler(void * p_context)
{
    nrf_drv_gpiote_out_toggle(LED_2);
    if(m_simple_timer_state == SIMPLE_TIMER_STATE_STOPPED){
        nrf_drv_timer_resume(&m_timer0);
        nrf_drv_gpiote_out_toggle(LED_1);
        m_simple_timer_state = SIMPLE_TIMER_STATE_STARTED;
    } 
    
}
/**@brief Create timers.
 */
static void create_timers()
{
    ret_code_t err_code;

    // Create timers
    err_code = app_timer_create(&m_repeated_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                repeated_timer_handler);
    APP_ERROR_CHECK(err_code);
}
int main(void)
{
    uint32_t old_val = 0;
    uint32_t err_code;

    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();

    //ppi_init();
    nrf_drv_ppi_init();
    nrf_drv_gpiote_init();
    led_blinking_setup();
    leds_config();
    lfclk_config();
    app_timer_init();
    create_timers();
    
    timer0_init(); // Timer used to increase m_counter every 100ms.
    // Start clock.
    nrf_drv_timer_enable(&m_timer0);
    err_code = app_timer_start(m_repeated_timer_id, APP_TIMER_TICKS(10000), NULL);
    APP_ERROR_CHECK(err_code);

    while (true)
    {
        //uint32_t counter = m_counter;
        if (b_counter > 0)
        {
            //old_val = counter;
            NRF_LOG_INFO("out impulses %u \n",m_counter);
            NRF_LOG_FLUSH();
            NRF_LOG_INFO("imp recieve %u \n",c_counter);
            NRF_LOG_FLUSH();
            NRF_LOG_INFO("buffer %u \n",buffer);
            NRF_LOG_FLUSH();
            b_counter=0;
            m_counter = 0;
        }
        // __SEV();
        // __WFE();
        // __WFE();
    }
}

/** @} */
