#include "SST89x5x4.H"
#include <string.h>

#define uchar unsigned char
#define uint unsigned int

#define TO_SECOND 20    //定时器定出一秒的溢出次数
#define CYCLE_NUM (Frequence+Period) //定时器计数器的循环周期

uint Frequence=10*TO_SECOND;      //浇水间隔
uint Period=5*TO_SECOND;          //浇水持续时间

sbit FAN=P1^1;      //风扇io口
sbit BEEP=P1^6;     //蜂鸣器io口

uint Time=0;       //时间计数器

bit water_flag=0; //是否在浇水标志位


void UartInit(void)		//9600bps@11.0592MHz串口初始化
{
	PCON &= 0x7F;		//波特率不倍速
	SCON = 0x50;		//8位数据,可变波特率
	TMOD &= 0x0F;		//清除定时器1模式位
	TMOD |= 0x20;		//设定定时器1为8位自动重装方式
	TL1 = 0xFD;		//设定定时初值
	TH1 = 0xFD;		//设定定时器重装值
	ET1 = 0;		//禁止定时器1中断
	TR1 = 1;		//启动定时器1
}

void Timer0Init(void)		//50毫秒@11.0592MHz 定时器初始化
{
	TMOD |= 0x01;		//设置定时器模式
	TL0 = 0x00;		//设置定时初值
	TH0 = 0x4C;		//设置定时初值
	TF0 = 0;		//清除TF0标志
	TR0 = 1;		//定时器0开始计时
}

void delay(int a)
{
	unsigned int j;
	while(a--)
		for(j=0;j<50;j++);
}

void Watering_on(void)  //开始浇水函数
{
	unsigned char L_value[8]={0x0E,0x0C,0x0D,0x09,0x0B,0x03,0x07,0x06};
	uchar i=0,m=0;
	for(i=0;i<3;i++){
				for(m=0;m<7;m++)
				{
					P2&=0xF0; 
					P2|=L_value[m];  //只改变前四位
					delay(10);
				}
			}
	P2&=0xF0;
}

void Watering_off()    //结束浇水函数
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

void Fanning_on()    //开风扇
{
	FAN=1;
}

void Fanning_off()   //关风扇
{
	FAN=0;
}

void Beep()      //蜂鸣器
{
	uchar i=0;
	for(i=0;i<200;i++){
		BEEP=1;
		delay(3);
		BEEP=0;
		delay(1);
	}
}


void display(uchar i)  //倒计时显示函数
{
	uchar tab[]={0xFF,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xf8,0x80,0x90}; //共阳极驱动表
	P0=tab[i];  //取反后可驱动共阴极
}

void Timer0() interrupt 1  //定时器0中断函数
{	
	uint temp=0;
	TL0 = 0x00;		//设置定时初值
	TH0 = 0x4C;		//设置定时初值
	Time++; //周期为CYCLE_NUM的循环
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

void U_send (char *ptr)   //串口发送函数
{
	ES=0;
	while(*ptr!='\0'){
		SBUF=*(ptr++);
		while(TI!=1);
		TI=0;
	}
	ES=1;
}

void U_interrupt ()interrupt 4  //串口接收函数
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
			if(flag==strlen(symbol_water)){  //控制浇水
				for(i=0;i<flag;i++){
					if(symbol_water[i]!=receive_data[i])
						break;
				}
				if(i==flag&&!water_flag){
					U_send("Watering!\n");
					Time=Frequence-1;
				}
			}
			if(flag==strlen(symbol_fan)){  //控制开风扇
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
			
			if(flag==strlen(symbol_cfan)){  // 控制关风扇
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

void INT0_water() interrupt 0  //外部中断0中断函数
{
	if(!water_flag){
		Time=Frequence-1;
	}
}

void main()
{
	//初始化部分
	P2=0x00;//IO口初始化
	FAN=0;
	BEEP=0;

	EA=1; //中断允许位初始化
	ES=1;
	ET0=1;
	EX0=1;
	IT0=1;

	PX0=1; //中断优先级初始化
	PT0=1;
	PS=0;

	Timer0Init(); //串口和定时器初始化
	UartInit();

	while(1);
}
