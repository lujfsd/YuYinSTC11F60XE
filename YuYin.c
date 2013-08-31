#include "STC11F60XE.h"
#include "intrins.h"
#include "ds18b20.h"
#include <string.h>

sbit JS = P2 ^ 4;//红外接收端口!
sbit Y  = P2 ^ 5; //红外发射端口 	 
sbit WF = P0 ^ 4; //无线发射端口 

sbit WIFI_LED = P3 ^ 6; //wifi复位LED指示灯
sbit WAKEUP_LED = P0 ^ 3; //唤醒状态指示灯
sbit RST = P1 ^ 4; //wifi复位RST 

#define LED_ON 1
#define LED_OFF !(LED_ON)

bit F = 0;	  //是否打开38KH方波调制
bit Wifi_Command_Mode = 0; //=1 wifi工作在命令模式 =0 工作在数据传输模式
bit Check_wifi = 1;
unsigned int RST_count1 = 0; //计数
unsigned int RST_count2 = 0;
unsigned char Temperature = 0; //温度
unsigned int T = 0;	//计数


unsigned int i = 0;//计数用 
unsigned int j = 0;//计数用
unsigned int c = 0;//计数用

unsigned int ui = 0;//串口接收数据长度!
xdata unsigned char US[800];//xdata unsigned char US[256]; //定义串口接收数据变量!


void Delay10ms()		//@22.1184MHz
{
	unsigned char i, j, k;

	i = 1;
	j = 216;
	k = 35;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}


void Delay100ms()		//@22.1184MHz
{
	unsigned char i, j, k;

	i = 9;
	j = 104;
	k = 139;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}


void U1_in()//串口1接收数据
{
	j = 0; //超时退出!
	ui = 0;
	while(j < 50000)//超时退出(大约1ms)!需要测试此值是否正确! 5000
	{
		if(RI == 1)
		{
			US[ui] = SBUF;
			if(US[ui] == '<' && US[ui - 1] == '<')
				break;
			RI = 0;
			ui++;
			j = 0;			
		}
		else
			j++;
		//Delay10us();//延时时间需要测试此值是否正确!(此处要加延时,要不数据接收不正确!)
	}	
	RI = 0;	
}

void U1_send(unsigned char i)//串口1发送单字节数据
{
	//TI = 0;			//令接收中断标志位为0（软件清零）
	SBUF = i;	//接收数据 SBUF 为单片机的接收发送缓冲寄存器
	while(TI==0);
	TI = 0;			//令接收中断标志位为0（软件清零）
}

void U1_sendS(unsigned char s[], unsigned int m)//串口1发送字符串数据,U1_sendS函数必须加"<<"结束标志!
{
	unsigned int n = 0;
	for(n = 0;n < m;n++)
		U1_send(s[n]);
}


void T0Init(void)		//13微秒@22.1184MHz
{
	AUXR &= 0x7F;		//定时器时钟12T模式
	TMOD &= 0xF0;		//设置定时器模式
	TMOD |= 0x02;		//设置定时器模式
	TL0 = 0xE8;		//设置定时初值
	TH0 = 0xE8;		//设置定时重载值
	TF0 = 0;		//清除TF0标志
	TR0 = 0;		//定时器0开始计时

	EA = 1;
	ET0 = 1;
}

int start_wifi_command()
{
	U1_sendS("+++",3);
	memset(US,0x00,sizeof(US));	
	U1_in();
	if(US[0] == 'a')
	{	
		memset(US,0x00,sizeof(US));
		//Delay50ms();
		U1_send('a');
		U1_in();			
		if(strstr(US,"+ok") != NULL)
		{
			Wifi_Command_Mode = 1;
			return 0; //切换成功
		}	
	}
	memset(US,0x00,sizeof(US));
	return 1;
}

int start_wifi_data()
{
	U1_sendS("AT+ENTM\r\n",9);
	U1_in();
	if(strstr(US,"+ok") != NULL)
	{		
		Wifi_Command_Mode = 0;
		return 0; //切换成功
	}
	return 1;	
}

void T0_C1 (void) interrupt 1  using 2 //单片机的中断号1对应的中断:定时器中断0
{		 
	T++;
	if(F == 1)
	    Y = ~Y;
}


typedef union //char型数据转int型数据类 
{  
	unsigned short int ue; 
	unsigned char 	 u[2]; 
}U16U8;
U16U8 idata M;//两个8位转16位

void U1Init(void)		//115200bps@22.1184MHz
{
	PCON |= 0x80;		//使能波特率倍速位SMOD
	SCON = 0x50;		//8位数据,可变波特率
	AUXR |= 0x04;		//独立波特率发生器时钟为Fosc,即1T
	BRT = 0xF4;		//设定独立波特率发生器重装值
	AUXR |= 0x01;		//串口1选择独立波特率发生器为波特率发生器
	AUXR |= 0x10;		//启动独立波特率发生器
	ES = 1;
}

void Rstinit()
{
	//配置为仅输入
	P1M1 |= (1<<4);
	P1M0 &= ~(1<<4);
}

void wifi_ap_open_led_blink()
{
	//灯闪烁处理
	WIFI_LED = !WIFI_LED;
	Delay100ms();
	Delay100ms();
	Delay100ms();
	Delay100ms();
	Delay100ms();
	WIFI_LED = !WIFI_LED;	
}
void main (void)
{
	WF = 0;
	WIFI_LED =LED_ON;// LED_ON;
	WAKEUP_LED = LED_OFF;
	U1Init();
	T0Init();
	Rstinit();
	Init_DS18B20();	
	//CH:<< 			红外采集命令		//CH:长度+数据<<	//采集后返回的数据
	//FH:长度+数据<<	红外发射命令
	//FW:长度+数据<<  	无线发射命令
	//FS:<<				心跳
	//网络传来的是byte格式的数据
	while(1)
	{
		if(Check_wifi)
		{
			if(!Wifi_Command_Mode)
			{
				start_wifi_command();
			}
			if(Wifi_Command_Mode)
			{
				//Delay50ms();
				Delay10ms();
				U1_sendS("AT+WMODE\r\n",10);
				Check_wifi = 0;	
			}
		}
		WIFI_LED = RST;	
		if(RST == 0)
		{
			while(RST == 0)
			{
				RST_count1++;
				if(RST_count1 == 65535)
				{
					RST_count1 = 0;
					RST_count2++;
				}
			}
			if(RST_count2 >= 5)
			{
				Wifi_Command_Mode = 0;
				Check_wifi = 1;
				RST_count1 = 0;
				RST_count2 = 0;
			}	
		}	
		if(RI==1)
		{
			U1_in();//获取串口发送的SJ数据!

			if(US[2] == ':')//接收到正确的控制数据!
			{
				switch(US[0])
				{
					case 'F'://红外、无线数据发射!
						WIFI_LED = LED_OFF;
						if(US[1]=='H')//红外
						{							
							i = 4;//第3与4位是数据长度,从第4位是红外、无线控制数据
							M.u[0] = US[3];
							M.u[1] = US[4];
							j = M.ue;
							TR0 = 1;		//启动定时器0
							while(i < j)//j是数据长度-1!
							{
								T = 0;
								F = 1;
								i++;
								if(US[i] == 0)//&&US[i+1]==0)
								{
									i += 2;
									M.u[0] = US[i];
									i++;	
									M.u[1] = US[i];
								}
								else
								{
									M.u[0] = 0;	
									M.u[1] = US[i];
								}
								while(T < M.ue);

								T = 0;
								F = 0;
								Y = 1;
								i++;
								if(US[i] == 0)//&&uip_appdata[i+1]==0)
								{
									i += 2;
									M.u[0] = US[i];
									i++;	
									M.u[1] = US[i];
								}
								else
								{
									M.u[0] = 0;	
									M.u[1] = US[i];
								}
								while(T < M.ue);								
							}
							TR0 = 0;		//关闭定时器0
							U1_sendS("FH<<", 4); 
							WIFI_LED = LED_ON;
						}
						else if(US[1]=='W')
						{						
							c = 0;
							TR0 = 1;		//启动定时器0
							while(c < 6)//重复次数!
							{
								T = 0;
								WF = 1;
								i = 4;//第3与4位是数据长度,从第5位是红外、无线控制数据
								while(T < 28);//(13 * 808 = 10504同步脉宽!									
								T = 0;
								WF = 0;
								M.u[0] = US[3];
								M.u[1] = US[4];
								j = M.ue;//主机生成的长度要减1
								while(T < 808);//(13 * 808 = 10504同步脉宽!

								while(i < j)
								{
									T = 0;
									WF = 1;
									i++;
									while(T < US[i]);

									T = 0;
									WF = 0;
									i++;//i在此,精准一些
									while(T < US[i]);
								}
								c++;
							}
							TR0 = 0;
							WF = 0;		//关闭定时器0
							U1_sendS("FW<<", 4);
						}
						else if(US[1]=='S')
						{
							U1_sendS("FS<<", 4);
						}

						break;

					case 'C'://红外采集!

					   	U1_sendS("CA<<", 4);//返回到主机请按遥控器("<<"在U1_sendS中添加)
						i = 5;//第3与4位是数据长度,从第4位是红外、无线控制数据
						j = 0;
						TR0 = 1;		//启动定时器0
						while(i < 756) //长度给这句有关-->>US[2] = i;//第三位是数据长度
						{ 
							T = 1;   //应该能提高准确率
     	  			while(JS == 0);
   	  				if(T > 5)
							{
								M.ue = T;
	              T = 1;
								if(M.u[0] > 0)
								{
									US[i] = 0;	//将接收的数据发送回去（删除//即生效）
									i++;
									US[i] = 0;	//将接收的数据发送回去（删除//即生效）
									i++;
									US[i] = M.u[0];	//将接收的数据发送回去（删除//即生效）
									i++;
								}
								US[i] = M.u[1];
								i++;	
								while(JS == 1)
								{								
									if(T > 6000)//无数据退出								
									{
										US[i] = 0;
										i++;

										M.ue = i;
										US[3] = M.u[0];//第3与4位是数据长度(包括数据头,不包括结尾!)
										US[4] = M.u[1];//第3与4位是数据长度(包括数据头,不包括结尾!)
									 		   
										US[i] = '<';
										i++;
										US[i] = '<';
										i++;

										US[0] = 'C';
										US[1] = 'H';
										US[2] = ':';																	
										U1_sendS(US, i);//红外采集成功

										i = 756;
										break;
									}
								}
								if(i < 756)
								{				
									M.ue = T;
				
									if(M.u[0] > 0)
									{
										US[i] = 0;
										i++;
										US[i] = 0;
										i++;
										US[i] = M.u[0];
										i++;
									}
									US[i] = M.u[1];
									i++;
									j = 0;
								}
							}
							else
							{
								while(JS == 1)
								{
									if(T > 50000)
									{
										T = 0;
										j++;
										if(j > 30)
										{
											i = 756;
											U1_sendS("CC<<", 4);//超时退出!大约20秒无操作退出!
											break;
										}
									}
								}
							}
						}
						TR0 = 0;		//关闭定时器0
						break;
					case 'D':		//温度
							if(US[1] == 'T')
							{
								memset(US,0x00,sizeof(US));
								US[0] = 'D';
								US[1] = 'T';
								while((US[2] = GetTemperature()) == 0x55);
								US[3] = '<';
								US[4] = '<';
								U1_sendS(US, 5);
							}
							break;
					case 'L': //唤醒状态指示灯
							if(US[1] == 'B')
							{
								WAKEUP_LED = LED_ON;
								U1_sendS("LB<<",4);
							}	
							else if(US[1] == 'D')
							{
								WAKEUP_LED = LED_OFF;
								U1_sendS("LD<<",4);
							}
							break;
					case 'S': //wifi复位
							if(US[1] == 'D')
							{
								Check_wifi = 1;
								Wifi_Command_Mode = 0;
								U1_sendS("SD<<",4);
							}
							break;
					default:break;	
				}
			}
			else if(strstr(US,"+ok") != NULL) //收到wifi模块返回的数据
			{
				if(strstr(US,"AP") != NULL) 	//wifi工作在AP模式
				{
					//Delay50ms();
					Delay10ms();
					U1_sendS("AT+WAKEY\r\n",10);
				}
				else if(strstr(US,"OPEN") != NULL) //AP模式下的open加密  
				{
					Check_wifi = 1;
					wifi_ap_open_led_blink();
				}
				else
				{
					if(start_wifi_data())
					{
						Check_wifi = 0;
						Wifi_Command_Mode = 0;
					}
				}
			}
		}
		memset(US,0x00,sizeof(US));//一个串口命令执行完毕, 清空
	}
}