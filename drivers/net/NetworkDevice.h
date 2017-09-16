/*
 *	Upanix - An x86 based Operating System
 *  Copyright (C) 2011 'Prajwala Prabhakar' 'srinivasa_prajwal@yahoo.co.in'
 *                                                                          
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *                                                                          
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                          
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/
 */
#pragma once

class PCIEntry;

//as per ieee 802.11
class NetworkDevice
{
public:
  NetworkDevice(PCIEntry& pciEntry);
  virtual ~NetworkDevice() = 0;

//void ath9k_tx(struct ieee80211_hw *hw, struct ieee80211_tx_control *control, struct sk_buff *skb)
  virtual void Tx() = 0;
//int ath9k_start(struct ieee80211_hw *hw)
  virtual int Start() = 0;
//void ath9k_stop(struct ieee80211_hw *hw)
  virtual void Stop() = 0;
//int ath9k_add_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
  virtual int AddInterface() = 0;
//int ath9k_change_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif, enum nl80211_iftype new_type, bool p2p)
  virtual int ChangeInterface() = 0;
//void ath9k_remove_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
  virtual void RemoveInterface() = 0;
//int ath9k_config(struct ieee80211_hw *hw, u32 changed)
  virtual int Configure() = 0;
//void ath9k_configure_filter(struct ieee80211_hw *hw, unsigned int changed_flags, unsigned int *total_flags, u64 multicast)
  virtual void ConfigureFilter() = 0;
//int ath9k_sta_state(struct ieee80211_hw *hw, struct ieee80211_vif *vif, struct ieee80211_sta *sta, enum ieee80211_sta_state old_state, enum ieee80211_sta_state new_state)
  virtual int STAState() = 0;
//void ath9k_sta_notify(struct ieee80211_hw *hw, struct ieee80211_vif *vif, enum sta_notify_cmd cmd, struct ieee80211_sta *sta)
  virtual void STANotify() = 0;
//int ath9k_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u16 queue, const struct ieee80211_tx_queue_params *params)
  virtual void ConfigureTx() = 0;
//void ath9k_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif, struct ieee80211_bss_conf *bss_conf, u32 changed)
  virtual void BSSInfoChanged() = 0;
//int ath9k_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd, struct ieee80211_vif *vif, struct ieee80211_sta *sta, struct ieee80211_key_conf *key)
  virtual int SetKey() = 0;
//u64 ath9k_get_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
	virtual uint64_t GetTSF() = 0;
//void ath9k_set_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u64 tsf)
  virtual void SetTFS() = 0;
//void ath9k_reset_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
	virtual void ResetTSF() = 0;
//int ath9k_ampdu_action(struct ieee80211_hw *hw, struct ieee80211_vif *vif, struct ieee80211_ampdu_params *params)
	virtual int AMPDUAction() = 0;
//int ath9k_get_survey(struct ieee80211_hw *hw, int idx, struct survey_info *survey)
	virtual int GetSurvey() = 0;
//void ath9k_set_coverage_class(struct ieee80211_hw *hw, s16 coverage_class)
	virtual void SetCoverageClass() = 0;
//void ath9k_flush(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u32 queues, bool drop)
	virtual void Flush() = 0;
//bool ath9k_tx_frames_pending(struct ieee80211_hw *hw)
	virtual bool TxFramesPending() = 0;
//int ath9k_tx_last_beacon(struct ieee80211_hw *hw)
	virtual int TxLastBeacon() = 0;
//int ath9k_get_stats(struct ieee80211_hw *hw, struct ieee80211_low_level_stats *stats) static int ath9k_set_antenna(struct ieee80211_hw *hw, u32 tx_ant, u32 rx_ant)
	virtual int GetStats() = 0;
//int ath9k_get_antenna(struct ieee80211_hw *hw, u32 *tx_ant, u32 *rx_ant)
	virtual int GetAntenna() = 0;
//void ath9k_release_buffered_frames(struct ieee80211_hw *hw, struct ieee80211_sta *sta, u16 tids, int nframes, enum ieee80211_frame_release_type reason, bool more_data)
	virtual void ReleaseBufferedFrames() = 0;
//void ath9k_sw_scan_start(struct ieee80211_hw *hw, struct ieee80211_vif *vif, const u8 *mac_addr)
	virtual void SWScanStart() = 0;
//void ath9k_sw_scan_complete(struct ieee80211_hw *hw, struct ieee80211_vif *vif) static int ath9k_get_txpower(struct ieee80211_hw *hw, struct ieee80211_vif *vif, int *dbm)
	virtual void SWScanComplete() = 0;
//void ath9k_wake_tx_queue(struct ieee80211_hw *hw, struct ieee80211_txq *queue)
	virtual void WakeTxQueue() = 0;
private:
  PCIEntry& _pciEntry;
};
