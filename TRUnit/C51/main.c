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
// ͨ��Э��:
#define TYPE_SETADDR 0x01
// ���õ�ַ��    AA55 01 FF XX CRC,XXΪ��ַ 0x01~0xFE��Ч
// �ɹ�����55AA 01 XX 01 CRC XXΪ��ַ
// ʧ�ܣ���55AA 01 XX EE CRC
#define TYPE_SETSWITCH 0x02
// ���ÿ���״̬��AA55 02 XX YY CRC,XXΪ��ַ(��ַһ�»�ΪXX=0xFF�㲥��ַʱ��ģ�����Ӧ),YYΪ���ݣ�λ�����£�
// B7   B6   B5   B4   B3   B2   B1   B0
// TRX1 C1_2 C1_1 C1_0 EN C2_2 C2_1 C2_0
// TRX��ʾ�շ��л� 1Ϊ��0Ϊ��
// EN=1��ʾ��Ƶ��·ͨ 0��
// C1��ʾ1��(���߳��� �ӳ���һ�濴�ұߣ���������������ֵ 000=0��  001=45��  111=315��   C2ͬ��
// �ɹ�����55AA 01 XX 01 CRC XXΪ��ַ
// ʧ�ܣ���55AA 01 XX EE CRC
// ����CRC�㷨Ϊ��AA55�󣨲���AA55�� ��CRC֮ǰ���������ֽ������͵ĵ�8λ
#define TYPE_IAP 0x03
// �����̼���AA55 03 FF 00 02

//	��������		��������	
//����ֵ	��λ	����	��λ	����
//0	70.92	27.7	66.3	27.88
//1	120.23	26.98	117.3	27.2
//2	162.23	28.49	162.22	28.53
//3	-148.48	28.4	-149.62	28
//4	-97.57	27.33	-100.76	27.01
//5	-53.72	26.72	-57.64	26.45
//6	-14.15	28.73	-16.84	28.29
//7	35.90 	29.52	31.34	29.15
//��һ��	��������		��������	
//����ֵ	��λ���	����	��λ���	����
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
    IAP_CONTR = 0;                              //�ر�IAP����
    IAP_CMD = 0;                                //�������Ĵ���
    IAP_TRIG = 0;                               //��������Ĵ���
    IAP_ADDRH = 0x80;                           //����ַ���õ���IAP����
    IAP_ADDRL = 0;
}

char IapRead(int addr)
{
    char dat;

    IAP_CONTR = 0x80;                           //ʹ��IAP
    IAP_TPS = 12;                               //���õȴ�����12MHz
    IAP_CMD = 1;                                //����IAP������
    IAP_ADDRL = addr;                           //����IAP�͵�ַ
    IAP_ADDRH = addr >> 8;                      //����IAP�ߵ�ַ
    IAP_TRIG = 0x5a;                            //д��������(0x5a)
    IAP_TRIG = 0xa5;                            //д��������(0xa5)
    _nop_();
    dat = IAP_DATA;                             //��IAP����
    IapIdle();                                  //�ر�IAP����

    return dat;
}

void IapProgram(int addr, char dat)
{
    IAP_CONTR = 0x80;                           //ʹ��IAP
    IAP_TPS = 12;                               //���õȴ�����12MHz
    IAP_CMD = 2;                                //����IAPд����
    IAP_ADDRL = addr;                           //����IAP�͵�ַ
    IAP_ADDRH = addr >> 8;                      //����IAP�ߵ�ַ
    IAP_DATA = dat;                             //дIAP����
    IAP_TRIG = 0x5a;                            //д��������(0x5a)
    IAP_TRIG = 0xa5;                            //д��������(0xa5)
    _nop_();
    IapIdle();                                  //�ر�IAP����
}

void IapErase(int addr)
{
    IAP_CONTR = 0x80;                           //ʹ��IAP
    IAP_TPS = 12;                               //���õȴ�����12MHz
    IAP_CMD = 3;                                //����IAP��������
    IAP_ADDRL = addr;                           //����IAP�͵�ַ
    IAP_ADDRH = addr >> 8;                      //����IAP�ߵ�ַ
    IAP_TRIG = 0x5a;                            //д��������(0x5a)
    IAP_TRIG = 0xa5;                            //д��������(0xa5)
    _nop_();                                    //
    IapIdle();                                  //�ر�IAP����
}


void main()
{
	unsigned int T2Val;
	int i;
	unsigned char TRX,Enable,C1,C2; 
	// ��ʼ��GPIO
	P1M0=0xff;P1M1=0x00;
	P3M0=0xfc;P3M1=0x01; // ����Ϊ��©��� ���Զ����һ�� �õ�ַ����ѡ��
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
	
	// ��ʼ������
	SCON = 0x50;
	T2Val = 65536-CRYSTAL/BAUDRATE/4;
	T2L = T2Val&0xFF;                                 //65536-11059200/115200/4=0FFE8H
	T2H = T2Val>>8;
	AUXR = 0x15;                                //������ʱ��
	// ��������ַ
	MCUADDRESS=IapRead(EEPROM_MCUADDRESS);
	if(MCUADDRESS == 0x00 || MCUADDRESS == 0xFF){
		MCUADDRESS = 0x01;
		IapErase(EEPROM_MCUADDRESS);
		IapProgram(EEPROM_MCUADDRESS,MCUADDRESS);
	}
	
	GPIO_TXEN = MCUADDRESS < 0x80;
	GPIO_RXEN = MCUADDRESS >= 0x80;
	// ��ʼ���ж�
	ES = 1;// �������ж�
	EA = 1;// �����ж�
	//����״̬��
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
								IAP_CONTR |= 0x60;//��λ��ISP
							}
						}break;
						default:UART_SendFrame(Type,0xEE);
					}
				}
				
				uartRxStat=UART_RX_IDLE;
			}
		}
}