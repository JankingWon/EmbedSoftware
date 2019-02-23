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



//�궨����ʱʱ��=2����
#define DelayTime 32000000
static char ReceivedData[128];
static char Command[64];
static char ServerIP[16];
//0��ʾ�������ӣ�1��ʾ���ڼ�����2��ʾ��ѯ״̬
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

//����ESP����
void SendString(char *PString)
{
    while (*PString)
    {
        UARTCharPut(UART1_BASE, *PString);
        PString++;
    }
    //�ַ���������󣬷���0x0d 0x0a���൱�ڷ�������
    UARTCharPut(UART1_BASE, 0x0d);
    UARTCharPut(UART1_BASE, 0x0a);
}
//����WIFI
void Reset(void)
{
    SendString("AT+RST");
    SysCtlDelay(DelayTime);
    WifiTimer0IntHandler();
}
//��������WIFI
void SlowSetWifi()
{
    //
    Status = 0;
    //1.��ʼ��������ɫ��
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1, 0x02);
    UARTCharPut(UART1_BASE, '+');
    UARTCharPut(UART1_BASE, '+');
    UARTCharPut(UART1_BASE, '+');
    SysCtlDelay(DelayTime);
    //���ÿ�������WIFI�Ĺ���
    ok = false;
    while (!ok)
    {

        SendString("AT+CWMODE_DEF=1");
        //��ʱ����֤ģ�����㹻ʱ����Ӧ
        SysCtlDelay( DelayTime);
    }
    ok = false;
    while (!ok)
    {

        SendString("AT+CWJAP_DEF=\"EmbedTest\",\"1234abcd\"");
        //��ʱ����֤ģ�����㹻ʱ����Ӧ
        SysCtlDelay(2 * DelayTime);
    }

    //2.����WIFI�ɹ�����ɫ��
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1, 0x04);

    //���ÿ������ӷ������Ĺ���
    //���԰ٶ�183.232.231.173
    ok = false;
    while (!ok)
    {
        SendString("AT+CIPMUX=0");
        //��ʱ����֤ģ�����㹻ʱ����Ӧ
        SysCtlDelay(DelayTime);
    }
    ok = false;
    while (!ok)
    {
        SendString("AT+CIPSTART=\"TCP\",\"111.230.11.142\",8080");
        //

        //��ʱ����֤ģ�����㹻ʱ����Ӧ
        SysCtlDelay(2 * DelayTime);
    }
    //���ÿ���������
    ok = false;
    while (!ok)
    {
        //
        SendString("AT+SAVETRANSLINK=1,\"111.230.11.142\",8080,\"TCP\"");
        //��ʱ����֤ģ�����㹻ʱ����Ӧ
        SysCtlDelay(2 * DelayTime);
    }
    ok = false;
    while (!ok)
    {
        SendString("AT+CIPMODE=1");
        //��ʱ����֤ģ�����㹻ʱ����Ӧ
        SysCtlDelay(DelayTime);
    }

    SendString("AT+CIPSEND");
    SysCtlDelay(DelayTime);

    //3.���ӷ������ɹ�����ɫ��
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1, 0x08);

    //����ģʽ
    Status = 1;
}
//������������״̬�ĺ���
void WifiTimer0IntHandler(void)
{
    // Clear the timer interrupt
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    //��֪�������ҽ�Ҫ�Ų�����
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

//���Է����ַ���
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

//��ʼ��UART
//����������Ϊ������Ƭ�����������
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
//��ʼ��ESPUART
//����������ΪESP8266������������
void InitESPUART(void)
{
    //���ô���
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinConfigure(GPIO_PB0_U1RX);
    GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    //����UART
    //����λ8��У��λ0��ֹͣλ1��Ҳ����ÿ�ν���8λ�������ݣ�ֻ�н��յ�8���ַ��Żᴥ��
    UARTConfigSetExpClk(
            UART1_BASE, SysCtlClockGet(), 115200,
            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
}
//��ʼ����ʱ��:���ÿ60s����һ��
void InitTimer(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    //����Timer0
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (SysCtlClockGet()) * 3000);

}

//ɨ�赱ǰ���õ�APs
//SendATCommand("AT+CWLAP");

//��ѯ����IP��ַ
//SendATCommand("AT+CIFSR");

int main(void)
{


    //���ڻ�û�м��ж��Ƿ�ɹ������!
    //ϵͳʱ������
    SysCtlClockSet(
    SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
    //UART����
    InitUART();
    InitESPUART();
    InitTimer();

    //����LED��
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE,
    GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    //���÷�����
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);//����GPIOF�˿ڲ��ṩһ��ʱ��
    GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_2);//����IOΪ���ģʽ
    GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2, GPIO_PIN_2);//��������ߵ�ƽ��Ĭ�ϲ���

    //1.��ʼ��������ɫ��
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1, 0x02);
    //�ж�
    IntMasterEnable(); //enable processor interrupts

    IntEnable(INT_UART1); //enable the UART interrupt
    UARTIntEnable(UART1_BASE, UART_INT_RX | UART_INT_RT);

    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    TimerEnable(TIMER0_BASE, TIMER_A);
    //�����������ѡ��һ�����ɣ�SlowSetWifi������
    //��һ��ֱ��������ȡFlash������
    //�ǵ�Ҫͬʱ����Status�ĳ�ֵ
    SlowSetWifi();

    //Reset();
    //��ʼ��ģ��
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
        //��ȡ���ٶ�
        UARTCharPut(UART1_BASE, '[');
        /// 16384.0
        printFLO2server(x );
        UARTCharPut(UART1_BASE, ',');
        printFLO2server(y);
        UARTCharPut(UART1_BASE, ',');
        printFLO2server(z );
        UARTCharPut(UART1_BASE, ']');


        //��ȡ�¶�
        UARTCharPut(UART1_BASE, '[');
        printFLO2server(t);

        UARTCharPut(UART1_BASE, ']');
        //��ȡ���ٶ�
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


        //�жϼ��ٶȱ���
        if (abs(x + y + z, xa0 + ya0 + za0) > 32768){
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2, GPIO_PIN_1);    //��
            printSTR2server("@");
        }

        //�ж��¶ȱ���
        else if(abs(t,temp0) >= 5){
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2, GPIO_PIN_1);    //��
            printSTR2server("@");
        }

        else
            GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_2, GPIO_PIN_2);    //����
        SysCtlDelay(0.1 * DelayTime);

    }

}
//�����ص���Ϣ
void WifiUARTIntHandler(void)
{

    uint32_t ui32Status;
    //�õ��ж�״̬
    ui32Status = UARTIntStatus(UART1_BASE, true);
    //����ж�
    UARTIntClear(UART1_BASE, ui32Status);
    memset(ReceivedData, 0, sizeof(ReceivedData));
    int i = 0;
    //������յ��ַ���
    while (UARTCharsAvail(UART1_BASE))
    {
        ReceivedData[i] = UARTCharGetNonBlocking(UART1_BASE);
        UARTCharPutNonBlocking(UART0_BASE, ReceivedData[i]);
        i++;
    }
    //״̬����������
    if (Status == 0)
    {
        UARTIntClear(UART1_BASE, '0');
        int j = 0;
        for (; j < strlen(ReceivedData) - 1; j++)
        {
            //��ʾ�Ͽ�������
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
    //״̬������������������
    else if (Status == 1)
    {
        UARTIntClear(UART1_BASE, '1');
        if (ReceivedData[0] == 'a')
        {
            //��ɫ�ƹ�
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                         0x0e);
            UARTCharPut(UART1_BASE, 'a');
        }
        else if (ReceivedData[0] == 'b')
        {
            //��ɫ
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                         0x0c);
            UARTCharPut(UART1_BASE, 'b');
        }
        else if (ReceivedData[0] == 'c')
        {
            //��ɫ�ƹ�
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                         0x06);
            UARTCharPut(UART1_BASE, 'c');
        }
        else
        {

        }
        //���ü�����
        TimerLoadSet(TIMER0_BASE, TIMER_A, (SysCtlClockGet()) * 3000);
        //״̬���˳�͸��ģʽ���������״̬
    }
    else if (Status == 2)
    {
        UARTIntClear(UART1_BASE, '2');
        int j = 0;
        for (; j < strlen(ReceivedData); j++)
        {
            //��ʾ�Ͽ�������
            if (ReceivedData[j] == '4' || ReceivedData[j] == '2')
            {
                //2.����WIFI�ɹ�����ɫ��
                GPIOPinWrite(GPIO_PORTF_BASE,
                GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                             0x04);
                Reset();
                return;
            }
            //��ʾ�Ͽ�wifi
            else if (ReceivedData[j] == '5')
            {
                //1.��ʼ��������ɫ��
                GPIOPinWrite(GPIO_PORTF_BASE,
                GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1,
                             0x02);
                Reset();
                return;
                //��ʾ������������
            }
            else if (ReceivedData[j] == '3')
            {

                //����͸��ģʽ
                SendString("AT+CIPMODE=1");
                SysCtlDelay(DelayTime);
                SendString("AT+CIPSEND");
                SysCtlDelay(DelayTime);
                //��֪�������Ų����
                UARTCharPut(UART1_BASE, '!');
                Status = 1;
                //3.���ӷ������ɹ�����ɫ��
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
        //1.��ʼ��������ɫ��
        //GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3|GPIO_PIN_2|GPIO_PIN_1, 0x02);
        //Reset();
    }
}
