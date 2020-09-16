#include "lib_uarte.h"



 typedef struct {
    uint8_t * p_data;
    uint32_t length;
} buffer_t;

NRF_LIBUARTE_ASYNC_DEFINE(libuarte, 0, 0, 0, NRF_LIBUARTE_PERIPHERAL_NOT_USED, 255, 3);

static uint8_t text[] = "UART example started.\r\n Loopback:\r\n";
static uint8_t text_size = sizeof(text);
static volatile bool m_loopback_phase;

NRF_QUEUE_DEF(buffer_t, m_buf_queue, 10, NRF_QUEUE_MODE_NO_OVERFLOW);

static void uart_event_handler(void * context, nrf_libuarte_async_evt_t * p_evt)
{
    nrf_libuarte_async_t * p_libuarte = (nrf_libuarte_async_t *)context;
    ret_code_t ret;

    switch (p_evt->type)
    {
        case NRF_LIBUARTE_ASYNC_EVT_ERROR:
            //bsp_board_led_invert(0);
            break;
        case NRF_LIBUARTE_ASYNC_EVT_RX_DATA:
            ret = nrf_libuarte_async_tx(p_libuarte,p_evt->data.rxtx.p_data, p_evt->data.rxtx.length);
            if (ret == NRF_ERROR_BUSY)
            {
                buffer_t buf = {
                    .p_data = p_evt->data.rxtx.p_data,
                    .length = p_evt->data.rxtx.length,
                };

                ret = nrf_queue_push(&m_buf_queue, &buf);
                APP_ERROR_CHECK(ret);
            }
            else
            {
                APP_ERROR_CHECK(ret);
            }
            //bsp_board_led_invert(1);
            m_loopback_phase = true;
            break;
        case NRF_LIBUARTE_ASYNC_EVT_TX_DONE:
            if (m_loopback_phase)
            {
                nrf_libuarte_async_rx_free(p_libuarte, p_evt->data.rxtx.p_data, p_evt->data.rxtx.length);
                if (!nrf_queue_is_empty(&m_buf_queue))
                {
                    buffer_t buf;
                    ret = nrf_queue_pop(&m_buf_queue, &buf);
                    APP_ERROR_CHECK(ret);
                    UNUSED_RETURN_VALUE(nrf_libuarte_async_tx(p_libuarte, buf.p_data, buf.length));
                }
            }
            //bsp_board_led_invert(2);
            break;
        default:
            break;
    }
}

void init_libuarte(){

ret_code_t ret;

       nrf_libuarte_async_config_t nrf_libuarte_async_config = {
            .tx_pin     = 25,
            .rx_pin     = 26,
            .baudrate   = NRF_UARTE_BAUDRATE_115200,
            .parity     = NRF_UARTE_PARITY_EXCLUDED,
            .hwfc       = NRF_UARTE_HWFC_DISABLED,
            .timeout_us = 100,
            .int_prio   = APP_IRQ_PRIORITY_LOW
    };
 

    ret = nrf_libuarte_async_init(&libuarte, &nrf_libuarte_async_config, uart_event_handler, (void *)&libuarte);

    APP_ERROR_CHECK(ret);

    nrf_libuarte_async_enable(&libuarte);

}