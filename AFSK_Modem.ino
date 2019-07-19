#define PTT 9
#define CDATA 5
#define RDATA 7
#define SCLK 4
#define CS 6
#define RXFF A3       //rx freeformat, generally set to 1
#define FX809IRQ 3
#define KEYPAD A6     //read resistor divider keypad


#define GENERAL_RESET  0x01
#define W_CONTROL_REG  0x40
#define R_STATUS_REG   0x41
#define R_RXDATA       0x42
#define W_TXDATA       0x43
#define W_SYNC         0x44
#define FX809SETTINGS 0b00111000

byte tx_buffer[64];
byte tx_buffer_counter = 0;

byte rx_buffer[64];
byte rx_buffer_counter = 0;



void write_byte(byte data)
{
  delayMicroseconds(3);
  for(int i=7;i>0;i--)
  {digitalWrite(CDATA,(data&bit(i)));
   delayMicroseconds(1);
   digitalWrite(SCLK,1);
   delayMicroseconds(2);
   digitalWrite(SCLK,0);
   delayMicroseconds(2);
  }
 delayMicroseconds(3);  
}

byte read_byte()
{
  byte rxdata = 0;
  delayMicroseconds(3);
  for(int i=7;i>0;i--)
  {
    digitalWrite(SCLK,1);
    delayMicroseconds(2);
    if(digitalRead(RDATA))
       rxdata+=bit(i);
    digitalWrite(SCLK,0);
    delayMicroseconds(2);
  }
  delayMicroseconds(3); 
  return rxdata;
}




void fx809_init()
{
  pinMode(CDATA,OUTPUT);
  pinMode(SCLK,OUTPUT);
  pinMode(CS,OUTPUT);
  pinMode(RXFF,OUTPUT);
  pinMode(RDATA,INPUT);
  pinMode(FX809IRQ,INPUT_PULLUP);
  
  digitalWrite(RXFF, HIGH);   //turn off freeformat mode
  digitalWrite(CS, HIGH);     //setup chip select to idle
  digitalWrite(PTT, LOW);
  digitalWrite(SCLK, LOW);    //data is clocked in/out rising edge
  attachInterrupt(FX809IRQ,FX809_ISR, FALLING); 
  
  digitalWrite(CS,0);
  write_byte(GENERAL_RESET);
  digitalWrite(CS,1);
  delay(1);

  digitalWrite(CS,0);
  write_byte(W_CONTROL_REG);
  write_byte(FX809SETTINGS);
  digitalWrite(CS,1);
  delay(1);

  digitalWrite(CS,0);
  write_byte(W_SYNC);
  write_byte(0xda);
  write_byte(0xba);
  digitalWrite(CS,1);
  delay(1);
  
}

void FX809_ISR()
{
  byte fx809status = 0;
  
  digitalWrite(CS,0);
  write_byte(R_STATUS_REG);
  fx809status = read_byte() & 0x3F;
  digitalWrite(CS,1);
  delayMicroseconds(5);

  if(fx809status & 0x01)    //Rx Data ready
  {
      digitalWrite(CS,0);
      write_byte(R_RXDATA);
      rx_buffer[rx_buffer_counter++] = read_byte();
      digitalWrite(CS,1);
  }

  if(fx809status & 0x02)    //RX Checksum true
  {}
  if(fx809status & 0x04)    //Tx Data Ready
  {}
  if(fx809status & 0x08)    //Tx Idle
  {}
  if(fx809status & 0x10)    //RX Sync Detect
  {}
  if(fx809status & 0x20)    //RX SyncP Detect
  {}

  
}

void fx809_transmit()
{
  int counter = 0;
  
  digitalWrite(CS,0);
  write_byte(W_CONTROL_REG);
  write_byte(FX809SETTINGS+1);    //enable tx 
  digitalWrite(CS,1);
  delay(10);
  digitalWrite(PTT,1);            
  delay(200);
  detachInterrupt(FX809IRQ);      //disable interrupt
  
  digitalWrite(CS,0);
  write_byte(W_TXDATA);
    
  while(counter< tx_buffer_counter)
  {
    write_byte(tx_buffer[counter++]); 
    while(digitalRead(FX809IRQ));       //wait interrupt pin to indicate tx buffer is empty
    }  
  
  digitalWrite(CS,1);
  delay(50);
  digitalWrite(PTT,0); 

  digitalWrite(CS,0);
  write_byte(W_CONTROL_REG);
  write_byte(FX809SETTINGS);    //enable rx 
  digitalWrite(CS,1);
             
  attachInterrupt(FX809IRQ,FX809_ISR, FALLING); 
}




void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PTT,OUTPUT);
  digitalWrite(PTT,LOW);
  
  digitalWrite(LED_BUILTIN, HIGH);   
  fx809_init();
  delay(2000);                       
  digitalWrite(LED_BUILTIN, LOW);    
 
  
}

void loop() {
  
  digitalWrite(LED_BUILTIN, HIGH);   
  
  strcpy(tx_buffer,"testing12345");
  tx_buffer_counter = 12;
  fx809_transmit();
  Serial.print("tx");
                                    
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(2000);
  
  if(rx_buffer_counter!=0)
  {  for(int i=0;i<rx_buffer_counter;i++)
      {Serial.print(rx_buffer[i],HEX); Serial.print(",");}
       Serial.println("");
    for(int i=0;i<rx_buffer_counter;i++)
      {Serial.print(rx_buffer[i]); Serial.print(" ");}
       Serial.println("");
    Serial.print(rx_buffer_counter); Serial.println("  Bytes received.");
    rx_buffer_counter=0;
  }






    
}
