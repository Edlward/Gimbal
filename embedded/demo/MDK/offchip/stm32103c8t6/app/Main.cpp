#include "stm32f10x.h"
#include "Configuration.h"
#include "TaskManager.h"
#include "USART.h"
#include "I2C.h"
#include "Timer.h"
#include "ADC.h"
#include "PWM.h"
#include "flash.h"
#include "InputCapture_TIM.h"
#include "InputCapture_EXIT.h"
#include "LED.h"
#include "mpu6050.h"
#include "HMC5883L.h"
#include "BLDCMotor.h"
#include "Flash.h"


#include "Communicate.h"
#include "Gimbal.h"
/************************************硬件定义*************************************/
//Timer T1(TIM1,1,2,3); //使用定时器计，溢出时间:1S+2毫秒+3微秒
USART com(1,115200,false);
I2C i2c2(2); 
mpu6050 mpu6050(i2c2,1000);
HMC5883L mag(i2c2,1000);
PWM pwm2(TIM2,1,1,1,1,20000);  //开启时钟2的4个通道，频率2Whz
PWM pwm3(TIM3,1,1,0,0,20000);  //开启时钟3的2个通道，频率2Whz
PWM pwm4(TIM4,1,1,1,0,20000);  //开启时钟4的3个通道，频率2Whz
//InputCapture_TIM t4(TIM4, 400, true, true, true, true);
//InputCapture_EXIT ch1(GPIOB,6);
ADC adcArray(4,5); //读取ADC value
flash infoStore(0x08000000+63*MEMORY_PAGE_SIZE,true);     //flash


//LED
GPIO ledGreenGPIO(GPIOB,0,GPIO_Mode_Out_PP,GPIO_Speed_50MHz);//LED GPIO
GPIO ledBlueGPIO(GPIOB,1,GPIO_Mode_Out_PP,GPIO_Speed_50MHz);//LED GPIO
LED ledGreen(ledGreenGPIO);//LED red
LED ledBlue(ledBlueGPIO);//LED blue

//BLDC Motor
BLDCMotor motorRoll(&pwm2,1,&pwm2,2,&pwm2,3,0.6);  //roll motor
BLDCMotor motorPitch(&pwm2,4,&pwm3,1,&pwm3,2,0.4); //pitch motor
BLDCMotor motorYaw(&pwm4,1,&pwm4,2,&pwm4,3,0.55);   //yaw motor


/**************************************************************************/


/*************************全局变量*****************************************/

Gimbal gimbal(mpu6050,mag,motorRoll,motorPitch,motorYaw,adcArray,4,5,infoStore);

Communicate communicate(gimbal,com);

/**************************************************************************/



/**
  *系统初始化
  *
  */
void init()
{
	ledBlue.Off();
	ledGreen.Off();
	if(mpu6050.Init())
		ledBlue.On();
	if(mag.Init())
		ledGreen.On();
	gimbal.Init(1);//电位器控制yaw轴，参数为2表示磁力计作为yaw轴的数据
	gimbal.mIsArmed = true;
	gimbal.mYawMode = 1;//跟随模式
}

int motorValueRoll,motorValuePitch,motorValueYaw;

/**
  *循环体
  *
  */
void loop()
{
	 static double updateSlice=0, dataSendSlice= 0,voltageSendTimeSlice=0; //taskmanager时间 测试
	
	//系统运行指示灯，1s闪烁一次
	ledBlue.Blink(0,0.5,false);
	//更新姿态、控制电机，500Hz
 	if(tskmgr.TimeSlice(updateSlice,0.002)) //每0.002秒(500Hz)执行一次
 	{
 		gimbal.UpdateIMU();//更新姿态
 		gimbal.UpdateMotor(&motorValueRoll,&motorValuePitch,&motorValueYaw);//控制电机
 	}
	
 	//输出电源值和飞机姿态数据、电机数据。12.5Hz
 	if(tskmgr.TimeSlice(dataSendSlice,0.1)) 
 	{
 //		if(gimbal.IsGyroCalibrated() && gimbal.IsMagCalibrated())//已经校准完毕
 //		{
 			ledGreen.Toggle();
 			communicate.ANO_DT_Send_Status(gimbal.mAngle.y*RtA,gimbal.mAngle.x*RtA,gimbal.mAngle.z*RtA,0,1,gimbal.mIsArmed);
 			//communicate.ANO_DT_Send_MotoPWM(motorValueRoll%256+256,motorValuePitch%256+256,motorValueYaw%256+256,0,0,0,0,0);
 			communicate.ANO_DT_Send_MotoPWM(motorValueRoll,motorValuePitch,motorValueYaw,0,0,0,0,0);
 			//communicate.ANO_DT_Send_Power(gimbal.UpdateVoltage(4,5.1,1,12)*100,0);
 			if(!gimbal.IsMagCalibrated())
 				communicate.ANO_DT_Send_Senser(mpu6050.GetAcc().x*1000,mpu6050.GetAcc().y*1000,mpu6050.GetAcc().z*1000,mpu6050.GetGyrRaw().x,mpu6050.GetGyrRaw().y,mpu6050.GetGyrRaw().z,mag.xMaxMinusMin,mag.yMaxMinusMin,mag.zMaxMinusMin,0);
 			else
 				communicate.ANO_DT_Send_Senser(mpu6050.GetAcc().x*1000,mpu6050.GetAcc().y*1000,mpu6050.GetAcc().z*1000,mpu6050.GetGyrRaw().x,mpu6050.GetGyrRaw().y,mpu6050.GetGyrRaw().z,mag.GetDataRaw().x,mag.GetDataRaw().y,mag.GetDataRaw().z,0);
 			communicate.ANO_DT_Send_RCData(0,(gimbal.mTargetAngle.z+180)*2.77778+1000,(gimbal.mTargetAngle.y+180)*2.77778+1000,(gimbal.mTargetAngle.x+180)*2.77778+1000,0,0,0,0,0,0);
 //		}
 //		else if(gimbal.IsGyroCalibrating()||gimbal.IsMagCalibrating())
 //			LOG("..");
//			com<<mpu6050.GetAcc().x<<"\t"<<mpu6050.GetAcc().y<<"\t"<<mpu6050.GetAcc().z<<"\t"<<mpu6050.GetGyrRaw().x<<"\t"<<mpu6050.GetGyrRaw().y<<"\t"<<mpu6050.GetGyrRaw().z<<"\t"<<mag.GetDataRaw().x<<"\t"<<mag.GetDataRaw().y<<"\t"<<mag.GetDataRaw().z<<"\n";
		
 	}
	
 	if(tskmgr.TimeSlice(voltageSendTimeSlice,2)) 
 	{
 		if(gimbal.IsGyroCalibrated() && gimbal.IsMagCalibrated())//已经校准完毕
 		{
 			communicate.ANO_DT_Send_Power(gimbal.UpdateVoltage(5.1,1,12)*100,0);
 		}
 	}
	
 	//处理来自地面站的消息
 	communicate.ANO_DT_Data_Receive_Deal(gimbal);
}


int main()
{
	TaskManager::DelayS(3);//延时，等待传感器上电自启动完毕
	init();	
	while(1)
	{
		loop();
	}
}




