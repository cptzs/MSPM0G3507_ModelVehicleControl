/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)



#define CPUCLK_FREQ                                                     80000000
/* Defines for SYSPLL_ERR_01 Workaround */
/* Represent 1.000 as 1000 */
#define FLOAT_TO_INT_SCALE                                               (1000U)
#define FCC_EXPECTED_RATIO                                                  1250
#define FCC_UPPER_BOUND                       (FCC_EXPECTED_RATIO * (1 + 0.003))
#define FCC_LOWER_BOUND                       (FCC_EXPECTED_RATIO * (1 - 0.003))

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);


/* Defines for PWM_0 */
#define PWM_0_INST                                                         TIMA0
#define PWM_0_INST_IRQHandler                                   TIMA0_IRQHandler
#define PWM_0_INST_INT_IRQN                                     (TIMA0_INT_IRQn)
#define PWM_0_INST_CLK_FREQ                                             16000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_0_C0_PORT                                                 GPIOB
#define GPIO_PWM_0_C0_PIN                                          DL_GPIO_PIN_8
#define GPIO_PWM_0_C0_IOMUX                                      (IOMUX_PINCM25)
#define GPIO_PWM_0_C0_IOMUX_FUNC                     IOMUX_PINCM25_PF_TIMA0_CCP0
#define GPIO_PWM_0_C0_IDX                                    DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWM_0_C1_PORT                                                 GPIOB
#define GPIO_PWM_0_C1_PIN                                         DL_GPIO_PIN_12
#define GPIO_PWM_0_C1_IOMUX                                      (IOMUX_PINCM29)
#define GPIO_PWM_0_C1_IOMUX_FUNC                     IOMUX_PINCM29_PF_TIMA0_CCP1
#define GPIO_PWM_0_C1_IDX                                    DL_TIMER_CC_1_INDEX



/* Defines for COMPARE_0 */
#define COMPARE_0_INST                                                  (TIMG12)
#define COMPARE_0_INST_IRQHandler                              TIMG12_IRQHandler
#define COMPARE_0_INST_INT_IRQN                                (TIMG12_INT_IRQn)
/* GPIO defines for channel 0 */
#define GPIO_COMPARE_0_C0_PORT                                             GPIOA
#define GPIO_COMPARE_0_C0_PIN                                     DL_GPIO_PIN_14
#define GPIO_COMPARE_0_C0_IOMUX                                  (IOMUX_PINCM36)
#define GPIO_COMPARE_0_C0_IOMUX_FUNC                IOMUX_PINCM36_PF_TIMG12_CCP0

/* Defines for COMPARE_1 */
#define COMPARE_1_INST                                                   (TIMG7)
#define COMPARE_1_INST_IRQHandler                               TIMG7_IRQHandler
#define COMPARE_1_INST_INT_IRQN                                 (TIMG7_INT_IRQn)
/* GPIO defines for channel 0 */
#define GPIO_COMPARE_1_C0_PORT                                             GPIOB
#define GPIO_COMPARE_1_C0_PIN                                     DL_GPIO_PIN_15
#define GPIO_COMPARE_1_C0_IOMUX                                  (IOMUX_PINCM32)
#define GPIO_COMPARE_1_C0_IOMUX_FUNC                 IOMUX_PINCM32_PF_TIMG7_CCP0

/* Defines for COMPARE_2 */
#define COMPARE_2_INST                                                   (TIMG6)
#define COMPARE_2_INST_IRQHandler                               TIMG6_IRQHandler
#define COMPARE_2_INST_INT_IRQN                                 (TIMG6_INT_IRQn)
/* GPIO defines for channel 0 */
#define GPIO_COMPARE_2_C0_PORT                                             GPIOB
#define GPIO_COMPARE_2_C0_PIN                                      DL_GPIO_PIN_6
#define GPIO_COMPARE_2_C0_IOMUX                                  (IOMUX_PINCM23)
#define GPIO_COMPARE_2_C0_IOMUX_FUNC                 IOMUX_PINCM23_PF_TIMG6_CCP0




/* Defines for TIMER_OEMT */
#define TIMER_OEMT_INST                                                  (TIMG0)
#define TIMER_OEMT_INST_IRQHandler                              TIMG0_IRQHandler
#define TIMER_OEMT_INST_INT_IRQN                                (TIMG0_INT_IRQn)
#define TIMER_OEMT_INST_LOAD_VALUE                                        (124U)
/* Defines for TIMER_SREVO */
#define TIMER_SREVO_INST                                                 (TIMA1)
#define TIMER_SREVO_INST_IRQHandler                             TIMA1_IRQHandler
#define TIMER_SREVO_INST_INT_IRQN                               (TIMA1_INT_IRQn)
#define TIMER_SREVO_INST_LOAD_VALUE                                         (0U)



/* Defines for UART_0 */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           40000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                         DL_GPIO_PIN_1
#define GPIO_UART_0_TX_PIN                                         DL_GPIO_PIN_0
#define GPIO_UART_0_IOMUX_RX                                      (IOMUX_PINCM2)
#define GPIO_UART_0_IOMUX_TX                                      (IOMUX_PINCM1)
#define GPIO_UART_0_IOMUX_RX_FUNC                       IOMUX_PINCM2_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                       IOMUX_PINCM1_PF_UART0_TX
#define UART_0_BAUD_RATE                                                (115200)
#define UART_0_IBRD_40_MHZ_115200_BAUD                                      (21)
#define UART_0_FBRD_40_MHZ_115200_BAUD                                      (45)
/* Defines for UART_1 */
#define UART_1_INST                                                        UART2
#define UART_1_INST_FREQUENCY                                           40000000
#define UART_1_INST_IRQHandler                                  UART2_IRQHandler
#define UART_1_INST_INT_IRQN                                      UART2_INT_IRQn
#define GPIO_UART_1_RX_PORT                                                GPIOB
#define GPIO_UART_1_TX_PORT                                                GPIOB
#define GPIO_UART_1_RX_PIN                                        DL_GPIO_PIN_16
#define GPIO_UART_1_TX_PIN                                        DL_GPIO_PIN_17
#define GPIO_UART_1_IOMUX_RX                                     (IOMUX_PINCM33)
#define GPIO_UART_1_IOMUX_TX                                     (IOMUX_PINCM43)
#define GPIO_UART_1_IOMUX_RX_FUNC                      IOMUX_PINCM33_PF_UART2_RX
#define GPIO_UART_1_IOMUX_TX_FUNC                      IOMUX_PINCM43_PF_UART2_TX
#define UART_1_BAUD_RATE                                                (115200)
#define UART_1_IBRD_40_MHZ_115200_BAUD                                      (21)
#define UART_1_FBRD_40_MHZ_115200_BAUD                                      (45)




/* Defines for SPI_1 */
#define SPI_1_INST                                                         SPI0
#define SPI_1_INST_IRQHandler                                   SPI0_IRQHandler
#define SPI_1_INST_INT_IRQN                                       SPI0_INT_IRQn
#define GPIO_SPI_1_PICO_PORT                                              GPIOA
#define GPIO_SPI_1_PICO_PIN                                       DL_GPIO_PIN_9
#define GPIO_SPI_1_IOMUX_PICO                                   (IOMUX_PINCM20)
#define GPIO_SPI_1_IOMUX_PICO_FUNC                   IOMUX_PINCM20_PF_SPI0_PICO
/* GPIO configuration for SPI_1 */
#define GPIO_SPI_1_SCLK_PORT                                              GPIOA
#define GPIO_SPI_1_SCLK_PIN                                      DL_GPIO_PIN_11
#define GPIO_SPI_1_IOMUX_SCLK                                   (IOMUX_PINCM22)
#define GPIO_SPI_1_IOMUX_SCLK_FUNC                   IOMUX_PINCM22_PF_SPI0_SCLK
#define GPIO_SPI_1_CS0_PORT                                               GPIOA
#define GPIO_SPI_1_CS0_PIN                                        DL_GPIO_PIN_8
#define GPIO_SPI_1_IOMUX_CS0                                    (IOMUX_PINCM19)
#define GPIO_SPI_1_IOMUX_CS0_FUNC                     IOMUX_PINCM19_PF_SPI0_CS0



/* Defines for ADC12_0 */
#define ADC12_0_INST                                                        ADC0
#define ADC12_0_INST_IRQHandler                                  ADC0_IRQHandler
#define ADC12_0_INST_INT_IRQN                                    (ADC0_INT_IRQn)
#define ADC12_0_ADCMEM_0                                      DL_ADC12_MEM_IDX_0
#define ADC12_0_ADCMEM_0_REF                   DL_ADC12_REFERENCE_VOLTAGE_EXTREF
#define ADC12_0_ADCMEM_0_REF_VOLTAGE_V                                      3.30
#define ADC12_0_ADCMEM_1                                      DL_ADC12_MEM_IDX_1
#define ADC12_0_ADCMEM_1_REF                   DL_ADC12_REFERENCE_VOLTAGE_EXTREF
#define ADC12_0_ADCMEM_1_REF_VOLTAGE_V                                      3.30
#define ADC12_0_ADCMEM_2                                      DL_ADC12_MEM_IDX_2
#define ADC12_0_ADCMEM_2_REF                   DL_ADC12_REFERENCE_VOLTAGE_EXTREF
#define ADC12_0_ADCMEM_2_REF_VOLTAGE_V                                      3.30
#define ADC12_0_ADCMEM_3                                      DL_ADC12_MEM_IDX_3
#define ADC12_0_ADCMEM_3_REF                   DL_ADC12_REFERENCE_VOLTAGE_EXTREF
#define ADC12_0_ADCMEM_3_REF_VOLTAGE_V                                      3.30
#define ADC12_0_ADCMEM_4                                      DL_ADC12_MEM_IDX_4
#define ADC12_0_ADCMEM_4_REF                   DL_ADC12_REFERENCE_VOLTAGE_EXTREF
#define ADC12_0_ADCMEM_4_REF_VOLTAGE_V                                      3.30
#define ADC12_0_ADCMEM_5                                      DL_ADC12_MEM_IDX_5
#define ADC12_0_ADCMEM_5_REF                   DL_ADC12_REFERENCE_VOLTAGE_EXTREF
#define ADC12_0_ADCMEM_5_REF_VOLTAGE_V                                      3.30
#define ADC12_0_ADCMEM_6                                      DL_ADC12_MEM_IDX_6
#define ADC12_0_ADCMEM_6_REF                   DL_ADC12_REFERENCE_VOLTAGE_EXTREF
#define ADC12_0_ADCMEM_6_REF_VOLTAGE_V                                      3.30
#define GPIO_ADC12_0_C0_PORT                                               GPIOA
#define GPIO_ADC12_0_C0_PIN                                       DL_GPIO_PIN_27
#define GPIO_ADC12_0_IOMUX_C0                                    (IOMUX_PINCM60)
#define GPIO_ADC12_0_IOMUX_C0_FUNC                (IOMUX_PINCM60_PF_UNCONNECTED)
#define GPIO_ADC12_0_C1_PORT                                               GPIOA
#define GPIO_ADC12_0_C1_PIN                                       DL_GPIO_PIN_26
#define GPIO_ADC12_0_IOMUX_C1                                    (IOMUX_PINCM59)
#define GPIO_ADC12_0_IOMUX_C1_FUNC                (IOMUX_PINCM59_PF_UNCONNECTED)
#define GPIO_ADC12_0_C2_PORT                                               GPIOA
#define GPIO_ADC12_0_C2_PIN                                       DL_GPIO_PIN_25
#define GPIO_ADC12_0_IOMUX_C2                                    (IOMUX_PINCM55)
#define GPIO_ADC12_0_IOMUX_C2_FUNC                (IOMUX_PINCM55_PF_UNCONNECTED)
#define GPIO_ADC12_0_C3_PORT                                               GPIOA
#define GPIO_ADC12_0_C3_PIN                                       DL_GPIO_PIN_24
#define GPIO_ADC12_0_IOMUX_C3                                    (IOMUX_PINCM54)
#define GPIO_ADC12_0_IOMUX_C3_FUNC                (IOMUX_PINCM54_PF_UNCONNECTED)
#define GPIO_ADC12_0_C7_PORT                                               GPIOA
#define GPIO_ADC12_0_C7_PIN                                       DL_GPIO_PIN_22
#define GPIO_ADC12_0_IOMUX_C7                                    (IOMUX_PINCM47)
#define GPIO_ADC12_0_IOMUX_C7_FUNC                (IOMUX_PINCM47_PF_UNCONNECTED)


/* Defines for VREF */
#define VREF_VOLTAGE_MV                                                     3300
#define GPIO_VREF_VREFPOS_PORT                                             GPIOA
#define GPIO_VREF_VREFPOS_PIN                                     DL_GPIO_PIN_23
#define GPIO_VREF_IOMUX_VREFPOS                                  (IOMUX_PINCM53)
#define GPIO_VREF_IOMUX_VREFPOS_FUNC                IOMUX_PINCM53_PF_UNCONNECTED
#define GPIO_VREF_VREFNEG_PORT                                             GPIOA
#define GPIO_VREF_VREFNEG_PIN                                     DL_GPIO_PIN_21
#define GPIO_VREF_IOMUX_VREFNEG                                  (IOMUX_PINCM46)
#define GPIO_VREF_IOMUX_VREFNEG_FUNC                IOMUX_PINCM46_PF_UNCONNECTED




/* Defines for DMA_CH_SPI0_TX */
#define DMA_CH_SPI0_TX_CHAN_ID                                               (4)
#define SPI_1_INST_DMA_TRIGGER                                (DMA_SPI0_TX_TRIG)
/* Defines for DMA_CH_UART0_RX */
#define DMA_CH_UART0_RX_CHAN_ID                                              (3)
#define UART_0_INST_DMA_TRIGGER_0                            (DMA_UART0_RX_TRIG)
/* Defines for DMA_CH_UART0_TX */
#define DMA_CH_UART0_TX_CHAN_ID                                              (2)
#define UART_0_INST_DMA_TRIGGER_1                            (DMA_UART0_TX_TRIG)
/* Defines for DMA_CH0 */
#define DMA_CH0_CHAN_ID                                                      (1)
#define UART_1_INST_DMA_TRIGGER_0                            (DMA_UART2_RX_TRIG)
/* Defines for DMA_CH1 */
#define DMA_CH1_CHAN_ID                                                      (0)
#define UART_1_INST_DMA_TRIGGER_1                            (DMA_UART2_TX_TRIG)


/* Port definition for Pin Group BUZZER */
#define BUZZER_PORT                                                      (GPIOA)

/* Defines for BUZZER0: GPIOA.16 with pinCMx 38 on package pin 9 */
#define BUZZER_BUZZER0_PIN                                      (DL_GPIO_PIN_16)
#define BUZZER_BUZZER0_IOMUX                                     (IOMUX_PINCM38)
/* Port definition for Pin Group LED */
#define LED_PORT                                                         (GPIOB)

/* Defines for LED0: GPIOB.0 with pinCMx 12 on package pin 47 */
#define LED_LED0_PIN                                             (DL_GPIO_PIN_0)
#define LED_LED0_IOMUX                                           (IOMUX_PINCM12)
/* Defines for LED1: GPIOB.1 with pinCMx 13 on package pin 48 */
#define LED_LED1_PIN                                             (DL_GPIO_PIN_1)
#define LED_LED1_IOMUX                                           (IOMUX_PINCM13)
/* Defines for LED2: GPIOB.4 with pinCMx 17 on package pin 52 */
#define LED_LED2_PIN                                             (DL_GPIO_PIN_4)
#define LED_LED2_IOMUX                                           (IOMUX_PINCM17)
/* Defines for LED3: GPIOB.5 with pinCMx 18 on package pin 53 */
#define LED_LED3_PIN                                             (DL_GPIO_PIN_5)
#define LED_LED3_IOMUX                                           (IOMUX_PINCM18)
/* Port definition for Pin Group BUTTON */
#define BUTTON_PORT                                                      (GPIOB)

/* Defines for UP: GPIOB.21 with pinCMx 49 on package pin 20 */
#define BUTTON_UP_PIN                                           (DL_GPIO_PIN_21)
#define BUTTON_UP_IOMUX                                          (IOMUX_PINCM49)
/* Defines for DOWN: GPIOB.25 with pinCMx 56 on package pin 27 */
#define BUTTON_DOWN_PIN                                         (DL_GPIO_PIN_25)
#define BUTTON_DOWN_IOMUX                                        (IOMUX_PINCM56)
/* Defines for LEFT: GPIOB.27 with pinCMx 58 on package pin 29 */
#define BUTTON_LEFT_PIN                                         (DL_GPIO_PIN_27)
#define BUTTON_LEFT_IOMUX                                        (IOMUX_PINCM58)
/* Defines for RIGHT: GPIOB.22 with pinCMx 50 on package pin 21 */
#define BUTTON_RIGHT_PIN                                        (DL_GPIO_PIN_22)
#define BUTTON_RIGHT_IOMUX                                       (IOMUX_PINCM50)
/* Defines for PREV: GPIOB.23 with pinCMx 51 on package pin 22 */
#define BUTTON_PREV_PIN                                         (DL_GPIO_PIN_23)
#define BUTTON_PREV_IOMUX                                        (IOMUX_PINCM51)
/* Defines for NEXT: GPIOB.24 with pinCMx 52 on package pin 23 */
#define BUTTON_NEXT_PIN                                         (DL_GPIO_PIN_24)
#define BUTTON_NEXT_IOMUX                                        (IOMUX_PINCM52)
/* Defines for ENTER: GPIOB.26 with pinCMx 57 on package pin 28 */
#define BUTTON_ENTER_PIN                                        (DL_GPIO_PIN_26)
#define BUTTON_ENTER_IOMUX                                       (IOMUX_PINCM57)
/* Defines for ESC: GPIOB.20 with pinCMx 48 on package pin 19 */
#define BUTTON_ESC_PIN                                          (DL_GPIO_PIN_20)
#define BUTTON_ESC_IOMUX                                         (IOMUX_PINCM48)
/* Port definition for Pin Group OLED */
#define OLED_PORT                                                        (GPIOA)

/* Defines for DC: GPIOA.7 with pinCMx 14 on package pin 49 */
#define OLED_DC_PIN                                              (DL_GPIO_PIN_7)
#define OLED_DC_IOMUX                                            (IOMUX_PINCM14)
/* Defines for NRST: GPIOA.10 with pinCMx 21 on package pin 56 */
#define OLED_NRST_PIN                                           (DL_GPIO_PIN_10)
#define OLED_NRST_IOMUX                                          (IOMUX_PINCM21)
/* Defines for ODOM_DIR: GPIOA.15 with pinCMx 37 on package pin 8 */
#define ENCODER_ODOM_DIR_PORT                                            (GPIOA)
// pins affected by this interrupt request:["ODOM_DIR"]
#define ENCODER_GPIOA_INT_IRQN                                  (GPIOA_INT_IRQn)
#define ENCODER_GPIOA_INT_IIDX                  (DL_INTERRUPT_GROUP1_IIDX_GPIOA)
#define ENCODER_ODOM_DIR_IIDX                               (DL_GPIO_IIDX_DIO15)
#define ENCODER_ODOM_DIR_PIN                                    (DL_GPIO_PIN_15)
#define ENCODER_ODOM_DIR_IOMUX                                   (IOMUX_PINCM37)
/* Defines for LEFT_DIR: GPIOB.18 with pinCMx 44 on package pin 15 */
#define ENCODER_LEFT_DIR_PORT                                            (GPIOB)
// pins affected by this interrupt request:["LEFT_DIR","RIGHT_DIR"]
#define ENCODER_GPIOB_INT_IRQN                                  (GPIOB_INT_IRQn)
#define ENCODER_GPIOB_INT_IIDX                  (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define ENCODER_LEFT_DIR_IIDX                               (DL_GPIO_IIDX_DIO18)
#define ENCODER_LEFT_DIR_PIN                                    (DL_GPIO_PIN_18)
#define ENCODER_LEFT_DIR_IOMUX                                   (IOMUX_PINCM44)
/* Defines for RIGHT_DIR: GPIOB.7 with pinCMx 24 on package pin 59 */
#define ENCODER_RIGHT_DIR_PORT                                           (GPIOB)
#define ENCODER_RIGHT_DIR_IIDX                               (DL_GPIO_IIDX_DIO7)
#define ENCODER_RIGHT_DIR_PIN                                    (DL_GPIO_PIN_7)
#define ENCODER_RIGHT_DIR_IOMUX                                  (IOMUX_PINCM24)
/* Port definition for Pin Group MOTOR */
#define MOTOR_PORT                                                       (GPIOB)

/* Defines for NSLP_A: GPIOB.10 with pinCMx 27 on package pin 62 */
#define MOTOR_NSLP_A_PIN                                        (DL_GPIO_PIN_10)
#define MOTOR_NSLP_A_IOMUX                                       (IOMUX_PINCM27)
/* Defines for PH_A: GPIOB.9 with pinCMx 26 on package pin 61 */
#define MOTOR_PH_A_PIN                                           (DL_GPIO_PIN_9)
#define MOTOR_PH_A_IOMUX                                         (IOMUX_PINCM26)
/* Defines for NSLP_B: GPIOB.14 with pinCMx 31 on package pin 2 */
#define MOTOR_NSLP_B_PIN                                        (DL_GPIO_PIN_14)
#define MOTOR_NSLP_B_IOMUX                                       (IOMUX_PINCM31)
/* Defines for PH_B: GPIOB.13 with pinCMx 30 on package pin 1 */
#define MOTOR_PH_B_PIN                                          (DL_GPIO_PIN_13)
#define MOTOR_PH_B_IOMUX                                         (IOMUX_PINCM30)
/* Port definition for Pin Group SERVO */
#define SERVO_PORT                                                       (GPIOB)

/* Defines for SERVO1: GPIOB.11 with pinCMx 28 on package pin 63 */
#define SERVO_SERVO1_PIN                                        (DL_GPIO_PIN_11)
#define SERVO_SERVO1_IOMUX                                       (IOMUX_PINCM28)
/* Defines for SERVO2: GPIOB.19 with pinCMx 45 on package pin 16 */
#define SERVO_SERVO2_PIN                                        (DL_GPIO_PIN_19)
#define SERVO_SERVO2_IOMUX                                       (IOMUX_PINCM45)
/* Port definition for Pin Group OEMT */
#define OEMT_PORT                                                        (GPIOA)

/* Defines for EN: GPIOA.28 with pinCMx 3 on package pin 35 */
#define OEMT_EN_PIN                                             (DL_GPIO_PIN_28)
#define OEMT_EN_IOMUX                                             (IOMUX_PINCM3)
/* Defines for ADDR0: GPIOA.31 with pinCMx 6 on package pin 39 */
#define OEMT_ADDR0_PIN                                          (DL_GPIO_PIN_31)
#define OEMT_ADDR0_IOMUX                                          (IOMUX_PINCM6)
/* Defines for ADDR1: GPIOA.30 with pinCMx 5 on package pin 37 */
#define OEMT_ADDR1_PIN                                          (DL_GPIO_PIN_30)
#define OEMT_ADDR1_IOMUX                                          (IOMUX_PINCM5)
/* Defines for ADDR2: GPIOA.29 with pinCMx 4 on package pin 36 */
#define OEMT_ADDR2_PIN                                          (DL_GPIO_PIN_29)
#define OEMT_ADDR2_IOMUX                                          (IOMUX_PINCM4)






/* Defines for MCAN0 */
#define MCAN0_INST                                                        CANFD0
#define GPIO_MCAN0_CAN_TX_PORT                                             GPIOA
#define GPIO_MCAN0_CAN_TX_PIN                                     DL_GPIO_PIN_12
#define GPIO_MCAN0_IOMUX_CAN_TX                                  (IOMUX_PINCM34)
#define GPIO_MCAN0_IOMUX_CAN_TX_FUNC               IOMUX_PINCM34_PF_CANFD0_CANTX
#define GPIO_MCAN0_CAN_RX_PORT                                             GPIOA
#define GPIO_MCAN0_CAN_RX_PIN                                     DL_GPIO_PIN_13
#define GPIO_MCAN0_IOMUX_CAN_RX                                  (IOMUX_PINCM35)
#define GPIO_MCAN0_IOMUX_CAN_RX_FUNC               IOMUX_PINCM35_PF_CANFD0_CANRX
#define MCAN0_INST_IRQHandler                                 CANFD0_IRQHandler
#define MCAN0_INST_INT_IRQN                                     CANFD0_INT_IRQn


/* Defines for MCAN0 MCAN RAM configuration */
#define MCAN0_INST_MCAN_STD_ID_FILT_START_ADDR     (0)
#define MCAN0_INST_MCAN_STD_ID_FILTER_NUM          (1)
#define MCAN0_INST_MCAN_EXT_ID_FILT_START_ADDR     (48)
#define MCAN0_INST_MCAN_EXT_ID_FILTER_NUM          (1)
#define MCAN0_INST_MCAN_TX_BUFF_START_ADDR         (148)
#define MCAN0_INST_MCAN_TX_BUFF_SIZE               (2)
#define MCAN0_INST_MCAN_FIFO_1_START_ADDR          (192)
#define MCAN0_INST_MCAN_FIFO_1_NUM                 (2)
#define MCAN0_INST_MCAN_TX_EVENT_START_ADDR        (164)
#define MCAN0_INST_MCAN_TX_EVENT_SIZE              (2)
#define MCAN0_INST_MCAN_EXT_ID_AND_MASK            (0x1FFFFFFFU)
#define MCAN0_INST_MCAN_RX_BUFF_START_ADDR         (208)
#define MCAN0_INST_MCAN_FIFO_0_START_ADDR          (172)
#define MCAN0_INST_MCAN_FIFO_0_NUM                 (3)

#define MCAN0_INST_MCAN_INTERRUPTS (DL_MCAN_INTERRUPT_RF0F | \
						DL_MCAN_INTERRUPT_RF0N)




/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);
void SYSCFG_DL_PWM_0_init(void);
void SYSCFG_DL_COMPARE_0_init(void);
void SYSCFG_DL_COMPARE_1_init(void);
void SYSCFG_DL_COMPARE_2_init(void);
void SYSCFG_DL_TIMER_OEMT_init(void);
void SYSCFG_DL_TIMER_SREVO_init(void);
void SYSCFG_DL_UART_0_init(void);
void SYSCFG_DL_UART_1_init(void);
void SYSCFG_DL_SPI_1_init(void);
void SYSCFG_DL_ADC12_0_init(void);
void SYSCFG_DL_VREF_init(void);
void SYSCFG_DL_DMA_init(void);

void SYSCFG_DL_SYSTICK_init(void);

void SYSCFG_DL_MCAN0_init(void);

bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
