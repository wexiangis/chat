
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "serialCmd.h"

enum SpCmdEnum{
    SP_CMD_NULL = 0,
    SP_CMD_SLEEP,
    SP_CMD_SAY,
    SP_CMD_EXIT,
};

#define CMD_MAX_LEN 512
struct CmdLine{
    int lineHead;
    char cmdResult[CMD_MAX_LEN];
    char cmd[CMD_MAX_LEN];
    enum SpCmdEnum spCmd;
    int spCmdParam;
    char spCmdParamBuff[CMD_MAX_LEN];
    //
    int lineNumber; //记录所在行号
    struct CmdLine *next;
};

struct ChatObject{
    char devPath[1024];
    char filePath[1024];
    char *fileContent;
    int timeOut;
    bool showInfo;
    int baudrate;
    //
    struct CmdLine *cmdLine;
};

bool pathExist(char *path)
{
    if(path == NULL)
        return false;
    if(access(path, F_OK) == 0) // F_OK 是否存在 R_OK 是否有读权限 W_OK 是否有写权限 X_OK 是否有执行权限
        return true;
    return false;
}

int cmdFileTranslate(struct ChatObject *object)
{
    FILE *fp;
    char lineBuff[CMD_MAX_LEN];
    int lineLen;
    int count, lineCount = 1, tempCount;
    char *tempPoint, hit[5];
    struct CmdLine *cmdLine = NULL, *cmdLineHead = NULL;
    //
    if((fp = fopen(object->filePath, "rt")) == NULL)
        return 0;
    //
    cmdLineHead = cmdLine = (struct CmdLine *)calloc(1, sizeof(struct CmdLine));
    while(1)
    {
        if(feof(fp))
            break;
        // 获取一行数据
        memset(lineBuff, 0, sizeof(lineBuff));
        if(fgets(lineBuff, sizeof(lineBuff), fp) == NULL)
            break;
        // 解析一行
        lineLen = strlen(lineBuff);
        if(lineLen == sizeof(lineBuff))
        {
            CONSOLE_PRINT("\r\n请保持指令行长度在 %d 字节以内 !!\r\n", (int)sizeof(lineBuff));
            return lineCount;
        }
        memset(hit, 0, sizeof(hit));//头标, 期望返回, 指令, 特殊指令, 特殊指令参数
        for(count = 0; count < lineLen; count++)
        {
            // 无效行
            if(lineBuff[count] == '#' || lineBuff[count] == '\r' || lineBuff[count] == '\n')
                break;
            // 期望返回
            else if(lineBuff[count] == '\"')
            {
                if(hit[1] == 0)
                    tempPoint = cmdLine->cmdResult;
                else
                    tempPoint = cmdLine->cmd;
                tempCount = 0;
                //
                for(count += 1; count < lineLen; count++)
                {
                    if(lineBuff[count] == '\\')
                    {
                        count += 1;
                        if(lineBuff[count] == 'r')
                           tempPoint[tempCount++] = 0x0D;
                        else if(lineBuff[count] == 'n')
                           tempPoint[tempCount++] = 0x0A;
                        else if(lineBuff[count] == 't')
                        {
                           tempPoint[tempCount++] = ' ';
                           tempPoint[tempCount++] = ' ';
                        }
                        else 
                            tempPoint[tempCount++] = lineBuff[count];
                    }
                    else if(lineBuff[count] == '\"')
                        break;
                    else
                        tempPoint[tempCount++] = lineBuff[count];
                }
                if(tempCount > 0 && count >= lineLen)
                    return lineCount;
                //
                if(hit[1] == 0)
                    hit[1] = 1;
                else
                {
                    hit[2] = 1;
                    break;
                }
            }
            // 分支号
            else if(hit[0] == 0 && lineBuff[count] > '0' && lineBuff[count] <= '9')
            {
                sscanf(&lineBuff[count], "%d", &cmdLine->lineHead);
                for(count += 1; count < lineLen; count++)
                {
                    if(lineBuff[count] < '0' || lineBuff[count] > '9')
                        break;
                }
                if(count >= lineLen)
                    return lineCount;
                //
                hit[0] = 1;
            }
            // 特殊指令
            else if(hit[3] == 0 && (lineBuff[count] == 'S' || lineBuff[count] == 'E'))
            {
                if(strncmp(&lineBuff[count], "SAY", 3) == 0)
                {
                    cmdLine->spCmd = SP_CMD_SAY;
                    count += 3;
                    int tempCount = 0;
                    for(; count < lineLen; count++)
                    {
                        if(lineBuff[count] == '\"')
                        {
                            for(count += 1; count < lineLen; count++)
                            {
                                if(lineBuff[count] == '\\')
                                {
                                    count += 1;
                                    if(lineBuff[count] == 'r')
                                        cmdLine->spCmdParamBuff[tempCount++] = 0x0D;
                                    else if(lineBuff[count] == 'n')
                                        cmdLine->spCmdParamBuff[tempCount++] = 0x0A;
                                    else if(lineBuff[count] == 't')
                                    {
                                        cmdLine->spCmdParamBuff[tempCount++] = ' ';
                                        cmdLine->spCmdParamBuff[tempCount++] = ' ';
                                    }
                                    else
                                        cmdLine->spCmdParamBuff[tempCount++] = lineBuff[count];
                                }
                                else if(lineBuff[count] == '\"')
                                    break;
                                else
                                    cmdLine->spCmdParamBuff[tempCount++] = lineBuff[count];
                            }
                            break;
                        }
                        else if(lineBuff[count] == '#' || lineBuff[count] == '\r' || lineBuff[count] == '\n')
                            count = lineLen;
                    }
                    if(tempCount > 0 && count >= lineLen)
                        return lineCount;
                    hit[4] = 1;
                }
                else if(strncmp(&lineBuff[count], "SLEEP", 5) == 0)
                {
                    cmdLine->spCmd = SP_CMD_SLEEP;
                    count += 5;
                }
                else if(strncmp(&lineBuff[count], "EXIT", 4) == 0)
                {
                    cmdLine->spCmd = SP_CMD_EXIT;
                    count += 4;
                }
                else
                    break;
                //
                hit[3] = 1;
                //
                for(; count < lineLen; count++)
                {
                    if(lineBuff[count] >= '0' && lineBuff[count] <= '9')
                    {
                        sscanf(&lineBuff[count], "%d", &cmdLine->spCmdParam);
                        count = lineLen;
                    }
                    else if(lineBuff[count] == '#' || lineBuff[count] == '\r' || lineBuff[count] == '\n')
                        count = lineLen;
                }
                //
                hit[4] = 1;
            }
        }
        // 确认该行可用
        if(hit[1] || hit[2] || hit[3])
        {
            cmdLine->lineNumber = lineCount;
            //
            if(object->showInfo)
            {
                //
                if(cmdLine->lineHead > 0)
                    CONSOLE_PRINT("line %d : %d ", cmdLine->lineNumber, cmdLine->lineHead);
                else
                    CONSOLE_PRINT("line %d : ", cmdLine->lineNumber);
                //
                if(hit[1])
                    CONSOLE_PRINT("\"%s\" ", cmdLine->cmdResult);
                //
                if(cmdLine->spCmd > SP_CMD_NULL)
                {
                    if(cmdLine->spCmd == SP_CMD_SAY)
                        CONSOLE_PRINT("SAY  %s\r\n", cmdLine->spCmdParamBuff);
                    else if(cmdLine->spCmd == SP_CMD_SLEEP)
                        CONSOLE_PRINT("SLEEP  %d\r\n", cmdLine->spCmdParam);
                    else if(cmdLine->spCmd == SP_CMD_EXIT)
                        CONSOLE_PRINT("EXIT  %d\r\n", cmdLine->spCmdParam);
                    else
                        CONSOLE_PRINT("NULL  %d\r\n", cmdLine->spCmdParam);
                }
                else
                    CONSOLE_PRINT("\"%s\"\r\n", cmdLine->cmd);
            }
            //
            cmdLine->next = (struct CmdLine *)calloc(1, sizeof(struct CmdLine));
            cmdLine = cmdLine->next;
        }
        else
            memset(cmdLine, 0, sizeof(sizeof(struct CmdLine)));
        //
        lineCount += 1;
    }
    //
    fclose(fp);
    // 返回链表
    if(cmdLine->next)
    {
        free(cmdLine->next);
        cmdLine->next = NULL;
    }
    object->cmdLine = cmdLineHead;
    if(object->showInfo)
        CONSOLE_PRINT("\r\n");
    //
    return 0;
}

#define EXPECT_MAX  1024
static struct CmdLine *_expectCmdLineArray[EXPECT_MAX] = {NULL};
static char *_expectCmdResultArray[EXPECT_MAX] = {NULL};

int _getExpectResult(struct CmdLine *currentCmdLine)
{
    struct CmdLine *next = NULL;
    int cHead = currentCmdLine->lineHead;
    int count = 0;
    //
    memset(_expectCmdLineArray, 0, sizeof(_expectCmdLineArray));
    memset(_expectCmdResultArray, 0, sizeof(_expectCmdResultArray));
    for(next = currentCmdLine->next, count = 0; next && count < EXPECT_MAX; next = next->next)
    {
        // CONSOLE_PRINT("_getExpectResult : count = %d, line = %d, head = %d\r\n", count, next->lineNumber, next->lineHead);
        // head == 0 候选项收集结束
        if(next->lineHead == 0)
        {
            _expectCmdResultArray[count] = next->cmdResult;
            _expectCmdLineArray[count] = next;
            count += 1;
            break;
        }
        // head == cHead + 1 在+1的head的行 和 0的head的行中收集
        else if(next->lineHead == cHead + 1)
        {
            _expectCmdResultArray[count] = next->cmdResult;
            _expectCmdLineArray[count] = next;
            count += 1;
        }
    }
    // CONSOLE_PRINT("_getExpectResult : return = %d\r\n", count);
    //
    return count;
}

int cmdRun(struct ChatObject *object)
{
    // 打开串口
    SerialCmd_Struct *serialCmd = serialCmd_init(object->devPath, object->baudrate);
    if(serialCmd == NULL)
        return -1;
    serialCmd->show = object->showInfo;
    // 开始遍历
    struct CmdLine *cmdLine = object->cmdLine;
    while(cmdLine)
    {
        if(object->showInfo) CONSOLE_PRINT("line >>> %d : ", cmdLine->lineNumber);
        // 优先使用特殊指令
        if(cmdLine->spCmd > SP_CMD_NULL)
        {
            if(cmdLine->spCmd == SP_CMD_SLEEP)
            {
                if(object->showInfo) CONSOLE_PRINT("SLEEP %d\r\n", cmdLine->spCmdParam);
                serialCmd_delay_ms(cmdLine->spCmdParam*1000);
            }
            else if(cmdLine->spCmd == SP_CMD_SAY)
            {
                if(object->showInfo) CONSOLE_PRINT("SAY\r\n");
                CONSOLE_PRINT("%s\r\n", cmdLine->spCmdParamBuff);
            }
            if(cmdLine->spCmd == SP_CMD_EXIT)
            {
                if(object->showInfo) CONSOLE_PRINT("EXIT %d\r\n", cmdLine->spCmdParam);
                serialCmd_release(serialCmd);
                return cmdLine->spCmdParam;
            }
            // 寻找下一条
            int ret = _getExpectResult(cmdLine);
            if(ret > 0)
                cmdLine = _expectCmdLineArray[0];
            else
                break;
        }
        // 其次使用指令
        else if(cmdLine->cmd[0])
        {
            if(object->showInfo) CONSOLE_PRINT("%s\r\n", cmdLine->cmd);
            int ret = _getExpectResult(cmdLine);
            if(ret > 0)
            {
                int ret2 = serialCmd_transfer(serialCmd, cmdLine->cmd, _expectCmdResultArray, object->timeOut, 1, NULL, 0);
                if(ret2 < 0)
                {
                    serialCmd_release(serialCmd);
                    return cmdLine->lineNumber;
                }
                else
                    cmdLine = _expectCmdLineArray[ret2];
            }
            else
                break;
        }
        else
        {
            if(object->showInfo) CONSOLE_PRINT("NULL\r\n");
            cmdLine = cmdLine->next;
        }
    }
    //
    serialCmd_release(serialCmd);
    return 0;
}

const char commit[] = "\r\nchat-me909s  V1.0\r\n"
    "-f filePath[指令脚本文件路径] \r\n"
    "-d devPath[tty串口设备路径默认/dev/ttyUSB0] \r\n"
    "-b baudrate[指定串口波特率默认115200] \r\n"
    "-t n[指令等待超时默认1000ms] \r\n"
    "-s [打印交互信息默认关闭] \r\n"
    "-c console[指定打印终端设备默认/dev/console] \r\n"
    "\r\n"
    "----- 文件格式说明 ----- \r\n\r\n"
    "第1行:\"期望返回\"              \"cmd1\"#正常指令行 \r\n"
    "第2行:1 \"cmd1可能的期望返回\"  \"cmd2\" #分支1 \r\n"
    "第3行:2 \"cmd2可能的期望返回\"  \"cmd3\" #分支2 \r\n"
    "第4行:2 \"cmd2可能的期望返回\"  \"cmd4\" #分支2 \r\n"
    "第5行:3 \"cmd3,4可能的期望返回\" \"cmd5\"#分支3 \r\n"
    "第6行:1 \"cmd1可能的期望返回\"  \"cmd6\" #分支1 \r\n"
    "第7行:\"可能来自cmd1,2,3,4,5,6的期望返回\"  \"cmd8\" #前面的分支最后都会跳转到这里(只要没有退出) \r\n"
    "第8行:SLEEP  1           #特殊指令 延时1秒 \r\n"
    "第9行:SAY    \"END\"       #特殊指令 相当于print \r\n"
    "第10行:EXIT   1          #特殊指令 结束并返回1 \r\n\r\n"
    "#其它说明1: 每行的期望返回对应的是上一条指令的返回 \r\n"
    "#其它说明2: 特殊指令可以用在分支内 如: 3 SLEEP 1 \r\n"
    "#其它说明3: 特殊指令前面也可以有期望返回项 如: 3 \"OK\" SLEEP 1 \r\n"
    "#其它说明4: 分支为n的行的命令发出后 其期望返回会从分支为n+1或0(没写分支号)的行获取 \r\n"
    "#           比如上面第2行的cmd2发出后 会去依次匹配第3,4,7行的返回\r\n"
    "#其它说明5: 正常运行结束返回0 其它返回行号表示所在行未得到匹配返回 \r\n\r\n"
    "----- 文件参考 -----\r\n\r\n"
    "\"\"   \"ATE\"                 #启用指令回显 \r\n"
    "\"OK\" \"ATH\"                 #结束之前的连接 \r\n"
    "\"OK\" \"AT+CGDCONT=1,\\\"IP\\\",\\\"CMNET\\\"\" #设置APN(注意使用双引号时加上反斜杠) \r\n"
    "\"OK\" \"AT^CARDMODE\"         #(huawei指令)检查SIM状态 \r\n"
    "1 \"ERROR\" \"AT^SIMSWITCH?\"  #(huawei指令)当前SIM卡选择 \r\n"
    "2 \"0\" \"AT^SIMSWITCH=1\"     #(huawei指令)切换到SIM卡1 \r\n"
    "2 \"1\" \"AT^SIMSWITCH=0\"     #(huawei指令)切换到SIM卡0 \r\n"
    "3 EXIT 9                   #切卡之后退出 等下一轮拨号 \r\n"
    "\"OK\"  \"ATDT*98*1#\"         #测试拨号(移动) \r\n"
    "\"CONNECT\"  EXIT  0         #成功返回 \r\n"
    "\r\n";

int main(int argc, char *argv[])
{
    if(argc < 2)
        goto TIPS_RETURN;
    // 解析传参
    char *param;
    int i;
    struct ChatObject chatObject;
    memset(&chatObject, 0, sizeof(struct ChatObject));
    strcpy(chatObject.devPath, "/dev/ttyUSB0");   //默认参数
    chatObject.baudrate = 115200;   //默认参数
    chatObject.timeOut = 1000;   //默认参数
    for(i = 1; i < argc; i++)
    {
        param = argv[i];
        //
        if(strncmp(param, "-d", 2) == 0 && i + 1 < argc)
        {
            i += 1;
            if(!pathExist(argv[i]))
            {
                CONSOLE_PRINT("\r\n参数: -d %s 设备不存在 !!\r\n\r\n", argv[i]);
                goto ERROR_RETURN;
            }
            memset(chatObject.devPath, 0 ,sizeof(chatObject.devPath));
            strcpy(chatObject.devPath, argv[i]);
        }
        else if(strncmp(param, "-c", 2) == 0 && i + 1 < argc)
        {
            i += 1;
            if(!pathExist(argv[i]))
            {
                CONSOLE_PRINT("\r\n参数: -c %s 终端设备不存在 !!\r\n\r\n", argv[i]);
                goto ERROR_RETURN;
            }
            memset(CONSOLE_PATH, 0 ,sizeof(CONSOLE_PATH));
            strcpy(CONSOLE_PATH, argv[i]);
        }
        else if(strncmp(param, "-f", 2) == 0 && i + 1 < argc)
        {
            i += 1;
            if(!pathExist(argv[i]))
            {
                CONSOLE_PRINT("\r\n参数: -f %s 文件不存在 !!\r\n\r\n", argv[i]);
                goto ERROR_RETURN;
            }
            memset(chatObject.filePath, 0 ,sizeof(chatObject.filePath));
            strcpy(chatObject.filePath, argv[i]);
        }
        else if(strncmp(param, "-b", 2) == 0 && i + 1 < argc)
        {
            i += 1;
            int tempInt = -1;
            sscanf(argv[i], "%d", &tempInt);
            if(tempInt == 9600 || tempInt == 115200 || tempInt == 230400 || tempInt == 460800)
                chatObject.baudrate = tempInt;
            else
            {
                CONSOLE_PRINT("\r\n参数: -b 错误 !! 波特率范围 9600/115200/230400/460800\r\n\r\n");
                goto ERROR_RETURN;
            }
        }
        else if(strncmp(param, "-t", 2) == 0 && i + 1 < argc)
        {
            i += 1;
            int tempInt = -1;
            sscanf(argv[i], "%d", &tempInt);
            if(tempInt > 0)
                chatObject.timeOut = tempInt;
            else
            {
                CONSOLE_PRINT("\r\n参数: -t 错误 !!\r\n\r\n");
                goto ERROR_RETURN;
            }
        }
        else if(strncmp(param, "-s", 2) == 0)
            chatObject.showInfo = true;
        else if(strncmp(param, "-?", 2) == 0 || strstr(param, "help"))
            goto TIPS_RETURN;
    }
    // 检查漏掉的关键参数
    if(chatObject.filePath[0] == 0)
    {
        CONSOLE_PRINT("\r\n缺少指令脚本文件路径 !! 请使用 -f filePath[指令脚本文件路径]\r\n\r\n");
        goto ERROR_RETURN;
    }
    // 解析指令脚本
    int ret = 0;
    if((ret = cmdFileTranslate(&chatObject)) > 0)
    {
        CONSOLE_PRINT("\r\n第 %d 行指令格式错误 !!\r\n\r\n", ret);
        goto ERROR_RETURN;
    }
    // 执行指令
    ret = cmdRun(&chatObject);
    if(chatObject.showInfo)
        CONSOLE_PRINT("\r\n\r\nEND %d\r\n\r\n", ret);
    return ret;

TIPS_RETURN:
    CONSOLE_PRINT("%s", commit);
    return -1;
ERROR_RETURN:
    return -1;
}

