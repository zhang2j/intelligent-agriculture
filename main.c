#include "SST89x5x4.H"
#include <string.h>
#include "Absacc.h"

/*注意！！！需要在编译环境设置中把Memory Model设置为Compact或者Large 如果用small的话会编译不通过*/

#define C8255_A XBYTE[0x7C00]  //A0/A1两根地址线为P2.0 P2.1
#define C8255_B XBYTE[0x7D00]  //此处为8255芯片的地址定义
#define C8255_C XBYTE[0x7E00]
#define C8255_CON XBYTE[0x7F00]
#define AD0809 XBYTE[0xEFFF]  //AD0809地址定义

#define uchar unsigned char //简化两种变量名称
#define uint unsigned int

#define STEP_MOTOR P1  //步进电机使用的IO口

#define TO_SECOND 20    //定时器定出一秒的溢出次数
#define CYCLE_NUM (Frequence+Period) //定时器计数器的循环周期

uint Frequence=30*TO_SECOND;      //浇水间隔
uint Period=5*TO_SECOND;          //浇水持续时间

uchar DISBUFF[]={0x00,0x00,0x00,0x00,0x00,0x00}; //显示缓冲区
uchar key_down=0; //按键是否被按下
uchar key_value=0; //按键值
uchar key_count=1; //键盘输出数码管编号

uchar  ADMARK;			// AD转换结束标志位
uchar  ADVALUE;			// AD采样值保存
uchar  YK;				// 反馈：测量温度值

sbit FAN=P2^5;      //风扇io口
sbit BEEP=P2^6;     //蜂鸣器io口

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

void Init0809(void)							//ADC0809初始化函数
{
	IT1 = 1;							// 外中断1为下降沿有效
	EX1 = 1;							// 允许INT1中断
	AD0809 = 1;						// 启动AD转换芯片
}

void delay(int a) //延时函数
{
	unsigned int j;
	while(a--)
		for(j=0;j<50;j++);
}

void delayms(int a) //1ms延时
{
	unsigned int j;
	while(a--)
		for(j=0;j<125;j++);
}

void Watering_on(void)  //开始浇水函数
{
	unsigned char L_value[8]={0x0E,0x0C,0x0D,0x09,0x0B,0x03,0x07,0x06}; //步进电机励磁顺序
	uchar i=0,m=0;
	for(i=0;i<3;i++){
				for(m=0;m<7;m++)
				{
					STEP_MOTOR&=0xF0; 
					STEP_MOTOR|=L_value[m];  //以上两行可以只改变STEP_MOTOR低四位
					delayms(5);
				}
			}
	STEP_MOTOR&=0xF0;
}

void Watering_off()    //结束浇水函数
{
	unsigned char L_value[8]={0x0E,0x0C,0x0D,0x09,0x0B,0x03,0x07,0x06};
	uchar i=0,m=0;
	for(i=0;i<3;i++){
				for(m=7;m>0;m--)
				{
					STEP_MOTOR&=0xF0;
					STEP_MOTOR|=L_value[m]; 
					delayms(5);
				}
			}
	STEP_MOTOR&=0xF0;
}

void Fanning_on()    //开风扇
{
	FAN=0;  //风扇低电平有效
}

void Fanning_off()   //关风扇
{
	FAN=1;
}

void Beep()      //蜂鸣器过热报警
{
	uchar i=0;
	for(i=0;i<200;i++){
		BEEP=1;
		delay(3);
		BEEP=0;
		delay(1);
	}
}

void Beep_short()      //蜂鸣器短响一声 用于键盘输入错误后的提示
{
	uchar i=0;
	for(i=0;i<20;i++){
		BEEP=1;
		delay(3);
		BEEP=0;
		delay(1);
	}
}



void display()  //倒计时显示函数
{
	uchar i=0,j=0xDF;
	for(;i<6;i++){ //六个共阴极数码管进行动态显示 利用了8255芯片
			 	C8255_A=j;
			 	C8255_B=DISBUFF[i];
				delay(2);
				j=(j>>1)|(j<<7);
			 }
}

void clear() //清屏 清除屏幕上用于显示键盘输入的部分的字符
{
	uchar i=0;
	for(;i<2;i++){
		DISBUFF[i]=0x00;
	}
	key_count=1;
}

void keyscan()					//按键扫描函数
{
	uchar cc;
	C8255_A = 0x00;				//X1～X4置0
	cc = C8255_C;				//得到Y1～Y4的值
	key_down = (~cc) & 0x0f;
}

void writebuffer()   //写显示缓冲
{
	static uchar key_num=0; //记录通过键盘输入的数的值
	unsigned char a[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07,
					 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71}; //共阴极数码管驱动码
	DISBUFF[key_count] = a[key_value];
	//将0-9九个数用于输入数值，C用于将数值设置为浇水时间，D用于将数值设置为浇水间隔，F用于清屏
	if(key_value<10&&key_count==1){//如果输入是0-9，并且是第一位
		key_num=key_value;
	}
	else if(key_value<10&&key_count==0){//如果输入了第二位，那么第一位乘10做十位，第二位相加做个位
		key_num=key_num*10+key_value;
	}

	key_count--;
	switch(key_value){//下面分别定义了C、D、F键被按下后的功能
		case 15: clear();break; //F清屏
		case 12: { //C设置浇水时间
			if(key_num>0&&key_num<100&&key_num>(Period/TO_SECOND)){
				Frequence=key_num*TO_SECOND;
				clear();
				}
			else{
				Beep_short(); //若设定失败则响铃一声提醒并且清屏
				clear();
				}
			break;
		}
		case 13: {//D设置浇水间隔
			if(key_num>0&&key_num<10&&key_num<Frequence){
				Period=key_num*TO_SECOND;
				clear();
				}
			else{
				Beep_short();
				clear();
				}
			break;
		}
		default: ;
	}

	if(key_count == -1){
		key_count = 1;}
	display();
	keyscan();
	while(key_down)				//键盘消抖
	{
		display();
		keyscan();
	}
}

void getkey()					//得到按键值
{
	unsigned char value;
	unsigned char i, j = 0xFE;

	for(i=0; i<4; i++)
	{
		C8255_A = j;
		value = C8255_C;

		if(!(value & 0x01))		//行1
		{
			key_value = i + 0;
			writebuffer();
			return;
		}

		if(!(value & 0x02))		//行2
		{
			key_value = i + 4;
			writebuffer();
			return;
		}

		if(!(value & 0x04))		//行3
		{
			key_value = i + 8;
			writebuffer();
			return;
		}

		if(!(value & 0x08))		//行4
		{
			key_value = i + 12;
			writebuffer();
			return;
		}

		j <<= 1;
	}
}

void Timer0() interrupt 1  //定时器0中断函数
{	
	uchar tab[]={0x00,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
	uint temp=0;
	TL0 = 0x00;		//设置定时初值
	TH0 = 0x4C;		//设置定时初值
	Time++; //周期为CYCLE_NUM的循环
	if(Time==Frequence){ //当时间到达设定好的周期时开始浇水
		Watering_on();
		water_flag=1; //正在浇水标志位，用于防止浇水时又有浇水命令二次浇水
	}

	if(water_flag){//当开始浇水后 开始进行倒计时显示
		if((Time-Frequence)%TO_SECOND==0){
			DISBUFF[5]=tab[(Period-(Time-Frequence))/TO_SECOND];//每过一秒刷新一次数码管
		}
		if(Time==CYCLE_NUM){//当浇水时间到达设定的值时 就停止浇水 并且重置时间为0
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
	char *symbol_water="water";//这三个是通过串口控制系统的口令 water是浇水 fan是开风扇 cfan是关风扇
	char *symbol_fan="fan";  //在发送时还需要在后面加上结束符"*"
	char *symbol_cfan="cfan";

	uchar receive_data[10]; //串口接收缓存 用于后续处理
	static uchar flag=0; //串口接收到第几位的标志
	uchar i=0;//循环变量

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
		Time=Frequence-1; //外部中断控制浇水
	}
}

void myint1(void) interrupt 2			// 外中断1，读AD转换结果
{
	ADVALUE = AD0809;
	ADMARK = 0x01;
	AD0809=1;  //发送开始AD转换信号
}

void main()
{
	uchar  TKMARK=0;			// 是否开了风扇标志位

	// 温度表，通过查询温度表可以把AD值转换为温度
	uchar a[]={0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
		       0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
		       0x1D, 0x1E, 0x1E, 0x1F, 0x20, 0x21, 0x23, 0x24, 0x25};

	//共阴极显示段码
	uchar b[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07,
				 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};

	//初始化部分
	P2=0x00;//IO口初始化
	FAN=1;
	BEEP=0;

	EA=1; //中断允许位初始化
	ES=1;
	ET0=1;
	EX0=1;
	IT0=1;

	PX0=1; //中断优先级初始化
	PT0=1;
	PS=0;

	C8255_CON=0x81;	 //8255初始化

	Timer0Init(); //串口和定时器初始化
	UartInit();

	Init0809();		//0809初始化


	while(1){
		display(); //循环调用显示函数 实现数码管动态显示

		keyscan();				//按键扫描
		if(key_down)			//判是否有键按下
		{
			display();
			delayms(20);//延时20ms进行按键消抖
			keyscan();
			if(key_down)
			{
				getkey();		//得到按键值
			}
		}

		
		if (ADMARK == 1){			//AD转换是否结束
			ADMARK=0;	
			YK = a[ADVALUE];					//查温度表
			DISBUFF[2]=b[YK%10];		//用数码管第二三位显示温度
			DISBUFF[3]=b[YK/10];
			if(YK>0x1E&&TKMARK==0){ //若温度大于30度 则开风扇响铃
				Fanning_on();
				Beep();
				TKMARK=1;  //是否开了风扇的标志位
			}
			if(YK<0x1C&&TKMARK==1){//若温度小于28度且开着风扇 则关风扇
				Fanning_off();
				TKMARK=0;
			}
		}
	}
}
