
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#include "serialCmd.h"

void serialCmd_delay_ms(unsigned int ms)
{
    struct timeval delay;
    if (ms > 1000)
    {
        delay.tv_sec = ms / 1000;
        delay.tv_usec = (ms % 1000)*1000; //us延时
    }
    else
    {
        delay.tv_sec = 0;
        delay.tv_usec = ms*1000; //us延时
    }
    select(0, NULL, NULL, NULL, &delay);
}

bool serialCmd_stringCompare(char *buff, int buffLen, char *target, int targetLen)
{
    int i, j;
    for(i = 0; i < buffLen; i++)
    {
        if(buff[i] == target[0])
        {
            for(j = 0; i < buffLen && j < targetLen; j++, i++)
            {
                if(buff[i] != target[j])
                    break;
            }
            if(j == targetLen)
                return true;
            i -= j;
        }
    }
    return false;
}

//-------------------- serial driver --------------------

static SERIAL_BAUD_ST g_attr_baud[] = {
    {921600, B921600},
    {460800, B460800},
    {230400, B230400},
    {115200, B115200},
    {57600, B57600},
    {38400, B38400},
    {19200, B19200},
    {9600, B9600},
    {4800, B4800},
    {2400, B2400},
    {1800, B1800},
    {1200, B1200},
};

int attr_baud_set(int fd, unsigned int baud)
{
    int i;
    int ret = 0;
    struct termios option;

    /* get old serial attribute */
    memset(&option, 0, sizeof(option));
    if (0 != tcgetattr(fd, &option))
    {
        printf("tcgetattr failed.\n");
        return -1;
    }

    for (i = 0; i < ARRAY_SIZE(g_attr_baud);  i++)
    {
        if (baud == g_attr_baud[i].lable)
        {
            ret = tcflush(fd, TCIOFLUSH);
            if (0 != ret)
            {
                printf("tcflush failed.\n");
                break;
            }
            ret = cfsetispeed(&option, g_attr_baud[i].baudrate);
            if (0 != ret)
            {
                printf("cfsetispeed failed.\n");
                ret = -1;
                break;
            }
            ret = cfsetospeed(&option, g_attr_baud[i].baudrate);
            if (0 != ret)
            {
                printf("cfsetospeed failed.\n");
                ret = -1;
                break;
            }
            ret = tcsetattr(fd, TCSANOW, &option);
            if  (0 != ret)
            {
                printf("tcsetattr failed.\n");
                ret = -1;
                break;
            }
            ret = tcflush(fd, TCIOFLUSH);
            if (0 != ret)
            {
                printf("tcflush failed.\n");
                break;
            }
        }
    }
    return ret;
}

int attr_other_set(int fd, SERIAL_ATTR_ST *serial_attr)
{
    struct termios option;

    /* get old serial attribute */
    memset(&option, 0, sizeof(option));
    if (0 != tcgetattr(fd, &option))
    {
        printf("tcgetattr failed.\n");
        return -1;
    }

    option.c_iflag = CLOCAL | CREAD;

    /* set datas size */
    option.c_cflag &= ~CSIZE;
    option.c_iflag = 0;

    switch (serial_attr->databits)
    {
        case 7:
            option.c_cflag |= CS7;
            break;

        case 8:
            option.c_cflag |= CS8;
            break;

        default:
            printf("invalid argument, unsupport datas size.\n");
            return -1;
    }

    /* set parity */
    switch (serial_attr->parity)
    {
        case 'n':
        case 'N':
            option.c_cflag &= ~PARENB;
            option.c_iflag &= ~INPCK;
            break;

        case 'o':
        case 'O':
            option.c_cflag |= (PARODD | PARENB);
            option.c_iflag |= INPCK;
            break;

        case 'e':
        case 'E':
            option.c_cflag |= PARENB;
            option.c_cflag &= ~PARODD;
            option.c_iflag |= INPCK;
            break;

        case 's':
        case 'S':
            option.c_cflag &= ~PARENB;
            option.c_cflag &= ~CSTOPB;
            break;

        default:
            printf("invalid argument, unsupport parity type.\n");
            return -1;
    }

    /* set stop bits  */
    switch (serial_attr->stopbits)
    {
        case 1:
            option.c_cflag &= ~CSTOPB;
            break;

        case 2:
            option.c_cflag |= CSTOPB;
            break;

        default:
            printf("invalid argument, unsupport stop bits.\n");
            return -1;
    }

    option.c_oflag = 0;
    option.c_lflag = 0;
    option.c_cc[VTIME] = TIMEOUT;
    option.c_cc[VMIN] = MIN_LEN;

    if (0 != tcflush(fd,TCIFLUSH))
    {
        printf("tcflush failed.\n");
        return -1;
    }

    if (0 != tcsetattr(fd, TCSANOW, &option))
    {
        printf("tcsetattr failed.\n");
        return -1;
    }

    return 0;
}

int attr_set(int fd, SERIAL_ATTR_ST *serial_attr)
{
    int ret = 0;

    if (NULL == serial_attr)
    {
        printf("invalid argument.\n");
        return -1;
    }

    if (0 == ret)
    {
        ret = attr_baud_set(fd, serial_attr->baud);
        if (0 == ret)
        {
            ret = attr_other_set(fd, serial_attr);
        }
    }

    return ret;
}

//-------------------- serialCmd driver --------------------

//
void serialCmd_thread_read(void *argv)
{
    int ret, count;
    char buff[1024];
    char **p;
    //
    SerialCmd_Struct *ss = (SerialCmd_Struct *)argv;
    if(ss == NULL)
        return ;
    //
    while(!ss->exit)
    {
        //read data
        memset(buff, 0, sizeof(buff));
        if(ss->fd > 0)
            ret = read(ss->fd, buff, sizeof(buff));
        else
        {
            ret = 0;
            serialCmd_delay_ms(100);    //延时100ms
        }
        //is work ?
        if(ss->transferFlag == false || ss->resultFlag)     // 非工作模式
        {
            ret = 0;
            serialCmd_delay_ms(100);    //延时100ms
        }
        else if(ret > 0 && ss->transferFlag)  // 工作模式下
        {
            if(ss->show)
            {
                printf("serialCmd recv : ret = %d (tF:%s rF:%s) check : \r\n", 
                    ret, 
                    (ss->transferFlag?"true":"false"), 
                    (ss->resultFlag?"true":"false"));
                p = ss->checkBuff;
                while(p)
                    printf("%s ", *p++);
                printf("\r\n%s\r\n", buff);
            }
            //备份返回, 如有需要
            if(ss->recvBuff && ret < ss->recvBuffSize)
                memcpy(ss->recvBuff, buff, ret);
            // 比对返回
            for(p = ss->checkBuff, count = 0; p; p++)
            {
                if(serialCmd_stringCompare(buff, ret, *p, strlen(*p)))
                {
                    ss->retHit = count;
                    ss->resultFlag = true;
                    break;
                }
            }
            //continue;
        }
        else if(ret < 0)    //串口错误, 跳出并结束线程
        {
            ss->err = true;
            printf("serialCmd read : err !\r\n");
            break;
        }
        serialCmd_delay_ms(1);    //延时1ms
    }
    //
    // if(ss->show)
    //     printf("serialCmd recv end !! (err:%s exit:%s)\r\n", 
    //         (ss->err?"true":"false"), 
    //         (ss->exit?"true":"false"));
}
//
bool serialCmd_restart(SerialCmd_Struct *ss)
{
    if(ss == NULL || ss->devPath == NULL || strlen(ss->devPath) == 0 || ss->boaudrate <= 0)
    {
        printf("serialCmd_restart : err !\r\n");
        return false;
    }
    //
    if(!ss->err)
    {
        ss->exit = true;
        pthread_join(ss->read_th, NULL);
        //
        close(ss->fd);
        ss->fd = 0;
    }
    //打开文件节点
    if((ss->fd = open(ss->devPath, O_RDWR)) <= 0)
    {
        printf("serialCmd_restart : open %s err\r\n", ss->devPath);
        return false;
    }
    //串口初始化
    memset(&ss->serial_attr, 0, sizeof(SERIAL_ATTR_ST));
    ss->serial_attr.baud = ss->boaudrate;
    ss->serial_attr.databits = 8;
    ss->serial_attr.stopbits = 1;
    ss->serial_attr.parity = 'n';
    attr_set(ss->fd, &ss->serial_attr);
    //
    ss->err = false;
    ss->exit = false;
    //创建读线程
    if(pthread_create(&ss->read_th, &ss->attr, (void *)serialCmd_thread_read, (void *)ss) != 0)
    {
        printf("serialCmd_restart : pthread_create thread read failed\r\n");
        return NULL;
    }
    //
    return true;
}
//
bool serialCmd_cmd_transfer(SerialCmd_Struct *ss)
{
    int i, timeOut;
    //
    if(ss == NULL)
        return false;
    //
    ss->resultFlag = false;
    ss->transferFlag = true;
    //
    serialCmd_delay_ms(1);    //延时1ms
    // 指令测试2次
    for(i = 0; i < ss->retryTime; i++)
    {
        if(ss->show)
            printf("serialCmd write : %s", ss->sendBuff);

        if(write(ss->fd, ss->sendBuff, strlen(ss->sendBuff)) < 0)  // 发出指令
        {
            ss->transferFlag = false;
            ss->err = true;
            printf("serialCmd write : err !\r\n");
            return false;
        }
        timeOut = 0;
        while(timeOut < ss->waitMinute)    // 在一定时间内检测返回
        {
            if(ss->resultFlag || ss->err)
            {
                ss->transferFlag = false;
                return true;
            }
            serialCmd_delay_ms(1);    //延时1ms
            timeOut += 1;
        }
    }
    //
    ss->transferFlag = false;
    serialCmd_delay_ms(1);
    return false;
}
//
int serialCmd_transfer(SerialCmd_Struct *ss, char *cmdBuff, char **checkBuff, int waitMinute, int retryTime, char *recvBuff, int recvBuffSize)
{
    bool ret;
    //
    if(ss == NULL)
        return -1;
    if(ss->err)
    {
        if(!serialCmd_restart(ss))
            return -1;
    }
    //
    ss->waitMinute = waitMinute;
    ss->retryTime = retryTime;
    //
    ss->transferFlag = false;
    ss->resultFlag = false;
    //
    memset(ss->sendBuff, 0, sizeof(ss->sendBuff));
    strcpy(ss->sendBuff, cmdBuff);
    //
    ss->checkBuff = checkBuff;
    ss->retHit = 0;
    //如有需要, 备份返回数据
    ss->recvBuff = recvBuff;
    ss->recvBuffSize = recvBuffSize;
    //
    ret = serialCmd_cmd_transfer(ss);
    ss->recvBuffSize = 0;
    ss->recvBuff = NULL;
    //
    if(ret)
    {
        if(ss->resultFlag)
            return ss->retHit;
    }
    return -1;
}
//
SerialCmd_Struct *serialCmd_init(char *devPath, int boaudrate)
{
    SerialCmd_Struct *ss;
    //
    if(devPath == NULL)
    {
        printf("serialCmd_init : devPath err : %s\r\n", devPath);
        return NULL;
    }
    if(boaudrate <= 0)
    {
        printf("serialCmd_init : boaudrate err : %d\r\n", boaudrate);
        return NULL;
    }
    //
    ss = (SerialCmd_Struct *)calloc(1, sizeof(SerialCmd_Struct));
    strcpy(ss->devPath, devPath);
    //打开文件节点
    if((ss->fd = open(ss->devPath, O_RDWR)) <= 0)
    {
        //printf("serialCmd_init : open %s err\r\n", ss->devPath);
        free(ss);
        return NULL;
    }
    //串口初始化
    memset(&ss->serial_attr, 0, sizeof(SERIAL_ATTR_ST));
    ss->serial_attr.baud = ss->boaudrate = boaudrate;
    ss->serial_attr.databits = 8;
    ss->serial_attr.stopbits = 1;
    ss->serial_attr.parity = 'n';
    attr_set(ss->fd, &ss->serial_attr);
    //创建读线程
    pthread_attr_init(&ss->attr);
    pthread_attr_setdetachstate(&ss->attr, PTHREAD_CREATE_DETACHED);   //禁用线程同步, 线程运行结束后自动释放
    if(pthread_create(&ss->read_th, &ss->attr, (void *)serialCmd_thread_read, (void *)ss) != 0)
    {
        printf("serialCmd_init : pthread_create thread read failed\r\n");
        free(ss);
        return NULL;
    }
    //
    return ss;
}
void serialCmd_release(SerialCmd_Struct *ss)
{
    if(ss == NULL)
        return;
    //
    if(!ss->err)
    {
        ss->exit = true;
        pthread_join(ss->read_th, NULL);
        //
        close(ss->fd);
        ss->fd = 0;
    }
    //
    pthread_attr_destroy(&ss->attr);
    //
    free(ss);
}

