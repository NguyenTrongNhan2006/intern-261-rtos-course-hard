#include "stm32f1xx.h"
#include "params.h"
#include "gpio.h"

static void tag_gpio_init(void)
{
    /* Bật clock GPIOB và AFIO. */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    /* Tắt JTAG nhưng giữ SWD; các remap khác để mặc định. */
    AFIO->MAPR = (2u << 24);

    gpio_config_output_pp_2mhz(
        W1E1_TAG_GPIO_PORT,
        W1E1_TAG_GPIO_PIN
    );

    /* Bắt đầu ở mức LOW. */
    gpio_clear(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN);
}

int main(void)
{
    tag_gpio_init();

    for (;;) {
        /* Phần tạo task và chạy scheduler sẽ được thêm tiếp theo. */
    }
}