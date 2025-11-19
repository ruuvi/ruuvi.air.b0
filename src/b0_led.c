/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "b0_led.h"
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <nrfx_config.h>
#include <nrfx_timer.h>
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>
#include <hal/nrf_gpio.h>

LOG_MODULE_DECLARE(B0, LOG_LEVEL_INF);

#if DT_NODE_EXISTS(DT_ALIAS(led_red))
#define LED_RED_NODE DT_ALIAS(led_red)
#else
#error "'led-red' devicetree alias is not defined"
#endif

#if DT_NODE_HAS_STATUS(LED_RED_NODE, okay) && DT_NODE_HAS_PROP(LED_RED_NODE, gpios)
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);
#else
#error "'led-red' devicetree alias is not defined properly"
#endif

#if DT_NODE_EXISTS(DT_ALIAS(led_green))
#define LED_GREEN_NODE DT_ALIAS(led_green)
#else
#error "'led-green' devicetree alias is not defined"
#endif
#if DT_NODE_HAS_STATUS(LED_GREEN_NODE, okay) && DT_NODE_HAS_PROP(LED_GREEN_NODE, gpios)
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
#else
#error "'led-green' devicetree alias is not defined properly"
#endif

#define LED_RED_PIN   NRF_DT_GPIOS_TO_PSEL(LED_RED_NODE, gpios)
#define LED_GREEN_PIN NRF_DT_GPIOS_TO_PSEL(LED_GREEN_NODE, gpios)

#define LED_RED_FLAGS   DT_GPIO_FLAGS(LED_RED_NODE, gpios)
#define LED_GREEN_FLAGS DT_GPIO_FLAGS(LED_GREEN_NODE, gpios)
static inline bool
is_active_low(const uint32_t flags)
{
    return flags & GPIO_ACTIVE_LOW;
}

static const nrfx_timer_t  s_timer  = NRFX_TIMER_INSTANCE(4);  // TIMER4
static const nrfx_gpiote_t s_gpiote = NRFX_GPIOTE_INSTANCE(0); // GPIOTE0
static nrf_ppi_channel_t   s_ppi_ch;
static uint8_t             s_ch_green; // GPIOTE channel for GREEN
static uint8_t             s_ch_red;   // GPIOTE channel for RED

static void
b0_led_init_gpio(const struct gpio_dt_spec* p_led_spec)
{
    if (!device_is_ready(p_led_spec->port))
    {
        LOG_ERR("LED %s:%d is not ready", p_led_spec->port->name, p_led_spec->pin);
        return;
    }

    const int32_t rc = gpio_pin_configure_dt(p_led_spec, GPIO_OUTPUT_INACTIVE);
    if (0 != rc)
    {
        LOG_ERR("Failed to configure LED %s:%d, rc %d", p_led_spec->port->name, p_led_spec->pin, rc);
        return;
    }
}

void
b0_led_init(void)
{
    b0_led_init_gpio(&led_red);
    b0_led_init_gpio(&led_green);
}

static void
b0_led_deinit_gpio(const struct gpio_dt_spec* p_led_spec)
{
    if (!device_is_ready(p_led_spec->port))
    {
        LOG_ERR("LED %s:%d is not ready", p_led_spec->port->name, p_led_spec->pin);
        return;
    }

    gpio_pin_set_dt(p_led_spec, 0);

    const int32_t rc = gpio_pin_configure_dt(p_led_spec, GPIO_DISCONNECTED);
    if (0 != rc)
    {
        LOG_ERR("Failed to configure LED %s:%d, rc %d", p_led_spec->port->name, p_led_spec->pin, rc);
        return;
    }
}

void
b0_led_deinit(void)
{
    b0_led_deinit_gpio(&led_red);
    b0_led_deinit_gpio(&led_green);
}

void
b0_led_red_set(const bool is_on)
{
    gpio_pin_set_dt(&led_red, is_on ? 1 : 0);
}

void
b0_led_green_set(const bool is_on)
{
    gpio_pin_set_dt(&led_green, is_on ? 1 : 0);
}

static void
gpiote_setup(void)
{
    // Allocate two GPIOTE channels
    nrfx_err_t err = nrfx_gpiote_channel_alloc(&s_gpiote, &s_ch_red);
    if (NRFX_SUCCESS != err)
    {
        LOG_ERR("nrfx_gpiote_channel_alloc(red) failed: %d", err);
        return;
    }
    err = nrfx_gpiote_channel_alloc(&s_gpiote, &s_ch_green);
    if (NRFX_SUCCESS != err)
    {
        LOG_ERR("nrfx_gpiote_channel_alloc(green) failed: %d", err);
        nrfx_gpiote_channel_free(&s_gpiote, s_ch_red);
        return;
    }

    // Configure the pins' electrical settings
    const nrfx_gpiote_output_config_t out_cfg = {
        .drive         = NRF_GPIO_PIN_S0S1, // standard drive
        .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
        .pull          = NRF_GPIO_PIN_NOPULL,
    };

    // Initial LED states: RED = ON, GREEN = OFF
    // Respect active-low: ON = LOW if active-low, else HIGH.
    const nrf_gpiote_outinit_t red_init   = is_active_low(LED_RED_FLAGS) ? NRF_GPIOTE_INITIAL_VALUE_LOW
                                                                         : NRF_GPIOTE_INITIAL_VALUE_HIGH;
    const nrf_gpiote_outinit_t green_init = is_active_low(LED_GREEN_FLAGS) ? NRF_GPIOTE_INITIAL_VALUE_HIGH
                                                                           : NRF_GPIOTE_INITIAL_VALUE_LOW;

    // Task configs: use the allocated channels, TOGGLE polarity
    const nrfx_gpiote_task_config_t red_task_cfg = {
        .task_ch  = s_ch_red,
        .polarity = NRF_GPIOTE_POLARITY_TOGGLE,
        .init_val = red_init,
    };
    const nrfx_gpiote_task_config_t green_task_cfg = {
        .task_ch  = s_ch_green,
        .polarity = NRF_GPIOTE_POLARITY_TOGGLE,
        .init_val = green_init,
    };

    // Configure output + task in one go
    err = nrfx_gpiote_output_configure(&s_gpiote, LED_RED_PIN, &out_cfg, &red_task_cfg);
    if (NRFX_SUCCESS != err)
    {
        LOG_ERR("nrfx_gpiote_output_configure(red) failed: %d", err);
        nrfx_gpiote_channel_free(&s_gpiote, s_ch_red);
        nrfx_gpiote_channel_free(&s_gpiote, s_ch_green);
        return;
    }
    err = nrfx_gpiote_output_configure(&s_gpiote, LED_GREEN_PIN, &out_cfg, &green_task_cfg);
    if (NRFX_SUCCESS != err)
    {
        LOG_ERR("nrfx_gpiote_output_configure(green) failed: %d", err);
        nrfx_gpiote_channel_free(&s_gpiote, s_ch_red);
        nrfx_gpiote_channel_free(&s_gpiote, s_ch_green);
        return;
    }

    // Enable task endpoints
    nrfx_gpiote_out_task_enable(&s_gpiote, LED_RED_PIN);
    nrfx_gpiote_out_task_enable(&s_gpiote, LED_GREEN_PIN);
}

static void
gpiote_teardown(void)
{
    nrfx_gpiote_out_task_disable(&s_gpiote, LED_RED_PIN);
    nrfx_gpiote_out_task_disable(&s_gpiote, LED_GREEN_PIN);
    nrfx_gpiote_pin_uninit(&s_gpiote, LED_RED_PIN);
    nrfx_gpiote_pin_uninit(&s_gpiote, LED_GREEN_PIN);
    nrfx_gpiote_channel_free(&s_gpiote, s_ch_red);
    nrfx_gpiote_channel_free(&s_gpiote, s_ch_green);
}

static void
timer_setup_500ms_event(void)
{
    // 1 MHz timer, 32-bit, no interrupts; we only use its COMPARE event.
    nrfx_timer_config_t tcfg = NRFX_TIMER_DEFAULT_CONFIG(1 * 1000 * 1000);
    tcfg.mode                = NRF_TIMER_MODE_TIMER;
    tcfg.bit_width           = NRF_TIMER_BIT_WIDTH_32;

    const nrfx_err_t err = nrfx_timer_init(&s_timer, &tcfg, NULL);
    if (NRFX_SUCCESS != err)
    {
        LOG_ERR("nrfx_timer_init failed: %d", err);
        return;
    }

    uint32_t ticks_500ms = nrfx_timer_ms_to_ticks(&s_timer, 500);
    nrfx_timer_extended_compare(
        &s_timer,
        NRF_TIMER_CC_CHANNEL0,
        ticks_500ms,
        NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, // auto-clear for periodic event
        false);
    nrfx_timer_enable(&s_timer);
}

static void
timer_teardown(void)
{
    nrfx_timer_disable(&s_timer);
    nrfx_timer_uninit(&s_timer);
}

static void
ppi_connect_timer_to_gpiote_fork(void)
{
    // Allocate one PPI channel
    nrfx_err_t err = nrfx_ppi_channel_alloc(&s_ppi_ch);
    if (NRFX_SUCCESS != err)
    {
        LOG_ERR("nrfx_ppi_channel_alloc failed: %d", err);
        return;
    }

    uint32_t ev_addr    = nrfx_timer_event_address_get(&s_timer, NRF_TIMER_EVENT_COMPARE0);
    uint32_t task_red   = nrfx_gpiote_out_task_address_get(&s_gpiote, LED_RED_PIN);
    uint32_t task_green = nrfx_gpiote_out_task_address_get(&s_gpiote, LED_GREEN_PIN);

    // Event -> RED toggle, fork -> GREEN toggle (order doesn't matter, both fire)
    err = nrfx_ppi_channel_assign(s_ppi_ch, ev_addr, task_red);
    if (NRFX_SUCCESS != err)
    {
        LOG_ERR("nrfx_ppi_channel_assign failed: %d", err);
        nrfx_ppi_channel_free(s_ppi_ch);
        return;
    }
    err = nrfx_ppi_channel_fork_assign(s_ppi_ch, task_green);
    if (NRFX_SUCCESS != err)
    {
        LOG_ERR("nrfx_ppi_channel_fork_assign failed: %d", err);
        nrfx_ppi_channel_free(s_ppi_ch);
        return;
    }
    nrfx_ppi_channel_enable(s_ppi_ch);
}

static void
ppi_disconnect(void)
{
    nrfx_ppi_channel_disable(s_ppi_ch);
    nrfx_ppi_channel_free(s_ppi_ch);
}

void
b0_led_start_blinking_red_green_500ms(void)
{
    gpiote_setup();
    timer_setup_500ms_event();
    ppi_connect_timer_to_gpiote_fork();
}

void
b0_led_stop_blinking(void)
{
    ppi_disconnect();
    timer_teardown();

    // Force LEDs off before handover
    nrf_gpio_cfg_output(LED_GREEN_PIN);
    nrf_gpio_cfg_output(LED_RED_PIN);
    if (is_active_low(LED_GREEN_FLAGS))
    {
        nrf_gpio_pin_set(LED_GREEN_PIN);
    }
    else
    {
        nrf_gpio_pin_clear(LED_GREEN_PIN);
    }
    if (is_active_low(LED_RED_FLAGS))
    {
        nrf_gpio_pin_set(LED_RED_PIN);
    }
    else
    {
        nrf_gpio_pin_clear(LED_RED_PIN);
    }

    gpiote_teardown();
}
