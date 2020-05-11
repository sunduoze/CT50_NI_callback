///< ct50 ni rework "Does not have NI battery installed."
///< @enzo 20200510

#include "stdio.h"///< printf lib
#include "Wire.h"///< i2c lib

#define MAX17201_MAX_NVM_NUM  7u

///< define var
uint8_t max_17201_0x16_addr = 0x0B;//0x16
uint8_t max_17201_0x6c_addr = 0x36;//0x6C

///< nDeviceName4_data
uint16_t ct50_ni_ver = 0x0001; ///< CT50 NI Version

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  Serial.begin(115200);
  Wire.begin();///< i2c init NULL: master addr: slave
  printf_begin();
  printf("\r\n\r\ntest fw version A01 @20200511\r\n");
  printf("ct50 ni callback process:%x !\r\n", ct50_ni_ver);
  
  if (max17201_wt_ct50_ni_data() != true){
    test_status(false);
  }
  else{
    test_status(true);
  }
}

void loop() {
  
}

void max17201_full_reset(void)
{
  ///< 6-9 is Full Reset; 8-9 is Fuel Guage Reset.
  ///< 6 Write 0x000F to the Command register 0x060 to POR the IC.
  i2c_dev_w_word(max_17201_0x6c_addr, 0x60, 0x000F);
  ///< 7 wait tPOR .min 10ms for the IC to reset. 
  delay(100); 
  ///< 8 Write 0x0001 to Counter Register 0x0BB to reset firmware.
  i2c_dev_w_word(max_17201_0x6c_addr, 0xBB, 0x0001);
  ///< 9 wait tPOR .min 10ms for the IC to reset. 
  delay(100); 
}

bool max17201_burn_nv_block(void)
{
  uint16_t r_data_temp = 0;
  ///< DS p.97.
  ///< 1 RAM data is storage. ??? place first flow.

  ///< 2 Clear CommStat.NVError bit.
  i2c_dev_w_word(max_17201_0x6c_addr, 0x61, 0x0000);
  delay(10);
  ///< 3 COPY NV BLOCK [E904h]
// This command copies the entire block from shadow RAM
// to NVM addr 180h to 1DFh excluding the UUID locations of 1BCh to 1BFh.
  i2c_dev_w_word(max_17201_0x6c_addr, 0x60, 0xE904);
  ///< 4 wait tBlcok Typ.368ms Max.7360ms
  delay(368); 
  ///< 5 Check the CommStat.NVError bit. If set, repeat the process. If clear, continue.
  r_data_temp = 0xffff;
  for (uint8_t i = 0; i < 70; i++)/// tBlcok timeout 7000ms Typ.368ms Max.7360ms
  {
  	i2c_dev_r_word(max_17201_0x6c_addr, 0x61, &r_data_temp);
  	if (r_data_temp != 0x0000)
  	{
  		delay(100);
  		if (i >= 70){
		    printf("ERROR: CommStat.NVError bit clear fail:0x%4x\r\n", r_data_temp);
  			return false;
  		}
  	}
  	else{
  		printf("CommStat.NVError bit clear ok:0x%4x\r\n", r_data_temp);
  		break;
  	}
  }
  ///< 6-9
  max17201_full_reset();
  return true;
}

bool max17201_verify_n_device_name4_data(const uint16_t data)
{
  uint16_t r_data_temp = 0;
  max17201_full_reset();
  printf("version idn :0x%4x\r\n", data);
  i2c_dev_r_word(max_17201_0x16_addr, 0xDF, &r_data_temp);

  if (r_data_temp == data){
      printf("verify ram data ok:0x%4x\r\n", r_data_temp);
      return true;
  }
  else{
     printf("ERROR verify ram data false:0x%4x\r\n", r_data_temp);
     return false;   
  }
}

bool max17201_wt_ct50_ni_data(void)
{ ////////////////////////////////////wt to RAM
	///< TO BE WRITE
	uint16_t r_data_temp = 0;

  max17201_read_remain_times();
	max17201_full_reset();
  printf("max17201_full_reset\r\n");
  if (max17201_verify_n_device_name4_data(ct50_ni_ver) == true){
  	printf("ct50 ni version, do not change data\r\n");
    
    if (max17201_shutdown() != true){
      printf("ct50 ni version, shutdown fail");
      return false;
    }
    printf("ct50 ni version, shutdown pass");
  	return true;
    
  }

  i2c_dev_w_word(max_17201_0x16_addr, 0xDF, ct50_ni_ver);
  delay(10);

  if (max17201_verify_n_device_name4_data(ct50_ni_ver) == true){
  	printf("version info ram changed ok !\r\n");
  }

  if (max17201_burn_nv_block() != true){
    printf("max17201_burn_nv_block fail !\r\n");
  	return false;
  }
  printf("max17201_burn_nv_block pass !\r\n");

  if (max17201_verify_n_device_name4_data(ct50_ni_ver) == false){
      printf("ERROR last verify nvm error, burn time is 0 !\r\n");
      max17201_shutdown();
      return false;
  }
  
  if (max17201_shutdown() != true){
    printf("after max17201_burn_nv_block, shutdown fail !\r\n");
  	return false;
  }
  printf("after max17201_burn_nv_block, shutdown pass !\r\n");
  return true;
}

bool max17201_read_remain_times(void)
{ 
  uint16_t r_data_temp = 0;
  uint8_t remain_times = 0; 

  i2c_dev_w_word(max_17201_0x6c_addr, 0x60, 0xE2FA);
  delay(5);///<tRECALL. max 5ms.
  i2c_dev_r_word(max_17201_0x16_addr, 0xED, &r_data_temp);
  r_data_temp &= 0xFF;///< only get high byte

  while(r_data_temp != 0){
    if(r_data_temp & 0x1){
      remain_times ++;
    }
    r_data_temp >>= 1;
  }
  remain_times --;
  
  remain_times = MAX17201_MAX_NVM_NUM - remain_times;
  if (remain_times == 0){
    printf("warning:remain times :%d \r\n", remain_times);
    return false;
  }
  else{
    printf("remain times :%d \r\n", remain_times);
    return true;
  }
}

bool max17201_shutdown(void)
{
  uint16_t r_data_temp = 0;
  ///< 
  i2c_dev_w_word(max_17201_0x6c_addr, 0x1D, 0x2294);
  delay(10);///< 200ms will read back fail
  i2c_dev_r_word(max_17201_0x6c_addr, 0x1D, &r_data_temp);
  if (r_data_temp == 0x2294){
    Wire.end();
    //#error  "此处等待确认，是否需要休眠？  如果不在意，则屏蔽false"

    printf("shutdown 0x1D ok, rd:0x%x\r\n", r_data_temp);
    return true;
    
  }
  else{
    Wire.end();
    //#error  "此处等待确认，是否需要休眠？  如果不在意，则屏蔽false"
    printf("shutdown 0x1D fail, rd:0x%x\r\n", r_data_temp);
    return false;
  }
}


void test_status(bool st )
{
  if(st == true)
  {
    while(1){
      digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(50);                       
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
      delay(50);                       
    }
  }
  if(st == false){
    while(1){
      //printf("ERROR:ERROR\r\n");
      digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(1000);                       
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
      delay(1000);    
    }
  }
}

///< 串口重映射
int serial_putc(char c, struct __file *)
{
  Serial.write(c);
  return c;
}
void printf_begin(void)
{
  fdevopen( &serial_putc, 0 );
}

bool i2c_dev_w_word(uint8_t dev_addr, uint8_t reg_addr, uint16_t data)
{
  Wire.beginTransmission(dev_addr);
  Wire.write((int)(reg_addr & 0xFF)); // and the right
  Wire.write(data & 0xFF);
  Wire.write(data >> 8);
  Wire.endTransmission();
  
  delay(2);
  return true;
}

bool i2c_dev_r_word(uint8_t dev_addr, uint8_t reg_addr, uint16_t *data)
{
  uint8_t data_temp = 0;
  Wire.beginTransmission(dev_addr);
  Wire.write((int)(reg_addr & 0xFF)); // and the right  
  Wire.endTransmission();
  Wire.requestFrom(dev_addr, 2); // now get the byte of data...
  *data = Wire.read();   
  data_temp = Wire.read();
  *data += (data_temp << 8);
  
  delay(2);
  return true;
}
//  Serial.println(pktSize, HEX);
