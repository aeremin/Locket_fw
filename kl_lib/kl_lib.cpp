/*
 * kl_lib_f0.cpp
 *
 *  Created on: 10.12.2012
 *      Author: kreyl
 */

#include "shell.h"
#include <stdarg.h>
#include <string.h>
#include "MsgQ.h"
#include <malloc.h>

#if 0 // ============================ General ==================================
// To replace standard error handler in case of virtual methods implementation
//extern "C" void __cxa_pure_virtual() {
//    Uart.PrintfNow("pure_virtual\r");
//}

// Amount of memory occupied by thread
uint32_t GetThdFreeStack(void *wsp, uint32_t size) {
    uint32_t n = 0;
    uint32_t RequestedSize = size - (sizeof(thread_t) +
            (size_t)PORT_GUARD_PAGE_SIZE +
            sizeof (struct port_intctx) +
            sizeof (struct port_extctx) +
            (size_t)PORT_INT_REQUIRED_STACK);
#if CH_DBG_FILL_THREADS
    uint8_t *startp = (uint8_t *)wsp;
    uint8_t *endp = (uint8_t *)wsp + RequestedSize;
    while (startp < endp)
        if(*startp++ == CH_DBG_STACK_FILL_VALUE) ++n;
#endif
    return n;
}

void PrintThdFreeStack(void *wsp, uint32_t size) {
    uint32_t RequestedSize = size - (sizeof(thread_t) +
            (size_t)PORT_GUARD_PAGE_SIZE +
            sizeof (struct port_intctx) +
            sizeof (struct port_extctx) +
            (size_t)PORT_INT_REQUIRED_STACK);

    Printf("Free stack memory: %u of %u bytes\r",
            GetThdFreeStack(wsp, size), RequestedSize);
}

#endif

/********************************************
arena;     total space allocated from system
ordblks;   number of non-inuse chunks
hblks;     number of mmapped regions
hblkhd;    total space in mmapped regions
uordblks;  total allocated space
fordblks;  total non-inuse space
keepcost;  top-most, releasable (via malloc_trim) space
**********************************************/
void PrintMemoryInfo() {
    struct mallinfo info = mallinfo();
    Printf(
            "total space allocated from system: %u\r"
            "number of non-inuse chunks: %u\r"
            "number of mmapped regions: %u\r"
            "total space in mmapped regions: %u\r"
            "total allocated space: %u\r"
            "total non-inuse space: %u\r"
            "top-most, releasable: %u\r",
            info.arena, info.ordblks, info.hblks, info.hblkhd,
            info.uordblks, info.fordblks, info.keepcost);
}

#if 1 // ============================ kl_string ================================
__attribute__ ((always_inline))
inline int kl_tolower(char c) {
    return (c >= 'A' and c <= 'Z')? (c + ('a' - 'A')) : c;
}

/* Compare S1 and S2, ignoring case, returning less than, equal to or
   greater than zero if S1 is lexicographically less than,
   equal to or greater than S2.  */
int kl_strcasecmp(const char *s1, const char *s2) {
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  int result;
  if (p1 == p2) return 0;
  while((result = kl_tolower(*p1) - kl_tolower(*p2++)) == 0) {
      if(*p1++ == '\0') break;
  }
  return result;
}
#endif

void Timer_t::Init() const {
    if(ITmr == TIM2)  { rccEnableTIM2(FALSE); }
    else if(ITmr == TIM3)  { rccEnableTIM3(FALSE); }
    else if(ITmr == TIM4)  { rccEnableTIM4(FALSE); }
    else if(ITmr == TIM6)  { rccEnableTIM6(FALSE); }
    else if(ITmr == TIM7)  { rccEnableTIM7(FALSE); }
    else if(ITmr == TIM9)  { rccEnableTIM9(FALSE); }
    else if(ITmr == TIM10)  { rccEnableTIM10(FALSE); }
    else if(ITmr == TIM11)  { rccEnableTIM11(FALSE); }
}

void Timer_t::Deinit() const {
    TMR_DISABLE(ITmr);
    if(ITmr == TIM2)  { rccDisableTIM2(); }
    else if(ITmr == TIM3)  { rccDisableTIM3(); }
    else if(ITmr == TIM4)  { rccDisableTIM4(); }
    else if(ITmr == TIM6)  { rccDisableTIM6(); }
    else if(ITmr == TIM7)  { rccDisableTIM7(); }
    else if(ITmr == TIM9)  { rccDisableTIM9(); }
    else if(ITmr == TIM10)  { rccDisableTIM10(); }
    else if(ITmr == TIM11)  { rccDisableTIM11(); }
}

void Timer_t::SetupPrescaler(uint32_t PrescaledFreqHz) const {
    ITmr->PSC = (Clk.GetTimInputFreq(ITmr) / PrescaledFreqHz) - 1;
}

void PinOutputPWM_t::Init() const {
    Timer_t::Init();

    ITmr->ARR = ISetup.TopValue;
    // Setup Output
    uint16_t tmp = (ISetup.Inverted == invInverted)? 0b111 : 0b110; // PWM mode 1 or 2
    switch(ISetup.TimerChnl) {
        case 1:
            ITmr->CCMR1 |= (tmp << 4);
            ITmr->CCER  |= TIM_CCER_CC1E;
            break;
        case 2:
            ITmr->CCMR1 |= (tmp << 12);
            ITmr->CCER  |= TIM_CCER_CC2E;
            break;
        case 3:
            ITmr->CCMR2 |= (tmp << 4);
            ITmr->CCER  |= TIM_CCER_CC3E;
            break;
        case 4:
            ITmr->CCMR2 |= (tmp << 12);
            ITmr->CCER  |= TIM_CCER_CC4E;
            break;
        default: break;
    }
    Enable();
    // GPIO
    AlterFunc_t AF = AF1; // For TIM2
    if(ANY_OF_2(ITmr, TIM3, TIM4)) AF = AF2;
    else if(ANY_OF_3(ITmr, TIM9, TIM10, TIM11)) AF = AF3;
    PinSetupAlterFunc(ISetup.PGpio, ISetup.Pin, ISetup.OutputType, pudNone, AF);
}

void Timer_t::SetUpdateFrequencyChangingPrescaler(uint32_t FreqHz) const {
    // Figure out input timer freq
    uint32_t UpdFreqMax = Clk.GetTimInputFreq(ITmr) / (ITmr->ARR + 1);
    uint32_t div = UpdFreqMax / FreqHz;
    if(div != 0) div--;
    ITmr->PSC = div;
    ITmr->CNT = 0;  // Reset counter to start from scratch
}

void Timer_t::SetUpdateFrequencyChangingTopValue(uint32_t FreqHz) const {
    uint32_t TopVal  = (Clk.GetTimInputFreq(ITmr) / FreqHz) - 1;
    SetTopValue(TopVal);
}

// Universal VirtualTimer callback
void TmrKLCallback(void *p) {
    chSysLockFromISR();
    ((IrqHandler_t*)p)->IIrqHandler();
    chSysUnlockFromISR();
}

void TmrKL_t::IIrqHandler() {    // Call it inside callback
    EvtMsg_t Msg(EvtId);
    EvtQMain.SendNowOrExitI(Msg);
    if(TmrType == tktPeriodic) StartI();
}

#if 1 // ============================= DEBUG ===================================
extern "C" {

void chDbgPanic(const char *msg1) {
    Printf("\r%S\r", msg1);
}

void PrintErrMsg(const char* S) {
    USART1->CR3 &= ~USART_CR3_DMAT;
    while(*S != 0) {
        while(!(USART1->SR & USART_SR_TXE));
        USART1->DR = *S;
        S++;
    }
}

void HardFault_Handler(void) {
    PrintErrMsg("\rHardFault\r");
    __ASM volatile("BKPT #01");
    while(true);
}

} // extern C
#endif

#if 1 // ================= FLASH & EEPROM ====================
#define FLASH_EraseTimeout      TIME_MS2I(45)
#define FLASH_ProgramTimeout    TIME_MS2I(45)
namespace Flash {

// ==== Common ====
void ClearPendingFlags() {
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGAERR | FLASH_SR_WRPERR;
}

#if defined STM32L4XX
void ClearErrFlags() {
    FLASH->SR |= FLASH_SR_OPTVERR | FLASH_SR_RDERR | FLASH_SR_FASTERR |
            FLASH_SR_MISERR | FLASH_SR_PGSERR | FLASH_SR_SIZERR |
            FLASH_SR_PGAERR | FLASH_SR_WRPERR | FLASH_SR_PROGERR | FLASH_SR_OPERR;
}

// Wait for a Flash operation to complete or a TIMEOUT to occur
static uint8_t WaitForLastOperation(systime_t Timeout_st) {
    systime_t start = chVTGetSystemTimeX();
    while(FLASH->SR & FLASH_SR_BSY) {
        if(Timeout_st != TIME_INFINITE) {
            if(chVTTimeElapsedSinceX(start) >= Timeout_st) return retvTimeout;
        }
    }
    if((FLASH->SR & FLASH_SR_OPERR) or (FLASH->SR & FLASH_SR_PROGERR) or
            (FLASH->SR & FLASH_SR_WRPERR) or (FLASH->SR & FLASH_SR_PGAERR) or
            (FLASH->SR & FLASH_SR_SIZERR) or (FLASH->SR & FLASH_SR_PGSERR) or
            (FLASH->SR & FLASH_SR_MISERR) or (FLASH->SR & FLASH_SR_FASTERR) or
            (FLASH->SR & FLASH_SR_RDERR) or (FLASH->SR & FLASH_SR_OPTVERR)) {
        return retvFail;
    }
    // Clear EOP if set
    if(FLASH->SR & FLASH_SR_EOP) FLASH->SR |= FLASH_SR_EOP;
    return retvOk;
}
#else
static uint8_t GetStatus(void) {
    if(FLASH->SR & FLASH_SR_BSY) return retvBusy;
#if defined STM32L1XX
    else if(FLASH->SR & FLASH_SR_WRPERR) return retvWriteProtect;
    else if(FLASH->SR & (uint32_t)0x1E00) return retvFail;
#elif defined STM32F2XX

#else
    else if(FLASH->SR & FLASH_SR_PGERR) return retvFail;
    else if(FLASH->SR & FLASH_SR_WRPRTERR) return retvFail;
#endif
    else return retvOk;
}

static uint8_t WaitForLastOperation(systime_t Timeout_st) {
    uint8_t status = retvOk;
    // Wait for a Flash operation to complete or a TIMEOUT to occur
    do {
        status = GetStatus();
        Timeout_st--;
    } while((status == retvBusy) and (Timeout_st != 0x00));
    if(Timeout_st == 0x00) status = retvTimeout;
    return status;
}
#endif

// When properly executed, the unlocking sequence clears the PELOCK bit in the FLASH_PECR register
static void UnlockEEAndPECR() {
    if(FLASH->PECR & FLASH_PECR_PELOCK) {
        // Unlocking the Data memory and FLASH_PECR register access
        FLASH->PEKEYR = 0x89ABCDEF;
        FLASH->PEKEYR = 0x02030405;
        FLASH->SR = FLASH_SR_WRPERR;        // Clear WriteProtectErr
        FLASH->PECR &= ~FLASH_PECR_FTDW;    // Disable fixed time programming
    }
}
// To lock the FLASH_PECR and the data EEPROM again, the software only needs to set the PELOCK bit in FLASH_PECR
static void LockEEAndPECR() { FLASH->PECR |= FLASH_PECR_PELOCK; }

// ==== Flash ====
void UnlockFlash() {
    UnlockEEAndPECR();
    FLASH->PRGKEYR = 0x8C9DAEBF;
    FLASH->PRGKEYR = 0x13141516;
}
void LockFlash() {
    FLASH->PECR |= FLASH_PECR_PRGLOCK;
}

// Beware: use Page Address (0...255), not absolute address kind of 0x08003f00
uint8_t ErasePage(uint32_t PageAddress) {
    uint8_t status = WaitForLastOperation(FLASH_EraseTimeout);
    if(status == retvOk) {
        // PECR and Flash must be unlocked
        FLASH->PECR |= FLASH_PECR_ERASE;
        FLASH->PECR |= FLASH_PECR_PROG;
        // Write 0x0000 0000 to the first word of the page to erase
        *((volatile uint32_t*)PageAddress) = 0;
        status = WaitForLastOperation(FLASH_EraseTimeout);
        FLASH->PECR &= ~FLASH_PECR_PROG;
        FLASH->PECR &= ~FLASH_PECR_ERASE;
    }
    return status;
}

uint8_t ProgramWord(uint32_t Address, uint32_t Data) {
    uint8_t status = WaitForLastOperation(FLASH_ProgramTimeout);
    if(status == retvOk) {
        // PECR and Flash must be unlocked
        *((volatile uint32_t*)Address) = Data;
        status = WaitForLastOperation(FLASH_ProgramTimeout);
    }
    return status;
}

uint8_t ProgramBuf(void *PData, uint32_t ByteSz, uint32_t Addr) {
    uint8_t status = retvOk;
    uint32_t *p = (uint32_t*)PData;
    uint32_t DataWordCount = (ByteSz + 3) / 4;
    chSysLock();
    UnlockFlash();
    // Erase flash
    ClearPendingFlags();
    status = ErasePage(Addr);
//    Uart.PrintfI("  Flash erase %u: %u\r", status);
    if(status != retvOk) {
        PrintfI("Flash erase error\r");
        goto end;
    }
    // Program flash
    for(uint32_t i=0; i<DataWordCount; i++) {
        status = ProgramWord(Addr, *p);
        if(status != retvOk) {
            PrintfI("Flash write error\r");
            goto end;
        }
        Addr += 4;
        p++;
    }
    end:
    LockFlash();
    chSysUnlock();
    return status;
}

// ==== Option bytes ====
void UnlockOptionBytes() {
#ifdef STM32L4XX
    FLASH->OPTKEYR = 0x08192A3B;
    FLASH->OPTKEYR = 0x4C5D6E7F;
#elif defined STM32L1XX
    UnlockEEAndPECR();
    FLASH->OPTKEYR = 0xFBEAD9C8;
    FLASH->OPTKEYR = 0x24252627;
#elif defined STM32F2XX

#else
    UnlockFlash();
    FLASH->OPTKEYR = FLASH_OPTKEY1;
    FLASH->OPTKEYR = FLASH_OPTKEY2;
#endif
}
void LockOptionBytes() {
#ifdef STM32L4XX
    FLASH->CR |= FLASH_CR_OPTLOCK;
#elif defined STM32L1XX
    // To lock the option byte block again, the software only needs to set the OPTLOCK bit in FLASH_PECR
    FLASH->PECR |= FLASH_PECR_OPTLOCK;
#elif defined STM32F2XX

#else
    CLEAR_BIT(FLASH->CR, FLASH_CR_OPTWRE);
    LockFlash();
#endif
}

void WriteOptionByteRDP(uint8_t Value) {
    UnlockFlash();
    ClearPendingFlags();
    UnlockOptionBytes();
    if(WaitForLastOperation(FLASH_ProgramTimeout) == retvOk) {
        uint32_t OptBytes = *(volatile uint32_t*)0x1FF80000;
        OptBytes &= 0xFF00FF00; // Clear RDP and nRDP
        OptBytes |= Value;      // Write RDP
        OptBytes |= (Value ^ 0xFF) << 16; // Write nRDP;
        *(volatile uint32_t*)0x1FF80000 = OptBytes;
        WaitForLastOperation(FLASH_ProgramTimeout);
    }
    LockOptionBytes();
    LockFlash();
}

// ==== Firmare lock ====
bool FirmwareIsLocked() {
    return (FLASH->OBR & 0xFF) != 0xAA;
}

void LockFirmware() {
    chSysLock();
    WriteOptionByteRDP(0x1D); // Any value except 0xAA or 0xCC
    // Set the OBL_Launch bit to reset system and launch the option byte loading
    FLASH->PECR |= FLASH_PECR_OBL_LAUNCH;
    chSysUnlock();
}

}; // Namespace FLASH
#endif

#if defined STM32L1XX // =================== Internal EEPROM ===================
#define EEPROM_BASE_ADDR    ((uint32_t)0x08080000)
namespace EE {
uint32_t Read32(uint32_t Addr) {
    return *((uint32_t*)(Addr + EEPROM_BASE_ADDR));
}

uint8_t Write32(uint32_t Addr, uint32_t W) {
    Addr += EEPROM_BASE_ADDR;
//    Uart.Printf("EAdr=%u\r", Addr);
    Flash::UnlockEEAndPECR();
    // Wait for last operation to be completed
    uint8_t status = Flash::WaitForLastOperation(FLASH_ProgramTimeout);
    if(status == retvOk) {
        *(volatile uint32_t*)Addr = W;
        status = Flash::WaitForLastOperation(FLASH_ProgramTimeout);
    }
    Flash::LockEEAndPECR();
    return status;
}

void ReadBuf(void *PDst, uint32_t Sz, uint32_t Addr) {
    uint32_t *p32 = (uint32_t*)PDst;
    Sz = Sz / 4;  // Size in words32
    while(Sz--) {
        *p32 = Read32(Addr);
        p32++;
        Addr += 4;
    }
}

uint8_t WriteBuf(void *PSrc, uint32_t Sz, uint32_t Addr) {
    uint32_t *p32 = (uint32_t*)PSrc;
    Addr += EEPROM_BASE_ADDR;
    Sz = (Sz + 3) / 4;  // Size in words32
    Flash::UnlockEEAndPECR();
    // Wait for last operation to be completed
    uint8_t status = Flash::WaitForLastOperation(FLASH_ProgramTimeout);
    while((status == retvOk) and (Sz > 0))  {
        *(volatile uint32_t*)Addr = *p32;
        status = Flash::WaitForLastOperation(FLASH_ProgramTimeout);
        p32++;
        Addr += 4;
        Sz--;
    }
    Flash::LockEEAndPECR();
    return status;
}

};
#endif

#if 1 // =========================== External IRQ ==============================
// IRQ handlers
extern "C" {
extern void PrintfCNow(const char *format, ...);

#if INDIVIDUAL_EXTI_IRQ_REQUIRED
IrqHandler_t* ExtiIrqHandler[16];
#else
#if defined STM32L1XX || defined STM32F4XX || defined STM32F2XX || defined STM32L4XX || defined STM32F1XX
ftVoidVoid ExtiIrqHandler[5], ExtiIrqHandler_9_5, ExtiIrqHandler_15_10;
#elif defined STM32F030 || defined STM32F0
ftVoidVoid ExtiIrqHandler_0_1, ExtiIrqHandler_2_3, ExtiIrqHandler_4_15;
#endif
#endif // INDIVIDUAL_EXTI_IRQ_REQUIRED

#if defined STM32L1XX || defined STM32F2XX || defined STM32L4XX || defined STM32F1XX
// EXTI pending register
#if defined STM32L1XX || defined STM32F2XX || defined STM32F1XX
#define EXTI_PENDING_REG    EXTI->PR
#elif defined STM32L4XX
#define EXTI_PENDING_REG    EXTI->PR1
#endif

// EXTI 0
void Vector58() {
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
    ftVoidVoid handler = ExtiIrqHandler[0];
    if(handler != nullptr) handler();
    else PrintfC("Unhandled %S\r", __FUNCTION__);
    EXTI_PENDING_REG = 0x0001; // Clean IRQ flags
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}

// EXTI 1
void Vector5C() {
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
    ftVoidVoid handler = ExtiIrqHandler[1];
    if(handler != nullptr) handler();
    else PrintfC("Unhandled %S\r", __FUNCTION__);
    EXTI_PENDING_REG = 0x0002; // Clean IRQ flags
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}

// EXTI 2
void Vector60() {
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
    ftVoidVoid handler = ExtiIrqHandler[2];
    if(handler != nullptr) handler();
    else PrintfC("Unhandled %S\r", __FUNCTION__);
    EXTI_PENDING_REG = 0x0004; // Clean IRQ flags
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}

// EXTI 3
void Vector64() {
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
    ftVoidVoid handler = ExtiIrqHandler[3];
    if(handler != nullptr) handler();
    else PrintfC("Unhandled %S\r", __FUNCTION__);
    EXTI_PENDING_REG = 0x0008; // Clean IRQ flags
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}

// EXTI 4
void Vector68() {
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
    ftVoidVoid handler = ExtiIrqHandler[4];
    if(handler != nullptr) handler();
    else PrintfC("Unhandled %S\r", __FUNCTION__);
    EXTI_PENDING_REG = 0x0010; // Clean IRQ flags
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}

// EXTI 9_5
void Vector9C() {
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
    if(ExtiIrqHandler_9_5 != nullptr) ExtiIrqHandler_9_5();
    else PrintfC("Unhandled %S\r", __FUNCTION__);
    EXTI_PENDING_REG = 0x03E0; // Clean IRQ flags
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}

// EXTI 15_10
void VectorE0() {
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
    if(ExtiIrqHandler_15_10 != nullptr) ExtiIrqHandler_15_10();
    else PrintfC("Unhandled %S\r", __FUNCTION__);
    EXTI_PENDING_REG = 0xFC00; // Clean IRQ flags
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}
#endif
} // extern c
#endif

#if 1 // ============== Conversion operations ====================
namespace Convert {
void U16ToArrAsBE(uint8_t *PArr, uint16_t N) {
    uint8_t *p8 = (uint8_t*)&N;
    *PArr++ = *(p8 + 1);
    *PArr   = *p8;
}
void U32ToArrAsBE(uint8_t *PArr, uint32_t N) {
    uint8_t *p8 = (uint8_t*)&N;
    *PArr++ = *(p8 + 3);
    *PArr++ = *(p8 + 2);
    *PArr++ = *(p8 + 1);
    *PArr   = *p8;
}
uint16_t ArrToU16AsBE(uint8_t *PArr) {
    uint16_t N;
    uint8_t *p8 = (uint8_t*)&N;
    *p8++ = *(PArr + 1);
    *p8 = *PArr;
    return N;
}
uint32_t ArrToU32AsBE(uint8_t *PArr) {
    uint32_t N;
    uint8_t *p8 = (uint8_t*)&N;
    *p8++ = *(PArr + 3);
    *p8++ = *(PArr + 2);
    *p8++ = *(PArr + 1);
    *p8 = *PArr;
    return N;
}

uint8_t TryStrToUInt32(char* S, uint32_t *POutput) {
    if(*S == '\0') return retvEmpty;
    char *p;
    *POutput = strtoul(S, &p, 0);
    return (*p == 0)? retvOk : retvNotANumber;
}
uint8_t TryStrToInt32(char* S, int32_t *POutput) {
    if(*S == '\0') return retvEmpty;
    char *p;
    *POutput = strtol(S, &p, 0);
    return (*p == '\0')? retvOk : retvNotANumber;
}

uint16_t BuildUint16(uint8_t Lo, uint8_t Hi) {
    uint16_t r = Hi;
    r <<= 8;
    r |= Lo;
    return r;
}

uint32_t BuildUint32(uint8_t Lo, uint8_t MidLo, uint8_t MidHi, uint8_t Hi) {
    uint32_t r = Hi;
    r <<= 8;
    r |= MidHi;
    r <<= 8;
    r |= MidLo;
    r <<= 8;
    r |= Lo;
    return r;
}

// ==== Float ====
uint8_t TryStrToFloat(char* S, float *POutput) {
    if(*S == '\0') return retvEmpty;
    char *p;
    *POutput = strtof(S, &p);
    return (*p == '\0')? retvOk : retvNotANumber;
}
}; // namespace
#endif

#if 1 // ============================== Clocking ===============================
Clk_t Clk;
#define CLK_STARTUP_TIMEOUT     9999

// ==== Inner use ====
uint8_t Clk_t::EnableHSE() {
    RCC->CR |= RCC_CR_HSEON;    // Enable HSE
    // Wait until ready, 1ms typical according to datasheet
    uint32_t StartUpCounter=0;
    do {
        if(RCC->CR & RCC_CR_HSERDY) return retvOk;   // HSE is ready
        StartUpCounter++;
    } while(StartUpCounter < 45000);
    RCC->CR &= ~RCC_CR_HSEON;   // Disable HSE
    return retvTimeout;
}

uint8_t Clk_t::EnableHSI() {
    RCC->CR |= RCC_CR_HSION;
    // Wait until ready
    uint32_t StartUpCounter=0;
    do {
        if(RCC->CR & RCC_CR_HSIRDY) return 0;   // HSI is ready
        StartUpCounter++;
    } while(StartUpCounter < CLK_STARTUP_TIMEOUT);
    return 1; // Timeout
}

uint8_t Clk_t::EnablePLL() {
    RCC->CR |= RCC_CR_PLLON;
    // Wait until ready
    uint32_t StartUpCounter=0;
    do {
        if(RCC->CR & RCC_CR_PLLRDY) return 0;   // PLL is ready
        StartUpCounter++;
    } while(StartUpCounter < CLK_STARTUP_TIMEOUT);
    return 1; // Timeout
}

uint8_t Clk_t::EnableMSI() {
    RCC->CR |= RCC_CR_MSION;
    // Wait until ready
    uint32_t StartUpCounter=0;
    do {
        if(RCC->CR & RCC_CR_MSIRDY) return 0;   // MSI is ready
        StartUpCounter++;
    } while(StartUpCounter < CLK_STARTUP_TIMEOUT);
    return 1; // Timeout
}

void Clk_t::UpdateFreqValues() {
    uint32_t tmp, PllMul, PllDiv;
    uint32_t SysClkHz;
    // Tables
    const uint32_t MSIClk[8] = {65536, 131072, 262144, 524188, 1048000, 2097000, 4194000};
    const uint8_t PllMulTable[9] = {3, 4, 6, 8, 12, 16, 24, 32, 48};
    const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
    const uint8_t APBPrescTable[8] = {0, 0, 0, 0, 1, 2, 3, 4};

    // Figure out SysClk
    tmp = RCC->CFGR & RCC_CFGR_SWS;
    tmp >>= 2;
    switch(tmp) {
        case 0b00: // MSI
            tmp = (RCC->ICSCR & RCC_ICSCR_MSIRANGE) >> 13;
            SysClkHz = MSIClk[tmp];
            break;

        case 0b01: // HSI
            SysClkHz = HSI_FREQ_HZ;
            break;

        case 0b10: // HSE
            SysClkHz = CRYSTAL_FREQ_HZ;
            break;

        case 0b11: // PLL used as system clock source
            // Get different PLL dividers
            tmp = (RCC->CFGR & RCC_CFGR_PLLMUL) >> 18;
            PllMul = PllMulTable[tmp];
            PllDiv = ((RCC->CFGR & RCC_CFGR_PLLDIV) >> 22) +1;
            // Which src is used as pll input?
            SysClkHz = ((RCC->CFGR & RCC_CFGR_PLLSRC) == RCC_CFGR_PLLSRC_HSI)? HSI_FREQ_HZ : CRYSTAL_FREQ_HZ;
            SysClkHz = (SysClkHz * PllMul) / PllDiv;
            break;
    } // switch

    // AHB freq
    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];
    AHBFreqHz = SysClkHz >> tmp;
    // APB freq
    uint32_t APB1prs = (RCC->CFGR & RCC_CFGR_PPRE1) >> 8;
    uint32_t APB2prs = (RCC->CFGR & RCC_CFGR_PPRE2) >> 11;
    tmp = APBPrescTable[APB1prs];
    APB1FreqHz = AHBFreqHz >> tmp;
    tmp = APBPrescTable[APB2prs];
    APB2FreqHz = AHBFreqHz >> tmp;
}

uint32_t Clk_t::GetTimInputFreq(TIM_TypeDef* ITmr) {
    uint32_t InputFreq = 0;
    // APB2
    if(ANY_OF_3(ITmr, TIM9, TIM10, TIM11)) {
        uint32_t APB2prs = (RCC->CFGR & RCC_CFGR_PPRE2) >> 11;
        if(APB2prs < 0b100) InputFreq = Clk.APB2FreqHz; // APB2CLK = HCLK / 1
        else InputFreq = Clk.APB2FreqHz * 2;           // APB2CLK = HCLK / (not 1)
    }
    // APB1
    else {
        uint32_t APB1prs = (RCC->CFGR & RCC_CFGR_PPRE1) >> 8;
        if(APB1prs < 0b100) InputFreq = Clk.APB1FreqHz; // APB1CLK = HCLK / 1
        else  InputFreq = Clk.APB1FreqHz * 2;           // APB1CLK = HCLK / (not 1)
    }
    return InputFreq;
}

// ==== Common use ====
// AHB, APB
void Clk_t::SetupBusDividers(AHBDiv_t AHBDiv, APBDiv_t APB1Div, APBDiv_t APB2Div) {
    // Setup dividers
    uint32_t tmp = RCC->CFGR;
    tmp &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);  // Clear bits
    tmp |= ((uint32_t)AHBDiv)  << 4;
    tmp |= ((uint32_t)APB1Div) << 8;
    tmp |= ((uint32_t)APB2Div) << 11;
    RCC->CFGR = tmp;
}

// Enables HSI, switches to HSI
uint8_t Clk_t::SwitchToHSI() {
    if(EnableHSI() != 0) return 1;
    uint32_t tmp = RCC->CFGR;
    tmp &= ~RCC_CFGR_SW;
    tmp |=  RCC_CFGR_SW_HSI;  // Select HSI as system clock src
    RCC->CFGR = tmp;
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI); // Wait till ready
    return 0;
}

// Enables HSE, switches to HSE
uint8_t Clk_t::SwitchToHSE() {
    // Try to enable HSE several times
    for(uint32_t i=0; i<11; i++) {
        if(EnableHSE() == retvOk) {
            uint32_t tmp = RCC->CFGR;
            tmp &= ~RCC_CFGR_SW;
            tmp |=  RCC_CFGR_SW_HSE;  // Select HSE as system clock src
            RCC->CFGR = tmp;
            while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE); // Wait till ready
            return retvOk;
        }
        else {
            DisableHSE();
            for(volatile uint32_t i=0; i<999; i++);
        }
    } // for
    return retvFail;
}

// Enables HSE, enables PLL, switches to PLL
uint8_t Clk_t::SwitchToPLL() {
    if(EnableHSE() != 0) return 1;
    if(EnablePLL() != 0) return 2;
    // Select PLL as system clock src
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    // Wait until ready
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
    return retvOk;
}

// Enables MSI, switches to MSI
uint8_t Clk_t::SwitchToMSI() {
    if(EnableMSI() != 0) return 1;
    uint32_t tmp = RCC->CFGR;
    tmp &= ~RCC_CFGR_SW;
    tmp |=  RCC_CFGR_SW_MSI;      // Select MSI as system clock src
    RCC->CFGR = tmp;
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_MSI); // Wait until ready
    return retvOk;
}

// Disable PLL first!
// HsePreDiv: 1...16; PllMul: pllMul[]
uint8_t Clk_t::SetupPLLDividers(PllMul_t PllMul, PllDiv_t PllDiv) {
    if(RCC->CR & RCC_CR_PLLON) return 1;    // PLL must be disabled to change dividers
    uint32_t tmp = RCC->CFGR;
    tmp &= RCC_CFGR_PLLDIV | RCC_CFGR_PLLMUL;
    tmp |= ((uint32_t)PllDiv) << 22;
    tmp |= ((uint32_t)PllMul) << 18;
    tmp |= RCC_CFGR_PLLSRC_HSE;
    RCC->CFGR = tmp;
    return 0;
}

void Clk_t::SetupFlashLatency(uint8_t AHBClk_MHz) {
    // Get VCore
    uint32_t tmp = PWR->CR;
    tmp &= PWR_CR_VOS;
    tmp >>= 11;
    VCore_t VCore = (VCore_t)tmp;
    if(     ((VCore == vcore1V2) and (AHBClk_MHz > 2)) or
            ((VCore == vcore1V5) and (AHBClk_MHz > 8)) or
            ((VCore == vcore1V8) and (AHBClk_MHz > 16))
            ) {
        FLASH->ACR |= FLASH_ACR_LATENCY;
    }
    else FLASH->ACR &= ~FLASH_ACR_LATENCY;
}

void Clk_t::PrintFreqs() {
    Printf("AHBFreq=%uMHz; APB1Freq=%uMHz; APB2Freq=%uMHz\r",
            Clk.AHBFreqHz/1000000, Clk.APB1FreqHz/1000000, Clk.APB2FreqHz/1000000);
}

// ==== V Core ====
void SetupVCore(VCore_t AVCore) {
    // PWR clock enable
    RCC->APB1ENR = RCC_APB1ENR_PWREN;
    // Core voltage setup
    while((PWR->CSR & PWR_CSR_VOSF) != 0); // Wait until regulator is stable
    uint32_t tmp = PWR->CR;
    tmp &= ~PWR_CR_VOS;
    tmp |= ((uint32_t)AVCore) << 11;
    PWR->CR = tmp;
    while((PWR->CSR & PWR_CSR_VOSF) != 0); // Wait until regulator is stable
}
#endif // Clocking

#if 1 // ================================= SPI =================================
void Spi_t::Setup(BitOrder_t BitOrder, CPOL_t CPOL, CPHA_t CPHA,
        int32_t Bitrate_Hz, BitNumber_t BitNumber) const {
    // Clocking
    if      (PSpi == SPI1) { rccEnableSPI1(FALSE); }
    else if (PSpi == SPI2) { rccEnableSPI2(FALSE); }
    // Mode: Master, NSS software controlled and is 1, 8bit, NoCRC, FullDuplex
    PSpi->CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR;
    if(BitOrder == boLSB) PSpi->CR1 |= SPI_CR1_LSBFIRST;    // MSB/LSB
    if(CPOL == cpolIdleHigh) PSpi->CR1 |= SPI_CR1_CPOL;     // CPOL
    if(CPHA == cphaSecondEdge) PSpi->CR1 |= SPI_CR1_CPHA;   // CPHA
    // Baudrate
    int32_t div;
    if(PSpi == SPI1) div = Clk.APB2FreqHz / Bitrate_Hz;
    else div = Clk.APB1FreqHz / Bitrate_Hz;
    SpiClkDivider_t ClkDiv = sclkDiv2;
    if     (div >= 128) ClkDiv = sclkDiv256;
    else if(div >= 64) ClkDiv = sclkDiv128;
    else if(div >= 32) ClkDiv = sclkDiv64;
    else if(div >= 16) ClkDiv = sclkDiv32;
    else if(div >= 8)  ClkDiv = sclkDiv16;
    else if(div >= 4)  ClkDiv = sclkDiv8;
    else if(div >= 2)  ClkDiv = sclkDiv4;
    PSpi->CR1 |= ((uint16_t)ClkDiv) << 3;
    // Bit number
    if(BitNumber == bitn16) PSpi->CR1 |= SPI_CR1_DFF;
    PSpi->CR2 = 0;
}

// IRQs
static ftVoidVoid Spi1RxIrqHandler = nullptr;
static ftVoidVoid Spi2RxIrqHandler = nullptr;

void Spi_t::SetupRxIrqCallback(ftVoidVoid AIrqHandler) const {
    if(PSpi == SPI1) Spi1RxIrqHandler = AIrqHandler;
    else if(PSpi == SPI2) Spi2RxIrqHandler = AIrqHandler;
}

extern "C" {
void VectorCC() {   // SPI1
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
    uint32_t SR = SPI1->SR;
    if(SR & SPI_SR_RXNE and Spi1RxIrqHandler) Spi1RxIrqHandler();
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}


void VectorD0() {   // SPI2
    CH_IRQ_PROLOGUE();
    chSysLockFromISR();
    uint32_t SR = SPI2->SR;
    if(SR & SPI_SR_RXNE and Spi2RxIrqHandler) Spi2RxIrqHandler();
    chSysUnlockFromISR();
    CH_IRQ_EPILOGUE();
}

} // extern C

#endif
