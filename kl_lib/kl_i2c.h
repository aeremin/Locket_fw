#pragma once

#include "kl_lib.h"
#include "board.h"

#if defined STM32L1XX || defined STM32F2XX || defined STM32F4XX
struct i2cParams_t {
    I2C_TypeDef *pi2c;
    GPIO_TypeDef *PGpio;
    uint16_t SclPin;
    uint16_t SdaPin;
    uint32_t BitrateHz;
    // DMA
    uint32_t DmaTxID, DmaRxID;
    uint32_t DmaModeTx, DmaModeRx;
};

class i2c_t {
private:
    const i2cParams_t *PParams;
    const stm32_dma_stream_t *PDmaTx, *PDmaRx;
    void IReset();
    void SendStart()     { PParams->pi2c->CR1 |= I2C_CR1_START; }
    void SendStop()      { PParams->pi2c->CR1 |= I2C_CR1_STOP; }
    void AckEnable()     { PParams->pi2c->CR1 |= I2C_CR1_ACK; }
    void AckDisable()    { PParams->pi2c->CR1 &= ~I2C_CR1_ACK; }
    bool RxIsNotEmpty()  { return (PParams->pi2c->SR1 & I2C_SR1_RXNE); }
    void ClearAddrFlag() { (void)PParams->pi2c->SR1; (void)PParams->pi2c->SR2; }
    void SignalLastDmaTransfer() { PParams->pi2c->CR2 |= I2C_CR2_LAST; }
    // Address and data
    void SendAddrWithWrite(uint8_t Addr) { PParams->pi2c->DR = (uint8_t)(Addr<<1); }
    void SendAddrWithRead (uint8_t Addr) { PParams->pi2c->DR = ((uint8_t)(Addr<<1)) | 0x01; }
    void SendData(uint8_t b) { PParams->pi2c->DR = b; }
    uint8_t ReceiveData() { return PParams->pi2c->DR; }
    // Flags operations
    uint8_t IBusyWait();
    uint8_t WaitEv5();
    uint8_t WaitEv6();
    uint8_t WaitEv8();
    uint8_t WaitAck();
    uint8_t WaitRx();
    uint8_t WaitStop();
    uint8_t WaitBTF();
public:
    bool Error;
    thread_reference_t ThdRef;
    void Init();
    void ScanBus();
    void Standby();
    void Resume();
    void Reset() {
        Standby();
        Resume();
    }
    uint8_t CheckAddress(uint32_t Addr);
    uint8_t Write     (uint8_t Addr, uint8_t *WPtr1, uint8_t WLength1);
    uint8_t WriteRead (uint8_t Addr, uint8_t *WPtr,  uint8_t WLength,  uint8_t *RPtr, uint8_t RLength);
    uint8_t WriteWrite(uint8_t Addr, uint8_t *WPtr1, uint8_t WLength1, uint8_t *WPtr2, uint8_t WLength2);
    i2c_t(const i2cParams_t *APParams) : PParams(APParams),
                Error(false), ThdRef(nullptr) {}
};

extern i2c_t i2c1;
#if I2C2_ENABLED
extern i2c_t i2c2;
#endif

#endif // MCU type
