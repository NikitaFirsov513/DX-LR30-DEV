








#include "driver_gpio.h"





/*
 *  函数名：void LedGpioInit(void)
 *  输入参数：无
 *  输出参数：无
 *  返回值：无
 *  函数作用：初始化LED的引脚，配置为上拉推挽输出
*/
void LedGpioInit(void)
{
    // 定义GPIO的结构体变量
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    // 使能LED的GPIO对应的时钟

    GPIO_InitStruct.Pin = R_LED_GPIO_PIN; //| G_LED_GPIO_PIN | B_LED_GPIO_PIN;        
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // 设置为推挽输出模式
    GPIO_InitStruct.Pull = GPIO_PULLUP;         // 默认上拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;// 引脚输出速度设置为慢

    // 初始化引脚配置
    HAL_GPIO_Init(R_LED_GPIO_PORT, &GPIO_InitStruct);
}
