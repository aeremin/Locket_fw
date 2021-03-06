#include "board.h"
#include "led.h"
#include "vibro.h"
#include "beeper.h"
#include "Sequences.h"
#include "radio_lvl1.h"
#include "kl_i2c.h"
#include "kl_lib.h"
#include "pill.h"
#include "pill_mgr.h"
#include "MsgQ.h"
#include "main.h"
#include "SimpleSensors.h"
#include "buttons.h"

#include "health.h"
#include "ability.h"
#include "player_type.h"
#include "qhsm.h"
#include "Glue.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

// EEAddresses
#define EE_ADDR_DEVICE_ID   0

void InitSM();
void SendEvent_Health(int QSig, unsigned int Value);
void SendEvent_Ability(int QSig, unsigned int Value);
void SendEvent_PlayerType(int QSig, unsigned int Value);
static healthQEvt e;

int32_t ID;
static const PinInputSetup_t DipSwPin[DIP_SW_CNT] = { DIP_SW8, DIP_SW7, DIP_SW6, DIP_SW5, DIP_SW4, DIP_SW3, DIP_SW2, DIP_SW1 };
static uint8_t ISetID(int32_t NewID);
void ReadIDfromEE();

// ==== Periphery ====
Vibro_t VibroMotor {VIBRO_SETUP};
Beeper_t Beeper {BEEPER_PIN};

LedRGBwPower_t Led { LED_R_PIN, LED_G_PIN, LED_B_PIN, LED_EN_PIN };

// ==== Timers ====
static TmrKL_t TmrEverySecond {TIME_MS2I(1000), evtIdEverySecond, tktPeriodic};
uint32_t TimeS = 0;
//static TmrKL_t TmrRxTableCheck {MS2ST(2007), evtIdCheckRxTable, tktPeriodic};
static void CheckRxData();
#endif

int main(void) {
    // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V5);
    Clk.SetMSI4MHz();
    Clk.UpdateFreqValues();

    // === Init OS ===
    halInit(Clk.APB1FreqHz);
    chSysInit();
    EvtQMain.Init();

    // ==== Init hardware ====
    Uart.Init();
    ReadIDfromEE();
    Printf("\r%S %S; ID=%u\r", APP_NAME, XSTRINGIFY(BUILD_TIME), ID);
    Clk.PrintFreqs();

    Led.Init();
    VibroMotor.Init();
    Beeper.Init();
    Beeper.StartOrRestart(bsqBeepBeep);
    SimpleSensors::Init();

    i2c1.Init();
    PillMgr.Init();

    // ==== Time and timers ====
    TmrEverySecond.StartOrRestart();

    // ==== Radio ====
    if(Radio.Init() == retvOk) Led.StartOrRestart(lsqStart);
    else Led.StartOrRestart(lsqFailure);
    VibroMotor.StartOrRestart(vsqBrrBrr);
    chThdSleepMilliseconds(1008);

    InitSM();

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdEverySecond:
                PillMgr.Check();
                CheckRxData();
                SendEvent_Health(TIME_TICK_1S_SIG, 0);
                SendEvent_Ability(TIME_TICK_1S_SIG, 0);
                SendEvent_PlayerType(TIME_TICK_1S_SIG, 0);
                TimeS++;
                if((TimeS % 60) == 0) {
                    SendEvent_Health(TIME_TICK_1M_SIG, 0);
                    SendEvent_Ability(TIME_TICK_1M_SIG, 0);
                    SendEvent_PlayerType(TIME_TICK_1M_SIG, 0);
                }
                break;

            // ==== Radio ====
            case evtIdLustraDamagePkt:
                SendEvent_Health(DMG_RCVD_SIG , Msg.Value);
                SendEvent_PlayerType(DMG_RCVD_SIG , Msg.Value);
                break;

            case evtIdShineOrderHost:
                SendEvent_Ability(SHINE_ORDER_SIG, 0);
                break;

            case evtIdShinePktMutant:
                SendEvent_Health(SHINE_RCVD_SIG, 0);
                break;
            case evtIdButtons:
                Printf("Btn %u\r", Msg.BtnEvtInfo.BtnID[0]);
                if(Msg.BtnEvtInfo.Type == beShortPress) {
                    if(Msg.BtnEvtInfo.BtnID[0] == 1) { // Central, check mutant
                        SendEvent_Ability(CENTRAL_BUTTON_PRESSED_SIG, 0);
                    }
                    else {
                        SendEvent_Health(FIRST_BUTTON_PRESSED_SIG, 0);
                    }
                }
                else if(Msg.BtnEvtInfo.Type == beLongCombo and Msg.BtnEvtInfo.BtnCnt == 3) {
                    Printf("Combo\r");
                    SendEvent_Ability(SHINE_SIG, 0);
                }
                break;

            case evtIdPillConnected:
                Printf("Pill: %u\r", PillMgr.Pill.DWord32);
                switch(PillMgr.Pill.DWord32) {
                    case 0: SendEvent_PlayerType(PILL_RESET_SIG, 0); break;
                    case 1: SendEvent_Health(PILL_HEAL_SIG, 0); break;
                    case 2: SendEvent_PlayerType(PILL_TAILOR_SIG, 0); break;
                    case 3: SendEvent_PlayerType(PILL_STALKER_SIG, 0); break;
                    case 4: SendEvent_PlayerType(PILL_LOCAL_SIG, 0); break;
                    case 5: SendEvent_Ability(PILL_MUTANT_SIG, 0); break;
                    default: break;
                }
                break;

            case evtIdPillDisconnected:
                Printf("Pill disconn\r");
                break;

            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;
            default: Printf("Unhandled Msg %u\r", Msg.ID); break;
        } // Switch
    } // while true
} // ITask()

void CheckRxData() {
    for(int i=0; i<LUSTRA_CNT; i++) {
        if(Radio.RxData[i].ProcessAndCheck()) {
            EvtQMain.SendNowOrExit(EvtMsg_t(evtIdLustraDamagePkt, Radio.RxData[i].Damage));
        }
    }
}

BaseChunk_t vsqSMBrr[] = {
        {csSetup, VIBRO_VOLUME},
        {csWait, 99},
        {csSetup, 0},
        {csEnd}
};

LedRGBChunk_t lsqSM[] = {
        {csSetup, 0, clRed},
        {csWait, 207},
        {csSetup, 0, {0,1,0}},
        {csEnd},
};

LedRGBChunk_t lsqSMSmooth[] = {
        {csSetup, 150, clRed},
        {csWait, 200},
        {csSetup, 150, {0,1,0}},
		{csWait, 400},
		{csEnd},
};

LedRGBChunk_t lsqSMDeath[] = {
        {csSetup, 0, clRed},
        {csWait, 200},
        {csSetup, 0, {0,0,0}},
		{csWait, 600},
        {csEnd},
};


void FlashAgony() {
    Led.StartOrRestart(lsqSMSmooth);
}

void FlashDeath() {
    Led.StartOrRestart(lsqSMDeath);
}

void Flash(unsigned int R, unsigned int G, unsigned int B, unsigned int Timeout) {
    lsqSM[0].Color.FromRGB(R, G, B);
    lsqSM[1].Time_ms = Timeout;
    Led.StartOrRestart(lsqSM);
}

void Vibro(unsigned int Timeout) {
    vsqSMBrr[1].Time_ms = Timeout;
    VibroMotor.StartOrRestart(vsqSMBrr);
}

void SendShining() {
	Radio.RMsgQ.SendWaitingAbility(RMsg_t(R_MSG_SEND_KILL), 999);
    Flash(255, 255, 255, 0);
}

void SetDefaultColor(uint8_t R, uint8_t G, uint8_t B) {
    lsqSM[2].Color.FromRGB(R, G, B);
    Led.StartOrRestart(lsqSM);
}

#define THE_WORD    0xCA115EA1
void ClearPill() {
    uint32_t DWord32 = THE_WORD;
    if(PillMgr.Write(0, &DWord32, 4) != retvOk) Printf("ClearPill fail\r");
}

// ==== Saving ====
#define EE_ADDR_STATE       256
#define EE_ADDR_ABILITY     260
#define EE_ADDR_TYPE        264
#define EE_ADDR_HP          268
#define EE_ADDR_MAX_HP      272
#define EE_ADDR_DANGERTIME  276
#define EE_ADDR_CHARGETIME  280

void State_Save(unsigned int State) {
    if(EE::Write32(EE_ADDR_STATE, State) == retvOk) {
#if DBG_VERBOSE_SAVING
        Printf("Saved State: %u\r", State);
#endif
    }
    else Printf("Saving State fail\r");
}

void Ability_Save(unsigned int Ability) {
    if(EE::Write32(EE_ADDR_ABILITY, Ability) == retvOk) {
#if DBG_VERBOSE_SAVING
        Printf("Saved Ability %u\r", Ability);
#endif
    }
    else Printf("Saving Ability fail\r");
}

void PlayerType_Save(unsigned int Type) {
    if(EE::Write32(EE_ADDR_TYPE, Type) == retvOk) {
#if DBG_VERBOSE_SAVING
        Printf("Saved Type %u\r", Type);
#endif
    }
    else Printf("Saving Type fail\r");
}

void HP_Save(unsigned int HP) {
    if(EE::Write32(EE_ADDR_HP, HP) == retvOk) {
#if DBG_VERBOSE_SAVING
        Printf("Saved HP %u\r", HP);
#endif
    }
    else Printf("Saving HP fail\r");
}

void MaxHP_Save(unsigned int MaxHP) {
    if(EE::Write32(EE_ADDR_MAX_HP, MaxHP) == retvOk) {
#if DBG_VERBOSE_SAVING
        Printf("Saved MaxHP %u\r", MaxHP);
#endif
    }
    else Printf("Saving MaxHP fail\r");
}

void DangerTime_Save(unsigned int Time) {
    if(EE::Write32(EE_ADDR_DANGERTIME, Time) == retvOk) {
#if DBG_VERBOSE_SAVING
        Printf("Saved DangerTime %u\r", Time);
#endif
    }
    else Printf("Saving DangerTime fail\r");
}

void ChargeTime_Save(unsigned int Time) {
    if(EE::Write32(EE_ADDR_CHARGETIME, Time) == retvOk) {
#if DBG_VERBOSE_SAVING
        Printf("Saved ChargeTime %u\r", Time);
#endif
    }
    else Printf("Saving ChargeTime fail\r");
}


void InitSM() {
    // Load saved data
    uint32_t State = EE::Read32(EE_ADDR_STATE);
    uint32_t Ability = EE::Read32(EE_ADDR_ABILITY);
    uint32_t Type = EE::Read32(EE_ADDR_TYPE);
    uint32_t HP = EE::Read32(EE_ADDR_HP);
    uint32_t MaxHP = EE::Read32(EE_ADDR_MAX_HP);
    uint32_t DangerTime = EE::Read32(EE_ADDR_DANGERTIME);
    uint32_t ChargeTime = EE::Read32(EE_ADDR_CHARGETIME);
    Printf("Loaded: State=%d Ability=%d Type=%d HP=%d MaxHP=%d DangerTime=%d ChargeTime=%d\r",
            State, Ability, Type, HP, MaxHP, DangerTime, ChargeTime);
    // Init
    Health_ctor(State, HP, MaxHP, DangerTime);
    Ability_ctor(Ability, ChargeTime);
    Player_type_ctor(Type, (Health*)the_health);
    QMSM_INIT(the_health, (QEvt *)0);
    QMSM_INIT(the_ability, (QEvt *)0);
    QMSM_INIT(the_player_type, (QEvt *)0);
}

void SendEvent_Health(int QSig, unsigned int Value) {
    e.super.sig = QSig;
    e.value = Value;
    // Printf("evtHealth: %d; %d\r", e.super.sig, e.value);
    QMSM_DISPATCH(the_health, &(e.super));
}

void SendEvent_Ability(int QSig, unsigned int Value) {
    e.super.sig = QSig;
    e.value = Value;
    // Printf("evtAbility: %d; %d\r", e.super.sig, e.value);
    QMSM_DISPATCH(the_ability, &(e.super));
}

void SendEvent_PlayerType(int QSig, unsigned int Value) {
    e.super.sig = QSig;
    e.value = Value;
    // Printf("evtPlayerType: %d; %d\r", e.super.sig, e.value);
    QMSM_DISPATCH(the_player_type, &(e.super));
}

#if 1 // ================= Command processing ====================
void OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
    __attribute__((unused)) int32_t dw32 = 0;  // May be unused in some configurations
//    Uart.Printf("%S\r", PCmd->Name);
    // Handle command
    if(PCmd->NameIs("Ping")) {
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));


    else if(PCmd->NameIs("evt1min")) {
        SendEvent_Health(TIME_TICK_1M_SIG, 0);
        SendEvent_Ability(TIME_TICK_1M_SIG, 0);
        SendEvent_PlayerType(TIME_TICK_1M_SIG, 0);
    }

    else if(PCmd->NameIs("GetID")) PShell->Reply("ID", ID);

    else if(PCmd->NameIs("SetID")) {
        if(PCmd->GetNext<int32_t>(&ID) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint8_t r = ISetID(ID);
        RMsg_t msg;
        msg.Cmd = R_MSG_SET_CHNL;
        msg.Value = ID2RCHNL(ID);
        Radio.RMsgQ.SendNowOrExit(msg);
        PShell->Ack(r);
    }

    else if(PCmd->NameIs("Rst")) {
        State_Save(DEAD);
        Ability_Save(DISABLED);
        PlayerType_Save(DEAD);
        HP_Save(DefaultHP);
        MaxHP_Save(DefaultHP);
        DangerTime_Save(0);
        ChargeTime_Save(0);
        PShell->Ack(retvOk);
    }

#if 1 // === Pill ===
    else if(PCmd->NameIs("ReadPill")) {
        int32_t DWord32;
        uint8_t Rslt = PillMgr.Read(0, &DWord32, 4);
        if(Rslt == retvOk) {
            PShell->Print("Read %d\r\n", DWord32);
        }
        else PShell->Ack(retvFail);
    }

    else if(PCmd->NameIs("WritePill")) {
        int32_t DWord32;
        if(PCmd->GetNext<int32_t>(&DWord32) == retvOk) {
            uint8_t Rslt = PillMgr.Write(0, &DWord32, 4);
            PShell->Ack(Rslt);
        }
        else PShell->Ack(retvCmdError);
    }
#endif

    else PShell->Ack(retvCmdUnknown);
}
#endif


#if 1 // =========================== ID management =============================
void ReadIDfromEE() {
    ID = EE::Read32(EE_ADDR_DEVICE_ID);  // Read device ID
    if(ID < ID_MIN or ID > ID_MAX) {
        Printf("\rUsing default ID\r");
        ID = ID_DEFAULT;
    }
}

uint8_t ISetID(int32_t NewID) {
    if(NewID < ID_MIN or NewID > ID_MAX) return retvFail;
    uint8_t rslt = EE::Write32(EE_ADDR_DEVICE_ID, NewID);
    if(rslt == retvOk) {
        ID = NewID;
        Printf("New ID: %u\r", ID);
        return retvOk;
    }
    else {
        Printf("EE error: %u\r", rslt);
        return retvFail;
    }
}
#endif
