
#ifndef __SERIALCMD_H_
#define __SERIALCMD_H_

#include <pthread.h>
#include <stdbool.h>
#include <termios.h>    //B921600,B460800,B230400 ...

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    unsigned int lable;
    unsigned int baudrate;
}SERIAL_BAUD_ST;

typedef struct {
    char parity;
    unsigned int baud;
    unsigned int databits;
    unsigned int stopbits;
}SERIAL_ATTR_ST;

#define TIMEOUT              1  //read operation timeout 1s = TIMEOUT/10
#define MIN_LEN              128//the min len datas
#define DEV_NAME_LEN         11
#define SERIAL_ATTR_BAUD     115200
#define SERIAL_ATTR_DATABITS 8
#define SERIAL_ATTR_STOPBITS 1
#define SERIAL_ATTR_PARITY   'n'
#define SERIAL_MODE_NORMAL   0
#define SERIAL_MODE_485      1
#define DELAY_RTS_AFTER_SEND 1  //1ms
#define SER_RS485_ENABLED    1  //485 enable

//
typedef struct{
    int fd;             //文件句柄
    char devPath[1024];
    SERIAL_ATTR_ST serial_attr;
    int boaudrate;
    //
    pthread_attr_t attr;
    pthread_t read_th;
    //
    int waitMinute;     //等待返回时间 ms
    int retryTime;      //尝试次数
    //
    bool transferFlag;  //发指令线程告诉接收线程开始接收标志: 开始等待
    bool resultFlag;    //接收线程告诉发指令线程得到了目标: 退出等待
    bool err;           //接收线程告诉发指令线程句柄读取出错: 退出等待
    //
    char sendBuff[128]; //发出指令
    char **checkBuff;   //期望返回内容 一个二维数组 可同时匹配多条 按顺序优先级匹配
    //
    char *recvBuff;     //指向传入的用来接收回复信息的地址
    int recvBuffSize;   //接收回复信息的地址最大长度
    //
    int retHit;         //返回命中checkBuff[n]中的n的序号
    //
    bool exit;          //告诉接收线程结束循环并退出
    bool show;          //显示通信过程
}SerialCmd_Struct;

//
int serialCmd_transfer(SerialCmd_Struct *ss, char *cmdBuff, char **checkBuff, int waitMinute, int retryTime, char *recvBuff, int recvBuffSize);

SerialCmd_Struct *serialCmd_init(char *devPath, int boaudrate);
bool serialCmd_restart(SerialCmd_Struct *ss);

void serialCmd_release(SerialCmd_Struct *ss);

void serialCmd_delay_ms(unsigned int ms);

#endif
