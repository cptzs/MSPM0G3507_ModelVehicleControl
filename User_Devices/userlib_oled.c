/// @brief OLED_12864 驱动库函数
/// @author 赵帅
/// @version 2.11b (优化版本)
/// @date 2025-07-06

/// @note 适用范围
//   TI MSPM0G3507 (MSPM0 系列微控制器)
//   支持其他具有SPI+DMA功能的ARM Cortex-M0+微控制器

/// @note 硬件资源需求
// 需要独占单片机的以下硬件功能：
//  一个带有DMA功能的SPI主机发送器
//  两个推挽输出管脚 (DC控制、NRST复位)
// 内存需求：
//  约37 Byte 的FLASH区 (字符索引)
//  约1.2 KB 的RAM区 (显存GRAM)
//  约768 Byte 的FLASH区 (字库FontLib，已优化为const)

/// @todo 使用方法
/// 第一步：根据你的单片机，配置ti_msp_dl_config.h中的SPI和GPIO设置。
/// 第二步：在需要的位置调用USER_OLED_Init()初始化OLED。
/// 第三步：使用USER_OLED_putString()等函数进行显示操作。
/// 第四步：程序结束时可调用USER_OLED_DeInit()释放资源。

/// @note 引脚连接说明 (基于MSPM0G3507)
// OLED模块    ->  MSPM0G3507引脚
// D/C         ->  可配置GPIO (数据/命令选择)
// NRST        ->  可配置GPIO (复位控制)
// NSS/CS      ->  可配置GPIO (片选，可选)
// MOSI        ->  SPI_MOSI引脚
// SCLK        ->  SPI_SCLK引脚
// VCC         ->  3.3V
// GND         ->  GND

#include "userlib_oled.h"
#include <math.h>

// 请引用你用到的外设头文件
//************************自定义引用头文件部分开始*****************************//

//************************自定义引用头文件部分结束*****************************//

/// 用户自定义外设库基本参数文件
#define CPU_FRQ ((uint32_t)80000000) // 80MHz，常用于MSPM0G3507
#define BUS_FRQ ((uint32_t)80000000) // 外设时钟总线最高频率和CPU一样
#define CLK_PER_MS (BUS_FRQ / 1000)

// 优化：使用枚举替代宏定义，提供类型安全
typedef enum
{
  OLED_PIN_RESET = 0,
  OLED_PIN_SET = 1
} OLED_PinState_t;

typedef enum
{
  OLED_MODE_CMD = 0, // 写命令
  OLED_MODE_DATA = 1 // 写数据
} OLED_Mode_t;

// 宏定义，预编译设置OLED显示方向
#define isReversed OLED_Dir_Normal // 正向
// #define isReversed OLED_Dir_Reverse // 反向

//-----------------OLED_SPI管脚操作定义----------------
#define SPI1_NRST_GPIO_Port OLED_PORT
#define SPI1_NRST_Pin OLED_NRST_PIN
#define SPI1_DC_GPIO_Port OLED_PORT
#define SPI1_DC_Pin OLED_DC_PIN
#define SPI1_CH SPI_1_INST
#define SPI1_DMA_CH DMA_CH_SPI0_TX_CHAN_ID

//-----------------OLED_SPI基本参数定义----------------
// 优化：使用枚举和常量定义，提高代码可读性
#define OLED_WIDTH 128               // OLED屏幕宽度
#define OLED_HEIGHT 64               // OLED屏幕高度
#define OLED_MAX_COLUMN 128          // 最大列数
#define OLED_MAX_ROW 64              // 最大行数
#define OLED_MAX_PAGE 8              // 最大页数 (64/8)
#define OLED_CHAR_WIDTH 6            // 字符宽度（像素）
#define OLED_CHAR_HEIGHT 8           // 字符高度（像素）
#define OLED_CHARS_PER_ROW 21        // 每行最大字符数 (128/6)
#define OLED_DEFAULT_BRIGHTNESS 0xFF // 默认亮度

//-----------------OLED_SPI命令字定义----------------
// 初始化常用指令字
#define OLED_cmd_DisplayOFF 0xAE   // 关闭OLED显示
#define OLED_cmd_DisplayON 0xAF    // 启动OLED显示
#define OLED_cmd_ScrSyncGram 0xA4  // 启动显示，显示内容同步跟随RAM改变
#define OLED_cmd_ScrAsyncGram 0xA5 // 启动显示，显示内容不同步跟随RAM改变
#define OLED_cmd_Nop 0xE3          // 执行一个时钟周期的空指令

// 显示方向控制指令字
#define OLED_cmd_SetVirticalShift \
  0xD3 // 设置垂直方向显示偏移，后接1个指令字节，可选0-63
#define OLED_cmd_SetHorizonalShift \
  0x40                                   // 设置水平方向显示偏移，后接1个指令字节，可选0-127
#define OLED_cmd_VirticalShift_None 0x00 // 设置为垂直无偏移
#define OLED_cmd_HorizonalDirL2R 0xA1    // 设置左右扫描方向为从左到右
#define OLED_cmd_HorizonalDirR2L 0xA0    // 设置左右扫描方向为从右到左
#define OLED_cmd_VirticalDirU2D 0xC8     // 设置上下扫描方向为从上到下
#define OLED_cmd_VirticalDirD2U 0xC0     // 设置上下扫描方向为从下到上

// 显示效果控制指令字
#define OLED_cmd_NormalDisplay 0xA6  // 设置非反向显示模式
#define OLED_cmd_ReverseDisplay 0xA7 // 设置反向显示模式
#define OLED_cmd_SetContrast \
  0x81 // 设置对比度控制寄存器，后接1个指令字节，可选0-255对比度
#define OLED_cmd_SetMainClock \
  0xD5                                // 设置显示主电路的时钟脉冲源与预分频，控制帧率,，后接1个指令字节，设置时钟源和分频数
#define OLED_cmd_MainClock100FPS 0x80 // 设置预分频数，当前锁定为每秒100帧
#define OLED_cmd_SetPrechgPeriod \
  0xD9 // 设置预充周期，后接1个指令字节，各可选1-15时钟周期的预充电和预放电（影响残影效果）
#define OLED_cmd_PrechgPeriod_Normal \
  0xF1 // 设置为15时钟周期的预充和1时钟周期的放电

// 自动滚动功能指令字
#define OLED_cmd_AutoScrollRight \
  0x26 // 设置屏幕自动滚动模式为向右，后接4个指令字节
#define OLED_cmd_AutoScrollLeft \
  0x27 // 设置屏幕自动滚动模式为向左，后接4个指令字节
#define OLED_cmd_AutoScrollUp \
  0x29 // 设置屏幕自动滚动模式为向上，后接5个指令字节
#define OLED_cmd_AutoScrollDown \
  0x2A                                      // 设置屏幕自动滚动模式为向下，后接5个指令字节
#define OLED_cmd_SetVirticalScrollArea 0xA3 // 设置垂直滚动区域，后接2个指令字节
#define OLED_cmd_DisableAutoScroll 0x2E     // 关闭屏幕自动滚动
#define OLED_cmd_EnableAutoScroll 0x2F      // 启动屏幕自动滚动

// 内存寻址和移位模式指令字
#define OLED_cmd_SetRamAddrMode \
  0x20                                   // 设置内存地址模式，后接1个指令字节(0x00行/0x01列/0x02页)
#define OLED_cmd_RamAddrMode_Row 0x00    // 内存地址模式为行模式
#define OLED_cmd_RamAddrMode_Column 0x01 // 内存地址模式为列模式
#define OLED_cmd_RamAddrMode_Page 0x02   // 内存地址模式为页模式
#define OLED_cmd_SetColumnAddr \
  0x21 // 设置列地址,后接2个指令字节，分别为列坐标起始和终止地址，范围0-127
#define OLED_cmd_SetPageAddr \
  0x22 // 设置页地址,后接2个指令字节，分别为页起始和终止地址，范围0-7

// 未知作用指令字
#define OLED_cmd_SetComPinMode \
  0xDA                                  // 设置通讯管脚模式为硬件指定，后接1个指令字节，指示通信管脚配置模式
#define OLED_cmd_ComPinMode_Normal 0x12 // 设置通信管脚配置模式为默认
#define OLED_cmd_SetComplexRatio \
  0xA8                                    // 设置复用率，后接1个指令字节，指示15到64复用率
#define OLED_cmd_ComplexRatio_Normal 0x3F // 设置复用率为标准值
#define OLED_cmd_SetVCOMH \
  0xDB                             // 设置管脚低电平状态的最高参考电位（超过这个数认为变为了高电平），后接1个指令字节，可选0.65/0.75/0.83*Vcc
#define OLED_cmd_VCOMH_Normal 0x40 // 设置低电平参考电位为标准0.75*Vcc
#define OLED_cmd_SetChgPump 0x8D   // 设置电荷泵状态
#define OLED_cmd_ChgPump_OFF 0x10  // 设置电荷泵关闭
#define OLED_cmd_ChgPump_7V5 0x14  // 设置电荷泵为输出7.5V
//------------------------------------------------------------------

//----------ASCII码字库定义，优化：存储在Flash中节省RAM------------
// 优化：将字库声明为const，存储在Flash中而非RAM (节省768字节RAM)
extern const uint8_t FontLib[128][6];

//---------OLED显存定义，实际占用1.024KB的RAM区---------
uint8_t GRAM[8][128] = {0}; // 主显存缓冲区: 8行×128列 = 1024字节
uint8_t WaveRAM[128] = {0}; // 波形绘图辅助缓存: 128字节
uint8_t BarRAM[8] = {0};    // 进度条状态缓存: 8字节

//-------临时缓存区定义，占用FLASH和RAM-------
static char str_temp[22] = {0}; // 数字转换临时缓存: 22字节
uint8_t act_length = 0;         // 实际长度标记: 1字节

// 总内存占用统计:
// 显存使用: 1024字节 (GRAM)
// 波形缓存使用: 128字节 (WaveRAM)
// 进度条缓存使用: 8字节 (BarRAM)
// 数字转换缓存使用: 22字节 (str_temp)
// Flash使用: 768字节 (FontLib字库)
// 总计: 1024 + 128 + 8 + 22 = 1182字节 (RAM) + 768字节 (Flash)

//****************************需要用户自定义的函数开始**********************************//

/// @brief 需要自定义的函数：堵塞式延时函数
/// @param ms 要延时的ms数
void __User_OLED_Delay(uint16_t ms) { delay_cycles((uint32_t)CLK_PER_MS * ms); }

/// @brief 需要自定义的函数：初始化OLED用到的硬件外设
void __User_OLED_HW_Init()
{
  NVIC_EnableIRQ(SPI_1_INST_INT_IRQN); // 启用SPI1中断
}

/// @brief 需要自定义的函数：反初始化OLED
void __User_OLED_HW_DeInit() {}

/// @brief 需要自定义的函数：OLED选择发送数据模式
void __User_OLED_SetTxMode_Data()
{
  DL_GPIO_setPins(SPI1_DC_GPIO_Port, SPI1_DC_Pin);
}

/// @brief 需要自定义的函数：OLED选择发送指令模式
void __User_OLED_SetTxMode_Cmd()
{
  DL_GPIO_clearPins(SPI1_DC_GPIO_Port, SPI1_DC_Pin);
}

/// @brief 需要自定义的函数：OLED进入正常工作模式
void __User_OLED_GoNormal()
{
  DL_GPIO_setPins(SPI1_DC_GPIO_Port, SPI1_NRST_Pin);
}

/// @brief 需要自定义的函数：OLED进入复位模式
void __User_OLED_GoReset()
{
  DL_GPIO_clearPins(SPI1_DC_GPIO_Port, SPI1_NRST_Pin);
}

/// @brief 需要自定义的函数：DMA模式SPI发送函数
/// @param pData 要发送的数据头地址
/// @param Size 要发送的字节数
void __User_OLED_SPI_Transmit_DMA(uint8_t *pdat, uint16_t size)
{

  // 优化：添加DMA状态检查，避免重复启动
  if (DL_DMA_isChannelEnabled(DMA, SPI1_DMA_CH))
  {
    DL_DMA_disableChannel(DMA, SPI1_DMA_CH); // 先停止当前传输
  }

  // 设置源地址和目标地址
  DL_DMA_setSrcAddr(DMA, SPI1_DMA_CH, (uint32_t)pdat);
  DL_DMA_setDestAddr(DMA, SPI1_DMA_CH, (uint32_t)(&SPI1_CH->TXDATA));
  // 设置传输数据长度
  DL_DMA_setTransferSize(DMA, SPI1_DMA_CH, size);
  // 开启DMA通道
  DL_DMA_enableChannel(DMA, SPI1_DMA_CH);
}

/// @brief 需要自定义的函数：堵塞式SPI发送函数
/// @param pdat 要发送的数据头指针
/// @param size 要发送的字节数
/// @param timeout 超时时间
void __User_OLED_SPI_Transmit(uint8_t *pdat, uint16_t size, uint16_t timeout)
{
  DL_SPI_transmitData8(SPI1_CH, *pdat);
  __User_OLED_Delay(timeout);
}

//****************************需要用户自定义的函数结束**********************************//

uint8_t isOLED_Initialized = 0;
uint8_t isOLED_AtWork = 0;

/**
 * @brief OLED 硬件SPI模式发送1个字节函数(堵塞式查询)
 * @param _data 要发送的字节
 * @return none
 * @deprecated none
 **/
void __User_OLED_Send(uint8_t _data)
{
  __User_OLED_SPI_Transmit(&_data, 1, 10);
}

/**
 * @brief 硬件SPI接口的OLED屏幕初始化函数 (优化版本)
 * @return 初始化状态
 **/
OLED_Status_t USER_OLED_Init(void)
{
  if (isOLED_Initialized)
    return OLED_ERROR_INIT;

  __User_OLED_HW_Init();  // 初始化OLED用到的硬件
  __User_OLED_GoNormal(); // 先拉高复位管脚，准备复位操作
  // 注意，拉低复位管脚最少间隔90us才有效，要给充分的延时等待
  __User_OLED_Delay(10);
  __User_OLED_GoReset(); // 拉低复位管脚，启动复位进程
  // 注意，拉低复位管脚最少间隔90us才有效，要给充分的延时等待
  __User_OLED_Delay(10);                     //
  __User_OLED_GoNormal();                    // 恢复正常工作状态
  __User_OLED_Delay(200);                    // 这里要恢复10ms以上才能进入工作流程
  __User_OLED_SetTxMode_Cmd();               // 调整到发送指令模式
  __User_OLED_Send(OLED_cmd_SetChgPump);     // 设置电荷泵命令
  __User_OLED_Send(OLED_cmd_ChgPump_OFF);    // 电荷泵关闭
  __User_OLED_Send(OLED_cmd_DisplayOFF);     // 关闭显示
  __User_OLED_Send(0x40);                    //*未知指令，参考手册
  __User_OLED_Send(0x00);                    //
  __User_OLED_Send(OLED_cmd_SetContrast);    // 设置对比度
  __User_OLED_Send(OLED_DEFAULT_BRIGHTNESS); // 对比度为默认值
  if (isReversed == OLED_Dir_Reverse)
  {
    __User_OLED_Send(OLED_cmd_VirticalDirD2U);  // 设置横向从右到左扫描
    __User_OLED_Send(OLED_cmd_HorizonalDirR2L); // 设置纵向从下到上扫描
  }
  else
  {
    __User_OLED_Send(OLED_cmd_VirticalDirU2D);  // 设置横向从左到右扫描
    __User_OLED_Send(OLED_cmd_HorizonalDirL2R); // 设置纵向从上到下扫描
  }
  __User_OLED_Send(OLED_cmd_NormalDisplay);       // 设置非反向显示模式
  __User_OLED_Send(OLED_cmd_SetComplexRatio);     // 设置显示刷新预分频器
  __User_OLED_Send(OLED_cmd_ComplexRatio_Normal); // 设置预分频器为1
  __User_OLED_Send(OLED_cmd_SetVirticalShift);    // 设置垂直显示偏移
  __User_OLED_Send(OLED_cmd_VirticalShift_None);  // 设置为无垂直偏移
  __User_OLED_Send(OLED_cmd_SetMainClock);        // 设置显示刷新率时钟源
  __User_OLED_Send(0xF0);                         // 设置为100FPS显示刷新率
  __User_OLED_Send(OLED_cmd_SetPrechgPeriod);     // 设置预充电、放电周期
  __User_OLED_Send(OLED_cmd_PrechgPeriod_Normal); // 预充电、放电周期为标准
  __User_OLED_Send(OLED_cmd_SetComPinMode);       // 设置硬件通信端口模式
  __User_OLED_Send(OLED_cmd_ComPinMode_Normal);   // 设置硬件通信端口为标准
  __User_OLED_Send(OLED_cmd_SetVCOMH);            // 设置低电平参考电位
  __User_OLED_Send(OLED_cmd_VCOMH_Normal);        // 低电平参考电位为0.75*VCC
  __User_OLED_Send(OLED_cmd_SetRamAddrMode);      // 设置内存寻址方式
  __User_OLED_Send(OLED_cmd_RamAddrMode_Row);     // 内存寻址方式为页寻址
  __User_OLED_Send(OLED_cmd_ScrSyncGram);         // 设置显示画面同步于显存
  __User_OLED_Send(OLED_cmd_SetChgPump);          // 设置电荷泵
  __User_OLED_Send(OLED_cmd_ChgPump_7V5);         // 电荷泵输出7.5V
  __User_OLED_Delay(10);                          // 等待OLED屏幕整个启动完成
  isOLED_Initialized = 1;                         // 标记OLED屏幕已经初始化完成

  USER_OLED_CleanScreen();              // 显存清屏
  __User_OLED_SetTxMode_Cmd();          // 调整到发送指令模式
  __User_OLED_Send(OLED_cmd_DisplayON); // 启动显示屏
  __User_OLED_Delay(10);                // 等待OLED屏幕整个启动完成
  __User_OLED_SetTxMode_Data();         // 调整到发送数据模式
  __User_OLED_SPI_Transmit_DMA(&GRAM[0][0],
                               1024); // 开始DMA模式写入显存数据
  isOLED_AtWork = 1;                  // 标记OLED屏幕正在工作
  return OLED_OK;
}

/**
 * @brief 硬件SPI接口的OLED屏幕反初始化函数
 * @return 反初始化状态
 */
OLED_Status_t USER_OLED_DeInit(void)
{
  if (!isOLED_Initialized)
  {
    return OLED_ERROR_INIT; // 未初始化
  }

  __User_OLED_HW_DeInit();
  isOLED_AtWork = 0;
  isOLED_Initialized = 0;
  return OLED_OK;
}

/**
 * @brief 显存清屏函数
 * @param none
 * @return none
 **/
void USER_OLED_CleanScreen(void) { memset(&GRAM[0][0], 0x00, 1024); }

/// @brief  显存清行函数
/// @param row 要清除的行
void USER_OLED_CleanRow(uint8_t row) { memset(&GRAM[row][0], 0x00, 128); }

/**
 * @brief 向GRAM中指定的位置输出一个8x6大小的英文字符串
 * @param row 第几排，范围0-7
 * @param column 第几列，范围0-20
 * @param string 要输出的字符串(必须是标准ASCII表中的单字节字符)
 * @param length 字符串长度（每行可输出21字符，超出会从行最左侧覆盖）
 * @return none
 **/
void USER_OLED_putString(uint8_t row, uint8_t column, const char *str,
                         uint8_t length)
{
  // 优化：添加参数边界检查
  if (row >= 8 || column >= 21 || str == NULL || length == 0)
  {
    return; // 参数无效，直接返回
  }

  uint8_t column_bit = column * 6;           // 计算字符串第一个字符的地址
  uint8_t lastword_column;                   // 字符串最后一个字符的地址
  lastword_column = column_bit + length * 6; // 计算字符串最后一个字符的地址
  if (lastword_column > 128)                 // 如果超出了屏幕右边界
  {
    lastword_column = 128; // 将字符串最后一个字符的地址设置为屏幕右边界
  }

  // 优化：使用memcpy提高效率，避免逐字节复制
  while (column_bit < lastword_column && *str != '\0') // 遍历字符串
  {
    // 优化：检查字符是否在有效范围内
    uint8_t char_index = (uint8_t)(*str);
    if (char_index >= 128)
    {
      char_index = 32; // 超出范围的字符用空格代替
    }

    // 优化：使用memcpy提高复制效率
    memcpy(&GRAM[row][column_bit], FontLib[char_index], 6);
    column_bit += 6;
    str++;
  }
}

/**
 * @brief 向GRAM中指定的排写入一个带数值标签的百分比进度条
 * @param row 第几横排，范围0-7
 * @param percent 进度条百分比（0-100）
 * @return none
 * @deprecated none
 **/
void USER_OLED_DrawBar(uint8_t row, uint8_t percent)
{
  uint8_t diff = 0; // 差值

  if (percent > 100)
  {
    USER_OLED_putString(row, 0, "Err :Overflow", 13);
    return;
  }
  else if (BarRAM[row] == percent)
  {
    return;
  }
  else if (BarRAM[row] > percent)
  {
    diff = BarRAM[row] - percent;
    memset(&GRAM[row][25 + percent], 0x00, diff);
    USER_OLED_putUI16(row, 0, percent, 3);
  }
  else
  {
    diff = percent - BarRAM[row];
    memset(&GRAM[row][25 + BarRAM[row]], 0x7F, diff);
    USER_OLED_putUI16(row, 0, percent, 3);
  }
  BarRAM[row] = percent;
}

/**
 * @brief 向GRAM里写入一个点（原点在左下角）
 * @param x 横坐标，0-127
 * @param y 纵坐标，0-63
 * @return none
 * @deprecated none
 **/
void USER_OLED_SetPoint(uint8_t x, uint8_t y)
{
  uint8_t row = 7 - (y >> 3);                    // 计算当前点在第几横排
  uint8_t shifted = 0x80 >> (y % 8);             // 计算y坐标的偏移量
  *(GRAM[row] + x) = *(GRAM[row] + x) | shifted; // 写入点数据
}

/**
 * @brief 清除GRAM里的一个点（原点在左下角）
 * @param x 横坐标，0-127
 * @param y 纵坐标，0-63
 * @return none
 * @deprecated none
 **/
void USER_OLED_ResetPoint(uint8_t x, uint8_t y)
{
  uint8_t row = 7 - (y >> 3);                    // 计算当前点在第几横排
  uint8_t shifted = 0x80 >> (y % 8);             // 计算y坐标的偏移量
  shifted = ~shifted;                            // 按位取反，进而可以通过与操作清零
  *(GRAM[row] + x) = *(GRAM[row] + x) & shifted; // 清除点数据
}

/**
 * @brief 在显存中绘制任意线段 (优化版本)
 * @param x1 起点x坐标,0-127
 * @param y1 起点y坐标,0-63
 * @param x2 终点x坐标,0-127
 * @param y2 终点y坐标,0-63
 * @return none
 * @note 优化点：
 *   1. 添加边界检查，避免越界访问
 *   2. 特殊情况快速处理（水平线、垂直线、单点）
 *   3. 优化坐标变换，减少switch开销
 *   4. 使用更高效的Bresenham算法变体
 **/
void USER_OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
  // 优化1：边界检查
  if (x1 > 127 || x2 > 127 || y1 > 63 || y2 > 63)
    return;

  // 优化2：特殊情况快速处理
  if (x1 == x2 && y1 == y2) // 单点
  {
    USER_OLED_SetPoint(x1, y1);
    return;
  }

  if (x1 == x2) // 垂直线
  {
    if (y1 > y2)
    {
      uint8_t temp = y1;
      y1 = y2;
      y2 = temp;
    }
    for (uint8_t y = y1; y <= y2; y++)
      USER_OLED_SetPoint(x1, y);
    return;
  }

  if (y1 == y2) // 水平线
  {
    if (x1 > x2)
    {
      uint8_t temp = x1;
      x1 = x2;
      x2 = temp;
    }
    for (uint8_t x = x1; x <= x2; x++)
      USER_OLED_SetPoint(x, y1);
    return;
  }

  // 优化3：使用更高效的Bresenham算法（标准版本）
  int16_t dx = abs((int16_t)x2 - (int16_t)x1);
  int16_t dy = abs((int16_t)y2 - (int16_t)y1);
  int16_t sx = (x1 < x2) ? 1 : -1; // x方向步长
  int16_t sy = (y1 < y2) ? 1 : -1; // y方向步长
  int16_t err = dx - dy;           // 误差项

  uint8_t x = x1, y = y1;

  while (1)
  {
    USER_OLED_SetPoint(x, y);

    if (x == x2 && y == y2)
      break; // 到达终点

    int16_t e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      x += sx;
    } // x方向移动
    if (e2 < dx)
    {
      err += dx;
      y += sy;
    } // y方向移动
  }
}

/**
 * @brief 绘制虚线段
 * @param x1 起点X坐标 (0-127)
 * @param y1 起点Y坐标 (0-63)
 * @param x2 终点X坐标 (0-127)
 * @param y2 终点Y坐标 (0-63)
 * @param dash_length 虚线段长度 (建议值: 2-8)
 * @param gap_length 间隙长度 (建议值: 2-8)
 * @return none
 * @note 实现原理：
 *   1. 使用Bresenham算法计算线段路径
 *   2. 根据虚线模式控制是否绘制像素点
 *   3. 通过计数器实现虚线段和间隙的交替
 **/
void USER_OLED_DrawDashedLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t dash_length, uint8_t gap_length)
{
  // 边界检查
  if (x1 > 127 || x2 > 127 || y1 > 63 || y2 > 63)
    return;

  // 参数检查
  if (dash_length == 0 || gap_length == 0)
    return;

  // 特殊情况：单点
  if (x1 == x2 && y1 == y2)
  {
    USER_OLED_SetPoint(x1, y1);
    return;
  }

  // 使用Bresenham算法计算线段路径
  int16_t dx = abs((int16_t)x2 - (int16_t)x1);
  int16_t dy = abs((int16_t)y2 - (int16_t)y1);
  int16_t sx = (x1 < x2) ? 1 : -1; // x方向步长
  int16_t sy = (y1 < y2) ? 1 : -1; // y方向步长
  int16_t err = dx - dy;           // 误差项

  uint8_t x = x1, y = y1;
  uint8_t pixel_count = 0;                           // 像素计数器
  bool draw_pixel = true;                            // 当前是否绘制像素
  uint8_t pattern_length = dash_length + gap_length; // 虚线模式总长度

  while (1)
  {
    // 判断当前像素是否应该绘制
    uint8_t position_in_pattern = pixel_count % pattern_length;
    draw_pixel = (position_in_pattern < dash_length);

    if (draw_pixel)
    {
      USER_OLED_SetPoint(x, y);
    }

    if (x == x2 && y == y2)
      break; // 到达终点

    // Bresenham算法步进
    int16_t e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      x += sx;
    } // x方向移动
    if (e2 < dx)
    {
      err += dx;
      y += sy;
    } // y方向移动

    pixel_count++; // 增加像素计数
  }
}

/**
 * @brief 动态更新波形函数
 * @param value 要更新的一个值，即屏幕最右侧新点的纵坐标（0-63）
 * @return none
 * @deprecated none
 **/
void USER_OLED_UpdateWave(uint8_t value)
{
  uint8_t i = 0;
  value &= 0x3F;
  for (; i < 127;
       i++) // 127次循环左移，将所有竖列的点向左平移一列，实现波形连续滚动
  {
    GRAM[WaveRAM[i]][i] = 0;     // 清除第i竖列的点
    WaveRAM[i] = WaveRAM[i + 1]; // 记录第i+1竖列的点位置在第几横排
    GRAM[WaveRAM[i]][i] =
        GRAM[WaveRAM[i]][i + 1]; // 将第i+1竖列的点移动到第i竖列
  }
  GRAM[WaveRAM[126]][127] = 0;     // 清除第127列的点
  WaveRAM[127] = 7 - (value >> 3); // 记录新加点在第几横排
  USER_OLED_SetPoint(127, value);  // 在第127竖列绘制新加的点
}

/**
 * @brief 将uint16_t类型的数转换为字符串 (优化版本)
 * @param num 要转换的数
 * @param radix 要转换的进制 (2, 10, 16)
 * @param str 转换后的字符串
 * @param len 要求转换后的字符串长度，不超过17
 * @param fill 不足的补给定的字符
 * @return 转换成功返回true，否则返回false
 * @note 优化点：
 *   1. 修复了输入0时的处理问题
 *   2. 统一转换逻辑，减少代码重复
 *   3. 添加进制验证和边界检查
 *   4. 使用查找表优化十六进制转换
 *   5. 提前计算位移量，提高效率
 */
bool __uint16_to_str(uint16_t num, uint8_t radix, uint8_t *str, uint8_t len,
                     char fill)
{
  // 优化1：参数验证
  if (str == NULL || len == 0 || len > 17)
    return false;

  // 优化2：进制验证
  if (radix != 2 && radix != 10 && radix != 16)
    return false;

  // 优化3：使用查找表优化十六进制转换
  static const char hex_chars[] = "0123456789ABCDEF";

  uint8_t str_temp[17];
  uint8_t i = 0;
  uint8_t shift_bits = 0;

  // 优化4：预计算位移量，避免重复计算
  switch (radix)
  {
  case 2:
    shift_bits = 1;
    break; // 右移1位
  case 16:
    shift_bits = 4;
    break; // 右移4位
  default:
    shift_bits = 0;
    break; // 十进制用除法
  }

  // 优化5：统一转换逻辑，特殊处理0
  if (num == 0)
  {
    str_temp[i++] = '0';
  }
  else
  {
    while (num > 0)
    {
      if (radix == 10)
      {
        str_temp[i++] = (num % 10) + '0';
        num /= 10;
      }
      else
      {
        str_temp[i++] = hex_chars[num & (radix - 1)]; // 使用位运算取余
        num >>= shift_bits;                           // 使用预计算的位移量
      }
    }
  }

  uint8_t length = i;

  // 优化6：边界检查
  if (length > len)
    return false;

  // 优化7：直接计算并填充
  uint8_t fill_count = len - length;

  // 填充前缀字符
  for (i = 0; i < fill_count; i++)
    str[i] = fill;

  // 优化8：逆序复制时避免重复计算
  for (uint8_t j = 0; j < length; j++)
    str[i++] = str_temp[length - 1 - j];

  // 优化9：简化最后一位处理
  if (len > 0 && str[len - 1] == fill)
    str[len - 1] = '0';

  str[len] = '\0';
  return true;
}

/**
 * @brief OLED显示8像素无符号16进制数函数
 * @param row 第几横排，范围0-7
 * @param column 第几列，范围0-127
 * @param nummber 16位及以下的无符号整数,范围0到65535
 * @param length 字符串长度，范围1-4
 * @return none
 **/
void USER_OLED_putX16(uint8_t row, uint8_t column, uint16_t nummber,
                      uint8_t length)
{
  if (__uint16_to_str(nummber, 16, (uint8_t *)&str_temp[0], length,
                      '0'))                             // 将数字转换为字符串
    USER_OLED_putString(row, column, str_temp, length); // 显示字符串
}

/**
 * @brief OLED显示小号8像素无符号整数函数
 * @param row 第几横排，范围0-7
 * @param column 第几列，范围0-127
 * @param nummber 16位及以下的无符号整数,范围0到65535
 * @param length 字符串长度，范围1-5
 * @return none
 **/
void USER_OLED_putUI16(uint8_t row, uint8_t column, uint16_t nummber,
                       uint8_t length)
{
  if (__uint16_to_str(nummber, 10, (uint8_t *)&str_temp[0], length,
                      ' '))                             // 将数字转换为字符串
    USER_OLED_putString(row, column, str_temp, length); // 显示字符串
}

/**
 * @brief OLED显示小号8像素10进制整数函数 (优化版本)
 * @param row 第几横排，范围0-7
 * @param column 第几列，范围0-127
 * @param nummber 16位及以下的整数,范围-32768到32767
 * @param length 字符串长度，范围1-6
 * @return none
 * @note 优化点：
 *   1. 添加参数边界检查，提高健壮性
 *   2. 简化代码逻辑，减少重复代码
 *   3. 处理特殊情况-32768（避免溢出）
 *   4. 优化类型转换，减少不必要的强制转换
 **/
void USER_OLED_putI16(uint8_t row, uint8_t column, int16_t nummber,
                      uint8_t length)
{
  // 优化1：参数边界检查
  if (row >= 8 || column >= 21 || length == 0 || length > 6)
  {
    return; // 参数无效，直接返回
  }

  uint16_t abs_value;
  uint8_t start_pos = 0;

  // 优化2：处理负数和特殊情况
  if (nummber < 0)
  {
    // 优化3：处理-32768特殊情况，避免溢出
    if (nummber == INT16_MIN) // -32768
    {
      abs_value = 32768U; // 使用无符号字面量避免溢出
    }
    else
    {
      abs_value = (uint16_t)(-nummber);
    }

    str_temp[0] = '-';
    start_pos = 1;

    // 优化4：检查长度是否足够放置负号
    if (length < 2)
    {
      return; // 长度不够显示负号和数字
    }
  }
  else
  {
    abs_value = (uint16_t)nummber;
  }

  // 优化5：统一调用转换函数，减少代码重复
  if (!__uint16_to_str(abs_value, 10,
                       (uint8_t *)&str_temp[start_pos],
                       length - start_pos, ' '))
  {
    return; // 转换失败，直接返回
  }

  // 优化6：去掉不必要的强制转换
  USER_OLED_putString(row, column, str_temp, length);
}

/**
 * @brief OLED显示小号8像素浮点数函数 (优化版本)
 * @param row 第几横排，范围0-7
 * @param column 第几列，范围0-127
 * @param nummber 单精度浮点数，范围-32768到32767
 * @param int_length 整数部分长度(包括符号位)，范围1-6
 * @param float_length 小数部分长度，范围1-4
 * @note 优化点：
 *   1. 添加参数边界检查和特殊值处理
 *   2. 复用现有的__uint16_to_str函数
 *   3. 简化代码逻辑，提高可读性
 *   4. 优化内存使用和性能
 *   5. 修复精度问题，添加四舍五入
 */
void USER_OLED_putFloat(uint8_t row, uint8_t column, float nummber,
                        uint8_t int_length, uint8_t float_length)
{
  // 优化1：参数边界检查
  if (row >= 8 || column >= 21 || int_length == 0 || int_length > 6 ||
      float_length == 0 || float_length > 4)
  {
    return; // 参数无效，直接返回
  }

  // 优化2：特殊值处理
  if (isnan(nummber))
  {
    USER_OLED_putString(row, column, "NaN", 3);
    return;
  }
  if (isinf(nummber))
  {
    USER_OLED_putString(row, column, nummber > 0 ? "Inf" : "-Inf",
                        nummber > 0 ? 3 : 4);
    return;
  }

  static char temp_str[16] = {0}; // 优化：减小缓冲区大小
  uint8_t pos = 0;                // 当前位置指针
  bool is_negative = false;       // 是否为负数

  // 优化3：处理负数
  if (nummber < 0.0f)
  {
    if (int_length < 2)
    {
      return; // 长度不够显示负号
    }
    is_negative = true;
    nummber = -nummber;
    temp_str[pos++] = '-';
  }

  // 优化4：分离整数和小数部分
  uint16_t int_part = (uint16_t)nummber;
  float frac_part = nummber - (float)int_part;

  // 优化5：添加四舍五入处理
  static const uint16_t pow10[] = {1, 10, 100, 1000, 10000};
  frac_part += 0.5f / pow10[float_length]; // 四舍五入修正

  if (frac_part >= 1.0f)
  {
    int_part++;
    frac_part -= 1.0f;
  }

  // 优化6：修复整数部分处理 - 直接在当前位置写入
  uint8_t int_actual_length = is_negative ? int_length - 1 : int_length;

  // 将整数部分转换为字符串并直接写入temp_str
  char int_str[8] = {0};
  if (!__uint16_to_str(int_part, 10, (uint8_t *)int_str, int_actual_length, ' '))
  {
    return; // 转换失败
  }

  // 复制整数部分到temp_str
  for (uint8_t i = 0; i < int_actual_length; i++)
  {
    temp_str[pos++] = int_str[i];
  }

  // 优化7：添加小数点
  temp_str[pos++] = '.';

  // 优化8：处理小数部分
  uint16_t frac_int = (uint16_t)(frac_part * pow10[float_length]);

  // 确保小数部分不会因为精度问题超出范围
  if (frac_int >= pow10[float_length])
  {
    frac_int = pow10[float_length] - 1;
  }

  // 将小数部分转换为字符串并直接写入temp_str
  char frac_str[8] = {0};
  if (!__uint16_to_str(frac_int, 10, (uint8_t *)frac_str, float_length, '0'))
  {
    return; // 转换失败
  }

  // 复制小数部分到temp_str
  for (uint8_t i = 0; i < float_length; i++)
  {
    temp_str[pos++] = frac_str[i];
  }

  // 优化9：添加字符串结束符并显示结果
  temp_str[pos] = '\0';
  uint8_t total_length = int_length + float_length + 1;
  USER_OLED_putString(row, column, temp_str, total_length);
}

/**
 * @brief 向GRAM中指定的位置输出一个8x6大小的英文字符
 * @param row 第几排，范围0-7
 * @param column 第几列，范围0-20
 * @param ch 要输出的字符(必须是标准ASCII表中的单字节字符)
 * @return none
 **/
void USER_OLED_putChar(uint8_t row, uint8_t column, char ch)
{
  // 参数边界检查
  if (row >= 8 || column >= 21)
  {
    return; // 参数无效，直接返回
  }

  uint8_t column_bit = column * 6;

  // 检查字符是否在有效范围内
  uint8_t char_index = (uint8_t)ch;
  if (char_index >= 128)
  {
    char_index = 32; // 超出范围的字符用空格代替
  }

  // 复制字符数据到GRAM
  memcpy(&GRAM[row][column_bit], FontLib[char_index], 6);
}

/**
 * @brief 在GRAM中绘制矩形
 * @param x1 左上角x坐标,0-127
 * @param y1 左上角y坐标,0-63
 * @param x2 右下角x坐标,0-127
 * @param y2 右下角y坐标,0-63
 * @param fill 是否填充矩形
 * @return none
 **/
void USER_OLED_DrawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool fill)
{
  // 边界检查
  if (x1 > 127 || x2 > 127 || y1 > 63 || y2 > 63)
    return;

  // 确保x1 <= x2, y1 <= y2
  if (x1 > x2)
  {
    uint8_t temp = x1;
    x1 = x2;
    x2 = temp;
  }
  if (y1 > y2)
  {
    uint8_t temp = y1;
    y1 = y2;
    y2 = temp;
  }

  if (fill)
  {
    // 填充矩形
    for (uint8_t y = y1; y <= y2; y++)
    {
      for (uint8_t x = x1; x <= x2; x++)
      {
        USER_OLED_SetPoint(x, y);
      }
    }
  }
  else
  {
    // 绘制矩形边框
    USER_OLED_DrawLine(x1, y1, x2, y1); // 上边
    USER_OLED_DrawLine(x1, y2, x2, y2); // 下边
    USER_OLED_DrawLine(x1, y1, x1, y2); // 左边
    USER_OLED_DrawLine(x2, y1, x2, y2); // 右边
  }
}

/**
 * @brief 在GRAM中绘制圆形
 * @param x0 圆心x坐标,0-127
 * @param y0 圆心y坐标,0-63
 * @param radius 半径
 * @param fill 是否填充圆形
 * @return none
 **/
void USER_OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t radius, bool fill)
{
  // 边界检查
  if (x0 > 127 || y0 > 63 || radius == 0)
    return;

  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;

  while (x >= y)
  {
    if (fill)
    {
      // 填充圆形 - 绘制水平线
      if (x0 >= x && x0 + x <= 127)
      {
        for (int16_t i = x0 - x; i <= x0 + x; i++)
        {
          if (i >= 0 && i <= 127)
          {
            if (y0 + y <= 63)
              USER_OLED_SetPoint(i, y0 + y);
            if (y0 - y >= 0)
              USER_OLED_SetPoint(i, y0 - y);
          }
        }
      }
      if (x0 >= y && x0 + y <= 127)
      {
        for (int16_t i = x0 - y; i <= x0 + y; i++)
        {
          if (i >= 0 && i <= 127)
          {
            if (y0 + x <= 63)
              USER_OLED_SetPoint(i, y0 + x);
            if (y0 - x >= 0)
              USER_OLED_SetPoint(i, y0 - x);
          }
        }
      }
    }
    else
    {
      // 绘制圆形边框 - 8个对称点
      if (x0 + x <= 127 && y0 + y <= 63)
        USER_OLED_SetPoint(x0 + x, y0 + y);
      if (x0 - x >= 0 && y0 + y <= 63)
        USER_OLED_SetPoint(x0 - x, y0 + y);
      if (x0 + x <= 127 && y0 - y >= 0)
        USER_OLED_SetPoint(x0 + x, y0 - y);
      if (x0 - x >= 0 && y0 - y >= 0)
        USER_OLED_SetPoint(x0 - x, y0 - y);
      if (x0 + y <= 127 && y0 + x <= 63)
        USER_OLED_SetPoint(x0 + y, y0 + x);
      if (x0 - y >= 0 && y0 + x <= 63)
        USER_OLED_SetPoint(x0 - y, y0 + x);
      if (x0 + y <= 127 && y0 - x >= 0)
        USER_OLED_SetPoint(x0 + y, y0 - x);
      if (x0 - y >= 0 && y0 - x >= 0)
        USER_OLED_SetPoint(x0 - y, y0 - x);
    }

    if (err <= 0)
    {
      y += 1;
      err += 2 * y + 1;
    }

    if (err > 0)
    {
      x -= 1;
      err -= 2 * x + 1;
    }
  }
}

/**
 * @brief 判断角度是否在指定范围内
 * @param angle 要检查的角度
 * @param start_angle 起始角度
 * @param end_angle 结束角度
 * @return true 如果角度在范围内，false 否则
 **/
static bool IsAngleInRange(uint16_t angle, uint16_t start_angle, uint16_t end_angle)
{
  if (start_angle <= end_angle)
  {
    // 正常情况：起始角度小于结束角度
    return (angle >= start_angle && angle <= end_angle);
  }
  else
  {
    // 跨越0度的情况：起始角度大于结束角度
    return (angle >= start_angle || angle <= end_angle);
  }
}

/**
 * @brief 绘制圆弧
 * @param x0 圆心X坐标
 * @param y0 圆心Y坐标
 * @param radius 半径
 * @param start_angle 起始角度（度，0度为向右，逆时针增加）
 * @param end_angle 结束角度（度，0度为向右，逆时针增加）
 * @return none
 **/
void USER_OLED_DrawArc(uint8_t x0, uint8_t y0, uint8_t radius, uint16_t start_angle, uint16_t end_angle)
{
  // 边界检查
  if (x0 > 127 || y0 > 63 || radius == 0)
    return;

  // 角度标准化到0-359度
  start_angle = start_angle % 360;
  end_angle = end_angle % 360;

  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;

  while (x >= y)
  {
    // 计算8个对称点的角度并检查是否在圆弧范围内
    // 第一象限点 (x, y)
    if (x0 + x <= 127 && y0 - y >= 0)
    {
      uint16_t angle = (uint16_t)(atan2(-(double)y, (double)x) * 180.0 / 3.14159265359) % 360;
      if (IsAngleInRange(angle, start_angle, end_angle))
        USER_OLED_SetPoint(x0 + x, y0 - y);
    }

    // 第二象限点 (-x, y)
    if (x0 - x >= 0 && y0 - y >= 0)
    {
      uint16_t angle = (uint16_t)(atan2(-(double)y, -(double)x) * 180.0 / 3.14159265359 + 360.0) % 360;
      if (IsAngleInRange(angle, start_angle, end_angle))
        USER_OLED_SetPoint(x0 - x, y0 - y);
    }

    // 第三象限点 (-x, -y)
    if (x0 - x >= 0 && y0 + y <= 63)
    {
      uint16_t angle = (uint16_t)(atan2((double)y, -(double)x) * 180.0 / 3.14159265359 + 360.0) % 360;
      if (IsAngleInRange(angle, start_angle, end_angle))
        USER_OLED_SetPoint(x0 - x, y0 + y);
    }

    // 第四象限点 (x, -y)
    if (x0 + x <= 127 && y0 + y <= 63)
    {
      uint16_t angle = (uint16_t)(atan2((double)y, (double)x) * 180.0 / 3.14159265359 + 360.0) % 360;
      if (IsAngleInRange(angle, start_angle, end_angle))
        USER_OLED_SetPoint(x0 + x, y0 + y);
    }

    // 对称点 (y, x), (-y, x), (-y, -x), (y, -x)
    if (x != y) // 避免重复绘制
    {
      // 点 (y, -x)
      if (x0 + y <= 127 && y0 + x <= 63)
      {
        uint16_t angle = (uint16_t)(atan2((double)x, (double)y) * 180.0 / 3.14159265359 + 360.0) % 360;
        if (IsAngleInRange(angle, start_angle, end_angle))
          USER_OLED_SetPoint(x0 + y, y0 + x);
      }

      // 点 (-y, -x)
      if (x0 - y >= 0 && y0 + x <= 63)
      {
        uint16_t angle = (uint16_t)(atan2((double)x, -(double)y) * 180.0 / 3.14159265359 + 360.0) % 360;
        if (IsAngleInRange(angle, start_angle, end_angle))
          USER_OLED_SetPoint(x0 - y, y0 + x);
      }

      // 点 (-y, x)
      if (x0 - y >= 0 && y0 - x >= 0)
      {
        uint16_t angle = (uint16_t)(atan2(-(double)x, -(double)y) * 180.0 / 3.14159265359 + 360.0) % 360;
        if (IsAngleInRange(angle, start_angle, end_angle))
          USER_OLED_SetPoint(x0 - y, y0 - x);
      }

      // 点 (y, x)
      if (x0 + y <= 127 && y0 - x >= 0)
      {
        uint16_t angle = (uint16_t)(atan2(-(double)x, (double)y) * 180.0 / 3.14159265359 + 360.0) % 360;
        if (IsAngleInRange(angle, start_angle, end_angle))
          USER_OLED_SetPoint(x0 + y, y0 - x);
      }
    }

    if (err <= 0)
    {
      y += 1;
      err += 2 * y + 1;
    }

    if (err > 0)
    {
      x -= 1;
      err -= 2 * x + 1;
    }
  }
}

/**
 * @brief 清除波形显示
 * @return none
 **/
void USER_OLED_ClearWave(void)
{
  memset(WaveRAM, 0, 128);
  // 清除GRAM中的波形数据
  for (uint8_t i = 0; i < 128; i++)
  {
    for (uint8_t row = 0; row < 8; row++)
    {
      GRAM[row][i] = 0;
    }
  }
}

/**
 * @brief 设置OLED对比度
 * @param contrast 对比度值，范围0-255
 * @return none
 **/
void USER_OLED_SetContrast(uint8_t contrast)
{
  if (!isOLED_Initialized)
    return;

  __User_OLED_SetTxMode_Cmd();
  __User_OLED_Send(OLED_cmd_SetContrast);
  __User_OLED_Send(contrast);
  __User_OLED_SetTxMode_Data();
}

/**
 * @brief 设置OLED显示开关
 * @param on 是否开启显示
 * @return none
 **/
void USER_OLED_SetDisplayOn(bool on)
{
  if (!isOLED_Initialized)
    return;

  __User_OLED_SetTxMode_Cmd();
  __User_OLED_Send(on ? OLED_cmd_DisplayON : OLED_cmd_DisplayOFF);
  __User_OLED_SetTxMode_Data();
}

/**
 * @brief 设置OLED反向显示
 * @param invert 是否反向显示
 * @return none
 **/
void USER_OLED_InvertDisplay(bool invert)
{
  if (!isOLED_Initialized)
    return;

  __User_OLED_SetTxMode_Cmd();
  __User_OLED_Send(invert ? OLED_cmd_ReverseDisplay : OLED_cmd_NormalDisplay);
  __User_OLED_SetTxMode_Data();
}

/// @brief OLED中断处理函数
/// @note 该函数在SPI0中断发生时被调用，处理DMA传输完成的事件。
/// @details 当SPI0的DMA传输完成时，清除中断标志，并开始新的DMA传输。
void SPI0_IRQHandler()
{
  DL_SPI_IIDX itSource = DL_SPI_getPendingInterrupt(SPI0);
  if (itSource == DL_SPI_IIDX_DMA_DONE_TX)
  {
    DL_SPI_clearInterruptStatus(SPI0, SPI_CPU_INT_IMASK_DMA_DONE_TX_MASK);
    __User_OLED_SPI_Transmit_DMA(&GRAM[0][0], 1024); // 开始DMA模式写入显存数据
  }
}

// 常用ASCII表 - 优化：使用const存储在Flash中节省RAM
// 字符偏移量0
/************************************6*8的点阵************************************/
const uint8_t FontLib[128][6] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  1 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  2 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  3 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  4 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  5 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  6 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  7 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  8 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp  9 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 10 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 11 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 12 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 13 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 14 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 15 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 16 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 17 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 18 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 19 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 20 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 21 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 22 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 23 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 24 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 25 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 26 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 27 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 28 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 29 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 30 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 31 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sp 32 */
    0x00, 0x00, 0x00, 0x2f, 0x00, 0x00, /* ! 33 */
    0x00, 0x00, 0x07, 0x00, 0x07, 0x00, /* " 34 */
    0x00, 0x14, 0x7f, 0x14, 0x7f, 0x14, /* # 35 */
    0x00, 0x24, 0x2a, 0x7f, 0x2a, 0x12, /* $ 36 */
    0x00, 0x62, 0x64, 0x08, 0x13, 0x23, /* % 37 */
    0x00, 0x36, 0x49, 0x55, 0x22, 0x50, /* & 38 */
    0x00, 0x00, 0x05, 0x03, 0x00, 0x00, /* ' 39 */
    0x00, 0x00, 0x1c, 0x22, 0x41, 0x00, /* ( 40 */
    0x00, 0x00, 0x41, 0x22, 0x1c, 0x00, /* ) 41 */
    0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, /* * 42 */
    0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, /* + 43 */
    0x00, 0x00, 0x00, 0xA0, 0x60, 0x00, /* , 44 */
    0x00, 0x08, 0x08, 0x08, 0x08, 0x08, /* - 45 */
    0x00, 0x00, 0x60, 0x60, 0x00, 0x00, /* . 46 */
    0x00, 0x20, 0x10, 0x08, 0x04, 0x02, /* / 47 */
    0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E, /* 0 48 */
    0x00, 0x00, 0x42, 0x7F, 0x40, 0x00, /* 1 49 */
    0x00, 0x42, 0x61, 0x51, 0x49, 0x46, /* 2 50 */
    0x00, 0x21, 0x41, 0x45, 0x4B, 0x31, /* 3 51 */
    0x00, 0x18, 0x14, 0x12, 0x7F, 0x10, /* 4 52 */
    0x00, 0x27, 0x45, 0x45, 0x45, 0x39, /* 5 53 */
    0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30, /* 6 54 */
    0x00, 0x01, 0x71, 0x09, 0x05, 0x03, /* 7 55 */
    0x00, 0x36, 0x49, 0x49, 0x49, 0x36, /* 8 56 */
    0x00, 0x06, 0x49, 0x49, 0x29, 0x1E, /* 9 57 */
    0x00, 0x00, 0x36, 0x36, 0x00, 0x00, /* : 58 */
    0x00, 0x00, 0x56, 0x36, 0x00, 0x00, /* ; 59 */
    0x00, 0x08, 0x14, 0x22, 0x41, 0x00, /* < 60 */
    0x00, 0x14, 0x14, 0x14, 0x14, 0x14, /* = 61 */
    0x00, 0x00, 0x41, 0x22, 0x14, 0x08, /* > 62 */
    0x00, 0x02, 0x01, 0x51, 0x09, 0x06, /* ? 63 */
    0x00, 0x32, 0x49, 0x59, 0x51, 0x3E, /* @ 64 */
    0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C, /* A 65 */
    0x00, 0x7F, 0x49, 0x49, 0x49, 0x36, /* B 66 */
    0x00, 0x3E, 0x41, 0x41, 0x41, 0x22, /* C 67 */
    0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C, /* D 68 */
    0x00, 0x7F, 0x49, 0x49, 0x49, 0x41, /* E 69 */
    0x00, 0x7F, 0x09, 0x09, 0x09, 0x01, /* F 70 */
    0x00, 0x3E, 0x41, 0x49, 0x49, 0x7A, /* G 71 */
    0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F, /* H 72 */
    0x00, 0x00, 0x41, 0x7F, 0x41, 0x00, /* I 73 */
    0x00, 0x20, 0x40, 0x41, 0x3F, 0x01, /* J 74 */
    0x00, 0x7F, 0x08, 0x14, 0x22, 0x41, /* K 75 */
    0x00, 0x7F, 0x40, 0x40, 0x40, 0x40, /* L 76 */
    0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F, /* M 77 */
    0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F, /* N 78 */
    0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E, /* O 79 */
    0x00, 0x7F, 0x09, 0x09, 0x09, 0x06, /* P 80 */
    0x00, 0x3E, 0x41, 0x51, 0x21, 0x5E, /* Q 81 */
    0x00, 0x7F, 0x09, 0x19, 0x29, 0x46, /* R 82 */
    0x00, 0x46, 0x49, 0x49, 0x49, 0x31, /* S 83 */
    0x00, 0x01, 0x01, 0x7F, 0x01, 0x01, /* T 84 */
    0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F, /* U 85 */
    0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F, /* V 86 */
    0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F, /* W 87 */
    0x00, 0x63, 0x14, 0x08, 0x14, 0x63, /* X 88 */
    0x00, 0x07, 0x08, 0x70, 0x08, 0x07, /* Y 89 */
    0x00, 0x61, 0x51, 0x49, 0x45, 0x43, /* Z 90 */
    0x00, 0x00, 0x7F, 0x41, 0x41, 0x00, /* [ 91 */
    0x00, 0x55, 0x2A, 0x55, 0x2A, 0x55, /* \ 92 */
    0x00, 0x00, 0x41, 0x41, 0x7F, 0x00, /* ] 93 */
    0x00, 0x04, 0x02, 0x01, 0x02, 0x04, /* ^ 94 */
    0x00, 0x40, 0x40, 0x40, 0x40, 0x40, /* _ 95 */
    0x00, 0x04, 0x08, 0x00, 0x00, 0x00, /* ` 96 */
    0x00, 0x20, 0x54, 0x54, 0x54, 0x78, /* a 97 */
    0x00, 0x7F, 0x48, 0x44, 0x44, 0x38, /* b 98 */
    0x00, 0x38, 0x44, 0x44, 0x44, 0x20, /* c 99 */
    0x00, 0x38, 0x44, 0x44, 0x48, 0x7F, /* d 100 */
    0x00, 0x38, 0x54, 0x54, 0x54, 0x18, /* e 101 */
    0x00, 0x08, 0x7E, 0x09, 0x01, 0x02, /* f 102 */
    0x00, 0x18, 0xA4, 0xA4, 0xA4, 0x7C, /* g 103 */
    0x00, 0x7F, 0x08, 0x04, 0x04, 0x78, /* h 104 */
    0x00, 0x00, 0x44, 0x7D, 0x40, 0x00, /* i 105 */
    0x00, 0x40, 0x80, 0x84, 0x7D, 0x00, /* j 106 */
    0x00, 0x7F, 0x10, 0x28, 0x44, 0x00, /* k 107 */
    0x00, 0x00, 0x41, 0x7F, 0x40, 0x00, /* l 108 */
    0x00, 0x7C, 0x04, 0x18, 0x04, 0x78, /* m 109 */
    0x00, 0x7C, 0x08, 0x04, 0x04, 0x78, /* n 110 */
    0x00, 0x38, 0x44, 0x44, 0x44, 0x38, /* o 111 */
    0x00, 0xFC, 0x24, 0x24, 0x24, 0x18, /* p 112 */
    0x00, 0x18, 0x24, 0x24, 0x18, 0xFC, /* q 113 */
    0x00, 0x7C, 0x08, 0x04, 0x04, 0x08, /* r 114 */
    0x00, 0x48, 0x54, 0x54, 0x54, 0x20, /* s 115 */
    0x00, 0x04, 0x3F, 0x44, 0x40, 0x20, /* t 116 */
    0x00, 0x3C, 0x40, 0x40, 0x20, 0x7C, /* u 117 */
    0x00, 0x1C, 0x20, 0x40, 0x20, 0x1C, /* v 118 */
    0x00, 0x3C, 0x40, 0x30, 0x40, 0x3C, /* w 119 */
    0x00, 0x44, 0x28, 0x10, 0x28, 0x44, /* x 120 */
    0x00, 0x1C, 0xA0, 0xA0, 0xA0, 0x7C, /* y 121 */
    0x00, 0x44, 0x64, 0x54, 0x4C, 0x44, /* z 122 */
    0x00, 0x30, 0xCC, 0x00, 0x00, 0x00, /* { 123 */
    0x00, 0x7C, 0x00, 0x00, 0x00, 0x00, /* | 124 */
    0x00, 0xCC, 0x30, 0x00, 0x00, 0x00, /* } 125 */
    0x10, 0x08, 0x08, 0x10, 0x10, 0x08, /* ~ 126 */
    0x10, 0x08, 0x08, 0x10, 0x10, 0x08, /* SP 127 */
};
