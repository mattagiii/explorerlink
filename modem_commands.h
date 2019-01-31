/*
 * modem_commands.h
 *
 *  Created on: Apr 30, 2018
 *      Author: Matt
 */

#ifndef MODEM_COMMANDS_H_
#define MODEM_COMMANDS_H_

#include <stdbool.h>
#include <stdint.h>


typedef struct {
    uint8_t *pucData;
} ModemCommand_t;

typedef struct {
    uint8_t *pucData;
    uint32_t ulCheckLength;
} ModemResponse_t;

/*
 * Common responses
 */
extern const ModemResponse_t rspOK;
extern const ModemResponse_t rspERROR;

/*
 * Commands, grouped with their responses
 */
extern const ModemCommand_t cmdAT;

extern const ModemCommand_t cmdATE0;
extern const ModemResponse_t rspATE0Echo;

extern const ModemCommand_t cmdATCCLK;
extern const ModemResponse_t rspATCCLK;

extern const ModemCommand_t cmdATCBC;
extern const ModemResponse_t rspATCBC;

extern const ModemCommand_t cmdATCSQ;
extern const ModemResponse_t rspATCSQ;

extern const ModemCommand_t cmdATCIPMODEQuery;
extern const ModemResponse_t rspATCIPMODECommandMode;
extern const ModemResponse_t rspATCIPMODEDataMode;

extern const ModemCommand_t cmdATCIPMODE0;
extern const ModemCommand_t cmdATCIPMODE1;

extern const ModemCommand_t cmdATNETOPENQuery;
extern const ModemResponse_t rspATNETOPENTrue;
extern const ModemResponse_t rspATNETOPENFalse;

extern const ModemCommand_t cmdATNETOPEN;
extern const ModemResponse_t rspATNETOPENSuccess;
extern const ModemResponse_t rspATNETOPENIPErr;

extern const ModemCommand_t cmdATNETCLOSE;
extern const ModemResponse_t rspATNETCLOSESuccess;

extern const ModemCommand_t cmdATCIPOPENQuery;
extern const ModemResponse_t rspATCIPOPENTrue;
extern const ModemResponse_t rspATCIPOPENFalse;
extern const ModemResponse_t rspATCIPOPENRest;

extern const ModemCommand_t cmdATCIPOPEN;
extern const ModemResponse_t rspATCIPOPENConnect;
extern const ModemResponse_t rspATCIPOPENSuccess;
extern const ModemResponse_t rspATCIPOPENFail;
extern const ModemResponse_t rspATCIPRcv;
extern const ModemResponse_t rspATCIPIPD;
extern const ModemResponse_t rspCLOSED;

extern const ModemCommand_t cmdPlus;

extern const ModemCommand_t cmdATO;

extern const ModemCommand_t cmdATCIPCLOSE;
extern const ModemResponse_t rspATCIPCLOSESuccess;

extern const ModemCommand_t cmdATCIPSEND;
extern const ModemResponse_t rspATCIPSENDPrompt;

extern const ModemResponse_t rspServerCommand;

#endif /* MODEM_COMMANDS_H_ */
