/********************************** (C) COPYRIGHT *******************************
 * File Name          : ws2812.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2024年11月19日
 * Description        : ws2812驱动层
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "config.h"
#include "ws2812.h"

__attribute__((aligned(4))) uint32_t flowing_buf[17]={
//        0x00000F,0x000F00,0x0f0000,0x000F00,0x000808,
//        0x00000F,0x000F00,0x0f0000,0x000F00,0x000f00,
        0x00000F,0x000F00,0x0f0000,0x000F00,0x000808,
        0x0c0909,0x060201,
};
__attribute__((aligned(4))) uint32_t flowing_buf_usb[17]={
        0x000800,0x000800,0x000800,0x000800,0x000800,
        0x000800,0x000800,0x000800,0x000800,0x000800,
        0x000800,0x000800,0x000800,0x000800,0x000800,
        0x000800,0x000800,
};
__attribute__((aligned(4))) uint32_t flowing_buf_ble[17]={
        0x000008,0x000008,0x000008,0x000008,0x000008,
        0x000008,0x000008,0x000008,0x000008,0x000008,
        0x000008,0x000008,0x000008,0x000008,0x000008,
        0x000008,0x000008,
};
__attribute__((aligned(4))) uint32_t flowing_buf_24[17]={
        0x080800,0x080800,0x080800,0x080800,0x080800,
        0x080800,0x080800,0x080800,0x080800,0x080800,
        0x080800,0x080800,0x080800,0x080800,0x080800,
        0x080800,0x080800,
};
__attribute__((aligned(4))) uint32_t Pwmout_buf[408]={0};

/*********************************************************************
 * @fn      Ws2812_Init
 *
 * @brief   ws2812初始化，pwm+DMA控制，中断开启。
 *
 * @param   none
 *
 * @return  none
 */
void Ws2812_Init(void)
{
    uint8_t i;
    GPIOA_ModeCfg(GPIO_Pin_11, GPIO_ModeOut_PP_5mA);
    GPIOA_ResetBits(GPIO_Pin_11);//pwm管脚初始化
    GPIOB_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    GPIOB_ResetBits(GPIO_Pin_9);  //开启背光
    //这里选择rgb的控制周期为1.2us
    //至于为什么是1.2us可参考ws2812手册数据传输时间章节
    PRINT("TMR2 DMA PWM\n");
    TMR2_PWMInit(High_Level, PWM_Times_1);
    TMR2_PWMCycleCfg(72); // 主频是60Mhz 每秒震荡60M次 震荡60次为1微秒(60*1.2=72)
    //TMR2_ClearITFlag(TMR1_2_IT_DMA_END);
    //TMR2_ITCfg(ENABLE, TMR1_2_IT_DMA_END);
    //PFIC_EnableIRQ(TMR2_IRQn);
}

/*********************************************************************
 * @fn      process_RGB_to_pwm
 *
 * @brief   功能函数，将RGB数据缓冲区的数据转换成pwm+dma数据
 *
 * @param           *buf : RGB color数据缓冲区起始地址
 *          num_elements : 数据长
 *               *output : pwm输出数据缓冲区起始地址,该缓冲区大小应为颜色缓冲区大小的24倍
 *
 * @return  none
 */
__HIGH_CODE
void process_RGB_to_pwm(const uint32_t *buf, uint32_t num_elements, uint32_t *output) {
    uint32_t output_index = 0;
    uint32_t byte_accumulator = 0;
    int bit_count = 0;
    for (uint32_t i = 0; i < num_elements; ++i) {
        uint32_t buf_data = buf[i];
        uint32_t current_word_1 = (buf_data&0xff0000)>>16;
        uint32_t current_word_2 = (buf_data&0xff00)>>8;
        uint32_t current_word_3 =  buf_data&0xff;  //调整RGB顺序
        uint32_t current_word_4 = (current_word_2<<16)+(current_word_1<<8)+current_word_3;
        uint32_t current_word = current_word_4 & 0x00ffffff; // 取低24位
        for (int j = 0; j < 24; ++j) {
            uint32_t bit = (current_word << j) & 0x800000; // 高位在前
            output[output_index] = bit ? 0x000024 : 0x000014;
            output_index++;
        }
    }
}
/*********************************************************************
 * @fn      Ws2812_move_control
 *
 * @brief   RGB动态控制
 *
 * @param   *pwm_buf - 初始颜色数据缓冲区
 * @param       mode - 动态类型
 * @param        len - 控制长
 *
 * @return  none
 */
void Ws2812_move_control(uint32_t *color_buf,RGB_move_control mode,uint32_t len)
{
    switch (mode) {
        case 0://流水灯模式,左移
        {
            uint32_t firstElement = color_buf[0];
               for (int i = 0; i < len - 1; i++) {
                   color_buf[i] = color_buf[i + 1];
               }
               color_buf[len - 1] = firstElement;
        }
           break;
        case 1://流水灯模式,右移
        {
            int lastElement = color_buf[len - 1];
            for (int i = len - 1; i > 0; i--) {
                color_buf[i] = color_buf[i - 1];
            }
            color_buf[0] = lastElement;
        }
            break;
        default:
            break;
    }
}

/*********************************************************************
 * @fn      PWM_DATA_DMA_send
 *
 * @brief   PWM通过DMA将数据下发到RGB
 *
 * @param   *Pwmout_buf_start - 待下发数据缓冲区起始地址
 * @param   len               - 待下发数据长
 *
 * @return  none
 */
void PWM_DATA_DMA_send(uint32_t* Pwmout_buf_start,uint32_t len)
{
    TMR2_Enable();
    //以下DMA配置与输出操作最好使用寄存器操作，库函数有问题
    R16_TMR2_DMA_BEG = (uint32_t)Pwmout_buf_start;  //DMA起始地址
    R16_TMR2_DMA_END = (uint32_t)(Pwmout_buf_start+len);    //DMA结束地址
    R8_TMR2_CTRL_MOD |= RB_TMR_OUT_EN;              //TMR2_PWMEnable();
    R8_TMR2_CTRL_DMA = RB_TMR_DMA_ENABLE;           //DMA输出使能
    TMR2_ClearITFlag(TMR1_2_IT_DMA_END);
    TMR2_ITCfg(ENABLE, TMR1_2_IT_DMA_END);
    PFIC_EnableIRQ(TMR2_IRQn);
}

/*********************************************************************
 * @fn      TMR2_IRQHandler
 *
 * @brief   TMR2中断函数
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void TMR2_IRQHandler(void)
{
    if(TMR2_GetITFlag(TMR0_3_IT_CYC_END))
    {
        TMR2_ClearITFlag(TMR0_3_IT_CYC_END);
        /* 计数器计满，硬件自动清零，重新开始计数 */
        /* 用户可自行添加需要的处理 */
    }
    if(TMR2_GetITFlag(TMR1_2_IT_DMA_END))
    {
        TMR2_ClearITFlag(TMR1_2_IT_DMA_END);
        TMR2_Disable();
        TMR2_PWMDisable();
        TMR2_ITCfg(DISABLE, TMR1_2_IT_DMA_END);
        PFIC_DisableIRQ(TMR2_IRQn);
        /* DMA 结束 */
    }
}
