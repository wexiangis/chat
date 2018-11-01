
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

const char commit[] = "\r\n./chat_for_me909s "
    "-d devPath[tty串口设备路径] "
    "-f filePath[指令脚本文件路径] "
    "-b baudrate[指定串口波特率默认115200] "
    "-t n[指令等待超时n秒] "
    "-s[打印交互信息] \r\n\r\n"
    "----- 文件格式说明 ----- \r\n\r\n"
    "\"期望返回\"              \"cmd1\" #正常指令行 \r\n"
    "1 \"cmd1可能的期望返回\"  \"cmd2\" #一次分支指令行 \r\n"
    "2 \"cmd2可能的期望返回\"  \"cmd5\" #二次分支指令行 \r\n"
    "2 \"cmd2可能的期望返回\"  \"cmd6\" #二次分支指令行 \r\n"
    "1 \"cmd1可能的期望返回\"  \"cmd3\" #一次分支指令行 \r\n"
    "1 \"cmd1可能的期望返回\"  \"cmd4\" #一次分支指令行 \r\n"
    "\"以上指令最后期望返回\"  \"cmd7\" #回到主流程 \r\n"
    "SLEEP  1           #特殊指令(不用双引号) 延时1秒 \r\n"
    "SAY    \"END\"       #特殊指令(不用双引号) 相当于print \r\n"
    "EXIT   1           #特殊指令(不用双引号) 结束并返回1 \r\n\r\n"
    "#其它说明1: 每行的期望返回对应的是上一条指令的返回 \r\n"
    "#其它说明2: 特殊指令可以用在分支内 如: 3 SLEEP 1 \r\n\r\n"
    "----- 文件参考 -----\r\n\r\n"
    "\"\"   \"ATE\"                 #启用指令回显 \r\n"
    "\"OK\" \"ATH\"                 #结束之前的连接 \r\n"
    "\"OK\" \"AT+CGDCONT=1,\\\"IP\\\",\\\"CMNET\\\"\" #设置APN(注意使用双引号时加上反斜杠) \r\n"
    "\"OK\" \"AT^CARDMODE\"         #(huawei指令)检查SIM状态 \r\n"
    "1 \"ERROR\" \"AT^SIMSWITCH?\"  #(huawei指令)当前SIM卡选择 \r\n"
    "2 \"0\" \"AT^SIMSWITCH=1\"     #(huawei指令)切换到SIM卡1 \r\n"
    "2 \"1\" \"AT^SIMSWITCH=0\"     #(huawei指令)切换到SIM卡0 \r\n"
    "\"OK\"  \"ATDT*98*1#\"         #测试拨号(移动) \r\n"
    "\r\n\r\n";

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
    struct CmdLine *cmdLine = NULL;
    //
    if((fp = fopen(object->filePath, "rt")) == NULL)
        return 0;
    //
    cmdLine = (struct CmdLine *)calloc(1, sizeof(struct CmdLine));
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
            printf("\r\n请保持指令行长度在 %d 字节以内 !!\r\n", (int)sizeof(lineBuff));
            return lineCount;
        }
        memset(hit, 0, sizeof(hit));//头标, 期望返回, 指令, 特殊指令, 特殊指令参数
        for(count = 0; count < lineLen; count++)
        {
            // 期望返回
            if(lineBuff[count] == '\"')
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
                    printf("line %d : %d ", cmdLine->lineNumber, cmdLine->lineHead);
                else
                    printf("line %d : ", cmdLine->lineNumber);
                //
                if(hit[1])
                    printf("\"%s\" ", cmdLine->cmdResult);
                //
                if(cmdLine->spCmd > SP_CMD_NULL)
                {
                    if(cmdLine->spCmd == SP_CMD_SAY)
                        printf("SAY  %s\r\n", cmdLine->spCmdParamBuff);
                    else if(cmdLine->spCmd == SP_CMD_SLEEP)
                        printf("SLEEP  %d\r\n", cmdLine->spCmdParam);
                    else if(cmdLine->spCmd == SP_CMD_EXIT)
                        printf("EXIT  %d\r\n", cmdLine->spCmdParam);
                    else
                        printf("NULL  %d\r\n", cmdLine->spCmdParam);
                }
                else
                    printf("\"%s\"\r\n", cmdLine->cmd);
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
    object->cmdLine = cmdLine;
    if(object->showInfo)
        printf("\r\n");
    //
    return 0;
}

int cmdRun(struct ChatObject *object)
{
    // 打开串口
    
    // 

    // 
    return 0;
}

int main(int argc, char *argv[])
{
    if(argc < 2)
        goto TIPS_RETURN;
    // 解析传参
    char *param;
    int i;
    struct ChatObject chatObject;
    memset(&chatObject, 0, sizeof(struct ChatObject));
    chatObject.baudrate = 115200;
    for(i = 1; i < argc; i++)
    {
        param = argv[i];
        //
        if(strncmp(param, "-d", 2) == 0 && i + 1 < argc)
        {
            i += 1;
            strcpy(chatObject.devPath, argv[i]);
            if(!pathExist(chatObject.devPath))
            {
                printf("\r\n参数: -d %s 设备不存在 !!\r\n\r\n", chatObject.devPath);
                goto ERROR_RETURN;
            }
        }
        else if(strncmp(param, "-f", 2) == 0 && i + 1 < argc)
        {
            i += 1;
            strcpy(chatObject.filePath, argv[i]);
            if(!pathExist(chatObject.filePath))
            {
                printf("\r\n参数: -f %s 文件不存在 !!\r\n\r\n", chatObject.filePath);
                goto ERROR_RETURN;
            }
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
                printf("\r\n参数: -b 错误 !! 波特率范围 9600/115200/230400/460800\r\n\r\n");
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
                printf("\r\n参数: -t 错误 !!\r\n\r\n");
                goto ERROR_RETURN;
            }
        }
        else if(strncmp(param, "-s", 2) == 0)
            chatObject.showInfo = true;
        else if(strncmp(param, "-?", 2) == 0 || strstr(param, "help"))
            goto TIPS_RETURN;
    }
    // 检查漏掉的关键参数
    if(chatObject.devPath[0] == 0)
    {
        printf("\r\n缺少设备路径 !! 请使用 -d devPath[tty串口设备路径]\r\n\r\n");
        goto ERROR_RETURN;
    }
    else if(chatObject.filePath[0] == 0)
    {
        printf("\r\n缺少指令脚本文件路径 !! 请使用 -f filePath[指令脚本文件路径]\r\n\r\n");
        goto ERROR_RETURN;
    }
    // 解析指令脚本
    int ret = 0;
    if((ret = cmdFileTranslate(&chatObject)) > 0)
    {
        printf("\r\n第 %d 行指令格式错误 !!\r\n\r\n", ret);
        goto ERROR_RETURN;
    }
    // 执行指令
    return cmdRun(&chatObject);

TIPS_RETURN:
    printf("%s", commit);
    return -1;
ERROR_RETURN:
    return -1;
}

