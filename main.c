#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "inc/hw_gpio.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_i2c.h"
#include "inc/tm4c123gh6pm.h"
#include "driverlib/fpu.h"
#include "driverlib/debug.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/i2c.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/timer.h"
#include "driverlib/adc.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"
#include "sensorlib/hw_mpu6050.h"
#include "utils/uartstdio.h"

#include "mpu6050.h"



//宏定义延时时间=2秒钟
#define DelayTime 32000000
static char ReceivedData[128];
static char Command[64];
static char ServerIP[16];
//0表示建立连接，1表示正在监听，2表示查询状态
static int Status = 0;
static bool ok = false;
static int count = 0;

float abs(float a, float b){
    if(a > b)
        return a -b;
    else
        return b -a;
}

char* itoa(int i, char b[])
{
    char const digit[] = "0123456789";
    char* p = b;
    if(i<0){
        *p++ = '-';
        i *= -1;
    }
    int shifter = i;
    do{
        ++p;
        shifter = shifter/10;
    }while(shifter);
    *p = '\0';
    do{
        *--p = digit[i%10];
        i = i/10;
    }while(i);
    return b;
}


void ftoa(float f,char *buf)
{
    int pos=0,ix,dp,num;
    if (f<0)
    {
        buf[pos++]='-';
        f = -f;
    }
    dp=0;
    while (f>=10.0)
    {
        f=f/10.0;
        dp++;
    }
    for (ix=1;ix<8;ix++)
    {
            num = f;
            f=f-num;
            if (num>9)
                buf[pos++]='#';
            else
                buf[pos++]='0'+num;
            if (dp==0) buf[pos++]='.';
            f=f*10.0;
            dp--;
    }
}


void printSTR2server(char* str){
    int i = 0;
    for(i = 0; i < strlen(str); i++){
        UARTCharPut(UART1_BASE, str[i]);
    }
}

void printINT2server(int num){
    char out[20] = "0";
    itoa(num, out);
    printSTR2server(out);

}

void printFLO2server(float num){
    char out[20] = "0";
    ftoa(num, out);
    printSTR2server(out);

}

void printARR2server(int* num){
    int i = 0;
        for(i = 0; i < strlen(num); i++){
            printINT2server(num[i]);
        }

}



void WifiTimer0IntHandler(void);

//发送ESP命令
void SendString(char *PString)
{
    while (*PString)
    {
        UARTCharPut(UART1_BASE, *PString);
        PString++;
    }
    //字符串发送完后，发送0x0d 0x0a，相当于发送新行
    UARTCharPut(UART1_BASE, 0x0d);
    UARTCharPut(UART1_BASE, 0x0a);
}
//重启WIFI
void Reset(void)
{
    SendString("AT+RST");
    SysCtlDelay(DelayTime);
    WifiTimer0IntHandler();
}
//慢慢设置WIFI
void SlowSetWifi()
{
    //
    Status = 0;
    //1.初始化：亮红色灯
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1, 0x02);
    UARTCharPut(UART1_BASE, '+');
    UARTCharPut(UART1_BASE, '+');
    UARTCharPut(UART1_BASE, '+');
    SysCtlDelay(DelayTime);
    //设置开机连接WIFI的功能
    ok = false;
    while (!ok)
    {

        SendString("AT+CWMODE_DEF=1");
        //延时，保证模块有足够时间响应
        SysCtlDelay( DelayTime);
    }
    ok = false;
    while (!ok)
    {

        SendString("AT+CWJAP_DEF=\"EmbedTest\",\"1234abcd\"");
        //延时，保证模块有足够时间响应
        SysCtlDelay(2 * DelayTime);
    }

    //2.连接WIFI成功：蓝色灯
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1, 0x04);

    //设置开机连接服务器的功能
    //测试百度183.232.231.173
    ok = false;
    while (!ok)
    {
        SendString("AT+CIPMUX=0");
        //延时，保证模块有足够时间响应
        SysCtlDelay(DelayTime);
    }
    ok = false;
    while (!ok)
    {
        SendString("AT+CIPSTART=\"TCP\",\"111.230.11.142\",8080");
        //

        //延时，保证模块有足够时间响应
        SysCtlDelay(2 * DelayTime);
    }
    //设置开机自启动
    ok = false;
    while (!ok)
    {
        //
        SendString("AT+SAVETRANSLINK=1,\"111.230.11.142\",8080,\"TCP\"");
        //延时，保证模块有足够时间响应
        SysCtlDelay(2 * DelayTime);
    }
    ok = false;
    while (!ok)
    {
        SendString("AT+CIPMODE=1");
        //延时，保证模块有足够时间响应
        SysCtlDelay(DelayTime);
    }

    SendString("AT+CIPSEND");
    SysCtlDelay(DelayTime);

    //3.连接服务器成功：绿色灯
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1, 0x08);

    //监听模式
    Status = 1;
}
//处理连接网络状态的函数
void WifiTimer0IntHandler(void)
{
    // Clear the timer interrupt
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    //告知服务器我将要排查网络
    UARTCharPut(UART1_BASE, '?');
    SysCtlDelay(DelayTime / 10);
    UARTCharPut(UART1_BASE, '+');
    UARTCharPut(UART1_BASE, '+');
    UARTCharPut(UART1_BASE, '+');
    SysCtlDelay(DelayTime);
    if (Status == 0)
    {
        /*if(count++ > 6){
         Reset();
         count = 0;
         }*/
        return;
    }

    Status = 2;
    SendString("AT+CIPSTATUS");

}

//测试发送字符串
void testSend()
{
    int i = 0;
    while (1)
    {
        char c[26] = "xyz";
        UARTCharPut(UART1_BASE, c[i++ % 3]);
        SysCtlDelay(DelayTime);
    }
}

//初始化UART
//这个可以理解为整个单片机的输入输出
void InitUART(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
    UARTStdioConfig(0, 115200, 16000000);

}
//初始化ESPUART
//这个可以理解为ESP8266命令的输入输出
void InitESPUART(void)
{
    //配置串口
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinConfigure(GPIO_PB0_U1RX);
    GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    //配置UART
    //数据位8，校验位0，停止位1，也就是每次接收8位长的数据，只有接收到8个字符才会触发
    UARTConfigSetExpClk(
            UART1_BASE, SysCtlClockGet(), 115200,
            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
}
//初始化定时器:大概每60s触发一次
void InitTimer(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    //配置Timer0
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (SysCtlClockGet()) * 3000);

}

//扫描当前可用的APs
//SendATCommand("AT+CWLAP");

//查询本机IP地址
//SendATCommand("AT+CIFSR");

int main(void)
{


    //现在还没有加判断是否成功的语句!
    //系统时钟配置
    SysCtlClockSet(
    SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
    //UART设置
    InitUART();
    InitESPUART();
    InitTimer();

    //设置LED灯
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE,
    GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    //设置蜂鸣器
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);//启用GPIOF端口并提供一个时钟
    GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_2);//配置IO为输出模式
    GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2, GPIO_PIN_2);//配置输出高电平，默认不响

    //1.初始化：亮红色灯
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1, 0x02);
    //中断
    IntMasterEnable(); //enable processor interrupts

    IntEnable(INT_UART1); //enable the UART interrupt
    UARTIntEnable(UART1_BASE, UART_INT_RX | UART_INT_RT);

    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    TimerEnable(TIMER0_BASE, TIMER_A);
    //以下两条语句选择一个即可，SlowSetWifi是设置
    //另一条直接重启读取Flash的数据
    //记得要同时更改Status的初值
    SlowSetWifi();

    //Reset();
    //初始化模块
    MPU6050_Init();

    float xa0 = (float)MPU6050_Get_Data(ACCEL_XOUT_H)+ X_ACCEL_OFFSET;
    float ya0 = (float)MPU6050_Get_Data(ACCEL_YOUT_H) + Y_ACCEL_OFFSET;
    float za0 = (float)MPU6050_Get_Data(ACCEL_ZOUT_H) + Z_ACCEL_OFFSET;
    float temp0 = (float)MPU6050_Get_Data(TEMP_OUT_H) / 340.0 + 36.53;

    while (1)
    {
        float x = (float)MPU6050_Get_Data(ACCEL_XOUT_H)+ X_ACCEL_OFFSET;
        float y = (float)MPU6050_Get_Data(ACCEL_YOUT_H) + Y_ACCEL_OFFSET;
        float z = (float)MPU6050_Get_Data(ACCEL_ZOUT_H) + Z_ACCEL_OFFSET;
        float t = (float)MPU6050_Get_Data(TEMP_OUT_H) / 340.0 + 36.53;
        //获取加速度
        UARTCharPut(UART1_BASE, '[');
        /// 16384.0
        printFLO2server(x );
        UARTCharPut(UART1_BASE, ',');
        printFLO2server(y);
        UARTCharPut(UART1_BASE, ',');
        printFLO2server(z );
        UARTCharPut(UART1_BASE, ']');


        //获取温度
        UARTCharPut(UART1_BASE, '[');
        printFLO2server(t);

        UARTCharPut(UART1_BASE, ']');
        //获取角速度
        UARTCharPut(UART1_BASE, '[');
        printFLO2server(
                ((float) MPU6050_Get_Data(GYRO_XOUT_H) + X_GYRO_OFFSET));
        UARTCharPut(UART1_BASE, ',');
        printFLO2server(
                ((float) MPU6050_Get_Data(GYRO_YOUT_H) + Y_GYRO_OFFSET));
        UARTCharPut(UART1_BASE, ',');
        printFLO2server(
                ((float) MPU6050_Get_Data(GYRO_ZOUT_H) + Z_GYRO_OFFSET));
        UARTCharPut(UART1_BASE, ']');


        //判断加速度报警
        if (abs(x + y + z, xa0 + ya0 + za0) > 32768){
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2, GPIO_PIN_1);    //响
            printSTR2server("@");
        }

        //判断温度报警
        else if(abs(t,temp0) >= 5){
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2, GPIO_PIN_1);    //响
            printSTR2server("@");
        }

        else
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2, GPIO_PIN_2);    //不响
        SysCtlDelay(0.1 * DelayTime);

    }

}
//处理返回的消息
void WifiUARTIntHandler(void)
{

    uint32_t ui32Status;
    //得到中断状态
    ui32Status = UARTIntStatus(UART1_BASE, true);
    //清除中断
    UARTIntClear(UART1_BASE, ui32Status);
    memset(ReceivedData, 0, sizeof(ReceivedData));
    int i = 0;
    //保存接收的字符串
    while (UARTCharsAvail(UART1_BASE))
    {
        ReceivedData[i] = UARTCharGetNonBlocking(UART1_BASE);
        UARTCharPutNonBlocking(UART0_BASE, ReceivedData[i]);
        i++;
    }
    //状态：建立连接
    if (Status == 0)
    {
        UARTIntClear(UART1_BASE, '0');
        int j = 0;
        for (; j < strlen(ReceivedData) - 1; j++)
        {
            //表示断开服务器
            if (ReceivedData[j] == 'O')
            {
                if (ReceivedData[j + 1] == 'K')
                {
                    ok = true;
                    break;
                }

            }
        }
    }
    //状态：监听服务器的数据
    else if (Status == 1)
    {
        UARTIntClear(UART1_BASE, '1');
        if (ReceivedData[0] == 'a')
        {
            //白色灯光
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                         0x0e);
            UARTCharPut(UART1_BASE, 'a');
        }
        else if (ReceivedData[0] == 'b')
        {
            //青色
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                         0x0c);
            UARTCharPut(UART1_BASE, 'b');
        }
        else if (ReceivedData[0] == 'c')
        {
            //粉色灯光
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                         0x06);
            UARTCharPut(UART1_BASE, 'c');
        }
        else
        {

        }
        //重置计数器
        TimerLoadSet(TIMER0_BASE, TIMER_A, (SysCtlClockGet()) * 3000);
        //状态：退出透传模式，检查网络状态
    }
    else if (Status == 2)
    {
        UARTIntClear(UART1_BASE, '2');
        int j = 0;
        for (; j < strlen(ReceivedData); j++)
        {
            //表示断开服务器
            if (ReceivedData[j] == '4' || ReceivedData[j] == '2')
            {
                //2.连接WIFI成功：蓝色灯
                GPIOPinWrite(GPIO_PORTF_BASE,
                GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                             0x04);
                Reset();
                return;
            }
            //表示断开wifi
            else if (ReceivedData[j] == '5')
            {
                //1.初始化：亮红色灯
                GPIOPinWrite(GPIO_PORTF_BASE,
                GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                             0x02);
                Reset();
                return;
                //表示网络连接正常
            }
            else if (ReceivedData[j] == '3')
            {

                //进入透传模式
                SendString("AT+CIPMODE=1");
                SysCtlDelay(DelayTime);
                SendString("AT+CIPSEND");
                SysCtlDelay(DelayTime);
                //告知服务器排查结束
                UARTCharPut(UART1_BASE, '!');
                Status = 1;
                //3.连接服务器成功：绿色灯
                if (GPIOPinRead(GPIO_PORTF_BASE,
                GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1) == 0
                        || GPIOPinRead(GPIO_PORTF_BASE,
                        GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1) == 2
                        || GPIOPinRead(GPIO_PORTF_BASE,
                        GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1) == 4)
                    GPIOPinWrite(GPIO_PORTF_BASE,
                    GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                                 0x08);
                return;
            }
        }
        //1.初始化：亮红色灯
        //GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3|GPIO_PIN_2|GPIO_PIN_1, 0x02);
        //Reset();
    }
}
