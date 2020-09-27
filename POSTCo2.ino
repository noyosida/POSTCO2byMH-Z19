#include <M5StickC.h>
#include <WiFiClientSecure.h>

const char* ssid     = "";
const char* password = "";

const String host = "script.google.com";   // コピーしたURLのホスト部
const String url = "";  


//MHZ19センサ用
unsigned long getDataTimer = 0; //時間保持用
const unsigned long getDataPeriod = 1000; //測定周期[ms]
int CO2 = 400;
int temperature = 20;
int count = 0;

//画面の明るさ
bool lcdOn = false;

void setup() {
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setRotation(0);

  //MH-Z19用シリアル
  Serial1.begin(9600, SERIAL_8N1, 0, 26);

  //デバッグ用シリアル
  Serial.begin(115200);
  Serial.println("MH-Z19 test");

  //測定レンジ変更
  setDetectionDange(5000);
}

void loop() {
  M5.update();

  //Aボタンを押すと、画面の明るさをON,OFFする
  if( M5.BtnA.pressedFor(1) ){
    if( lcdOn == true )
    {
      lcdOn = false;
      M5.Axp.ScreenBreath( 0 );
    }
    else
    {
      lcdOn = true;
      M5.Axp.ScreenBreath( 15 );
    }
  }

  //Bボタンを押すと、キャリブレーション
  if( M5.BtnB.pressedFor(1000) ){
    setZeropoint();
    Serial.println(" calibrated");
    delay( 4000 );
  }

  //測定
  if( getGasConcentration( &CO2 , &temperature ) )
  {
    Serial.println("CO2[ppm]: " + String(CO2) + "\tTemperature['C]: " + String(temperature));      
  }

  //測定結果の表示
  int yLocation = 0;
  M5.Lcd.setCursor(0, yLocation, 2);
  M5.Lcd.println("CO2"); yLocation+=16;
  String str = "      " + (String)CO2;
  M5.Lcd.drawRightString(str, M5.Lcd.width(), yLocation,4); yLocation+=24;
  M5.Lcd.drawRightString("[ppm] ", M5.Lcd.width(), yLocation,2); yLocation+=16;
  
  yLocation+=8;
  M5.Lcd.setCursor(0, yLocation , 2);
  M5.Lcd.println("Temperature"); yLocation+=16;
  str = "      " + (String)(temperature);
  M5.Lcd.drawRightString(str, M5.Lcd.width(), yLocation,4); yLocation+=24;
  M5.Lcd.drawRightString("['C] ", M5.Lcd.width(), yLocation,2); yLocation+=16;

  if (count == 1800){
    postValues(CO2);
    count = 0;
  } 
  delay(1000);
  count++;
}

void postValues(int CO2) {
  // WiFi接続開始
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(WiFi.status());
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  
  WiFiClientSecure sslclient;
  String params;

  // センサデータ取得、パラメータ準備
  params = "co2=" + (String)CO2;
  Serial.println(params);

  // SSL接続開始、POSTメッセージ送信
  if (sslclient.connect(host.c_str(), 443) > 0) {
    sslclient.println("POST " + url + " HTTP/1.1");
    sslclient.println("Host: " + host);
    sslclient.println("User-Agent: ESP8266/1.0");
    sslclient.println("Connection: close");
    sslclient.println("Content-Type: application/x-www-form-urlencoded;");
    sslclient.print("Content-Length: ");
    sslclient.println(params.length());
    sslclient.println();
    sslclient.println(params);
    delay(10);
    String response = sslclient.readString();
    Serial.println(response);
    int bodypos =  response.indexOf("\r\n");
  } else {
    // HTTP client errors
    Serial.println("[HTTPS] no connection or no HTTP server.");
  }

  // 送信完了、接続終了
  WiFi.disconnect();
}

/*--- MH-Z19用 ---*/
#define MHZ19_DATA_LEN 9

//CO2,温度を取得する
bool getGasConcentration(int *CO2 , int *temperature )
{
  byte command[MHZ19_DATA_LEN] = {0xff,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
  sendCommand( command , sizeof(command) );
  
  byte response[MHZ19_DATA_LEN];
  recieveResponse( response );


  //コマンドチェック
  if( response[1] != 0x86 )
  {
    Serial.printf("Response Error '%x'\n",response[1]);
    return false; //timeOut
  }

  int CO2Temp = (int)response[2] * 256 + (int)response[3];
  int temperatureTemp = (int)response[4] -40;

  *CO2 = CO2Temp;
  *temperature = temperatureTemp;

  return true;
}

//ゼロポイントキャリブレーション
bool setZeropoint()
{
  byte command[9] = {0xff,0x01,0x87,0x00,0x00,0x00,0x00,0x00,0x78};
  sendCommand( command , sizeof(command) );

  byte response[MHZ19_DATA_LEN];
  recieveResponse( response );

  //コマンドチェック
  if( response[1] != 0x87 )
  {
    Serial.printf("Response Error '%x'\n",response[1]);
    Serial.printf("reaponse %x %x %x %x %x %x %x %x %x\n", response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7], response[8]);
    return false; //timeOut
  }

  return true;

}

//測定レンジ変更
bool setDetectionDange(int range)
{
  if( range != 2000 && range != 5000 )
  {
     Serial.printf("invalid range. Please set 2000 or 5000 \n");
     return false;
  }
  
  byte command[9] = {0xff,0x01,0x99,0x00,0x00,0x00,0x00,0x00,0x00};  
  command[3] = (byte)(range/256);
  command[4] = (byte)(range%256);
  command[8] = calcCheckSum(command);
  
  sendCommand( command , sizeof(command) );

  byte response[MHZ19_DATA_LEN];
  recieveResponse( response );

  //コマンドチェック
  if( response[1] != 0x99 )
  {
    Serial.printf("Response Error '%x'\n",response[1]);
    Serial.printf("reaponse %x %x %x %x %x %x %x %x %x\n", response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7], response[8]);
    return false; //timeOut
  }

  return true;

}


//コマンドの送信
void sendCommand(byte command[], int length)
{
  //Serial.printf("command %x %x %x %x %x %x %x %x %x\n", command[0], command[1], command[2], command[3], command[4], command[5], command[6], command[7], command[8]);
  for( int i=0 ; i<length ; i++ )
    Serial1.write(command[i]);
}

//チェックサムの計算
byte calcCheckSum( byte data[] )
{
  byte checkSum = 0;

  for (int x = 1; x<MHZ19_DATA_LEN-1; x++)
  {
    checkSum += data[x];
  }
  checkSum = 255 - checkSum;
  checkSum++;

  return checkSum;
}

//センサから応答を受信する
bool recieveResponse( byte response[] )
{
  unsigned long timeStamp = millis();

  //返答の先頭の0xFFが来るのを待つ
  while(1){
    if( Serial1.available() )
    {
      byte res;
      Serial1.readBytes(&res, 1);
      if( res == 0xff )
        break;
    }
    if (millis() - timeStamp >= 2000)
    {
      Serial.println("0xFF wait Time Out");
      return false; //timeOut
    }

    //millisがオーバーフローしたらgetDataTimerをリセット
    if( timeStamp > millis() )
      timeStamp = millis();

  }

  //8バイトたまるのを待つ
  timeStamp = millis();
  while (Serial1.available() < MHZ19_DATA_LEN-1)
  {
    if (millis() - timeStamp >= 2000)
    {
      Serial.println("Recieve Time Out");
      return false; //timeOut
    }

    //millisがオーバーフローしたらgetDataTimerをリセット
    if( timeStamp > millis() )
      timeStamp = millis();
  }

  //データの読み出し
  response[0] = 0xff;
  Serial1.readBytes(&(response[1]), MHZ19_DATA_LEN-1);
  //Serial.printf("reaponse %x %x %x %x %x %x %x %x %x\n", response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7], response[8]);

  //チェックサムチェック
  if( response[8] != calcCheckSum( response ) )
  {
    Serial.printf("Check Sum Error read'%x' correct'%x'\n", response[8], calcCheckSum(response ) );
    Serial.printf("reaponse %x %x %x %x %x %x %x %x %x\n", response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7], response[8]);
    return false;
  }
  return true;
}