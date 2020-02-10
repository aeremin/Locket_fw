#include "uart.h"
#include "kl_i2c.h"

#if defined STM32L1XX || defined STM32F2XX || defined STM32F4XX
#define I2C_DMATX_MODE(Chnl) \
                        STM32_DMA_CR_CHSEL(Chnl) |   \
                        DMA_PRIORITY_LOW | \
                        STM32_DMA_CR_MSIZE_BYTE | \
                        STM32_DMA_CR_PSIZE_BYTE | \
                        STM32_DMA_CR_MINC |     /* Memory pointer increase */ \
                        STM32_DMA_CR_DIR_M2P    /* Direction is memory to peripheral */ \
                        | STM32_DMA_CR_TCIE

#define I2C_DMARX_MODE(Chnl) \
                        STM32_DMA_CR_CHSEL(Chnl) |   \
                        DMA_PRIORITY_LOW | \
                        STM32_DMA_CR_MSIZE_BYTE | \
                        STM32_DMA_CR_PSIZE_BYTE | \
                        STM32_DMA_CR_MINC |         /* Memory pointer increase */ \
                        STM32_DMA_CR_DIR_P2M        /* Direction is peripheral to memory */ \
                        | STM32_DMA_CR_TCIE

#if defined STM32F2XX
#define I2C1_DMA_CHNL   1
#define I2C2_DMA_CHNL   7
#define I2C3_DMA_CHNL   3
#endif

static const i2cParams_t I2C1Params = {
        I2C1,
        I2C1_GPIO, I2C1_SCL, I2C1_SDA,
        I2C1_BAUDRATE,
        I2C1_DMA_TX,
        I2C1_DMA_RX,
        I2C_DMATX_MODE(I2C1_DMA_CHNL),
        I2C_DMARX_MODE(I2C1_DMA_CHNL)
};
i2c_t i2c1 {&I2C1Params};

#if I2C2_ENABLED
static const i2cParams_t I2C2Params = {
        I2C2,
        I2C2_GPIO, I2C2_SCL, I2C2_SDA,
        I2C2_BAUDRATE,
        I2C2_DMA_TX,
        I2C2_DMA_RX,
        (I2C_DMATX_MODE(I2C2_DMA_CHNL)),
        (I2C_DMARX_MODE(I2C2_DMA_CHNL))
};
i2c_t i2c2 {&I2C2Params};
#endif

extern "C"
void i2cDmaIrqHandler(void *p, uint32_t flags) {
    chSysLockFromISR();
    i2c_t *pi2c = (i2c_t*)p;
    chThdResumeI(&pi2c->ThdRef, (msg_t)0);
    chSysUnlockFromISR();
}

void i2c_t::Init() {
    Standby();
    Resume();
    I2C_TypeDef *pi2c = PParams->pi2c;  // To make things shorter
    // ==== DMA ====
    PDmaTx = dmaStreamAlloc(PParams->DmaTxID, IRQ_PRIO_MEDIUM, i2cDmaIrqHandler, this);
    PDmaRx = dmaStreamAlloc(PParams->DmaRxID, IRQ_PRIO_MEDIUM, i2cDmaIrqHandler, this);
    dmaStreamSetPeripheral(PDmaTx, &pi2c->DR);
    dmaStreamSetPeripheral(PDmaRx, &pi2c->DR);
}

void i2c_t::Standby() {
    if(PParams->pi2c == I2C1) { rccResetI2C1(); rccDisableI2C1(); }
    else if(PParams->pi2c == I2C2) { rccResetI2C2(); rccDisableI2C2(); }
    // Disable GPIOs
    PinSetupAnalog(PParams->PGpio, PParams->SclPin);
    PinSetupAnalog(PParams->PGpio, PParams->SdaPin);
}

void i2c_t::Resume() {
    Error = false;
    // ==== GPIOs ====
    AlterFunc_t PinAF;
#if defined STM32L1XX || defined STM32F2XX || defined STM32F4XX
    PinAF = AF4; // for all I2Cs everywhere
#else
#error "I2C AF not defined"
#endif

    PinSetupAlterFunc(PParams->PGpio, PParams->SclPin, omOpenDrain, pudNone, PinAF);
    PinSetupAlterFunc(PParams->PGpio, PParams->SdaPin, omOpenDrain, pudNone, PinAF);
    // ==== Clock and reset ====
    if(PParams->pi2c == I2C1) { rccEnableI2C1(FALSE); rccResetI2C1(); }
#ifdef I2C2
    else if (PParams->pi2c == I2C2) { rccEnableI2C2(FALSE); rccResetI2C2(); }
#endif
#ifdef I2C3
    else if (PParams->pi2c == I2C3) { rccEnableI2C3(FALSE); rccResetI2C3(); }
#endif

    // Minimum clock is 2 MHz
    uint32_t ClkMhz = Clk.APB1FreqHz / 1000000;
    uint16_t tmpreg = PParams->pi2c->CR2;
    tmpreg &= (uint16_t)~I2C_CR2_FREQ;
    if(ClkMhz < 2)  ClkMhz = 2;
    if(ClkMhz > 32) ClkMhz = 32;
    tmpreg |= ClkMhz;
    PParams->pi2c->CR2 = tmpreg;
    PParams->pi2c->CR1 &= (uint16_t)~I2C_CR1_PE; // Disable i2c to setup TRise & CCR
    PParams->pi2c->TRISE = (uint16_t)(((ClkMhz * 300) / 1000) + 1);
    // 16/9
    tmpreg = (uint16_t)(Clk.APB1FreqHz / (PParams->BitrateHz * 25));
    if(tmpreg == 0) tmpreg = 1; // minimum allowed value
    tmpreg |= I2C_CCR_FS | I2C_CCR_DUTY;
    PParams->pi2c->CCR = tmpreg;
    PParams->pi2c->CR1 |= I2C_CR1_PE;    // Enable i2c back
    // ==== DMA ====
    PParams->pi2c->CR2 |= I2C_CR2_DMAEN;
}

void i2c_t::IReset() {
    Standby();
    Resume();
}

uint8_t i2c_t::WriteRead(uint8_t Addr,
        uint8_t *WPtr, uint8_t WLength,
        uint8_t *RPtr, uint8_t RLength) {
    uint8_t Rslt = retvOk;
    if(IBusyWait() != retvOk) { Rslt = retvBusy; goto WriteReadEnd; }
    // Clear flags
    PParams->pi2c->SR1 = 0;
    while(RxIsNotEmpty()) (void)PParams->pi2c->DR;   // Read DR until it empty
    ClearAddrFlag();
    // Start transmission
    SendStart();
    if(WaitEv5() != retvOk) { Rslt = retvFail; goto WriteReadEnd; }
    SendAddrWithWrite(Addr);
    if(WaitEv6() != retvOk) { SendStop(); Rslt = retvFail; goto WriteReadEnd; }
    ClearAddrFlag();
    // Start TX DMA if needed
    if(WLength != 0) {
        if(WaitEv8() != retvOk) { Rslt = retvFail; goto WriteReadEnd; }
        dmaStreamSetMemory0(PDmaTx, WPtr);
        dmaStreamSetMode   (PDmaTx, PParams->DmaModeTx);
        dmaStreamSetTransactionSize(PDmaTx, WLength);
        chSysLock();
        dmaStreamEnable(PDmaTx);
        chThdSuspendS(&ThdRef);    // Wait IRQ
        chSysUnlock();
        dmaStreamDisable(PDmaTx);
    }
    // Read if needed
    if(RLength != 0) {
        if(WaitEv8() != retvOk) { Rslt = retvFail; goto WriteReadEnd; }
        // Send repeated start
        SendStart();
        if(WaitEv5() != retvOk) { Rslt = retvFail; goto WriteReadEnd; }
        SendAddrWithRead(Addr);
        if(WaitEv6() != retvOk) { SendStop(); Rslt = retvFail; goto WriteReadEnd; }
        // If single byte is to be received, disable ACK before clearing ADDR flag
        if(RLength == 1) AckDisable();
        else AckEnable();
        ClearAddrFlag();
        dmaStreamSetMemory0(PDmaRx, RPtr);
        dmaStreamSetMode   (PDmaRx, PParams->DmaModeRx);
        dmaStreamSetTransactionSize(PDmaRx, RLength);
        SignalLastDmaTransfer(); // Inform DMA that this is last transfer => do not ACK last byte
        chSysLock();
        dmaStreamEnable(PDmaRx);
        chThdSuspendS(&ThdRef);    // Wait IRQ
        chSysUnlock();
        dmaStreamDisable(PDmaRx);
    } // if != 0
    else WaitBTF(); // if nothing to read, just stop
    SendStop();
    WriteReadEnd:
    return Rslt;
}

uint8_t i2c_t::WriteWrite(uint8_t Addr,
        uint8_t *WPtr1, uint8_t WLength1,
        uint8_t *WPtr2, uint8_t WLength2) {
    uint8_t Rslt = retvOk;
    if(IBusyWait() != retvOk) { Rslt = retvBusy; goto WriteWriteEnd; }
    // Clear flags
    PParams->pi2c->SR1 = 0;
    while(RxIsNotEmpty()) (void)PParams->pi2c->DR;   // Read DR until it empty
    ClearAddrFlag();
    // Start transmission
    SendStart();
    if(WaitEv5() != retvOk) { Rslt = retvFail; goto WriteWriteEnd; }
    SendAddrWithWrite(Addr);
    if(WaitEv6() != retvOk) { SendStop(); Rslt = retvFail; goto WriteWriteEnd; }
    ClearAddrFlag();
    // Start TX DMA if needed
    if(WLength1 != 0) {
        if(WaitEv8() != retvOk) { Rslt = retvFail; goto WriteWriteEnd; }
        dmaStreamSetMemory0(PDmaTx, WPtr1);
        dmaStreamSetMode   (PDmaTx, PParams->DmaModeTx);
        dmaStreamSetTransactionSize(PDmaTx, WLength1);
        chSysLock();
        dmaStreamEnable(PDmaTx);
        chThdSuspendS(&ThdRef);    // Wait IRQ
        chSysUnlock();
        dmaStreamDisable(PDmaTx);
    }
    if(WLength2 != 0) {
        if(WaitEv8() != retvOk) { Rslt = retvFail; goto WriteWriteEnd; }
        dmaStreamSetMemory0(PDmaTx, WPtr2);
        dmaStreamSetMode   (PDmaTx, PParams->DmaModeTx);
        dmaStreamSetTransactionSize(PDmaTx, WLength2);
        chSysLock();
        dmaStreamEnable(PDmaTx);
        chThdSuspendS(&ThdRef);    // Wait IRQ
        chSysUnlock();
        dmaStreamDisable(PDmaTx);
    }
    WaitBTF();
    SendStop();
    WriteWriteEnd:
    return Rslt;
}

uint8_t i2c_t::CheckAddress(uint32_t Addr) {
    uint8_t Rslt = retvFail;
    if(IBusyWait() != retvOk) {
        Rslt = retvBusy;
        Printf("i2cC Busy\r");
        goto ChckEnd;
    }
    IReset(); // Reset I2C
    // Clear flags
    PParams->pi2c->SR1 = 0;
    while(RxIsNotEmpty()) (void)PParams->pi2c->DR;   // Read DR until it empty
    ClearAddrFlag();
    // Start transmission
    SendStart();
    if(WaitEv5() == retvOk) {
        SendAddrWithWrite(Addr);
        if(WaitEv6() == retvOk) Rslt = retvOk;
        else Rslt = retvNotFound;
    }
    SendStop();
    ChckEnd:
    return Rslt;
}

uint8_t i2c_t::Write(uint8_t Addr, uint8_t *WPtr1, uint8_t WLength1) {
    uint8_t Rslt = retvOk;
    if(IBusyWait() != retvOk) { Rslt = retvBusy; goto WriteEnd; }
    // Clear flags
    PParams->pi2c->SR1 = 0;
    while(RxIsNotEmpty()) (void)PParams->pi2c->DR;   // Read DR until it empty
    ClearAddrFlag();
    // Start transmission
    SendStart();
    if(WaitEv5() != retvOk) { Rslt = retvFail; goto WriteEnd; }
    SendAddrWithWrite(Addr);
    if(WaitEv6() != retvOk) { SendStop(); Rslt = retvFail; goto WriteEnd; }
    ClearAddrFlag();
    // Start TX DMA if needed
    if(WLength1 != 0) {
        if(WaitEv8() != retvOk) { Rslt = retvFail; goto WriteEnd; }
        dmaStreamSetMemory0(PDmaTx, WPtr1);
        dmaStreamSetMode   (PDmaTx, PParams->DmaModeTx);
        dmaStreamSetTransactionSize(PDmaTx, WLength1);
        chSysLock();
        dmaStreamEnable(PDmaTx);
        chThdSuspendS(&ThdRef);    // Wait IRQ
        chSysUnlock();
        dmaStreamDisable(PDmaTx);
    }
    WaitBTF();
    SendStop();
    WriteEnd:
    return Rslt;
}

void i2c_t::ScanBus() {
    Printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
    uint8_t AddrHi, Addr;
    for(AddrHi = 0; AddrHi < 0x80; AddrHi += 0x10) {
        Printf("\r%02X: ", AddrHi);
        for(uint8_t n=0; n<0x10; n++) {
            Addr = AddrHi + n;
            if(Addr <= 0x01 or Addr > 0x77) Printf("   ");
            else {
                // Try to get response from addr
                if(IBusyWait() != retvOk) {
                    Printf("i2cBusyWait\r");
                    return;
                }
                // Clear flags
                PParams->pi2c->SR1 = 0;
                while(RxIsNotEmpty()) (void)PParams->pi2c->DR;   // Read DR until it empty
                ClearAddrFlag();
                // Start transmission
                SendStart();
                if(WaitEv5() != retvOk) continue;
                SendAddrWithWrite(Addr);
                if(WaitEv6() == retvOk) Printf("%02X ", Addr);
                else Printf("__ ");
                SendStop();
            }
        } // for n
    } // for AddrHi
    PrintfEOL();
}

// ==== Flag operations ====
#define RETRY_CNT_LONG      450
#define RETRY_CNT_SHORT     450
// Busy flag
uint8_t i2c_t::IBusyWait() {
    uint8_t RetryCnt = 4;
    while(RetryCnt--) {
        if(!(PParams->pi2c->SR2 & I2C_SR2_BUSY)) return retvOk;
        chThdSleepMilliseconds(1);
    }
    Error = true;
    return retvTimeout;
}

// BUSY, MSL & SB flags
uint8_t i2c_t::WaitEv5() {
    uint32_t RetryCnt = RETRY_CNT_LONG;
    while(RetryCnt--) {
        uint16_t Flag1 = PParams->pi2c->SR1;
        uint16_t Flag2 = PParams->pi2c->SR2;
        if((Flag1 & I2C_SR1_SB) and (Flag2 & (I2C_SR2_MSL | I2C_SR2_BUSY))) return retvOk;
    }
    Error = true;
    return retvFail;
}

uint8_t i2c_t::WaitEv6() {
    uint32_t RetryCnt = RETRY_CNT_SHORT;
    uint16_t Flag1;
    do {
        Flag1 = PParams->pi2c->SR1;
        if((RetryCnt-- == 0) or (Flag1 & I2C_SR1_AF)) return retvFail;   // Fail if timeout or NACK
    } while(!(Flag1 & I2C_SR1_ADDR)); // ADDR set when Address is sent and ACK received
    return retvOk;
}

uint8_t i2c_t::WaitEv8() {
    uint32_t RetryCnt = RETRY_CNT_SHORT;
    while(RetryCnt--)
        if(PParams->pi2c->SR1 & I2C_SR1_TXE) return retvOk;
    Error = true;
    return retvTimeout;
}

uint8_t i2c_t::WaitRx() {
    uint32_t RetryCnt = RETRY_CNT_LONG;
    while(RetryCnt--)
        if(PParams->pi2c->SR1 & I2C_SR1_RXNE) return retvOk;
    return retvTimeout;
}

uint8_t i2c_t::WaitStop() {
    uint32_t RetryCnt = RETRY_CNT_LONG;
    while(RetryCnt--)
        if(PParams->pi2c->CR1 & I2C_CR1_STOP) return retvOk;
    return retvTimeout;
}

uint8_t i2c_t::WaitBTF() {
    uint32_t RetryCnt = RETRY_CNT_LONG;
    while(RetryCnt--)
        if(PParams->pi2c->SR1 & I2C_SR1_BTF) return retvOk;
    return retvTimeout;
}
#endif // MCU type
