#include "STC8G.h"
#include "intrins.h"
#define GPIO_CTRL1P P13
#define GPIO_CTRL1N P14
#define GPIO_CTRL2P P10
#define GPIO_CTRL2N P37
#define GPIO_CTRL3P P35
#define GPIO_CTRL3N P36
#define GPIO_CTRL4P P17
#define GPIO_CTRL4N P54
#define GPIO_CTRL5P P15
#define GPIO_CTRL5N P16
#define GPIO_CTRL6P P12
#define GPIO_CTRL6N P11
#define GPIO_RXEN P34
#define GPIO_TXEN P33
#define BAUDRATE 115200
#define CRYSTAL 11059200
#define EEPROM_MCUADDRESS 0x0400
// 通信协议:
#define TYPE_SETADDR 0x01
// 设置地址：    AA55 01 FF XX CRC,XX为地址 0x01~0xFE有效
// 成功：回55AA 01 XX 01 CRC XX为地址
// 失败：回55AA 01 XX EE CRC
#define TYPE_SETSWITCH 0x02
// 设置开关状态：AA55 02 XX YY CRC,XX为地址(地址一致或为XX=0xFF广播地址时，模块会响应),YY为数据，位域如下：
// B7   B6   B5   B4   B3   B2   B1   B0
// TRX1 C1_2 C1_1 C1_0 EN C2_2 C2_1 C2_0
// TRX表示收发切换 1为发0为收
// EN=1表示射频回路通 0断
// C1表示1号(出线朝下 从出波一面看右边）天线移相器移相值 000=0°  001=45°  111=315°   C2同理
// 成功：回55AA 01 XX 01 CRC XX为地址
// 失败：回55AA 01 XX EE CRC
// 上述CRC算法为从AA55后（不含AA55） 到CRC之前的数据逐字节相加求和的低8位
#define TYPE_IAP 0x03
// 升级固件：AA55 03 FF 00 02

//	右上天线		右下天线	
//控制值	相位	幅度	相位	幅度
//0	70.92	27.7	66.3	27.88
//1	120.23	26.98	117.3	27.2
//2	162.23	28.49	162.22	28.53
//3	-148.48	28.4	-149.62	28
//4	-97.57	27.33	-100.76	27.01
//5	-53.72	26.72	-57.64	26.45
//6	-14.15	28.73	-16.84	28.29
//7	35.90 	29.52	31.34	29.15
//归一化	右上天线		右下天线	
//控制值	相位误差	幅度	相位误差	幅度
//0	0 	0.0 	0 	0.0 
//1	4 	-0.7 	6 	-0.7 
//2	1 	0.8 	6 	0.7 
//3	6 	0.7 	9 	0.1 
//4	12 	-0.4 	13 	-0.9 
//5	10 	-1.0 	11 	-1.4 
//6	5 	1.0 	7 	0.4 
//7	10 	1.8 	10 	1.3 

unsigned char MCUADDRESS;

enum{UART_RX_IDLE,UART_RX_GotPreamble1,UART_RX_Recving,UART_RX_FINISH} uartRxStat=UART_RX_IDLE;
unsigned char UARTRxBuf[8]; 
unsigned char UARTRxCount=0;

enum{UART_TX_IDLE,UART_TX_Ongoing} uartTxStat=UART_TX_IDLE;
unsigned char UARTTxBuf[8]; 
unsigned char UARTTxLen=0;
unsigned char UARTTxCount=0;




void UART1_Routine() interrupt 4
{
	unsigned char D;
    if (TI)
    {
        TI = 0;
				switch(uartTxStat){
					case UART_TX_IDLE:break;
					case UART_TX_Ongoing:
						UARTTxCount ++;
						if(UARTTxCount >= UARTTxLen){
							uartTxStat=UART_TX_IDLE;
						}else{
							SBUF = UARTTxBuf[UARTTxCount];
						}
						break;
				}
    }
    if (RI)
    {
        RI = 0;
				D = SBUF;
				switch(uartRxStat){
					case UART_RX_IDLE:
						if(D == 0xAA){
							UARTRxCount=0;
							UARTRxBuf[UARTRxCount] = D;
							UARTRxCount++;
							uartRxStat=UART_RX_GotPreamble1;
							break;
						}
						break;
					case UART_RX_GotPreamble1:
						if(D == 0x55){
							UARTRxBuf[UARTRxCount] = D;
							UARTRxCount++;
							uartRxStat=UART_RX_Recving;
							break;
						}else{
							uartRxStat=UART_RX_IDLE;
							UARTRxCount=0;
						}
						break;
					case UART_RX_Recving:
							UARTRxBuf[UARTRxCount] = D;
							UARTRxCount++;
							if(UARTRxCount >= 6){
								uartRxStat=UART_RX_FINISH;
							}
					case UART_RX_FINISH:
						break;
				}
			
			
    }
}
void UART_SendFrame(char Type,char Data){
	UARTTxBuf[0] = 0x55;
	UARTTxBuf[1] = 0xAA;
	UARTTxBuf[2] = Type;
	UARTTxBuf[3] = MCUADDRESS;
	UARTTxBuf[4] = Data;
	UARTTxBuf[5] = UARTTxBuf[2]+UARTTxBuf[3]+UARTTxBuf[4];
	
	uartTxStat=UART_TX_Ongoing;
	UARTTxCount=0;
	UARTTxLen=6;
	SBUF = UARTTxBuf[UARTTxCount];
}

void IapIdle()
{
    IAP_CONTR = 0;                              //关闭IAP功能
    IAP_CMD = 0;                                //清除命令寄存器
    IAP_TRIG = 0;                               //清除触发寄存器
    IAP_ADDRH = 0x80;                           //将地址设置到非IAP区域
    IAP_ADDRL = 0;
}

char IapRead(int addr)
{
    char dat;

    IAP_CONTR = 0x80;                           //使能IAP
    IAP_TPS = 12;                               //设置等待参数12MHz
    IAP_CMD = 1;                                //设置IAP读命令
    IAP_ADDRL = addr;                           //设置IAP低地址
    IAP_ADDRH = addr >> 8;                      //设置IAP高地址
    IAP_TRIG = 0x5a;                            //写触发命令(0x5a)
    IAP_TRIG = 0xa5;                            //写触发命令(0xa5)
    _nop_();
    dat = IAP_DATA;                             //读IAP数据
    IapIdle();                                  //关闭IAP功能

    return dat;
}

void IapProgram(int addr, char dat)
{
    IAP_CONTR = 0x80;                           //使能IAP
    IAP_TPS = 12;                               //设置等待参数12MHz
    IAP_CMD = 2;                                //设置IAP写命令
    IAP_ADDRL = addr;                           //设置IAP低地址
    IAP_ADDRH = addr >> 8;                      //设置IAP高地址
    IAP_DATA = dat;                             //写IAP数据
    IAP_TRIG = 0x5a;                            //写触发命令(0x5a)
    IAP_TRIG = 0xa5;                            //写触发命令(0xa5)
    _nop_();
    IapIdle();                                  //关闭IAP功能
}

void IapErase(int addr)
{
    IAP_CONTR = 0x80;                           //使能IAP
    IAP_TPS = 12;                               //设置等待参数12MHz
    IAP_CMD = 3;                                //设置IAP擦除命令
    IAP_ADDRL = addr;                           //设置IAP低地址
    IAP_ADDRH = addr >> 8;                      //设置IAP高地址
    IAP_TRIG = 0x5a;                            //写触发命令(0x5a)
    IAP_TRIG = 0xa5;                            //写触发命令(0xa5)
    _nop_();                                    //
    IapIdle();                                  //关闭IAP功能
}


void main()
{
	unsigned int T2Val;
	int i;
	unsigned char TRX,Enable,C1,C2; 
	// 初始化GPIO
	P1M0=0xff;P1M1=0x00;
	P3M0=0xfc;P3M1=0x01; // 串口为开漏输出 可以多个接一起 用地址进行选择
	P5M0 |= 0x10;
	P5M1 &= 0xEF;
	GPIO_TXEN =0;
	GPIO_RXEN =0;
	
	GPIO_CTRL4N =1;
	GPIO_CTRL4P =0;
	GPIO_CTRL5N =1;
	GPIO_CTRL5P =0;
	GPIO_CTRL6N =1;
	GPIO_CTRL6P =0;
	
	GPIO_CTRL1N =1;
	GPIO_CTRL1P =0;
	GPIO_CTRL2N =1;
	GPIO_CTRL2P =0;
	GPIO_CTRL3N =1;
	GPIO_CTRL3P =0;
	
	// 初始化串口
	SCON = 0x50;
	T2Val = 65536-CRYSTAL/BAUDRATE/4;
	T2L = T2Val&0xFF;                                 //65536-11059200/115200/4=0FFE8H
	T2H = T2Val>>8;
	AUXR = 0x15;                                //启动定时器
	// 读本机地址
	MCUADDRESS=IapRead(EEPROM_MCUADDRESS);
	if(MCUADDRESS == 0x00 || MCUADDRESS == 0xFF){
		MCUADDRESS = 0x01;
		IapErase(EEPROM_MCUADDRESS);
		IapProgram(EEPROM_MCUADDRESS,MCUADDRESS);
	}
	
	GPIO_TXEN = MCUADDRESS < 0x80;
	GPIO_RXEN = MCUADDRESS >= 0x80;
	// 初始化中断
	ES = 1;// 开串口中断
	EA = 1;// 开总中断
	//串口状态机
    while (1){
			if(UART_RX_FINISH==uartRxStat){
				unsigned char Type = UARTRxBuf[2];
				unsigned char Dest = UARTRxBuf[3];
				unsigned char Data = UARTRxBuf[4];
				unsigned char calcCRC = UARTRxBuf[2]+UARTRxBuf[3]+UARTRxBuf[4];
				if(calcCRC==UARTRxBuf[5] && (Dest==MCUADDRESS||Dest==0xFF)){
					switch(Type){
						case TYPE_SETADDR:{
							if(Data == 0x00 || Data == 0xFF)UART_SendFrame(Type,0xEE);
							else{
								MCUADDRESS = Data;
								IapErase(EEPROM_MCUADDRESS);
								IapProgram(EEPROM_MCUADDRESS,MCUADDRESS);
								UART_SendFrame(Type,MCUADDRESS);
							}
						}break;
						case TYPE_SETSWITCH:{
							// TODO
							TRX = (Data & 0x80)>>7;
							C1 = (Data & 0x70)>>4;
							Enable = (Data & 0x08)>>3;
							C2 = (Data & 0x07);
							
							GPIO_TXEN = TRX && Enable;
							GPIO_RXEN = (!TRX) && Enable;
							
							GPIO_CTRL6N = ((C1&0x4)>>2);
							GPIO_CTRL6P = !((C1&0x4)>>2);
							GPIO_CTRL5N = ((C1&0x2)>>1);
							GPIO_CTRL5P = !((C1&0x2)>>1);
							GPIO_CTRL4N = (C1&0x1);
							GPIO_CTRL4P = !(C1&0x1);
							
							GPIO_CTRL3N = ((C2&0x4)>>2);
							GPIO_CTRL3P = !((C2&0x4)>>2);
							GPIO_CTRL2N = ((C2&0x2)>>1);
							GPIO_CTRL2P = !((C2&0x2)>>1);
							GPIO_CTRL1N = (C2&0x1);
							GPIO_CTRL1P = !(C2&0x1);
							
							UART_SendFrame(Type,0x00);
						}break;
						case TYPE_IAP:{
							if(Data != 0x00)UART_SendFrame(Type,0xEE);
							else{
								UART_SendFrame(Type,0x00);
								for(i=0;i<10000;i++);
								IAP_CONTR |= 0x60;//复位到ISP
							}
						}break;
						default:UART_SendFrame(Type,0xEE);
					}
				}
				
				uartRxStat=UART_RX_IDLE;
			}
		}
}