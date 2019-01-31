/*
 * modem_commands.c
 *
 *  Created on: Apr 30, 2018
 *      Author: Matt
 */

#include <stddef.h>
#include <stdint.h>
#include "modem_commands.h"

/*
 * Common responses
 */
const ModemResponse_t rspOK = {.pucData = "OK\r\n",};
const ModemResponse_t rspERROR = {.pucData = "ERROR\r\n",};


/*
 * Commands, grouped with their responses
 */
const ModemCommand_t cmdAT = {.pucData = "AT\r\n",};


const ModemCommand_t cmdATE0 = {.pucData = "ATE0\r\n",};
const ModemResponse_t rspATE0Echo = {.pucData = "ATE0\r\r\n",};


const ModemCommand_t cmdATCCLK = {.pucData = "AT+CCLK?\r\n",};
const ModemResponse_t rspATCCLK = {.pucData = "+CCLK: ", .ulCheckLength = 7};


const ModemCommand_t cmdATCBC = {.pucData = "AT+CBC\r\n",};
const ModemResponse_t rspATCBC = {.pucData = "+CBC: ", .ulCheckLength = 6};

const ModemCommand_t cmdATCSQ = {.pucData = "AT+CSQ\r\n",};
const ModemResponse_t rspATCSQ = {.pucData = "+CSQ: ", .ulCheckLength = 6};

const ModemCommand_t cmdATCIPMODEQuery = {.pucData = "AT+CIPMODE?\r\n",};
const ModemResponse_t rspATCIPMODECommandMode = {.pucData = "+CIPMODE: 0\r\n",};
const ModemResponse_t rspATCIPMODEDataMode = {.pucData = "+CIPMODE: 1\r\n",};

const ModemCommand_t cmdATCIPMODE0 = {.pucData = "AT+CIPMODE=0\r\n",};
const ModemCommand_t cmdATCIPMODE1 = {.pucData = "AT+CIPMODE=1\r\n",};

const ModemCommand_t cmdATNETOPENQuery = {.pucData = "AT+NETOPEN?\r\n",};
const ModemResponse_t rspATNETOPENTrue = {.pucData = "+NETOPEN: 1,", .ulCheckLength = 12};
const ModemResponse_t rspATNETOPENFalse = {.pucData = "+NETOPEN: 0,", .ulCheckLength = 12};


const ModemCommand_t cmdATNETOPEN = {.pucData = "AT+NETOPEN\r\n",};
const ModemResponse_t rspATNETOPENSuccess = {.pucData = "+NETOPEN: 0\r\n",};
const ModemResponse_t rspATNETOPENIPErr = {.pucData = "+IP ERROR: Network is already opened\r\n",};

const ModemCommand_t cmdATNETCLOSE = {.pucData = "AT+NETCLOSE\r\n",};
const ModemResponse_t rspATNETCLOSESuccess = {.pucData = "+NETCLOSE: 0\r\n",};

const ModemCommand_t cmdATCIPOPENQuery = {.pucData = "AT+CIPOPEN?\r\n",};
const ModemResponse_t rspATCIPOPENTrue = {.pucData = "+CIPOPEN: 0,\"TCP\",\"208.113.167.211\",21234,-1\r\r\n",};
const ModemResponse_t rspATCIPOPENFalse = {.pucData = "+CIPOPEN: 0\r\r\n",};
const ModemResponse_t rspATCIPOPENRest = {.pucData = "+CIPOPEN: ", .ulCheckLength = 10};

const ModemCommand_t cmdATCIPOPEN = {.pucData = "AT+CIPOPEN=0,\"TCP\",\"208.113.167.211\",21234\r\n",};
const ModemResponse_t rspATCIPOPENConnect = {.pucData = "CONNECT 115200\r\n",};
const ModemResponse_t rspATCIPOPENSuccess = {.pucData = "+CIPOPEN: 0,0\r\n",};
const ModemResponse_t rspATCIPOPENFail = {.pucData = "+CIPOPEN: 0,", .ulCheckLength = 12};
const ModemResponse_t rspATCIPRcv = {.pucData = "RECV FROM: 208.113.167.211:21234\r\n",};
const ModemResponse_t rspATCIPIPD = {.pucData = "+IPD", .ulCheckLength = 4};
const ModemResponse_t rspCLOSED = {.pucData = "CLOSED\r\n",};

const ModemCommand_t cmdPlus = {.pucData = "+",};

const ModemCommand_t cmdATO = {.pucData = "ATO\r\n",};

const ModemCommand_t cmdATCIPCLOSE = {.pucData = "AT+CIPCLOSE=0\r\n",};
const ModemResponse_t rspATCIPCLOSESuccess = {.pucData = "+CIPCLOSE: 0,0\r\n",};

const ModemCommand_t cmdATCIPSEND = {.pucData = "AT+CIPSEND=0,",};
const ModemResponse_t rspATCIPSENDPrompt = {.pucData = ">", .ulCheckLength = 1};

const ModemResponse_t rspServerCommand = {.pucData = "YYY", .ulCheckLength = 3};
