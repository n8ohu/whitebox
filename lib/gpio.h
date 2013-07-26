#ifndef __WHITEBOX_GPIO__
#define __WHITEBOX_GPIO__

#define GPIO_OUTPUT_MODE (0 << 0)
#define GPIO_INPUT_MODE  (1 << 0)

void GPIO_config(unsigned gpio, int inout);
void GPIO_set_output(unsigned gpio, unsigned value);
int GPIO_get_input(unsigned gpio);

#endif /* __WHITEBOX_GPIO__ */
