#include "SST89x5x4.H"
#include <string.h>

#define uchar unsigned char
#define uint unsigned int

#define TO_SECOND 20    //��ʱ������һ����������
#define CYCLE_NUM (Frequence+Period) //��ʱ����������ѭ������

uint Frequence=10*TO_SECOND;      //��ˮ���
uint Period=5*TO_SECOND;          //��ˮ����ʱ��

sbit FAN=P1^1;      //����io��
sbit BEEP=P1^6;     //������io��

uint Time=0;       //ʱ�������

bit water_flag=0; //�Ƿ��ڽ�ˮ��־λ


void UartInit(void)		//9600bps@11.0592MHz���ڳ�ʼ��
{
	PCON &= 0x7F;		//�����ʲ�����
	SCON = 0x50;		//8λ����,�ɱ䲨����
	TMOD &= 0x0F;		//�����ʱ��1ģʽλ
	TMOD |= 0x20;		//�趨��ʱ��1Ϊ8λ�Զ���װ��ʽ
	TL1 = 0xFD;		//�趨��ʱ��ֵ
	TH1 = 0xFD;		//�趨��ʱ����װֵ
	ET1 = 0;		//��ֹ��ʱ��1�ж�
	TR1 = 1;		//������ʱ��1
}

void Timer0Init(void)		//50����@11.0592MHz ��ʱ����ʼ��
{
	TMOD |= 0x01;		//���ö�ʱ��ģʽ
	TL0 = 0x00;		//���ö�ʱ��ֵ
	TH0 = 0x4C;		//���ö�ʱ��ֵ
	TF0 = 0;		//���TF0��־
	TR0 = 1;		//��ʱ��0��ʼ��ʱ
}

void delay(int a)
{
	unsigned int j;
	while(a--)
		for(j=0;j<50;j++);
}

void Watering_on(void)  //��ʼ��ˮ����
{
	unsigned char L_value[8]={0x0E,0x0C,0x0D,0x09,0x0B,0x03,0x07,0x06};
	uchar i=0,m=0;
	for(i=0;i<3;i++){
				for(m=0;m<7;m++)
				{
					P2&=0xF0; 
					P2|=L_value[m];  //ֻ�ı�ǰ��λ
					delay(10);
				}
			}
	P2&=0xF0;
}

void Watering_off()    //������ˮ����
{
	unsigned char L_value[8]={0x0E,0x0C,0x0D,0x09,0x0B,0x03,0x07,0x06};
	uchar i=0,m=0;
	for(i=0;i<3;i++){
				for(m=7;m>0;m--)
				{
					P2&=0xF0;
					P2|=L_value[m]; 
					delay(10);
				}
			}
			P2&=0xF0;
}

void Fanning_on()    //������
{
	FAN=1;
}

void Fanning_off()   //�ط���
{
	FAN=0;
}

void Beep()      //������
{
	uchar i=0;
	for(i=0;i<200;i++){
		BEEP=1;
		delay(3);
		BEEP=0;
		delay(1);
	}
}


void display(uchar i)  //����ʱ��ʾ����
{
	uchar tab[]={0xFF,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xf8,0x80,0x90}; //������������
	P0=tab[i];  //ȡ���������������
}

void Timer0() interrupt 1  //��ʱ��0�жϺ���
{	
	uint temp=0;
	TL0 = 0x00;		//���ö�ʱ��ֵ
	TH0 = 0x4C;		//���ö�ʱ��ֵ
	Time++; //����ΪCYCLE_NUM��ѭ��
	if(Time==Frequence){
		Watering_on();
		water_flag=1;
	}

	if(water_flag){
		if((Time-Frequence)%TO_SECOND==0){
			display((Period-(Time-Frequence))/TO_SECOND);
		}
		if(Time==CYCLE_NUM){
			Watering_off();
			water_flag=0;
			Time=0;
		}		
	}
}

void U_send (char *ptr)   //���ڷ��ͺ���
{
	ES=0;
	while(*ptr!='\0'){
		SBUF=*(ptr++);
		while(TI!=1);
		TI=0;
	}
	ES=1;
}

void U_interrupt ()interrupt 4  //���ڽ��պ���
{
	char *symbol_water="water";
	char *symbol_fan="fan";
	char *symbol_cfan="cfan";

	uchar receive_data[10];
	static uchar flag=0;
	uchar i=0;

	if(RI){
		RI=0;
		if(SBUF!='*'&&flag<8){
			receive_data[flag++]=SBUF;
		}
		else{
			if(flag==strlen(symbol_water)){  //���ƽ�ˮ
				for(i=0;i<flag;i++){
					if(symbol_water[i]!=receive_data[i])
						break;
				}
				if(i==flag&&!water_flag){
					U_send("Watering!\n");
					Time=Frequence-1;
				}
			}
			if(flag==strlen(symbol_fan)){  //���ƿ�����
				for(i=0;i<flag;i++){
					if(symbol_fan[i]!=receive_data[i])
						break;
				}
				if(i==flag){
					U_send("Fanning!\n");
					Fanning_on();
					Beep();
				}
			}
			
			if(flag==strlen(symbol_cfan)){  // ���ƹط���
				for(i=0;i<flag;i++){
					if(symbol_cfan[i]!=receive_data[i])
						break;
				}
				if(i==flag){
					U_send("Turn off the fan!\n");
					Fanning_off();
				}
			}
			flag=0;
		}
	}
}

void INT0_water() interrupt 0  //�ⲿ�ж�0�жϺ���
{
	if(!water_flag){
		Time=Frequence-1;
	}
}

void main()
{
	//��ʼ������
	P2=0x00;//IO�ڳ�ʼ��
	FAN=0;
	BEEP=0;

	EA=1; //�ж�����λ��ʼ��
	ES=1;
	ET0=1;
	EX0=1;
	IT0=1;

	PX0=1; //�ж����ȼ���ʼ��
	PT0=1;
	PS=0;

	Timer0Init(); //���ںͶ�ʱ����ʼ��
	UartInit();

	while(1);
}
