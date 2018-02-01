/* add your code here */





#include <net/if.h>

extern "C" {
#include "iwlwifi/iwl-csr.h"
#include "iwl-drv.h"
#include "iwl-trans.h"
#include "iwl-fh.h"
#include "iwl-prph.h"
#include "iwl-config.h"
#include "iwl-eeprom-parse.h"
#include "iwlwifi/pcie/internal.h"
#include "iwlwifi/iwl-scd.h"
}

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOPacketQueue.h>
#include <IOKit/IOMemoryCursor.h>

#include "Logging.h"

#include "IwlTransOps.h"
#include "IwlOpModeOps.h"



// Configuration
#define CONFIG_IWLMVM // Need NVM mode at least to see that code is compiling
#define CONFIG_IWLWIFI_PCIE_RTPM // Powerman

#define    RELEASE(x)    if(x){(x)->release();(x)=NULL;}


enum {
    kOffPowerState,
    kOnPowerState,
    kNumPowerStates
};

static IOPMPowerState gPowerStates[kNumPowerStates] = {
    // kOffPowerState
    {kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // kOnPowerState
    {kIOPMPowerStateVersion1, (kIOPMPowerOn | kIOPMDeviceUsable), kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};



class IntelWifi : public IOEthernetController, public IwlTransOps
{
    OSDeclareDefaultStructors(IntelWifi)
    
public:
    virtual int start_hw(struct iwl_trans *trans, bool low_power) override;
    virtual int start_fw(struct iwl_trans *trans, const struct fw_img *fw, bool run_in_rfkill) override;
    virtual void op_mode_leave(struct iwl_trans *trans) override;
    virtual void set_pmi(struct iwl_trans *trans, bool state) override;
    virtual void configure(struct iwl_trans *trans, const struct iwl_trans_config *trans_cfg) override;
    virtual void fw_alive(struct iwl_trans *trans, u32 scd_addr) override;
    virtual void stop_device(struct iwl_trans *trans, bool low_power) override;
    virtual bool txq_enable(struct iwl_trans *trans, int queue, u16 ssn,
                               const struct iwl_trans_txq_scd_cfg *cfg,
                                       unsigned int queue_wdg_timeout) override;
    virtual void txq_disable(struct iwl_trans *trans, int queue, bool configure_scd) override;
    int send_cmd(struct iwl_trans *trans, struct iwl_host_cmd *cmd) override;

    
    virtual bool init(OSDictionary *properties) override;
    virtual void free() override;
    
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    
    virtual bool configureInterface(IONetworkInterface* netif) override;
    
    virtual IOReturn enable(IONetworkInterface* netif) override;
    virtual IOReturn disable(IONetworkInterface* netif) override;
    virtual IOReturn getHardwareAddress(IOEthernetAddress* addrP) override;
    virtual IOReturn setHardwareAddress(const IOEthernetAddress* addrP) override;
    
    virtual IOReturn setPromiscuousMode(bool active) override
    {
        return kIOReturnSuccess;
    }
    
    virtual IOReturn setMulticastMode(bool active) override
    {
        return kIOReturnSuccess;
    }
    
    bool createMediumDict();
    
    virtual const OSString* newVendorString() const override;
    virtual const OSString* newModelString() const override;
    
protected:
    IOPCIDevice *pciDevice;
    IOEthernetInterface *netif;
    IOWorkLoop *fWorkLoop;
    OSDictionary *mediumDict;
    
    IONetworkStats *fNetworkStats;
    IOEthernetStats *fEthernetStats;
    IOFilterInterruptEventSource* fInterruptSource;
    
    IOMemoryMap *fMemoryMap;
    
    struct iwl_nvm_data *fNvmData;
    const struct iwl_cfg* fConfiguration;
    struct iwl_trans* fTrans;
    
    
private:
    inline void releaseAll() {
        RELEASE(fInterruptSource);
        RELEASE(fWorkLoop);
        RELEASE(mediumDict);
        
        RELEASE(fMemoryMap);
        if (fTrans) {
            iwl_trans_pcie_free(fTrans);
            fTrans = NULL;
        }
        
        RELEASE(pciDevice);
    }
    
    static void interruptOccured(OSObject* owner, IOInterruptEventSource* sender, int count);
    static bool interruptFilter(OSObject* owner, IOFilterInterruptEventSource * src);
    static IOReturn gateAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
    
    int findMSIInterruptTypeIndex();
    
    // trans.c
    void iwl_pcie_set_pwr(struct iwl_trans *trans, bool vaux); // line 186
    void iwl_pcie_apm_config(struct iwl_trans *trans); // line 204
    int iwl_pcie_apm_init(struct iwl_trans *trans); // line 232
    void iwl_pcie_apm_stop(struct iwl_trans *trans, bool op_mode_leave); // line 460
    int iwl_pcie_nic_init(struct iwl_trans *trans); // line 505

    void iwl_pcie_apply_destination(struct iwl_trans *trans); // line 826
    int iwl_pcie_load_given_ucode(struct iwl_trans *trans, const struct fw_img *image); // line 900
    int iwl_pcie_load_given_ucode_8000(struct iwl_trans *trans, const struct fw_img *image); // line 952
    bool iwl_pcie_check_hw_rf_kill(struct iwl_trans *trans); // line 989

    void iwl_pcie_init_msix(struct iwl_trans_pcie *trans_pcie); // line 1115
    
    void _iwl_trans_pcie_stop_device(struct iwl_trans *trans, bool low_power); // line 1130
    
    int iwl_trans_pcie_start_fw(struct iwl_trans *trans, const struct fw_img *fw, bool run_in_rfkill); // line 1224
    
    void iwl_trans_pcie_fw_alive(struct iwl_trans *trans, u32 scd_addr); // line 1312
    void iwl_trans_pcie_handle_stop_rfkill(struct iwl_trans *trans, bool was_in_rfkill); // line 1318
    void iwl_trans_pcie_stop_device(struct iwl_trans *trans, bool low_power); // line 1347
    void iwl_trans_pcie_rf_kill(struct iwl_trans *trans, bool state); // line 1360
    void iwl_pcie_set_interrupt_capa(struct iwl_trans *trans); // line 1489
    int _iwl_trans_pcie_start_hw(struct iwl_trans *trans, bool low_power); // line 1636
    int iwl_trans_pcie_start_hw(struct iwl_trans *trans, bool low_power); // line 1675
    void iwl_trans_pcie_op_mode_leave(struct iwl_trans *trans); // line 1687
    
    void iwl_trans_pcie_free(struct iwl_trans* trans); // line 1776
    
    struct iwl_trans* iwl_trans_pcie_alloc(const struct iwl_cfg *cfg); // line 2988
    
    // trans-gen2.c
    int iwl_pcie_gen2_apm_init(struct iwl_trans *trans);
    void iwl_pcie_gen2_apm_stop(struct iwl_trans *trans, bool op_mode_leave);
    void _iwl_trans_pcie_gen2_stop_device(struct iwl_trans *trans, bool low_power);
    void iwl_trans_pcie_gen2_stop_device(struct iwl_trans *trans, bool low_power);
    int iwl_pcie_gen2_nic_init(struct iwl_trans *trans);
    void iwl_trans_pcie_gen2_fw_alive(struct iwl_trans *trans, u32 scd_addr);
    int iwl_trans_pcie_gen2_start_fw(struct iwl_trans *trans,
                                     const struct fw_img *fw, bool run_in_rfkill);

    

    
    // rx.c
    irqreturn_t iwl_pcie_irq_handler(int irq, void *dev_id);
    
    void iwl_pcie_handle_rfkill_irq(struct iwl_trans *trans);
    void iwl_pcie_irq_handle_error(struct iwl_trans *trans);
    
    int iwl_pcie_rx_init(struct iwl_trans *trans);
    void iwl_pcie_rx_handle(struct iwl_trans *trans, int queue);
    void iwl_pcie_rx_handle_rb(struct iwl_trans *trans,
                                          struct iwl_rxq *rxq,
                                          struct iwl_rx_mem_buffer *rxb,
                                          bool emergency);
    
    // tx.c
    int iwl_pcie_txq_alloc(struct iwl_trans *trans, struct iwl_txq *txq, int slots_num, bool cmd_queue); // line 487
    int iwl_pcie_txq_init(struct iwl_trans *trans, struct iwl_txq *txq, int slots_num, bool cmd_queue); // line 551
    void iwl_pcie_tx_start(struct iwl_trans *trans, u32 scd_base_addr); // line 715
    int iwl_pcie_tx_alloc(struct iwl_trans *trans); // line 907
    int iwl_pcie_tx_init(struct iwl_trans *trans); // line 973
    void iwl_pcie_txq_progress(struct iwl_txq *txq); // line 1034
    void iwl_pcie_cmdq_reclaim(struct iwl_trans *trans, int txq_id, int idx); // line 1211
    bool iwl_trans_pcie_txq_enable(struct iwl_trans *trans, int txq_id, u16 ssn,
                                   const struct iwl_trans_txq_scd_cfg *cfg,
                                   unsigned int wdg_timeout); // line 1283
    void iwl_trans_pcie_txq_disable(struct iwl_trans *trans, int txq_id, bool configure_scd); // line 1404
    int iwl_pcie_enqueue_hcmd(struct iwl_trans *trans,
                                         struct iwl_host_cmd *cmd); // line 1440
    void iwl_pcie_hcmd_complete(struct iwl_trans *trans,
                                           struct iwl_rx_cmd_buffer *rxb); // line 1723
    int iwl_pcie_send_hcmd_async(struct iwl_trans *trans,
                                 struct iwl_host_cmd *cmd);
    
    int iwl_pcie_send_hcmd_sync(struct iwl_trans *trans,
                                struct iwl_host_cmd *cmd); // line 1829
    int iwl_trans_pcie_send_hcmd(struct iwl_trans *trans, struct iwl_host_cmd *cmd); // line 1935
    int iwl_trans_pcie_tx(struct iwl_trans *trans, struct sk_buff *skb,
                          struct iwl_device_cmd *dev_cmd, int txq_id); // line 2256
    
    // other
    
    inline void iwl_trans_txq_enable(struct iwl_trans *trans, int queue,
                                             int fifo, int sta_id, int tid,
                                             int frame_limit, u16 ssn,
                                     unsigned int queue_wdg_timeout) {
        struct iwl_trans_txq_scd_cfg cfg = {
            .fifo = (u8)fifo,
            .sta_id = (u8)sta_id,
            .tid = (u8)tid,
            .frame_limit = frame_limit,
            .aggregate = sta_id >= 0,
        };
        
        txq_enable(trans, queue, ssn, &cfg, queue_wdg_timeout);
    }
    inline void iwl_trans_ac_txq_enable(struct iwl_trans *trans, int queue, int fifo,
                     unsigned int queue_wdg_timeout)
    {
        struct iwl_trans_txq_scd_cfg cfg = {
            .fifo = (u8)fifo,
            .sta_id = (u8)-1,
            .tid = IWL_MAX_TID_COUNT,
            .frame_limit = IWL_FRAME_LIMIT,
            .aggregate = false,
        };
    
        txq_enable(trans, queue, 0, &cfg, queue_wdg_timeout);
    }

    
    
    ifnet_t fIfNet;
    
    UInt16 fDeviceId;
    UInt16 fSubsystemId;
    
public:IwlOpModeOps *opmode;
private:
    struct ieee80211_hw *hw;
    IOCommandGate *gate;
  
};
