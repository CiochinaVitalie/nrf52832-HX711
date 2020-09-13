
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

#define PPI_EXAMPLE_TIMERS_PHASE_SHIFT_DELAY    (10)    // 1s = 10 * 100ms (Timer 0 interrupt)
#define PPI_EXAMPLE_TIMER0_INTERVAL             (200)   // Timer interval in milliseconds
#define PPI_EXAMPLE_TIMER1_INTERVAL             (1000)  // Timer interval in milliseconds
#define PPI_EXAMPLE_TIMER2_INTERVAL             (20000)  // Timer interval in milliseconds

#define MAX_TEST_DATA_BYTES     (15U)                /**< max number of test bytes to be used for tx and rx. */
#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 256                         /**< UART RX buffer size. */


#define COMPARE_COUNTERTIME  (3UL) 
#define PD_SCK                    28    
#define DOUT                      31    
/* Defines frequency and duty cycle for clock signal - default: 1 MHz 50%*/  


void uart_error_handle(app_uart_evt_t * p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}

const nrf_drv_rtc_t rtc = NRF_DRV_RTC_INSTANCE(0);

static const nrf_drv_timer_t m_timer0 = NRF_DRV_TIMER_INSTANCE(0);
static const nrf_drv_timer_t m_timer1 = NRF_DRV_TIMER_INSTANCE(1);
static const nrf_drv_timer_t m_timer2 = NRF_DRV_TIMER_INSTANCE(2);

static nrf_ppi_channel_t m_ppi_channel1;
static nrf_ppi_channel_t m_ppi_channel2;
static nrf_ppi_channel_t ppi_channel3;

static volatile uint32_t m_counter,b_counter,c_counter,buffer;

static void timer0_event_handler(nrf_timer_event_t event_type, void * p_context)
{
    ++m_counter;
    if(m_counter==50){
        nrf_drv_timer_pause(&m_timer0);
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
    // ++m_counter;
    //  if (m_counter>=24)
    //     {

    //         //m_counter=0;
    //         nrf_drv_timer_disable(&m_timer0);
    //         b_counter=1;
    //     }
    //++c_counter;
    //while(b_counter==0);
}

static void led_blinking_setup()
{
    uint32_t compare_evt_addr;
    uint32_t gpiote_task_addr;
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

    compare_evt_addr = nrf_drv_timer_event_address_get(&m_timer0, NRF_TIMER_EVENT_COMPARE0);
    gpiote_task_addr = nrf_drv_gpiote_out_task_addr_get(PD_SCK);

    err_code = nrf_drv_ppi_channel_assign(ppi_channel3, compare_evt_addr, gpiote_task_addr);
    APP_ERROR_CHECK(err_code);

    // err_code = nrf_drv_ppi_channel_fork_assign(ppi_channel3,nrf_drv_gpiote_in_event_addr_get(DOUT));
    // APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_ppi_channel_enable(ppi_channel3);
    APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_out_task_enable(PD_SCK);
    nrf_drv_gpiote_in_event_enable(DOUT, true);
}

/** @brief Function for initializing the PPI peripheral.
*/
static void ppi_init(void)
{
    uint32_t err_code = NRF_SUCCESS;

    err_code = nrf_drv_ppi_init();
    APP_ERROR_CHECK(err_code);

    /* Configure 1st available PPI channel to stop TIMER0 counter on TIMER1 COMPARE[0] match,
     * which is every even number of seconds.
     */
    // err_code = nrf_drv_ppi_channel_alloc(&m_ppi_channel1);
    // APP_ERROR_CHECK(err_code);
    // err_code = nrf_drv_ppi_channel_assign(m_ppi_channel1,
    //                                       nrf_drv_timer_event_address_get(&m_timer1,
    //                                                                       NRF_TIMER_EVENT_COMPARE0),
    //                                       nrf_drv_timer_task_address_get(&m_timer0,
    //                                                                      NRF_TIMER_TASK_STOP));
    // APP_ERROR_CHECK(err_code);

    /* Configure 2nd available PPI channel to start TIMER0 counter at TIMER2 COMPARE[0] match,
     * which is every odd number of seconds.
     */
    // err_code = nrf_drv_ppi_channel_alloc(&m_ppi_channel2);
    // APP_ERROR_CHECK(err_code);
    // err_code = nrf_drv_ppi_channel_assign(m_ppi_channel2,
    //                                       nrf_drv_rtc_event_address_get(&rtc,
    //                                                                       NRF_RTC_EVENT_COMPARE_0),
    //                                       nrf_drv_timer_task_address_get(&m_timer0,
    //                                                                      NRF_TIMER_TASK_START));
    // APP_ERROR_CHECK(err_code);

    //Enable both configured PPI channels
    // err_code = nrf_drv_ppi_channel_enable(m_ppi_channel1);
    // APP_ERROR_CHECK(err_code);
    // err_code = nrf_drv_ppi_channel_enable(m_ppi_channel2);
    // APP_ERROR_CHECK(err_code);
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

/** @brief Function for Timer 1 initialization.
 *  @details Initializes TIMER1 peripheral to generate an event every 2 seconds. The events are
 *           generated at even numbers of seconds after starting the example (2, 4, 6 ...) and they
 *           are used to stop TIMER0 via PPI: TIMER1->EVENT_COMPARE[0] triggers TIMER0->TASK_STOP.
 */
static void timer1_init(void)
{
    // Check TIMER1 configuration for details.
    nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;
    timer_cfg.frequency = NRF_TIMER_FREQ_1MHz;
    ret_code_t err_code = nrf_drv_timer_init(&m_timer1, &timer_cfg, empty_timer_handler);
    APP_ERROR_CHECK(err_code);

    nrf_drv_timer_extended_compare(&m_timer1,
                                   NRF_TIMER_CC_CHANNEL1,
                                   nrf_drv_timer_ms_to_ticks(&m_timer1,
                                                             240),
                                   NRF_TIMER_SHORT_COMPARE0_STOP_MASK,
                                   false);
}

///////////////////////////////////////////////////////////////////////////////////////////////
static void uart_init(void){
        uint32_t err_code;
        app_uart_comm_params_t const comm_params =
        {
                .rx_pin_no    = RX_PIN_NUMBER,
                .tx_pin_no    = TX_PIN_NUMBER,
                .rts_pin_no   = RTS_PIN_NUMBER,
                .cts_pin_no   = CTS_PIN_NUMBER,
                .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
                .use_parity   = false,
                .baud_rate    = NRF_UART_BAUDRATE_115200

        };
        APP_UART_FIFO_INIT(&comm_params,
                           UART_RX_BUF_SIZE,
                           UART_TX_BUF_SIZE,
                           uart_error_handle,
                           APP_IRQ_PRIORITY_LOWEST,
                           err_code);
        APP_ERROR_CHECK(err_code);
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
    //nrf_drv_timer_enable(&m_timer0);
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

    ppi_init();
    nrf_drv_gpiote_init();
    led_blinking_setup();
    leds_config();
    lfclk_config();
    app_timer_init();
    create_timers();
    err_code = app_timer_start(m_repeated_timer_id, APP_TIMER_TICKS(10000), NULL);
    // APP_ERROR_CHECK(err_code);
    // rtc_config();

   
    timer0_init(); // Timer used to increase m_counter every 100ms.
   // timer1_init(); // Timer to generate events on even number of seconds - stopping Timer 0
    // timer2_init(); // Timer to generate events on odd number of seconds - starting Timer 0


   // NRF_LOG_INFO("PPI example started.");

    // Start clock.
   nrf_drv_timer_enable(&m_timer0);

    /* Below delay is implemented to ensure that Timer0 interrupt will execute before PPI action.
     * Please be aware that such solution was tested only in this simple example code. In case
     * of more complex systems with higher level interrupts this may lead to not correct timers
     * synchronization.
     */
    //nrf_delay_us(5);
   //nrf_drv_timer_enable(&m_timer1);
    // nrf_delay_us(5);
    // nrf_drv_rtc_enable(&rtc);
    //m_counter = (uint32_t)-PPI_EXAMPLE_TIMERS_PHASE_SHIFT_DELAY;

    // Timer 2 will start one second after Timer 1 (m_counter will equal 0 after 1s)
    // while (m_counter != 0) 
    // {
    //     // just wait
    // }
    //nrf_drv_timer_enable(&m_timer2);

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

        }
        // __SEV();
        // __WFE();
        // __WFE();
    }
}

/** @} */
