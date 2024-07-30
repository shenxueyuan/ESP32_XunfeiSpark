#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include <WiFiClientSecure.h>
#include "HTTPClient.h"
#include "Audio1.h"
#include "Audio2.h"
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>AI 智能对话 后台系统</title><style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; } h1 { color: #333; } a { display: inline-block; padding: 10px 20px; margin: 10px; border: none; background-color: #333; color: white; text-decoration: none; cursor: pointer; } a:hover { background-color: #555; }</style></head><body><h1>AI-Chat Configuration</h1><a href='/wifi'>Wi-Fi Management</a><a href='/music'>Music Management</a></body></html>";
    request->send(200, "text/html", html);
  }
};

//串口通讯
#include <SoftwareSerial.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

// 生成随机数
#include <iostream>  
#include <string>  
#include <random>  
#include <chrono> 

using namespace websockets;

// 定义引脚和常量
#define key_boot 0   // boot按键引脚
// #define key_speak 23 // 外置按键引脚
#define led 2   //板载led引脚


// 定义语音模块的串行通信引脚和速率  
#define RX_PIN 4 // 假设语音模块的RX连接到ESP的4号引脚  
#define TX_PIN 5 // 假设语音模块的TX连接到ESP的5号引脚  
#define BAUD 115200 // 假设语音模块使用115200波特率  

// 初始化软件串行通信 
SoftwareSerial voiceWakeSerial(RX_PIN, TX_PIN);

// AP 模式的SSID和密码
const char *ap_ssid = "ESP32-Setup";
const char *ap_password = "12345678";
// Web服务器和Preferences对象
AsyncWebServer server(80);
Preferences preferences;
DNSServer dnsServer;

// 星火大模型的账号参数
String APPID = "e7df2284";                             // 星火大模型的App ID
String APIKey = "fbc15ca65a5bf76806a140e8b4600f71";    // API Key
String APISecret = "YzMyMDE2YWExMzkyOWU0YmQ4YjIzZmE1"; // API Secret

// 通义千问
String qwenApiKey = "sk-b60fe4859ae942beb0e5d0cd118b567e";
String qwenApiUrl = "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation";

// 百度TTS获取token
String baiduApiUrl = "https://aip.baidubce.com/oauth/2.0/token?client_id=YcYPjhxw0V7wzhcu07xnLbU5&client_secret=ANDLajJTKt4juGarl8cME1BS3aXmRMdV&grant_type=client_credentials";

// 阿里 websocket
String aliAsrURL = "ws://nls-gateway-cn-shanghai.aliyuncs.com/ws/v1?token=025554a2d70e4a389cfa317838ac24a4";

String answerHello = "嗯，收到。";
String answerHello2 = "收到。";
String accessToken = "24.e6e04657327b33f31c8927d754e54823.2592000.1723806315.282335 - 86622984";
boolean wifiFlag = true;
String deviceToken = "";


// 定义一些全局变量
bool ledstatus = true;
bool startPlay = false;
bool lastsetence = false;
bool isReady = false;
unsigned long urlTime = 0;
unsigned long pushTime = 0;
int mainStatus = 0;
int receiveFrame = 0;
int noise = 50;
int volume = 100;// 音量大小
int textLimit=100; // 超过多长 要分割，立马播放
int llmType = 2; // 1:讯飞AI 2:通义千问
HTTPClient https; // 创建一个HTTP客户端对象

int wifiStatus = 0;// 网络热点开关 1：打开 0：关闭

hw_timer_t *timer = NULL; // 定义硬件定时器对象

uint8_t adc_start_flag = 0;
uint8_t adc_complete_flag = 0;
uint32_t stt_connect_time = 0;



// 创建音频对象
Audio1 audioRecord;
Audio2 audioTTS(false, 3, I2S_NUM_1); // 参数: 是否使用SD卡, 音量, I2S端口号
// 参数: 是否使用内部DAC（数模转换器）如果设置为true，将使用ESP32的内部DAC进行音频输出。否则，将使用外部I2S设备。
// 指定启用的音频通道。可以设置为1（只启用左声道）或2（只启用右声道）或3（启用左右声道）
// 指定使用哪个I2S端口。ESP32有两个I2S端口，I2S_NUM_0和I2S_NUM_1。可以根据需要选择不同的I2S端口。

// 定义I2S引脚
#define I2S_DOUT 27 // DIN引脚
#define I2S_BCLK 26 // BCLK引脚
#define I2S_LRC 25  // LRC引脚

// 函数声明
// WIFI 相关操作
void handleRoot(AsyncWebServerRequest *request);
void handleWifiManagement(AsyncWebServerRequest *request);
void handleMusicManagement(AsyncWebServerRequest *request);
void handleSave(AsyncWebServerRequest *request);
void handleDelete(AsyncWebServerRequest *request);
void handleList(AsyncWebServerRequest *request);
void handleSaveMusic(AsyncWebServerRequest *request);
void handleDeleteMusic(AsyncWebServerRequest *request);
void handleListMusic(AsyncWebServerRequest *request);
String generateRandomString(size_t length);
void initMusic();

void initVoiceWakeSerial();
void getVoiceWakeData();
void printWakeData(int read1,int read2);

void getText(String role, String content);
void checkLen();
float calculateRMS(uint8_t *buffer, int bufferSize);
void ConnServerAI();
void ConnServerASR();
void WakeupAndStart();

void startWIfiAP(bool isOpen);

int dealCommand();
void getBaiduAccessToken();
void connecttospeech(String content);
void sendMsgToQwenAILLM(String queston);
void segmentAnswer();
void sendMsgToXunfeiAILLM();

// 创建动态JSON文档对象和数组
std::vector<String> text;

// 定义字符串变量
String url = "";
String urlASR = "";
String Date = "";
String per = "5118";

// 函数声明
DynamicJsonDocument gen_params(const char *appid, const char *domain);
void displayWrappedText(const string &text1, int x, int y, int maxWidth);

String askquestion = "";
String Answer = ""; // 用于语音合成，要分段
std::vector<String> subAnswers;
int subindex = 0;
String text_temp = "";

String welcome = "小朋友，你好啊，我是大象，快来跟我聊天吧";
String welcomeATM = "小朋友，你好啊，我是赛文奥特曼，让我们一起来拯救地球吧！";
String roleDaxiang = "你是一个人类儿童，名字叫大象，工作是陪伴儿童学习诗、词、歌、赋，并解答儿童的十万个为什么，回答问题时要引导儿童身心健康，答案缩减到50字以内;";
String roleAoteMan = "你的角色是日本动漫里的赛文奥特曼，你的职责是陪伴儿童，教导儿童勇敢、积极向上面对问题，帮助儿童日常学习、身心健康。并以奥特曼视角解答问题，并且答案缩减到50字以内;";
String roleContent = roleDaxiang;

// 星火大模型参数
const char *appId1 = "e7df2284"; // 替换为自己的星火大模型参数
const char *domain1 = "4.0Ultra";
const char *websockets_server = "ws://spark-api.xf-yun.com/v4.0/chat";
const char *websockets_server1 = "ws://iat-api.xfyun.cn/v2/iat";
using namespace websockets; // 使用WebSocket命名空间

// 创建WebSocket客户端对象
WebsocketsClient webSocketClientAI;
WebsocketsClient webSocketClientASR;

int loopcount = 0; // 循环计数器
int flag = 0;       // 用来确保subAnswer1一定是大模型回答最开始的内容
int conflag = 0;

// 移除讯飞星火回复中没用的符号
void removeChars(const char *input, char *output, const char *removeSet)
{
    int j = 0;
    for (int i = 0; input[i] != '\0'; ++i)
    {
        bool shouldRemove = false;
        for (int k = 0; removeSet[k] != '\0'; ++k)
        {
            if (input[i] == removeSet[k])
            {
                shouldRemove = true;
                break;
            }
        }
        if (!shouldRemove)
        {
            output[j++] = input[i];
        }
    }
    output[j] = '\0'; // 结束符
}

// 将回复的文本转成语音
void onMessageCallbackAI(WebsocketsMessage message)
{
    // 创建一个静态JSON文档对象，用于存储解析后的JSON数据，最大容量为4096字节
    StaticJsonDocument<4096> jsonDocument;

    // 解析收到的JSON数据
    DeserializationError error = deserializeJson(jsonDocument, message.data());

    // 如果解析没有错误
    if (!error)
    {
        // 从JSON数据中获取返回码
        int code = jsonDocument["header"]["code"];

        // 如果返回码不为0，表示出错
        if (code != 0)
        {
            // 输出错误信息和完整的JSON数据
            Serial.print("sth is wrong: ");
            Serial.println(code);
            Serial.println(message.data());

            // 关闭WebSocket客户端
            webSocketClientAI.close();
        }
        else
        {
            // 增加接收到的帧数计数器
            receiveFrame++;
            Serial.print("receiveFrame:");
            Serial.println(receiveFrame);
            // 获取JSON数据中的payload部分
            JsonObject choices = jsonDocument["payload"]["choices"];

            // 获取status状态
            int status = choices["status"];

            // 获取文本内容
            const char *content = choices["text"][0]["content"];
            const char *removeSet = "\n*$"; // 定义需要移除的符号集
            // 计算新字符串的最大长度
            int length = strlen(content) + 1;
            char *cleanedContent = new char[length];
            removeChars(content, cleanedContent, removeSet);
            Serial.println(cleanedContent);
            

            Answer += cleanedContent;

            if (Answer.length() >= textLimit && (audioTTS.isplaying == 0))
            {
                String subAnswer = Answer.substring(0, textLimit);
                Serial.print("subAnswer-line190:");
                Serial.println(subAnswer);

                int lastPeriodIndex = subAnswer.lastIndexOf("。");

                if (lastPeriodIndex != -1)
                {
                    String answer = Answer.substring(0, lastPeriodIndex + 1);
                    Serial.print("answer-line197: ");
                    Serial.println(answer);
                    Answer = Answer.substring(lastPeriodIndex + 2);
                    Serial.print("Answer-line200: ");
                    Serial.println(Answer);
                    connecttospeech(answer.c_str());
                    Serial.print("speech-line201");
                }
                else
                {
                    const char *chinesePunctuation = "[.,;!?()<>‘。、，；！？《》（）“”‘’]";

                    int lastChineseSentenceIndex = -1;

                    for (int i = 0; i < Answer.length(); ++i)
                    {
                        char currentChar = Answer.charAt(i);

                        if (strchr(chinesePunctuation, currentChar) != NULL)
                        {
                            lastChineseSentenceIndex = i;
                        }
                    }
                    if (lastChineseSentenceIndex != -1)
                    {
                        String answer = Answer.substring(0, lastChineseSentenceIndex + 1);
                        connecttospeech(answer.c_str());
                        Serial.print("speech-line224");
                        Answer = Answer.substring(lastChineseSentenceIndex + 2);
                    }
                }
                startPlay = true;
                conflag = 1;
            }

            if (status == 2)
            {
                getText("assistant", Answer);
                if (audioTTS.isplaying == 0)
                {
                    connecttospeech(Answer.c_str());
                    Serial.print("speech-line244");
                    Answer = "";
                    conflag = 1;
                }
            }
        }
    }
}

// 统一调用百度TTS
void connecttospeech(String content){
    delay(200);
    // Serial.print("speech-->conent:");Serial.println(content);
    // Serial.print("speech-->per:");Serial.println(per);
    // Serial.print("speech-->accesstoken:");Serial.println(accessToken);
    // Serial.print("speech-->deviceToken:");Serial.println(deviceToken);
    audioTTS.connecttospeech(content.c_str(), "zh",per.c_str(),accessToken.c_str(),deviceToken.c_str());
}


// 问题发送给大模型
void onEventsCallbackAI(WebsocketsEvent event, String data)
{
    // 当WebSocket连接打开时触发
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        sendMsgToXunfeiAILLM();
        // 向串口输出提示信息
        Serial.println("Send message to server-ai!");
    }
    // 当WebSocket连接关闭时触发
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        // 向串口输出提示信息
        Serial.println("Connnection-ai Closed");
    }
    // 当收到Ping消息时触发
    else if (event == WebsocketsEvent::GotPing)
    {
        // 向串口输出提示信息
        Serial.println("Got a Ping!");
    }
    // 当收到Pong消息时触发
    else if (event == WebsocketsEvent::GotPong)
    {
        // 向串口输出提示信息
        Serial.println("Got a Pong!");
    }
}

void sendMsgToXunfeiAILLM()
{
    // 生成连接参数的JSON文档
    DynamicJsonDocument jsonData = gen_params(appId1, domain1);

    // 将JSON文档序列化为字符串
    String jsonString;
    serializeJson(jsonData, jsonString);

    // 向串口输出生成的JSON字符串
    Serial.println(jsonString);

    // 通过WebSocket客户端发送JSON字符串到服务器
    webSocketClientAI.send(jsonString);
    jsonData.clear();
}


DynamicJsonDocument gen_params_qwen()
{
    // 创建一个容量为1500字节的动态JSON文档
    DynamicJsonDocument data(1500);

    data["model"] = "qwen-turbo";
    data["max_tokens"] = 100;
    data["temperature"] = 0.6;
    // data["stream"] = true;

    JsonObject input = data.createNestedObject("input");
    // 在message对象中创建一个名为text的嵌套数组
    JsonArray textArray = input.createNestedArray("messages");

    JsonObject systemMessage = textArray.createNestedObject();
    systemMessage["role"] = "system";
    systemMessage["content"] = roleContent;
    // 将jsonVector中的内容添加到JsonArray中
    for (const auto& jsonStr : text) {
        DynamicJsonDocument tempDoc(512);
        DeserializationError error = deserializeJson(tempDoc, jsonStr);
        if (!error) {
            textArray.add(tempDoc.as<JsonVariant>());
        } else {
            Serial.print("反序列化失败: ");
            Serial.println(error.c_str());
        }
    }
    // 返回构建好的JSON文档
    return data;
}

// 将问题 发送给 阿里通义千问
void sendMsgToQwenAILLM(String inputText) 
{
    HTTPClient http;
    http.setTimeout(10000);
    http.begin(qwenApiUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String(qwenApiKey));
    // 生成连接参数的JSON文档
    DynamicJsonDocument jsonData = gen_params_qwen();

    // 将JSON文档序列化为字符串
    String jsonString;
    serializeJson(jsonData, jsonString);

    // String payload = "{\"model\":\"qwen-turbo\",\"max_tokens\":100,\"input\":{\"messages\":[{\"role\": \"system\",\"content\": \""+roleContent+"\"},{\"role\": \"user\",\"content\": \"" + inputText + "\"}]}}";

    Serial.print("qwen--jsonString:");
    Serial.println(jsonString);

    int httpResponseCode = http.POST(jsonString);
    if (httpResponseCode == 200) {
        String response = http.getString();
        http.end();
    
        StaticJsonDocument<1024> jsonDoc;
        deserializeJson(jsonDoc, response);
        String outputText = jsonDoc["output"]["text"];
        Serial.print("qwen--answer:");Serial.println(outputText);
        
        Answer = outputText;
 
        getText("assistant", Answer);
        segmentAnswer();
        jsonDoc.clear();
        outputText.clear();
    } else {
        http.end();
        Serial.printf("通义千问error %i \n", httpResponseCode);
    }
}



void getBaiduAccessToken() 
{
    HTTPClient http;
    http.setTimeout(10000);
    http.begin(baiduApiUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String(qwenApiKey));
    String payload = "";
    int httpResponseCode = http.POST(payload);
    if (httpResponseCode == 200) {
        String response = http.getString();
        http.end();
        Serial.print("baidu-token-response:");
        Serial.println(response);

        // Parse JSON response
        DynamicJsonDocument jsonDoc(1024);
        deserializeJson(jsonDoc, response);
        String access_token =  jsonDoc["access_token"];
        access_token.replace("-"," - ");
        preferences.putString("accessToken",access_token);
        accessToken = access_token;
    } else {
        http.end();
        Serial.printf("百度token %i \n", httpResponseCode);
    }
}


// 将内容分割成小段
void segmentAnswer(){
    String answer = "";
    if(Answer == ""){
        return;
    }


    subAnswers.push_back(Answer.c_str());
    startPlay = true;
    
    // ↓把回答文字缩短，这里不拆了。不然 播放语音有明显的断句。影响体验。↓


    // int lastPeriodIndex = Answer.indexOf("。");

    // while(lastPeriodIndex != -1 ){
    //     answer = Answer.substring(0, lastPeriodIndex + 1);
    //     subAnswers.push_back(answer.c_str());
    //     Answer = Answer.substring(lastPeriodIndex + 2);
    //     lastPeriodIndex = Answer.indexOf("。");
    //     startPlay = true;
    // }
 
    // int secondIndex = Answer.indexOf("，");
    // while(secondIndex != -1 ){
    //     answer = Answer.substring(0, secondIndex + 1);
    //     subAnswers.push_back(answer.c_str());
    //     Answer = Answer.substring(secondIndex + 2);
    //     secondIndex = Answer.indexOf("，");
    //     startPlay = true;
    // }
 
    // answer = Answer.substring(0, Answer.length());
    // if(answer.length() >= 5){
    //     subAnswers.push_back(answer.c_str());
    //     startPlay = true;
    // }
    Answer = "";
}

// 将接收到的语音转成文本
void onMessageCallbackASR(WebsocketsMessage message)
{
    // 创建一个静态JSON文档对象，用于存储解析后的JSON数据，最大容量为4096字节
    StaticJsonDocument<2048> jsonDocument;

    // 解析收到的JSON数据
    DeserializationError error = deserializeJson(jsonDocument, message.data());

    if (error)
    {
        // 如果解析出错，输出错误信息和收到的消息数据
        Serial.println("error:");
        Serial.println(error.c_str());
        Serial.println(message.data());
        return;
    }
    // 如果解析没有错误

    // 从JSON数据中获取返回码
    int code = jsonDocument["code"];
    // 如果返回码不为0，表示出错
    if (code != 0)
    {
        // 输出错误码和完整的JSON数据
        Serial.println(code);
        Serial.println(message.data());

        // 关闭WebSocket客户端
        webSocketClientASR.close();
    }
    else
    {
        // 输出收到的讯飞云返回消息
        Serial.println("xunfeiyun return message:");
        Serial.println(message.data());
        receiveFrame = 0;

        // if(llmType==1){
        //     connecttospeech(answerHello.c_str());
        // }else{
        //     connecttospeech(answerHello2.c_str());
        // }

        // 获取JSON数据中的结果部分，并提取文本内容
        JsonArray ws = jsonDocument["data"]["result"]["ws"].as<JsonArray>();
        if (jsonDocument["data"]["status"] != 2)
        {
            askquestion = "";
        }

        for (JsonVariant i : ws)
        {
            for (JsonVariant w : i["cw"].as<JsonArray>())
            {
                askquestion += w["w"].as<String>();
            }
        }

        // 输出提取的问句
        Serial.println(askquestion);

        // 获取状态码
        if (jsonDocument["data"]["status"] == 2)
        {
            // 如果状态码为2，表示消息处理完成
            Serial.println("status == 2");
            webSocketClientASR.close();

            // 如果问句为空，播放错误提示语音
            if (askquestion == "")
            {
                askquestion = "我先下去了，有需要再叫唤醒我吧。";
                connecttospeech(askquestion.c_str());
            }
            else
            {
                // 处理一般的问答请求                
                Answer = "";
                lastsetence = false;
                isReady = true;

                // 是否命中任务 0:未命中 1:命中
                int commandFlag = dealCommand();
                if(commandFlag == 0 ){
                    if (llmType == 1){
                        getText("user", askquestion);
                        // 发送给讯飞大模型
                        ConnServerAI();
                    }else if(llmType ==2){
                        // 发送给 通义千问大模型
                        getText("user", askquestion);
                        sendMsgToQwenAILLM(askquestion);
                    }
                }
            }
        }
    }
}

int dealCommand(){
    int flag = 1;//默认命中任务

    if ((askquestion.indexOf("开") > -1 
                || (askquestion.indexOf("切") > -1 ) || (askquestion.indexOf("换") > -1 )
            ) 
            && (askquestion.indexOf("讯飞") > -1))
    {
        askquestion = "已切换为讯飞星火大模型";
        llmType = 1;
        connecttospeech(askquestion.c_str());
        // 打印内容
        askquestion = "";
        conflag = 1;
    }
    else if ((askquestion.indexOf("开") > -1 
                || (askquestion.indexOf("切") > -1 ) || (askquestion.indexOf("换") > -1 )
            ) 
            && (askquestion.indexOf("千问") > -1 || askquestion.indexOf("阿里") > -1))
    {
        askquestion = "已切换为阿里通义千问大模型";
        llmType = 2;
        connecttospeech(askquestion.c_str());
        // 打印内容
        askquestion = "";
        conflag = 1;
    }
    else if ((askquestion.indexOf("切") > -1 || (askquestion.indexOf("换") > -1 )) && askquestion.indexOf("奥特曼") > -1)
    {
        askquestion = welcomeATM;
        roleContent = roleAoteMan;
        per = "5003";
        preferences.putString("per", per);
        connecttospeech(askquestion.c_str());
        // 打印内容
        askquestion = "";
        conflag = 1;
    }
    else if ((askquestion.indexOf("切") > -1 || (askquestion.indexOf("换") > -1 )) && (askquestion.indexOf("普通") > -1 || askquestion.indexOf("大象") > -1))
    {
        askquestion = welcome;
        roleContent = roleDaxiang;
        per = "5118";
        preferences.putString("per", per);
        connecttospeech(askquestion.c_str());
        // 打印内容
        askquestion = "";
        conflag = 1;
    }
    else if (askquestion.indexOf("恢复") > -1 || askquestion.indexOf("出厂设置") > -1)
    {
        askquestion = "好的，系统已恢复出厂设置。";
        
        roleContent = roleAoteMan;
        per = "5118";
        preferences.putString("per", per);
        llmType = 2;
        getBaiduAccessToken();
        connecttospeech(askquestion.c_str());
        askquestion = "";
        conflag = 1;
    }
    else if (((askquestion.indexOf("听") > -1 || askquestion.indexOf("放") > -1) && (askquestion.indexOf("歌") > -1 || askquestion.indexOf("音乐") > -1)) || mainStatus == 1)
    {
        String musicName = "";
        String musicID = "";

        preferences.begin("music_store", true);

        // 查找音乐名称对应的ID
        int numMusic = preferences.getInt("numMusic", 0);
        for (int i = 0; i < numMusic; ++i)
        {
            musicName = preferences.getString(("musicName" + String(i)).c_str(), "");
            musicID = preferences.getString(("musicId" + String(i)).c_str(), "");
            Serial.println("音乐名称: " + musicName);
            Serial.println("音乐ID: " + musicID);
            if (askquestion.indexOf(musicName.c_str()) > -1)
            {
                Serial.println("找到了！");
                break;
            }
            else
            {
                musicID = "";
            }
        }

        // 输出结果
        if (musicID == "") {
            mainStatus = 1;
            Serial.println("未找到对应的音乐");
            lastsetence = false;
            isReady = true;
            // todo 提问 大模型
            if(llmType == 1){
                getText("user", askquestion);
                ConnServerAI();
            }else{
                sendMsgToQwenAILLM(askquestion);
            }
        } else {
            // 自建音乐服务器，按照文件名查找对应歌曲
            mainStatus = 0;
            String audioStreamURL = "https://music.163.com/song/media/outer/url?id=" + musicID + ".mp3";
            Serial.println(audioStreamURL.c_str());
            audioTTS.connecttohost(audioStreamURL.c_str());
            delay(2000);

            askquestion = "正在播放音乐：" + musicName;
            Serial.println(askquestion);
            Serial.println("音乐名称: " + musicName);
            Serial.println("音乐ID: " + musicID);
            askquestion = "";
            // 设置播放开始标志
            startPlay = true;
            flag = 1;
            Answer = "音乐播放完了，主人还想听什么音乐吗？";
            conflag = 1;
        }
        preferences.end();
    }
    
    else{
        flag = 0;// 未命中任务
    }
    return flag;
}

// 录音
void onEventsCallbackASR(WebsocketsEvent event, String data)
{
    // 当WebSocket连接打开时触发
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        // 向串口输出提示信息
        Serial.println("Send message to xunfeiyun");
        uint32_t current_time = esp_timer_get_time();
        Serial.print("Time since STT connection: ");
        Serial.println((current_time - stt_connect_time) / 1000);

        // 初始化变量
        int silence = 0;
        int firstframe = 1;
        int j = 0;
        int voicebegin = 0;
        int voice = 0;
        int null_voice = 0;

        // 创建一个JSON文档对象
        StaticJsonDocument<2000> doc;

        // 无限循环，用于录制和发送音频数据
        while (1)
        {
            // 清空JSON文档
            doc.clear();

            // 创建data对象
            JsonObject data = doc.createNestedObject("data");

            // 录制音频数据
            audioRecord.Record();

            // 计算音频数据的RMS值
            float rms = calculateRMS((uint8_t *)audioRecord.wavData[0], 1280);
            printf("%d %f\n", 0, rms);

            // 一直没说话，8秒则结束
            if(null_voice >= 200)
            {
                connecttospeech("我先下去了，有需要再叫我吧。");
                webSocketClientASR.close();
                return;
            }

            // 判断是否为噪音
            if (rms < noise)
            {
                null_voice ++;
                if (voicebegin == 1)
                {
                    silence++;
                }
            }
            else
            {
                voice++;
                if (voice >= 5)
                {
                    voicebegin = 1;
                }
                else
                {
                    voicebegin = 0;
                }
                silence = 0;
            }

            // 如果静音达到10个周期，发送结束标志的音频数据
            if (silence == 10)
            {
                data["status"] = 2;
                data["format"] = "audio/L16;rate=16000";
                data["audio"] = base64::encode((byte *)audioRecord.wavData[0], 1280);
                data["encoding"] = "raw";
                j++;

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClientASR.send(jsonString);
                delay(40);
                break;
            }

            // 处理第一帧音频数据
            if (firstframe == 1)
            {
                data["status"] = 0;
                data["format"] = "audio/L16;rate=16000";
                data["audio"] = base64::encode((byte *)audioRecord.wavData[0], 1280);
                data["encoding"] = "raw";
                j++;

                JsonObject common = doc.createNestedObject("common");
                common["app_id"] = appId1;

                JsonObject business = doc.createNestedObject("business");
                business["domain"] = "iat";
                business["language"] = "zh_cn";
                business["accent"] = "mandarin";
                // // 不使用动态修正
                business["vinfo"] = 1;
                // 使用动态修正
                // business["dwa"] = "wpgs";
                business["vad_eos"] = 1000;

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClientASR.send(jsonString);
                firstframe = 0;
                delay(40);
            }
            else
            {
                // 处理后续帧音频数据
                data["status"] = 1;
                data["format"] = "audio/L16;rate=16000";
                data["audio"] = base64::encode((byte *)audioRecord.wavData[0], 1280);
                data["encoding"] = "raw";

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClientASR.send(jsonString);
                delay(40);
            }
        }
    }
    // 当WebSocket连接关闭时触发
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        // 向串口输出提示信息
        Serial.println("Connnection-ASR Closed");
    }
    // 当收到Ping消息时触发
    else if (event == WebsocketsEvent::GotPing)
    {
        // 向串口输出提示信息
        Serial.println("Got a Ping!");
    }
    // 当收到Pong消息时触发
    else if (event == WebsocketsEvent::GotPong)
    {
        // 向串口输出提示信息
        Serial.println("Got a Pong!");
    }
}

void ConnServerAI()
{
    // 向串口输出WebSocket服务器的URL
    Serial.println("url:" + url);

    // 设置WebSocket客户端的消息回调函数
    webSocketClientAI.onMessage(onMessageCallbackAI);

    // 设置WebSocket客户端的事件回调函数
    webSocketClientAI.onEvent(onEventsCallbackAI);

    // 开始连接WebSocket服务器
    Serial.println("Begin connect to server-ai......");

    // 尝试连接到WebSocket服务器
    if (webSocketClientAI.connect(url.c_str()))
    {
        // 如果连接成功，输出成功
        Serial.println("Connected to server-ai!");
    }
    else
    {
        // 如果连接失败，输出失败信息
        Serial.println("Failed to connect to server-ai!");
    }
    
}
 
void ConnServerASR()
{
    stt_connect_time = esp_timer_get_time();
    Serial.print("Time since STT connectio-startn: ");
    Serial.println(stt_connect_time);

    // Serial.println("urlASR:" + urlASR);
    webSocketClientASR.onMessage(onMessageCallbackASR);
    webSocketClientASR.onEvent(onEventsCallbackASR);
    // Connect to WebSocket
    Serial.println("Begin connect to server-asr......");
    if (webSocketClientASR.connect(urlASR.c_str()))
    {
        Serial.println("Connected to server-asr!");
    }
    else
    {
        Serial.println("Failed to connect to server-asr!");
    }
}

void voicePlay()
{
    // 检查音频是否正在播放以及回答内容是否为空
    if ((audioTTS.isplaying == 0) && (Answer != "" || subindex <= subAnswers.size()))
    {
        if (subindex < subAnswers.size())
        {
            connecttospeech(subAnswers[subindex].c_str());
            subindex++;
            conflag = 1;
            // 设置开始播放标志
            startPlay = true;
        }
        else
        {
            connecttospeech(Answer.c_str());
            Answer = "";
            startPlay = false;
            conflag = 1;
        }
    }
    else
    {
        // 如果音频正在播放或回答内容为空，不做任何操作
    }
}

const char *wifiData[][2] = {
    {"LEHOO", "Lehoo1688"}, // 替换为自己常用的wifi名和密码
    // 继续添加需要的 Wi-Fi 名称和密码
};

int wifiConnect()
{
    // 断开当前WiFi连接
    WiFi.disconnect(true);

    preferences.begin("wifi_store");

    int numNetworks = preferences.getInt("numNetworks", 0);


    Serial.print("Wifi count: ");
    Serial.println(numNetworks);

    if(numNetworks == 0){
       preferences.end(); 
    }

    // 获取存储的 WiFi 配置
    for (int i = 0; i < numNetworks; ++i)
    {
        String ssid = preferences.getString(("ssid" + String(i)).c_str(), "");
        String password = preferences.getString(("password" + String(i)).c_str(), "");
        
        // 尝试连接到存储的 WiFi 网络
        if (ssid.length() > 0 && password.length() > 0)
        {
            Serial.print("Connecting to ");
            Serial.println(ssid);
            Serial.print("password:");
            Serial.println(password);
            
            uint8_t count = 0;
            WiFi.begin(ssid.c_str(), password.c_str());
            // 等待WiFi连接成功
            while (WiFi.status() != WL_CONNECTED)
            {
                // 闪烁板载LED以指示连接状态
                digitalWrite(led, ledstatus);
                ledstatus = !ledstatus;
                count++;

                // 如果尝试连接超过30次，则认为连接失败
                if (count >= 30)
                {
                    Serial.printf("\r\n-- wifi connect fail! --\r\n");
                    
                    break;
                }

                // 等待100毫秒
                vTaskDelay(100);
            }

            if (WiFi.status() == WL_CONNECTED)
            {
                // 向串口输出连接成功信息和IP地址
                Serial.printf("\r\n-- wifi connect success! --\r\n");
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());
                
                // 网络连接成功，关闭AP网络
                startWIfiAP(false);
                // 输出当前空闲堆内存大小
                Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
                
                preferences.end(); 
                return 1;
            }
        }
    }
    
    return 0;
}
 

// https://www.xfyun.cn/doc/spark/general_url_authentication.html#_1-2-%E9%89%B4%E6%9D%83%E5%8F%82%E6%95%B0
String getXunFeiUrl(String Spark_url, String host, String path, String Date)
{
    // 拼接签名原始字符串
    String signature_origin = "host: " + host + "\n";
    signature_origin += "date: " + Date + "\n";
    signature_origin += "GET " + path + " HTTP/1.1";
    // 示例：signature_origin="host: spark-api.xf-yun.com\ndate: Mon, 04 Mar 2024 19:23:20 GMT\nGET /v4.0/chat HTTP/1.1";

    // 使用 HMAC-SHA256 进行加密
    unsigned char hmac[32];                                 // 存储HMAC结果
    mbedtls_md_context_t ctx;                               // HMAC上下文
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;          // 使用SHA256哈希算法
    const size_t messageLength = signature_origin.length(); // 签名原始字符串的长度
    const size_t keyLength = APISecret.length();            // 密钥的长度

    // 初始化HMAC上下文
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    // 设置HMAC密钥
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)APISecret.c_str(), keyLength);
    // 更新HMAC上下文
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)signature_origin.c_str(), messageLength);
    // 完成HMAC计算
    mbedtls_md_hmac_finish(&ctx, hmac);
    // 释放HMAC上下文
    mbedtls_md_free(&ctx);

    // 将HMAC结果进行Base64编码
    String signature_sha_base64 = base64::encode(hmac, sizeof(hmac) / sizeof(hmac[0]));

    // 替换Date字符串中的特殊字符
    Date.replace(",", "%2C");
    Date.replace(" ", "+");
    Date.replace(":", "%3A");

    // 构建Authorization原始字符串
    String authorization_origin = "api_key=\"" + APIKey + "\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"" + signature_sha_base64 + "\"";

    // 将Authorization原始字符串进行Base64编码
    String authorization = base64::encode(authorization_origin);

    // 构建最终的URL
    String url = Spark_url + '?' + "authorization=" + authorization + "&date=" + Date + "&host=" + host;

    // 向串口输出生成的URL
    Serial.println(url);

    // 返回生成的URL
    return url;
}

// 更新WebSocket连接的URL
void getTimeFromServer()
{
    // 定义用于获取时间的URL
    String timeurl = "https://www.baidu.com";

    // 创建HTTPClient对象
    HTTPClient http;

    // 初始化HTTP连接
    http.begin(timeurl);

    // 定义需要收集的HTTP头字段
    const char *headerKeys[] = {"Date"};

    // 设置要收集的HTTP头字段
    http.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));

    // 发送HTTP GET请求
    int httpCode = http.GET();

    // 从HTTP响应头中获取Date字段
    Date = http.header("Date");

    // 输出获取到的Date字段到串口
    Serial.println(Date);

    // String subDate = Date.substring(Date.indexOf("202"),Date.lastIndexOf(" "));
    // Serial.println(subDate);

    // 结束HTTP连接
    http.end();

    // 更新WebSocket连接的URL
    setXunFeiUrl();
    
    // 根据实际情况可以添加延时，以便避免频繁请求
    // delay(50); // 可以根据实际情况调整延时时间
}

void addWifi(){
    int numNetworks = preferences.getInt("numNetworks", 0);
    if(numNetworks < 1){
        preferences.putString("ssid0", "LEHOO");
        preferences.putString("password0", "Lehoo1688");
        preferences.putInt("numNetworks", 1);
    }
}


void setup()
{
    // 初始化串口通信，波特率为115200
    Serial.begin(115200);

    // 初始化离线语音唤醒
    initVoiceWakeSerial();

    // 配置引脚模式
    // 配置按键引脚为上拉输入模式，用于boot按键检测
    pinMode(key_boot, INPUT_PULLUP);
    // 外置按钮检测
    // pinMode(key_speak, INPUT_PULLUP);
    // 将GPIO2设置为输出模式
    pinMode(led, OUTPUT);

    // 初始化音频模块audioRecord
    audioRecord.init();

    int result = wifiConnect();

    // 初始化 Preferences
    preferences.begin("wifi-config");

    per = preferences.getString("per","5118");
    
    accessToken = preferences.getString("accessToken");

    deviceToken = preferences.getString("deviceToken","");

    if(deviceToken == ""){
        deviceToken = generateRandomString(32);
        preferences.putString("deviceToken",deviceToken);
    }

    Serial.print("deviceToken:");
    Serial.println(deviceToken);

    if(accessToken == ""){
        getBaiduAccessToken();
    }
    // Serial.print("accessToken:");
    // Serial.println(accessToken);

    preferences.end();

    // addWifi();
    // 如果网络未连接，打开AP网络
    if(result !=1){
        startWIfiAP(true);
    }

    // 从服务器获取当前时间
    getTimeFromServer();

    volume = preferences.getInt("volume", 100);
    setVolume();

    // 记录当前时间，用于后续时间戳比较
    urlTime = millis();

    // 初始化音乐
    initMusic();
    delay(2000);

}

void pauseVoice(){
    conflag = 0;
    // 停止播放音频
    audioTTS.isplaying = 0;
    audioTTS.stopSong();
    startPlay = false;
    isReady = false;
    Answer = "";
    flag = 0;
    subindex = 0;
    subAnswers.clear();
    webSocketClientASR.close();
}

void setVolume2(int vol){
    volume = vol;
    
    setVolume();
    
}

// 调整音量
void setVolume()
{
    if(volume > 100){
        volume = 100;
    }
    if(volume < 30){
        volume = 30;
    }
    preferences.begin("volume-config");
    // 设置音频输出引脚和音量
    audioTTS.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audioTTS.setVolume(volume);
    preferences.putInt("volume", volume);
    Serial.print("保存音量-当前音量：");
    Serial.println(volume);
    preferences.end();
}
// 开启 AP 网络
void startWIfiAP(bool isOpen)
{
    if(isOpen){
        wifiStatus = 1;// 打开网络
        // 启动 AP 模式创建热点
        WiFi.softAP(ap_ssid, ap_password);
        Serial.println("Started Access Point");
        // 启动 Web 服务器
        server.on("/", HTTP_GET, handleRoot);
        server.on("/wifi", HTTP_GET, handleWifiManagement);
        server.on("/music", HTTP_GET, handleMusicManagement);
        server.on("/save", HTTP_POST, handleSave);
        server.on("/delete", HTTP_POST, handleDelete);
        server.on("/list", HTTP_GET, handleList);
        server.on("/saveMusic", HTTP_POST, handleSaveMusic);
        server.on("/deleteMusic", HTTP_POST, handleDeleteMusic);
        server.on("/listMusic", HTTP_GET, handleListMusic);

        dnsServer.start(53, "*", WiFi.softAPIP());
        server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP
        server.begin();
        Serial.println("WebServer started, waiting for configuration...");
    }else{
        wifiStatus = 0;// 关闭网络
        WiFi.softAPdisconnect(true);
        Serial.println("WebServer closed...");
    }
}

void loop()
{
    // 轮询处理WebSocket客户端消息
    webSocketClientAI.poll();
    webSocketClientASR.poll();
 
    // 如果开始播放语音
    if (startPlay)
    {
        // 调用voicePlay函数播放语音
        voicePlay();
    }

    // 音频处理循环
    audioTTS.loop();

    // 如果音频正在播放
    if (audioTTS.isplaying == 1)
    {
        // 点亮板载LED指示灯
        digitalWrite(led, HIGH);
    }
    else
    {
        // 熄灭板载LED指示灯
        digitalWrite(led, LOW);
    }


    // boot检测按键是否按下
    if (digitalRead(key_boot) == LOW)
    {
        WakeupAndStart();
    }
    // 添加连续对话功能
    if (audioTTS.isplaying == 0 && Answer == "" && subindex == subAnswers.size() && conflag == 1)
    {
        Serial.println("开启连续对话...");
        WakeupAndStart();
    }

    if(wifiStatus == 1){
        dnsServer.processNextRequest();   
    }
    // 循环读取串口信息
    getVoiceWakeData();
}
/**
 * ASR唤醒 并 开始录音
 */
void WakeupAndStart()
{

    webSocketClientASR.close();
    // 点亮板载LED指示灯
    digitalWrite(led, HIGH);
    delay(200);
    digitalWrite(led, LOW);

    conflag = 0;
    Serial.print("loopcount：");
    Serial.println(loopcount);

    loopcount++;
    // 停止播放音频
    audioTTS.isplaying = 0;
    audioTTS.pauseResume();
    startPlay = false;
    isReady = false;
    Answer = "";
    flag = 0;
    subindex = 0;
    subAnswers.clear();
    Serial.printf("Start recognition\r\n\r\n");

    adc_start_flag = 1;

    // 如果距离上次时间同步超过4分钟 // 超过4分钟，重新做一次鉴权
    if (urlTime + 240000 < millis()) {
        // 更新时间戳
        urlTime = millis();
        // 从服务器获取当前时间
        getTimeFromServer();
    }
    askquestion = "";

    // 连接到WebSocket服务器-语音识别
    ConnServerASR();
    
    adc_complete_flag = 0;

}

// 设置websocket API地址 ，用于鉴权鉴权
void setXunFeiUrl()
{
    url = getXunFeiUrl("ws://spark-api.xf-yun.com/v4.0/chat", "spark-api.xf-yun.com", "/v4.0/chat", Date);
    urlASR = getXunFeiUrl("ws://iat-api.xfyun.cn/v2/iat", "iat-api.xfyun.cn", "/v2/iat", Date);
}

// 显示文本
void getText(String role, String content)
{
     // 检查并调整文本长度
    checkLen();

    // 创建一个静态JSON文档，容量为512字节
    StaticJsonDocument<512> jsoncon;

    // 设置JSON文档中的角色和内容
    jsoncon["role"] = role;
    jsoncon["content"] = content;
    Serial.print("jsoncon：");
    Serial.println(jsoncon.as<String>());

    // 将JSON文档序列化为字符串
    String jsonString;
    serializeJson(jsoncon, jsonString);

    // 将字符串存储到vector中
    text.push_back(jsonString);

    // 输出vector中的内容
    for (const auto& jsonStr : text) {
        Serial.println(jsonStr);
    }

    // 清空临时JSON文档
    jsoncon.clear();
}

// 实时清理较早的历史对话记录
void checkLen()
{
    size_t totalBytes = 0;

    // 计算vector中每个字符串的长度
    for (const auto& jsonStr : text) {
        totalBytes += jsonStr.length();
    }
    Serial.print("text size:");
    Serial.println(totalBytes);
    if (totalBytes > 800)
    {
        text.erase(text.begin(), text.begin() + 2);
    }
}

DynamicJsonDocument gen_params(const char *appid, const char *domain)
{
    // 创建一个容量为2048字节的动态JSON文档
    DynamicJsonDocument data(1800);

    // 创建一个名为header的嵌套JSON对象，并添加app_id和uid字段
    JsonObject header = data.createNestedObject("header");
    header["app_id"] = appid;
    header["uid"] = deviceToken;

    // 创建一个名为parameter的嵌套JSON对象
    JsonObject parameter = data.createNestedObject("parameter");

    // 在parameter对象中创建一个名为chat的嵌套对象，并添加domain, temperature和max_tokens字段
    JsonObject chat = parameter.createNestedObject("chat");
    chat["domain"] = domain;
    chat["temperature"] = 0.6;
    chat["max_tokens"] = 100;

    // 创建一个名为payload的嵌套JSON对象
    JsonObject payload = data.createNestedObject("payload");

    // 在payload对象中创建一个名为message的嵌套对象
    JsonObject message = payload.createNestedObject("message");

    // 在message对象中创建一个名为text的嵌套数组
    JsonArray textArray = message.createNestedArray("text");

    JsonObject systemMessage = textArray.createNestedObject();
    systemMessage["role"] = "system";
    systemMessage["content"] = roleContent;

    // 将jsonVector中的内容添加到JsonArray中
    for (const auto& jsonStr : text) {
        DynamicJsonDocument tempDoc(512);
        DeserializationError error = deserializeJson(tempDoc, jsonStr);
        if (!error) {
            textArray.add(tempDoc.as<JsonVariant>());
        } else {
            Serial.print("反序列化失败: ");
            Serial.println(error.c_str());
        }
    }

    // 返回构建好的JSON文档
    return data;
}

float calculateRMS(uint8_t *buffer, int bufferSize)
{
    float sum = 0;  // 初始化总和变量
    int16_t sample; // 定义16位整数类型的样本变量

    // 遍历音频数据缓冲区，每次处理两个字节（16位）
    for (int i = 0; i < bufferSize; i += 2)
    {
        // 将两个字节组合成一个16位的样本值
        sample = (buffer[i + 1] << 8) | buffer[i];

        // 将样本值平方后累加到总和中
        sum += sample * sample;
    }

    // 计算平均值（样本总数为bufferSize / 2）
    sum /= (bufferSize / 2);

    // 返回总和的平方根，即RMS值
    return sqrt(sum);
}

// 处理根路径的请求
void handleRoot(AsyncWebServerRequest *request)
{
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>AI 智能对话 后台系统</title><style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; } h1 { color: #333; } a { display: inline-block; padding: 10px 20px; margin: 10px; border: none; background-color: #333; color: white; text-decoration: none; cursor: pointer; } a:hover { background-color: #555; }</style></head><body><h1>AI-Chat Configuration</h1><a href='/wifi'>Wi-Fi Management</a><a href='/music'>Music Management</a></body></html>";
    request->send(200, "text/html", html);
}

void handleWifiManagement(AsyncWebServerRequest *request)
{
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Wi-Fi Management</title><style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; } h1 { color: #333; } form { display: inline-block; margin-top: 20px; } input[type='text'], input[type='password'] { padding: 10px; margin: 10px 0; width: 200px; } input[type='submit'], input[type='button'] { padding: 10px 20px; margin: 10px 5px; border: none; background-color: #333; color: white; cursor: pointer; } input[type='submit']:hover, input[type='button']:hover { background-color: #555; }</style></head><body><h1>Wi-Fi Management</h1><form action='/save' method='post'><label for='ssid'>Wi-Fi SSID:</label><br><input type='text' id='ssid' name='ssid'><br><label for='password'>Password:</label><br><input type='password' id='password' name='password'><br><input type='submit' value='Save'></form><form action='/delete' method='post'><label for='ssid'>Wi-Fi SSID to Delete:</label><br><input type='text' id='ssid' name='ssid'><br><input type='submit' value='Delete'></form><a href='/list'><input type='button' value='List Wi-Fi Networks'></a><p><a href='/'>Go Back</a></p></body></html>";
    request->send(200, "text/html", html);
}

void handleMusicManagement(AsyncWebServerRequest *request)
{
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Music Management</title><style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; } h1 { color: #333; } form { display: inline-block; margin-top: 20px; } input[type='text'], input[type='password'] { padding: 10px; margin: 10px 0; width: 200px; } input[type='submit'], input[type='button'] { padding: 10px 20px; margin: 10px 5px; border: none; background-color: #333; color: white; cursor: pointer; } input[type='submit']:hover, input[type='button']:hover { background-color: #555; }</style></head><body><h1>Music Management</h1><form action='/saveMusic' method='post'><label for='musicName'>Music Name:</label><br><input type='text' id='musicName' name='musicName'><br><label for='musicId'>Music ID:</label><br><input type='text' id='musicId' name='musicId'><br><input type='submit' value='Save Music'></form><form action='/deleteMusic' method='post'><label for='musicName'>Music Name to Delete:</label><br><input type='text' id='musicName' name='musicName'><br><input type='submit' value='Delete Music'></form><a href='/listMusic'><input type='button' value='List Saved Music'></a><p><a href='/'>Go Back</a></p></body></html>";
    request->send(200, "text/html", html);
}

void handleSave(AsyncWebServerRequest *request)
{
    Serial.println("Start Save!");
    String ssid = request->arg("ssid");
    String password = request->arg("password");

    preferences.begin("wifi_store", false);
    int numNetworks = preferences.getInt("numNetworks", 0);

    for (int i = 0; i < numNetworks; ++i)
    {
        String storedSsid = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (storedSsid == ssid)
        {
            preferences.putString(("password" + String(i)).c_str(), password);
            Serial.println("Succeess Update!");
            request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Configuration Updated!</h1><p>Network password updated successfully.</p><p><a href='/'>Go Back</a></p></body></html>");
            preferences.end();
            return;
        }
    }

    preferences.putString(("ssid" + String(numNetworks)).c_str(), ssid);
    preferences.putString(("password" + String(numNetworks)).c_str(), password);
    preferences.putInt("numNetworks", numNetworks + 1);
    Serial.println("Succeess Save!");

    request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Configuration Saved!</h1><p>Network information added successfully.</p><p><a href='/'>Go Back</a></p></body></html>");
    preferences.end();
}

void handleDelete(AsyncWebServerRequest *request)
{
    Serial.println("Start Delete!");
    String ssidToDelete = request->arg("ssid");

    preferences.begin("wifi_store", false);
    int numNetworks = preferences.getInt("numNetworks", 0);

    for (int i = 0; i < numNetworks; ++i)
    {
        String storedSsid = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (storedSsid == ssidToDelete)
        {
            preferences.remove(("ssid" + String(i)).c_str());
            preferences.remove(("password" + String(i)).c_str());

            for (int j = i; j < numNetworks - 1; ++j)
            {
                String nextSsid = preferences.getString(("ssid" + String(j + 1)).c_str(), "");
                String nextPassword = preferences.getString(("password" + String(j + 1)).c_str(), "");

                preferences.putString(("ssid" + String(j)).c_str(), nextSsid);
                preferences.putString(("password" + String(j)).c_str(), nextPassword);
            }

            preferences.remove(("ssid" + String(numNetworks - 1)).c_str());
            preferences.remove(("password" + String(numNetworks - 1)).c_str());
            preferences.putInt("numNetworks", numNetworks - 1);
            // u8g2.setCursor(0, 11 + 12);
            // u8g2.print("wifi删除成功！");
            Serial.println("Succeess Delete!");

            request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Network Deleted!</h1><p>The network has been deleted.</p><p><a href='/'>Go Back</a></p></body></html>");
            preferences.end();
            return;
        }
    }
    Serial.println("Fail to Delete!");

    request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Network Not Found!</h1><p>The specified network was not found.</p><p><a href='/'>Go Back</a></p></body></html>");
    preferences.end();
}

void handleList(AsyncWebServerRequest *request)
{
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Saved Wi-Fi Networks</h1><ul>";

    preferences.begin("wifi_store", true);
    int numNetworks = preferences.getInt("numNetworks", 0);

    for (int i = 0; i < numNetworks; ++i)
    {
        String ssid = preferences.getString(("ssid" + String(i)).c_str(), "");
        String password = preferences.getString(("password" + String(i)).c_str(), "");
        html += "<li>ssid" + String(i) + ": " + ssid + " " + password + "</li>";
    }

    html += "</ul><p><a href='/'>Go Back</a></p></body></html>";

    request->send(200, "text/html", html);
    preferences.end();
}

void initMusic(){
    // 小星星 1451396976
    // 拔萝卜 28700292
    // 摇篮曲 1417892410
    // 莫扎特 419485659
    // 小兔子乖乖 566443169
    // 两只老虎 566443167
    // 世上只有妈妈好 566443174
    // 小白兔 1451390672
    // 生日快乐歌 2082151352
    // 丢手绢 1451401289
    // 小燕子 1451401297

    preferences.begin("music_store", false);

    int numMusic = preferences.getInt("numMusic", 0);
    Serial.print("音乐数量》：");
    Serial.println(numMusic);
    if(numMusic == 0){
        numMusic = 11;

        preferences.putString(("musicName" + String(0)).c_str(), "小星星");
        preferences.putString(("musicId" + String(0)).c_str(), "1451396976");

        preferences.putString(("musicName" + String(1)).c_str(), "拔萝卜");
        preferences.putString(("musicId" + String(1)).c_str(), "28700292");
        
        preferences.putString(("musicName" + String(2)).c_str(), "摇篮曲");
        preferences.putString(("musicId" + String(2)).c_str(), "1417892410");
        
        preferences.putString(("musicName" + String(3)).c_str(), "莫扎特");
        preferences.putString(("musicId" + String(3)).c_str(), "419485659");
        
        preferences.putString(("musicName" + String(4)).c_str(), "小兔子乖乖");
        preferences.putString(("musicId" + String(4)).c_str(), "566443169");
        
        preferences.putString(("musicName" + String(5)).c_str(), "两只老虎");
        preferences.putString(("musicId" + String(5)).c_str(), "566443167");
        
        preferences.putString(("musicName" + String(6)).c_str(), "世上只有妈妈好");
        preferences.putString(("musicId" + String(6)).c_str(), "566443174");
        
        preferences.putString(("musicName" + String(7)).c_str(), "小白兔");
        preferences.putString(("musicId" + String(7)).c_str(), "1451390672");
        
        preferences.putString(("musicName" + String(8)).c_str(), "生日快乐歌");
        preferences.putString(("musicId" + String(8)).c_str(), "2082151352");
        
        preferences.putString(("musicName" + String(9)).c_str(), "丢手绢");
        preferences.putString(("musicId" + String(9)).c_str(), "1451401289");
        
        preferences.putString(("musicName" + String(10)).c_str(), "小燕子");
        preferences.putString(("musicId" + String(10)).c_str(), "1451401297");


        preferences.putInt("numMusic", 11);
    }
        preferences.end();
}

void handleSaveMusic(AsyncWebServerRequest *request)
{
    Serial.println("Start Save Music!");
    String musicName = request->arg("musicName");
    String musicId = request->arg("musicId");

    preferences.begin("music_store", false);
    int numMusic = preferences.getInt("numMusic", 0);

    for (int i = 0; i < numMusic; ++i)
    {
        String storedMusicName = preferences.getString(("musicName" + String(i)).c_str(), "");
        if (storedMusicName == musicName)
        {
            preferences.putString(("musicId" + String(i)).c_str(), musicId);
            Serial.println("Success Update Music!");
            request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Music Configuration</title></head><body><h1>Music ID Updated!</h1><p>Music ID updated successfully.</p><p><a href='/'>Go Back</a></p></body></html>");
            preferences.end();
            return;
        }
    }

    preferences.putString(("musicName" + String(numMusic)).c_str(), musicName);
    preferences.putString(("musicId" + String(numMusic)).c_str(), musicId);
    preferences.putInt("numMusic", numMusic + 1);
    Serial.println("Success Save Music!");

    request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Music Configuration</title></head><body><h1>Music Saved!</h1><p>Music information added successfully.</p><p><a href='/'>Go Back</a></p></body></html>");
    preferences.end();
}

void handleDeleteMusic(AsyncWebServerRequest *request)
{
    Serial.println("Start Delete Music!");
    String musicNameToDelete = request->arg("musicName");

    preferences.begin("music_store", false);
    int numMusic = preferences.getInt("numMusic", 0);

    for (int i = 0; i < numMusic; ++i)
    {
        String storedMusicName = preferences.getString(("musicName" + String(i)).c_str(), "");
        if (storedMusicName == musicNameToDelete)
        {
            preferences.remove(("musicName" + String(i)).c_str());
            preferences.remove(("musicId" + String(i)).c_str());

            for (int j = i; j < numMusic - 1; ++j)
            {
                String nextMusicName = preferences.getString(("musicName" + String(j + 1)).c_str(), "");
                String nextMusicId = preferences.getString(("musicId" + String(j + 1)).c_str(), "");

                preferences.putString(("musicName" + String(j)).c_str(), nextMusicName);
                preferences.putString(("musicId" + String(j)).c_str(), nextMusicId);
            }

            preferences.remove(("musicName" + String(numMusic - 1)).c_str());
            preferences.remove(("musicId" + String(numMusic - 1)).c_str());
            preferences.putInt("numMusic", numMusic - 1);
            Serial.println("Success Delete Music!");

            request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Music Configuration</title></head><body><h1>Music Deleted!</h1><p>The music has been deleted.</p><p><a href='/'>Go Back</a></p></body></html>");
            preferences.end();
            return;
        }
    }
    Serial.println("Fail to Delete Music!");

    request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Music Configuration</title></head><body><h1>Music Not Found!</h1><p>The specified music was not found.</p><p><a href='/'>Go Back</a></p></body></html>");
    preferences.end();
}

void handleListMusic(AsyncWebServerRequest *request)
{
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Music Configuration</title></head><body><h1>Saved Music</h1><ul>";

    preferences.begin("music_store", true);
    int numMusic = preferences.getInt("numMusic", 0);

    for (int i = 0; i < numMusic; ++i)
    {
        String musicName = preferences.getString(("musicName" + String(i)).c_str(), "");
        String musicId = preferences.getString(("musicId" + String(i)).c_str(), "");
        html += "<li>musicName" + String(i) + ": " + musicName + " " + musicId + "</li>";
    }

    html += "</ul><p><a href='/'>Go Back</a></p></body></html>";

    request->send(200, "text/html", html);
    preferences.end();
}


String generateRandomString(size_t length) {  
    const std::string chars =   
        "0123456789"  
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  
        "abcdefghijklmnopqrstuvwxyz";  
      
    std::random_device rd;  // 用于获取随机数种子  
    std::mt19937 gen(rd()); // 以随机设备为种子初始化Mersenne Twister伪随机数生成器  
    std::uniform_int_distribution<> dis(0, chars.size() - 1); // 定义分布范围  
  
    // std::string str(length, ' ');  
    String str = "";
    for (size_t i = 0; i < length; ++i) {  
        str += chars[dis(gen)]; // 生成随机字符并添加到字符串中  
    }  
    return str;  
} 


//初始化
void initVoiceWakeSerial(){
  //打开串口
  voiceWakeSerial.begin(BAUD);
}

//循环获取语音模块的串口数据
void getVoiceWakeData(){
  //从语音模块读取响应数据
  if (voiceWakeSerial.available()>=1) 
  {
    //接收语音模块数据
    int read1 = voiceWakeSerial.read();
    int read2 = voiceWakeSerial.read();
    
    //判断
    char result[5] = "";
    itoa(read1, result, 10);
    
    if(read1 == 1){
        //收到语音唤醒打开录音
        printWakeData(read1,read2);
        WakeupAndStart();
        // 避免重复收到消息
        delay(500);
    }
    else if(read1 == 2){
        // 音量最大
        printWakeData(read1,read2);
        pauseVoice();
        setVolume2(100);
    }else if(read1 == 3){
        // 音量最小
        printWakeData(read1,read2);
        pauseVoice();
        setVolume2(30);
    }else if(read1 == 4){
        // 音量中等
        printWakeData(read1,read2);
        pauseVoice();
        setVolume2(70);
    }else if(read1 == 5){
        // 增大音量
        printWakeData(read1,read2);
        pauseVoice();
        setVolume2(volume+10);
    }else if(read1 == 6){
        // 减小音量
        printWakeData(read1,read2);
        pauseVoice();
        setVolume2(volume-10);
    }else if(read1 == 7){
        // 暂停播放
        printWakeData(read1,read2);
        pauseVoice();
    }else if(read1 == 8){
        // 停止播放
        printWakeData(read1,read2);
        pauseVoice();
    }else if(read1 == 9){
        // 打开网络
        printWakeData(read1,read2);
        pauseVoice();
        startWIfiAP(true);
    }else if(read1 == 16){ 
        // 关闭网络
        printWakeData(read1,read2);
        pauseVoice();
        startWIfiAP(false);
    }else if(read1 == 17){ 
        // 退下、再见
        printWakeData(read1,read2);
        pauseVoice();
    }
  }
}

// 打印收到的消息，延迟500毫秒，避免抖动重复收到消息
void printWakeData(int read1,int read2){
    Serial.print("收到内容：");
    Serial.print(read1);Serial.print(" ");
    Serial.print(read2);Serial.println();
}