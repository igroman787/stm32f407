#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_uart.h"
#include "stm32f4xx_hal_usart.h"
#include "stm32f4xx_hal_adc.h"
#include <string.h>
#include <stdlib.h>



int uart2_buffer_fill = 0;
char uart2RxBuffer[1024] = {0};
static UART_HandleTypeDef huart2;

int uart3_buffer_fill = 0;
char uart3RxBuffer[1024] = {0};
static UART_HandleTypeDef huart3;

ADC_HandleTypeDef g_AdcHandle_gas;
ADC_HandleTypeDef g_AdcHandle_temp;
char str_myid[512] = {0};
int license = 0;
int needGsmReboot = 0;

unsigned int milliseconds = 0;
unsigned long timestamps = 0;
unsigned long last_update = 0;
unsigned long last_datasend = 0;
unsigned long last_datasave = 0;

int memory_size = 3600;
char memory[3600][3][11];

int buffer_size = 100;
char buffer[100][2][11];


void bubble_sort(int *data, int size) {
   int i, j;
   for (i = 0; i < size; ++i) {
      for (j = size - 1; j > i; --j) {
         if (data[j] < data[j-1]) {
            int t = data[j - 1];
            data[j - 1] = data[j];
            data[j] = t;
         }
      }
   }
}

int GetClearArraySize(int *data, int size)
{
    int output = 0;
    for (int i = 0; i < size; i++)
    {
        if (data[i] != 0x00)
        {
            output++;
        }
    }
    return output;
}

int GetMediana(int* data, int size)
{
    int clear_size = GetClearArraySize(data, size);
    int mediana = data[clear_size/2];
    return mediana;
}

int GetAverage(int* data, int size)
{
	long a = 0;
	for (int i = 0; i < size; i++)
	{
        if (data[i] == 0x00)
        {
            continue;
        }
        
		a = a + data[i];
	}
	return a/size;
}

int IndexOf(char* str1, char* str2)
{
    if (strstr(str1, str2) == NULL)
    {
        return -1;
    }
    
    char* istr = strstr (str1, str2);
    int output = istr-str1+0;
    return output;
}

void Countup()
{
    milliseconds++;
    
    if (milliseconds > 1000)
    {
        milliseconds = 0;
        timestamps++;
    }
}

void WriteToMemory(char* gas, char* temp)
{
	int b = 0;
	char str_timestamps[11] = {0};
    sprintf(str_timestamps, "%lu", timestamps);
	
	for (int i = 0; i < memory_size; i++)
	{
		if (memory[i][0][0] == 0x00)
		{
            strcpy(memory[i][0], str_timestamps);
            strcpy(memory[i][1], gas);
            strcpy(memory[i][2], temp);
			
			if (i == memory_size - 1)
			{
				b = 0;
			}
			else
			{
				b = i + 1;
			}
			
            memset(memory[b], 0, sizeof(memory[b])); // очистка памяти.
            
            return;
		}
	}
}

void WriteToBuffer(char* gas, char* temp)
{
    int b = 0;
    for (int i = 0; i < buffer_size; i++)
	{
		if (buffer[i][0][0] == 0x00)
		{
            strcpy(buffer[i][0], gas);
            strcpy(buffer[i][1], temp);
			
			if (i == memory_size - 1)
			{
				b = 0;
			}
			else
			{
				b = i + 1;
			}
			
            memset(buffer[b], 0, sizeof(buffer[b])); // очистка буфера.
            
            return;
		}
	}
}

int GetSize(char* str)
{
    int i = 0;
    int output = 0;
    while(1)
    {
        if (str[i] == 0x00)
        {
            output = i;
            break;
        }
        i++;
    }
    return output;
}

void ResetNumber()
{
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
}

void ShowNumber(int n)
{
    ResetNumber();
    while (n--)
    {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_RESET);
    }
}

void ClearUart3()
{
    // Отчистить буфер приема UART
    uart3_buffer_fill = 0;
    memset(uart3RxBuffer, 0, sizeof(uart3RxBuffer)); // очистка буфера для вывода.
    
}

void ClearUart2()
{
    // Отчистить буфер приема UART
    uart2_buffer_fill = 0;
    memset(uart2RxBuffer, 0, sizeof(uart2RxBuffer)); // очистка буфера для вывода.
    
}

void send_to_uart(uint8_t data)
{
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
    while(!(USART2->SR & USART_SR_TC));
    USART2->DR=data;
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
}

void send_str(char * string)
{
    
    uint8_t i=0;
    while(string[i])
    {
        send_to_uart(string[i]);
        i++;
    }
    send_to_uart('\r');
    send_to_uart('\n');
}

char get_from_uart()
{
    uint8_t uart_data = 0x00;
    // Смотрим буфер UART
    if (USART2->SR & USART_SR_RXNE) // ... не пришло ли что-то в UART ?
    {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
        uart_data=USART2->DR; //Считываем то что пришло в переменную...
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
    }
    return uart_data;
}

void GsmTransmit(char * string)
{
    send_str(string);
}

void ShowError(int n, int needSTM32Reboot)
{
    // Отключить GSM модуль
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    
    // Высветить номер ошибки на цифровом индикаторе
    //ShowNumber(n);
    
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
    for (int i = 0; i < 30; i++)
    {
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_14);
        HAL_Delay(1000);
        if (i == 29 && needSTM32Reboot == 1)
        {
            // Самоперезагрузка
            HAL_NVIC_SystemReset();
        }
    }
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
}

int CheckGsmAns(char* str, int needSTM32Reboot)
{
	int result = 0;
	
    // Проверка на вшивость
    if (GetSize(str) == 0)
    {
        ShowError(101, needSTM32Reboot);
		result = -1;
    }
    else if(IndexOf(str, "OK") > -1)
    {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
        HAL_Delay(300);
    }
    else if (IndexOf(str, "ERROR") > -1)
    {
        ShowError(101, needSTM32Reboot);
		result = -1;
    }
	
	if (result == -1)
	{
		needGsmReboot = 1;
	}
	
	return result;
}

void Uart2RxStrWaiting(char* waitingStr, int timeout)
{
    int time = 0;
    while(1)
    {
        if (IndexOf(uart2RxBuffer, "ERROR") > -1)
        {
            break;
        }
        if (IndexOf(uart2RxBuffer, waitingStr) > -1)
        {
            break;
        }
        if (time > timeout)
        {
			break;
        }
        HAL_Delay(1);
        time++;
    }
}

void Uart3RxStrWaiting(char* waitingStr, int timeout)
{
    int time = 0;
    while(1)
    {
        if (IndexOf(uart3RxBuffer, "ERROR") > -1)
        {
            break;
        }
        if (IndexOf(uart3RxBuffer, waitingStr) > -1)
        {
            break;
        }
        if (time > timeout)
        {
			break;
        }
        HAL_Delay(1);
        time++;
    }
}

int GsmCmdWithStrWaiting(char* cmd, char* waitingStr, int timeout, int needSTM32Reboot)
{
	int result = 0;
	
    // Отчистить буфер приема UART
    uart2_buffer_fill = 0;
    memset(uart2RxBuffer, 0, sizeof(uart2RxBuffer)); // очистка буфера для ввода.
    
    // Отправить команду
    GsmTransmit(cmd);
    
    // Ожидание конца сообщений
    Uart2RxStrWaiting(waitingStr, timeout);
    
    // Проверить ответ
    result = CheckGsmAns(uart2RxBuffer, needSTM32Reboot);
	
	return result;
}

int GsmCmd(char* cmd, int needSTM32Reboot)
{
	int result = 0;
	result = GsmCmdWithStrWaiting(cmd, "OK", 30000, needSTM32Reboot);
	return result;
}

int GsmHttpRequest(char* req)
{
	int result = 0;
	
    // Отчистить буфер приема UART
    ClearUart2();
    
    // Отправить команду
    result = GsmCmdWithStrWaiting("AT+CIPSEND", ">", 3000, 0);
    send_str(req);
    send_to_uart(0x1A);
    
    // Ожидание конца сообщений
    Uart2RxStrWaiting("The request was accepted", 30000);
    
    // Проверить ответ
    result = CheckGsmAns(uart2RxBuffer, 0);
	
	return result;
}

void Wait(int n)
{
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
    for(int i = 0; i < n; i++)
    {
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
        HAL_Delay(1000);
    }
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
}

void FastWait(int n)
{
    n = n*5;
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
    for(int i = 0; i < n; i++)
    {
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
        HAL_Delay(200);
    }
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
}

void Im_Init_GPIO()
{
    // Разрешить тактирование порта.
    __HAL_RCC_GPIOA_CLK_ENABLE();
    //__HAL_RCC_GPIOB_CLK_ENABLE();
    //__HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    //__HAL_RCC_GPIOE_CLK_ENABLE();
    
    // Настройки порта.
    GPIO_InitTypeDef button1;
    button1.Pin   = GPIO_PIN_0;                // Ввод.
    button1.Mode  = GPIO_MODE_INPUT;           // Цифровой вход.
    button1.Pull  = GPIO_NOPULL;               // Без подтяжки.
    button1.Speed = GPIO_SPEED_FREQ_HIGH;      // Максимальная скорость.
    HAL_GPIO_Init(GPIOA, &button1);
    
    // Настройки порта.
    GPIO_InitTypeDef led;
    led.Pin   = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
    led.Mode  = GPIO_MODE_OUTPUT_PP;        // Цифровой выход.
    led.Pull  = GPIO_NOPULL;                // Без подтяжки.
    led.Speed = GPIO_SPEED_FREQ_HIGH;       // Максимальная скорость.
    HAL_GPIO_Init(GPIOD, &led);
    
    // Настройка порта вывода
    GPIO_InitTypeDef out;
    out.Pin   = GPIO_PIN_4 | GPIO_PIN_5;
    out.Mode  = GPIO_MODE_OUTPUT_PP;       // Цифровой выход.
    out.Pull  = GPIO_NOPULL;               // Без подтяжки.
    out.Speed = GPIO_SPEED_FREQ_HIGH;      // Максимальная скорость.
    HAL_GPIO_Init(GPIOA, &out);
}

void Im_Init_GasSensor()
{
    GPIO_InitTypeDef gpioInit;
 
    __GPIOC_CLK_ENABLE();
    __ADC1_CLK_ENABLE();
 
    gpioInit.Pin = GPIO_PIN_1;
    gpioInit.Mode = GPIO_MODE_ANALOG;
    gpioInit.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &gpioInit);
 
    HAL_NVIC_SetPriority(ADC_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);
 
    ADC_ChannelConfTypeDef adcChannel;
 
    g_AdcHandle_gas.Instance = ADC1;
 
    g_AdcHandle_gas.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
    g_AdcHandle_gas.Init.Resolution = ADC_RESOLUTION_12B;
    g_AdcHandle_gas.Init.ScanConvMode = DISABLE;
    g_AdcHandle_gas.Init.ContinuousConvMode = ENABLE;
    g_AdcHandle_gas.Init.DiscontinuousConvMode = DISABLE;
    g_AdcHandle_gas.Init.NbrOfDiscConversion = 0;
    g_AdcHandle_gas.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    g_AdcHandle_gas.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
    g_AdcHandle_gas.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    g_AdcHandle_gas.Init.NbrOfConversion = 1;
    g_AdcHandle_gas.Init.DMAContinuousRequests = ENABLE;
    g_AdcHandle_gas.Init.EOCSelection = DISABLE;
 
    HAL_ADC_Init(&g_AdcHandle_gas);
 
    adcChannel.Channel = ADC_CHANNEL_11;
    adcChannel.Rank = 1;
    adcChannel.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    adcChannel.Offset = 0;
 
    if (HAL_ADC_ConfigChannel(&g_AdcHandle_gas, &adcChannel) != HAL_OK)
    {
        asm("bkpt 255");
    }
    
    HAL_ADC_Start(&g_AdcHandle_gas);
    
}

void Im_Init_TempSensor()
{
    GPIO_InitTypeDef gpioInit;
 
    __GPIOC_CLK_ENABLE();
    __ADC2_CLK_ENABLE();
 
    gpioInit.Pin = GPIO_PIN_2;
    gpioInit.Mode = GPIO_MODE_ANALOG;
    gpioInit.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &gpioInit);
 
    HAL_NVIC_SetPriority(ADC_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);
 
    ADC_ChannelConfTypeDef adcChannel;
 
    g_AdcHandle_temp.Instance = ADC2;
 
    g_AdcHandle_temp.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
    g_AdcHandle_temp.Init.Resolution = ADC_RESOLUTION_12B;
    g_AdcHandle_temp.Init.ScanConvMode = DISABLE;
    g_AdcHandle_temp.Init.ContinuousConvMode = ENABLE;
    g_AdcHandle_temp.Init.DiscontinuousConvMode = DISABLE;
    g_AdcHandle_temp.Init.NbrOfDiscConversion = 0;
    g_AdcHandle_temp.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    g_AdcHandle_temp.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
    g_AdcHandle_temp.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    g_AdcHandle_temp.Init.NbrOfConversion = 1;
    g_AdcHandle_temp.Init.DMAContinuousRequests = ENABLE;
    g_AdcHandle_temp.Init.EOCSelection = DISABLE;
 
    HAL_ADC_Init(&g_AdcHandle_temp);
 
    adcChannel.Channel = ADC_CHANNEL_12;
    adcChannel.Rank = 1;
    adcChannel.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    adcChannel.Offset = 0;
 
    if (HAL_ADC_ConfigChannel(&g_AdcHandle_temp, &adcChannel) != HAL_OK)
    {
        asm("bkpt 255");
    }
    
    HAL_ADC_Start(&g_AdcHandle_temp);
    
}

void Im_Init_UART()
{
    /* Peripheral clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();
    
    /**USART2 GPIO Configuration    
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX 
    */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.Pin = GPIO_PIN_2;
    GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.Alternate = GPIO_AF7_USART2;
    GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.Pin = GPIO_PIN_3;
    GPIO_InitStructure.Mode = GPIO_MODE_AF_OD;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);
     
    //Заполняем структуру настройками UARTa
    huart2.Instance           = USART2;
    huart2.Init.BaudRate      = 115200;
    huart2.Init.WordLength    = UART_WORDLENGTH_8B;
    huart2.Init.StopBits      = UART_STOPBITS_1;
    huart2.Init.Parity        = UART_PARITY_NONE;
    huart2.Init.HwFlowCtl     = UART_HWCONTROL_NONE;
    huart2.Init.Mode          = UART_MODE_TX_RX;
    
    //Инициализируем UART
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        ShowError(101, 1);
    }
    
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE); // разрешаем прерывание по приему от UART2
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
	
	
	/* Peripheral clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();
    
    /**USART3 GPIO Configuration    
    PD8     ------> USART3_TX
    PD9     ------> USART3_RX 
    */
    //GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.Pin = GPIO_PIN_8;
    GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.Alternate = GPIO_AF7_USART3;
    GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStructure);
    
    GPIO_InitStructure.Pin = GPIO_PIN_9;
    GPIO_InitStructure.Mode = GPIO_MODE_AF_OD;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStructure);
     
    //Заполняем структуру настройками UARTa
    huart3.Instance           = USART3;
    huart3.Init.BaudRate      = 9600;
    huart3.Init.WordLength    = UART_WORDLENGTH_8B;
    huart3.Init.StopBits      = UART_STOPBITS_1;
    huart3.Init.Parity        = UART_PARITY_NONE;
    huart3.Init.HwFlowCtl     = UART_HWCONTROL_NONE;
    huart3.Init.Mode          = UART_MODE_TX_RX;
    
    //Инициализируем UART
    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        ShowError(101, 1);
    }
    
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE); // разрешаем прерывание по приему от UART3
    HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    
}

void Im_Init_Gsm()
{
    // Подать питание на GSM модуль
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    
    // Запуск модуля A6
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    FastWait(10); // Fast wait 10 seconds
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    
}

void Im_Deinit_Gsm()
{
    // Отключить GSM модуль
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_Delay(1000);
}

int Im_Init_GPRS(int needSTM32Reboot)
{
	int result = 0;
    // Проверка состояния модуля
    for(int i = 0; i < 30; i++)
    {
        result = GsmCmd("AT+CPAS", needSTM32Reboot); // Состояние модуля
		
		// Проверить статус
		if (result == -1)
		{
			return result;
		}
		
        if (IndexOf(uart2RxBuffer, "+CPAS:0\r") > -1)
        {
            // Ok
            break;
        }
        else
        {
            // Продолжаем опрашивать состояние
            HAL_Delay(1000);
        }
    }
    
    // Проверка уровня сигнала
    for(int i = 0; i < 30; i++)
    {
        result = GsmCmd("AT+CSQ", needSTM32Reboot); // Уровень сигнала
		
		// Проверить статус
		if (result == -1)
		{
			return result;
		}
		
        if (IndexOf(uart2RxBuffer, "+CSQ: 0,0\r") > -1)
        {
            // Продолжаем искать сигнал
            HAL_Delay(1000);
        }
        else
        {
            // Ok
            break;
        }
    }
    
    // Проверка регистрации в сети
    for(int i = 0; i < 30; i++)
    {
        result = GsmCmd("AT+CREG?", needSTM32Reboot);
		
		// Проверить статус
		if (result == -1)
		{
			return result;
		}
		
        if (IndexOf(uart2RxBuffer, "+CREG: 1,1\r") > -1)
        {
            // Ok
            break;
        }
        else
        {
            HAL_Delay(1000);
            result = GsmCmd("AT+CREG=1", needSTM32Reboot); // Зарегистрироваться в сети
			
			// Проверить статус
			if (result == -1)
			{
				return result;
			}
        }
    }
    
    // Проверка модуля GPRS
    for(int i = 0; i < 30; i++)
    {
        result = GsmCmd("AT+CGATT?", needSTM32Reboot);
		
		// Проверить статус
		if (result == -1)
		{
			return result;
		}
		
        if (IndexOf(uart2RxBuffer, "+CGATT:1\r") > -1)
        {
            // Ok
            break;
        }
        else
        {
            HAL_Delay(1000);
            result = GsmCmd("AT+CGATT=1", needSTM32Reboot); // Включить GPRS
			// Проверить статус
			if (result == -1)
			{
				return result;
			}
        }
    }
    
    
    //GsmCmd("AT+CSTT=\"internet\",\"gdata\",\"gdata\""); // Настройка APN Megafon
    result = GsmCmd("AT+CSTT=\"internet.beeline.ru\",\"beeline\",\"beeline\"", needSTM32Reboot); // Настройка APN Beeline
	
	// Проверить статус
	if (result == -1)
	{
		return result;
	}
	
    result = GsmCmd("AT+CIICR", needSTM32Reboot); // Устанавливаем беспроводное подключение GPRS
	
	// Проверить статус
	if (result == -1)
	{
		return result;
	}
    
    //GsmCmd("AT+CIFSR"); // Узнаём свой IP-адрес
    //GsmCmd("AT+CIPSTATUS"); // Состояние подключений
    
    //GsmCmd("AT+CIPSTART=\"TCP\",\"bobturanov.beget.tech\",80");
    //GsmCmd("AT+CIPSTATUS"); // Состояние подключений
    
    //GsmCmd("AT+CIPCLOSE"); // Close a TCP/IP translation
    //GsmCmd("AT+CIPSTATUS"); // Состояние подключений
    
    return result;
}

void GetMyID()
{
    // Получаем свой ID
    volatile uint32_t *UniqueID = (uint32_t *)0x1FFF7A10;
    char str_id_part1[11] = {0};
    char str_id_part2[11] = {0};
    char str_id_part3[11] = {0};
    sprintf(str_id_part1, "%d", (int)UniqueID[0]);
    sprintf(str_id_part2, "%d", (int)UniqueID[1]);
    sprintf(str_id_part3, "%d", (int)UniqueID[2]);
    char str_id[512] = {0};
    strcat(str_id, str_id_part1);
    strcat(str_id, str_id_part2);
    strcat(str_id, str_id_part3);
	
	strcpy(str_myid, str_id);
}

int GetTemp()
{
    ClearUart3();
    Uart3RxStrWaiting("<temp>", 2000);
    Uart3RxStrWaiting("</temp>", 2000);
    char* ans = uart3RxBuffer;
    
    int start = IndexOf(ans, "<temp>") + strlen("<temp>");
    int end = IndexOf(ans, "</temp>");
    int len = end-start;
    
    int a = 0;
    char output_str[len];
    
    for (int i = start; i < end; i++)
    {
        output_str[a] = ans[i];
        a++;
    }
    
    double output_double = atof(output_str);
    int output_int = output_double * 100;
    
    return output_int;
}

int TcpConnectOpen()
{
	int result = 0;
	
    // Проверка состояния подключений
    for(int i = 0; i < 30; i++)
    {
        result = GsmCmd("AT+CIPSTATUS", 0); // Состояние подключений
		
		// Проверить статус
		if (result == -1)
		{
			return result;
		}
		
        if (IndexOf(uart2RxBuffer, "0,CONNECT OK") > -1)
        {
            // Ok
            break;
        }
        else
        {
            // Пытаемся создать соединение.
            HAL_Delay(1000);
            result = GsmCmd("AT+CIPSTART=\"TCP\",\"gasmonitor.kgeu.ru\",80", 0);
        }
    }
	
	return result;
}

int TcpConnectClose()
{
	int result = 0;
	
    // Проверка состояния подключений
    for(int i = 0; i < 30; i++)
    {
        result = GsmCmd("AT+CIPSTATUS", 0); // Состояние подключений
		
		// Проверить статус
		if (result == -1)
		{
			return result;
		}
		
        if (IndexOf(uart2RxBuffer, "0,CONNECT OK") > -1)
        {
            HAL_Delay(1000);
            result = GsmCmd("AT+CIPCLOSE", 0);
        }
        else
        {
            // Ok.
            break;
        }
    }
	
	return result;
}

int GetLicenseStatus()
{
    if (license == 1)
    {
        return 1;
    }
    
    // Проверка состояния подключений
    for(int i = 0; i < 30; i++)
    {
        GsmCmd("AT+CIPSTATUS", 1); // Состояние подключений
        if (IndexOf(uart2RxBuffer, "0,CONNECT OK") > -1)
        {
            // Ok
            break;
        }
        else
        {
            // Пытаемся создать соединение.
            HAL_Delay(1000);
            GsmCmd("AT+CIPSTART=\"TCP\",\"schistory.space\",80", 1);
        }
    }
    
    // Отчистить буфер приема UART
    uart2_buffer_fill = 0;
    memset(uart2RxBuffer, 0, sizeof(uart2RxBuffer)); // очистка буфера для вывода.
    
    // Отправить команду
    GsmCmdWithStrWaiting("AT+CIPSEND", ">", 3000, 1);
    send_str("GET /license.html HTTP/1.1\r\nHost:schistory.space\r\n");
    send_to_uart(0x1A);
    
    // Ожидание конца сообщений
    Uart2RxStrWaiting("The license request was accepted", 30000);
	
	// Изменить статус лицензии
    license = 1;
    
    // Закрыть соединение
    TcpConnectClose();
	
	return 0;
}

void setup()
{
    // Инициализация HAL
    HAL_Init();
    
    // Инициализация портов GPIO
    Im_Init_GPIO();
    
    // Инициализация портов Gas sensor
    Im_Init_GasSensor();
    
    // Инициализация портов Temp sensor
    //Im_Init_TempSensor();
    
    // Инициализация портов UART
    Im_Init_UART();
    
    // Узнаем свой ID
    GetMyID();
	
	// Очищаем память
	memset(memory, 0, sizeof(memory));
    memset(buffer, 0, sizeof(buffer));
    
    // Ждем 5 секунд
    Wait(5);
	
    // Включить GSM модуль
	Im_Init_Gsm();
		
	// Инициализируем GPRS
	Im_Init_GPRS(1);
    
	// Проверить лицензию
	GetLicenseStatus();
    
}

int UpdateTimestaps(int needSTM32Reboot)
{
	int result = 0;
	
    // Открыть соединение
	result = TcpConnectOpen();
    
    // Проверить статус
	if (result == -1)
	{
		return result;
	}
	
    char* req = "GET /gettimestamps.php HTTP/1.1\r\nHost:gasmonitor.kgeu.ru\r\n";
    
    // Отчистить буфер приема UART
    uart2_buffer_fill = 0;
    memset(uart2RxBuffer, 0, sizeof(uart2RxBuffer)); // очистка буфера для вывода.
    
    // Отправить команду
    result = GsmCmdWithStrWaiting("AT+CIPSEND", ">", 3000, needSTM32Reboot);
    
    // Проверить статус
	if (result == -1)
	{
		return result;
	}
    
    send_str(req);
    send_to_uart(0x1A);
    
    // Ожидание конца сообщений
    Uart2RxStrWaiting("</timestamps>", 30000);
    
    // Проверить ответ
    result = CheckGsmAns(uart2RxBuffer, needSTM32Reboot);
    
    // Проверить статус
	if (result == -1)
	{
		return result;
	}
    
    char* ans = uart2RxBuffer;
    
    int start = IndexOf(ans, "<timestamps>") + strlen("<timestamps>");
    int end = IndexOf(ans, "</timestamps>");
    int len = end-start;
    
    int a = 0;
    char str_timestamps[len];
    
    for (int i = start; i < end; i++)
    {
        str_timestamps[a] = ans[i];
        a++;
    }
    
    timestamps = atoi(str_timestamps);
    
    // Закрыть соединение
    result = TcpConnectClose();
    
    // Проверить статус
	if (result == -1)
	{
		return result;
	}
}

void SaveDataFromSensor()
{
	// Получить результат с газового датчика
	char str_ADCValue_gas[11] = {0};
    if (HAL_ADC_PollForConversion(&g_AdcHandle_gas, 1000000) == HAL_OK)
    {
        int g_ADCValue_gas = HAL_ADC_GetValue(&g_AdcHandle_gas);
		sprintf(str_ADCValue_gas, "%d", g_ADCValue_gas);
    }
    else
    {
        ShowError(101, 0);
    }
    
    // Получить результат с датчика температуры
	int temp_int = GetTemp();
    char str_ADCValue_temp[11] = {0};
	sprintf(str_ADCValue_temp, "%d", temp_int);
	
	// Записать данные во временный буфер
	WriteToBuffer(str_ADCValue_gas, str_ADCValue_temp);
	
}

void AverageAndSaveValues()
{
	int gas_array[100] = {0};
	int temp_array[100] = {0};
	int gas_array_size = sizeof(gas_array)/sizeof(gas_array[0]);
    int temp_array_size = sizeof(temp_array)/sizeof(temp_array[0]);
	
    for (int i = 0; i < buffer_size; i++)
    {
        if (buffer[i][0][0] == 0x00)
        {
            continue;
        }
		
		gas_array[i] = atoi(buffer[i][0]);
		temp_array[i] = atoi(buffer[i][1]);
        
        // Очистить строку из буфера
        memset(buffer[i], 0, sizeof(buffer[i]));
    }
	
	// Найти медиану
	int gas = GetAverage(gas_array, gas_array_size);
	int temp = GetMediana(temp_array, temp_array_size);
	
    char str_gas[11] = {0};
    char str_temp[11] = {0};
    sprintf(str_gas, "%d", gas);
    sprintf(str_temp, "%d", temp);
    
    // Проверка на вшивость
    if (gas == 0 && temp == 0)
    {
        return;
    }
    
    // Записать в память
    WriteToMemory(str_gas, str_temp);
}

int CompileAndSendRequest(int i)
{
	int result = 0;
	
    // Проверка на вшивость
    if (memory[i][0][0] == 0x00)
    {
        return 0;
    }
    
    // Открыть соединение
	result = TcpConnectOpen();
	
	// Проверить статус
	if (result == -1)
	{
		return result;
	}
    
	// Считать строку из памяти
	char str_timestamps[11] = {0};
	char str_gas[11] = {0};
	char str_temp[11] = {0};
    
    strcpy(str_timestamps, memory[i][0]);
    strcpy(str_gas, memory[i][1]);
    strcpy(str_temp, memory[i][2]);
	
	// Формируем запрос из нескольких строк
    char req[256] = {0};
    char* req_start_part = "GET /1.php";
    char* req_part_timestamps = "?timestamps=";
    char* req_part_id = "&id=";
    char* req_part_a = "&a=";
    char* req_part_temp = "&temp=";
    char* req_end_part = " HTTP/1.1\r\nHost:gasmonitor.kgeu.ru\r\n";
    
	// Request start
    strcat(req, req_start_part);
    
    // Timestamps
    strcat(req, req_part_timestamps);
    strcat(req, str_timestamps);
    
    // ID
    strcat(req, req_part_id);
    strcat(req, str_myid);
    
    // Gas value
    strcat(req, req_part_a);
    strcat(req, str_gas);
    
    // Temperature value
    strcat(req, req_part_temp);
    strcat(req, str_temp);
    
	// Request end
    strcat(req, req_end_part);
	
	// Send HTTP Request
	result = GsmHttpRequest(req);
	
	// Очистить строку из памяти
	if (result == 0)
	{
		memset(memory[i], 0, sizeof(memory[i]));
	}
	
	return result;
}

void SendDataFromMemory()
{
	int result = 0;
	
    // Отправить сохраненные данные
	for (int i = 0; i < memory_size; i++)
	{
		result = CompileAndSendRequest(i);
		
		// Прекратить если передача не удалась
		if (result == -1)
		{
			break;
		}
	}
    
    // Закрыть соединение
    result = TcpConnectClose();
}

void loop()
{
    // Индикатор
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
    
	// Проверка временной метки, с жесткой перезагрузкой при неудаче
    if (timestamps > last_update + 259200 || last_update == 0)
    {
		// Синхронизировать время с сервером раз в трое суток
        UpdateTimestaps(1);
        last_update = timestamps;
    }
    
    // Проверка временной метки, мягкий режим
    if (timestamps > last_update + 10800)
    {
		// Синхронизировать время с сервером раз в трое суток
        UpdateTimestaps(0);
        last_update = timestamps;
    }
    
    // Снимать показания с датчиков, сохраняем раз в минуту
    if (timestamps > last_datasave + 60)
    {
        AverageAndSaveValues();
        last_datasave = timestamps;
    }
    else
    {
        SaveDataFromSensor();
    }
	
	// Отправить собранные данные раз в 10 минут (600 секунд)
	if (timestamps > last_datasend + 600)
	{
		SendDataFromMemory();
		last_datasend = timestamps;
	}
	
	// Перезагрузить модем если это требуется
	if (needGsmReboot == 1)
	{
		Im_Deinit_Gsm();
		Im_Init_Gsm();
		Im_Init_GPRS(0);
        needGsmReboot = 0;
	}
}

int main(void)
{
    // Инициализация системы и портов ввода-вывода
    setup();
    
    // Бесконечный цикл loop...
    while(1)
    {
        loop();
    }
    //return 0;
}

void SysTick_Handler(void)
{
    HAL_IncTick();
    HAL_SYSTICK_IRQHandler();
    
    Countup();
}
 
void USART2_IRQHandler(void)
{
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
    if(USART2->SR & USART_SR_RXNE) //Если прерывание вызвано по приёму USART2
    {
        USART2->SR &= ~USART_SR_RXNE; //Сбрасываем флаг приёма USART2
        uart2RxBuffer[uart2_buffer_fill++] = USART2->DR;
        if (uart2_buffer_fill == 1023)
        {
            uart2_buffer_fill = 1022;
        }
    }
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
}

void USART3_IRQHandler(void)
{
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
    if(USART3->SR & USART_SR_RXNE) //Если прерывание вызвано по приёму USART3
    {
        USART3->SR &= ~USART_SR_RXNE; //Сбрасываем флаг приёма USART3
        uart3RxBuffer[uart3_buffer_fill++] = USART3->DR;
        if (uart3_buffer_fill == 1023)
        {
            uart3_buffer_fill = 1022;
        }
    }
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
}
